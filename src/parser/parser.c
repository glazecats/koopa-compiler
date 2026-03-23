#include "parser.h"
#include "ast_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const TokenArray *tokens;
    size_t current;
    ParserError *error;
    int has_error;
    size_t error_index;
    size_t expression_recursion_depth;
    size_t expression_recursion_limit;
    size_t statement_recursion_depth;
    size_t statement_recursion_limit;
    size_t loop_depth;
    int track_function_return_count;
    size_t function_return_count;
    int track_function_loop_count;
    size_t function_loop_count;
    int track_function_if_count;
    size_t function_if_count;
    int track_function_break_count;
    size_t function_break_count;
    int track_function_continue_count;
    size_t function_continue_count;
    int track_function_declaration_count;
    size_t function_declaration_count;
} Parser;

/*
 * Protect recursive-descent hotspots from stack overflow.
 * If the nesting depth is unreasonable, fail gracefully with a parser error.
 */
#define PARSER_EXPRESSION_RECURSION_LIMIT 2048U
#define PARSER_STATEMENT_RECURSION_LIMIT 2048U

static const Token *peek(const Parser *p) {
    if (p->current >= p->tokens->size) {
        return &p->tokens->data[p->tokens->size - 1];
    }
    return &p->tokens->data[p->current];
}

static const Token *previous(const Parser *p) {
    if (p->current == 0) {
        return &p->tokens->data[0];
    }
    return &p->tokens->data[p->current - 1];
}

static int is_at_end(const Parser *p) {
    return peek(p)->type == TOKEN_EOF;
}

static const Token *advance(Parser *p) {
    if (!is_at_end(p)) {
        p->current++;
    }
    return previous(p);
}

static int check(const Parser *p, TokenType type) {
    if (is_at_end(p)) {
        return 0;
    }
    return peek(p)->type == type;
}

static int match(Parser *p, TokenType type) {
    if (!check(p, type)) {
        return 0;
    }
    advance(p);
    return 1;
}

static void set_error(Parser *p, const Token *at, const char *fmt, ...) {
    size_t idx;
    va_list args;

    if (!p->error) {
        return;
    }

    if (at < p->tokens->data) {
        idx = 0;
    } else if (at >= p->tokens->data + p->tokens->size) {
        idx = p->tokens->size - 1;
    } else {
        idx = (size_t)(at - p->tokens->data);
    }

    if (p->has_error && idx < p->error_index) {
        return;
    }

    if (!p->has_error || idx > p->error_index) {
        p->has_error = 1;
        p->error_index = idx;
    }

    p->error->line = at->line;
    p->error->column = at->column;

    va_start(args, fmt);
    vsnprintf(p->error->message, sizeof(p->error->message), fmt, args);
    va_end(args);
}

static int enter_expression_recursion(Parser *p, const char *rule_name) {
    if (p->expression_recursion_depth >= p->expression_recursion_limit) {
        set_error(p,
                  peek(p),
                  "Parser expression recursion limit (%zu) exceeded while parsing %s",
                  p->expression_recursion_limit,
                  rule_name);
        return 0;
    }
    p->expression_recursion_depth++;
    return 1;
}

static void leave_expression_recursion(Parser *p) {
    if (p->expression_recursion_depth > 0) {
        p->expression_recursion_depth--;
    }
}

static int enter_statement_recursion(Parser *p, const char *rule_name) {
    if (p->statement_recursion_depth >= p->statement_recursion_limit) {
        set_error(p,
                  peek(p),
                  "Parser statement recursion limit (%zu) exceeded while parsing %s",
                  p->statement_recursion_limit,
                  rule_name);
        return 0;
    }
    p->statement_recursion_depth++;
    return 1;
}

static void leave_statement_recursion(Parser *p) {
    if (p->statement_recursion_depth > 0) {
        p->statement_recursion_depth--;
    }
}

static int consume_token(Parser *p, TokenType type, const char *what, const Token **out_token);

static int consume(Parser *p, TokenType type, const char *what) {
    return consume_token(p, type, what, NULL);
}

static int consume_token(Parser *p, TokenType type, const char *what, const Token **out_token) {
    const Token *at;

    if (check(p, type)) {
        at = peek(p);
        if (out_token) {
            *out_token = at;
        }
        advance(p);
        return 1;
    }
    if (out_token) {
        *out_token = NULL;
    }
    set_error(p, peek(p), "Expected %s, got %s", what, lexer_token_type_name(peek(p)->type));
    return 0;
}

static int parse_expression(Parser *p);
static int parse_statement_with_flow(Parser *p,
                                     int *out_guaranteed_return,
                                     int *out_may_fallthrough,
                                     int *out_may_break);
static int parse_declaration(Parser *p, AstProgram *top_level_program);

static AstExpression *parse_expression_ast_primary_only(Parser *p);
static AstExpression *parse_expression_ast_postfix(Parser *p);
static AstExpression *parse_expression_ast_unary(Parser *p);
static AstExpression *parse_expression_ast_multiplicative(Parser *p);
static AstExpression *parse_expression_ast_additive(Parser *p);
static AstExpression *parse_expression_ast_shift(Parser *p);
static AstExpression *parse_expression_ast_relational(Parser *p);
static AstExpression *parse_expression_ast_equality(Parser *p);
static AstExpression *parse_expression_ast_bitwise_and(Parser *p);
static AstExpression *parse_expression_ast_bitwise_xor(Parser *p);
static AstExpression *parse_expression_ast_bitwise_or(Parser *p);
static AstExpression *parse_expression_ast_logical_and(Parser *p);
static AstExpression *parse_expression_ast_logical_or(Parser *p);
static AstExpression *parse_expression_ast_conditional(Parser *p);
static AstExpression *parse_expression_ast_assignment(Parser *p);
static AstExpression *parse_expression_ast_comma(Parser *p);

static int parse_assignment(Parser *p);
static int parse_comma(Parser *p);

static int match_assignment_operator(Parser *p, const Token **out_op_tok) {
    if (match(p, TOKEN_ASSIGN) || match(p, TOKEN_PLUS_ASSIGN) || match(p, TOKEN_MINUS_ASSIGN) ||
        match(p, TOKEN_STAR_ASSIGN) || match(p, TOKEN_SLASH_ASSIGN) ||
        match(p, TOKEN_PERCENT_ASSIGN) || match(p, TOKEN_AMP_ASSIGN) ||
        match(p, TOKEN_CARET_ASSIGN) || match(p, TOKEN_PIPE_ASSIGN) ||
        match(p, TOKEN_SHIFT_LEFT_ASSIGN) || match(p, TOKEN_SHIFT_RIGHT_ASSIGN)) {
        if (out_op_tok) {
            *out_op_tok = previous(p);
        }
        return 1;
    }
    if (out_op_tok) {
        *out_op_tok = NULL;
    }
    return 0;
}

static int token_slice_has_balanced_parens(const TokenArray *tokens,
                                           size_t start,
                                           size_t end,
                                           int *out_zero_depth_inside) {
    size_t i;
    int depth = 0;
    int zero_depth_inside = 0;

    if (!tokens || !tokens->data || start >= end) {
        if (out_zero_depth_inside) {
            *out_zero_depth_inside = 1;
        }
        return 0;
    }

    for (i = start; i < end; ++i) {
        if (tokens->data[i].type == TOKEN_LPAREN) {
            depth++;
        } else if (tokens->data[i].type == TOKEN_RPAREN) {
            if (depth == 0) {
                return 0;
            }
            depth--;
            if (depth == 0 && i + 1 < end) {
                zero_depth_inside = 1;
            }
        }
    }

    if (depth != 0) {
        return 0;
    }

    if (out_zero_depth_inside) {
        *out_zero_depth_inside = zero_depth_inside;
    }
    return 1;
}

static void trim_wrapping_parentheses(const TokenArray *tokens, size_t *io_start, size_t *io_end) {
    size_t start;
    size_t end;
    int zero_depth_inside;

    if (!tokens || !tokens->data || !io_start || !io_end) {
        return;
    }

    start = *io_start;
    end = *io_end;

    while (end > start + 1 && tokens->data[start].type == TOKEN_LPAREN &&
           tokens->data[end - 1].type == TOKEN_RPAREN &&
           token_slice_has_balanced_parens(tokens, start, end, &zero_depth_inside) &&
           !zero_depth_inside) {
        start++;
        end--;
    }

    *io_start = start;
    *io_end = end;
}

static int token_slice_is_parenthesized_identifier_form(const TokenArray *tokens,
                                                        size_t start,
                                                        size_t end) {
    trim_wrapping_parentheses(tokens, &start, &end);
    return end == start + 1 && tokens->data[start].type == TOKEN_IDENTIFIER;
}

static int token_slice_is_callable_form(const TokenArray *tokens,
                                        size_t start,
                                        size_t end) {
    size_t i;

    if (!tokens || !tokens->data || start >= end) {
        return 0;
    }

    trim_wrapping_parentheses(tokens, &start, &end);

    if (end <= start || tokens->data[start].type != TOKEN_IDENTIFIER) {
        return 0;
    }

    i = start + 1;
    while (i < end) {
        size_t j;
        int depth = 0;

        if (tokens->data[i].type != TOKEN_LPAREN) {
            return 0;
        }

        for (j = i; j < end; ++j) {
            if (tokens->data[j].type == TOKEN_LPAREN) {
                depth++;
            } else if (tokens->data[j].type == TOKEN_RPAREN) {
                if (depth == 0) {
                    return 0;
                }
                depth--;
                if (depth == 0) {
                    i = j + 1;
                    break;
                }
            }
        }

        if (depth != 0) {
            return 0;
        }
    }

    return i == end;
}

static int parse_initializer_expression(Parser *p) {
    /* C declarator initializers use assignment-expression, not comma-expression. */
    return parse_assignment(p);
}

static AstExpression *alloc_ast_expression(AstExpressionKind kind,
                                           int line,
                                           int column) {
    AstExpression *expr = (AstExpression *)malloc(sizeof(AstExpression));
    if (!expr) {
        return NULL;
    }
    expr->kind = kind;
    expr->line = line;
    expr->column = column;
    expr->as.identifier.name = NULL;
    expr->as.identifier.name_length = 0;
    return expr;
}

static AstExpression *build_binary_expression_node(Parser *p,
                                                   const Token *op_tok,
                                                   AstExpression *left,
                                                   AstExpression *right) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_BINARY, op_tok->line, op_tok->column);
    if (!expr) {
        set_error(p, op_tok, "Out of memory while building expression AST");
        ast_expression_free_internal(left);
        ast_expression_free_internal(right);
        return NULL;
    }

    expr->as.binary.op = op_tok->type;
    expr->as.binary.left = left;
    expr->as.binary.right = right;
    return expr;
}

static AstExpression *build_unary_expression_node(Parser *p,
                                                  const Token *op_tok,
                                                  AstExpression *operand) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_UNARY, op_tok->line, op_tok->column);
    if (!expr) {
        set_error(p, op_tok, "Out of memory while building expression AST");
        ast_expression_free_internal(operand);
        return NULL;
    }

    expr->as.unary.op = op_tok->type;
    expr->as.unary.operand = operand;
    return expr;
}

static AstExpression *build_postfix_expression_node(Parser *p,
                                                    const Token *op_tok,
                                                    AstExpression *operand) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_POSTFIX, op_tok->line, op_tok->column);
    if (!expr) {
        set_error(p, op_tok, "Out of memory while building expression AST");
        ast_expression_free_internal(operand);
        return NULL;
    }

    expr->as.postfix.op = op_tok->type;
    expr->as.postfix.operand = operand;
    return expr;
}

static void free_ast_expression_array(AstExpression **items, size_t count) {
    size_t i;

    if (!items) {
        return;
    }

    for (i = 0; i < count; ++i) {
        ast_expression_free_internal(items[i]);
    }

    free(items);
}

static int append_ast_expression_arg(Parser *p,
                                     AstExpression ***items,
                                     size_t *count,
                                     size_t *capacity,
                                     AstExpression *arg) {
    AstExpression **new_items;
    size_t next_capacity;

    if (!items || !count || !capacity || !arg) {
        return 0;
    }

    if (*count == *capacity) {
        next_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        new_items = (AstExpression **)realloc(*items, next_capacity * sizeof(AstExpression *));
        if (!new_items) {
            set_error(p, peek(p), "Out of memory while building expression AST");
            ast_expression_free_internal(arg);
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[*count] = arg;
    (*count)++;
    return 1;
}

static AstExpression *build_call_expression_node(Parser *p,
                                                 const Token *lparen_tok,
                                                 AstExpression *callee,
                                                 AstExpression **args,
                                                 size_t arg_count) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_CALL, lparen_tok->line, lparen_tok->column);
    if (!expr) {
        set_error(p, lparen_tok, "Out of memory while building expression AST");
        ast_expression_free_internal(callee);
        free_ast_expression_array(args, arg_count);
        return NULL;
    }

    expr->as.call.callee = callee;
    expr->as.call.args = args;
    expr->as.call.arg_count = arg_count;
    return expr;
}

static int ast_expression_is_identifier_lvalue(const AstExpression *expr) {
    if (!expr) {
        return 0;
    }

    if (expr->kind == AST_EXPR_IDENTIFIER) {
        return 1;
    }

    if (expr->kind == AST_EXPR_PAREN) {
        return ast_expression_is_identifier_lvalue(expr->as.inner);
    }

    return 0;
}

static int ast_expression_is_callable_callee(const AstExpression *expr) {
    if (!expr) {
        return 0;
    }

    if (expr->kind == AST_EXPR_IDENTIFIER || expr->kind == AST_EXPR_CALL) {
        return 1;
    }

    if (expr->kind == AST_EXPR_PAREN) {
        return ast_expression_is_callable_callee(expr->as.inner);
    }

    return 0;
}

static AstExpression *build_ternary_expression_node(Parser *p,
                                                    const Token *question_tok,
                                                    AstExpression *condition,
                                                    AstExpression *then_expr,
                                                    AstExpression *else_expr) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_TERNARY,
                                               question_tok->line,
                                               question_tok->column);
    if (!expr) {
        set_error(p, question_tok, "Out of memory while building expression AST");
        ast_expression_free_internal(condition);
        ast_expression_free_internal(then_expr);
        ast_expression_free_internal(else_expr);
        return NULL;
    }

    expr->as.ternary.condition = condition;
    expr->as.ternary.then_expr = then_expr;
    expr->as.ternary.else_expr = else_expr;
    return expr;
}

static AstExpression *parse_parenthesized_expression_ast_primary_only(Parser *p,
                                                                      const Token *lparen) {
    AstExpression *inner;
    AstExpression *paren_expr;

    inner = parse_expression_ast_comma(p);
    if (!inner) {
        return NULL;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        ast_expression_free_internal(inner);
        return NULL;
    }

    paren_expr = alloc_ast_expression(AST_EXPR_PAREN, lparen->line, lparen->column);
    if (!paren_expr) {
        set_error(p, lparen, "Out of memory while building expression AST");
        ast_expression_free_internal(inner);
        return NULL;
    }
    paren_expr->as.inner = inner;
    return paren_expr;
}

static AstExpression *parse_expression_ast_primary_only(Parser *p) {
    const Token *tok;
    AstExpression *expr;

    if (!enter_expression_recursion(p, "primary-expression-ast")) {
        return NULL;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        tok = previous(p);
        expr = alloc_ast_expression(AST_EXPR_IDENTIFIER, tok->line, tok->column);
        if (!expr) {
            set_error(p, tok, "Out of memory while building expression AST");
            leave_expression_recursion(p);
            return NULL;
        }
        expr->as.identifier.name = (char *)malloc(tok->length + 1);
        if (!expr->as.identifier.name) {
            set_error(p, tok, "Out of memory while building expression AST");
            ast_expression_free_internal(expr);
            leave_expression_recursion(p);
            return NULL;
        }
        memcpy(expr->as.identifier.name, tok->lexeme, tok->length);
        expr->as.identifier.name[tok->length] = '\0';
        expr->as.identifier.name_length = tok->length;
        leave_expression_recursion(p);
        return expr;
    }

    if (match(p, TOKEN_NUMBER)) {
        tok = previous(p);
        expr = alloc_ast_expression(AST_EXPR_NUMBER, tok->line, tok->column);
        if (!expr) {
            set_error(p, tok, "Out of memory while building expression AST");
            leave_expression_recursion(p);
            return NULL;
        }
        expr->as.number_value = tok->number_value;
        leave_expression_recursion(p);
        return expr;
    }

    if (match(p, TOKEN_LPAREN)) {
        tok = previous(p);
        expr = parse_parenthesized_expression_ast_primary_only(p, tok);
        leave_expression_recursion(p);
        return expr;
    }

    set_error(p, peek(p), "Expected primary expression, got %s", lexer_token_type_name(peek(p)->type));
    leave_expression_recursion(p);
    return NULL;
}

static AstExpression *parse_expression_ast_unary(Parser *p) {
    AstExpression *expr;
    const Token *op_tok;

    if (!enter_expression_recursion(p, "unary-expression-ast")) {
        return NULL;
    }

    if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS_MINUS)) {
        op_tok = previous(p);
        expr = parse_expression_ast_unary(p);
        if (!expr) {
            leave_expression_recursion(p);
            return NULL;
        }
        if (!ast_expression_is_identifier_lvalue(expr)) {
            set_error(p,
                      op_tok,
                      "Expected identifier lvalue for %s operand",
                      lexer_token_type_name(op_tok->type));
            ast_expression_free_internal(expr);
            leave_expression_recursion(p);
            return NULL;
        }
        expr = build_unary_expression_node(p, op_tok, expr);
        leave_expression_recursion(p);
        return expr;
    }

    if (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS) || match(p, TOKEN_BANG) ||
        match(p, TOKEN_TILDE)) {
        op_tok = previous(p);
        expr = parse_expression_ast_unary(p);
        if (!expr) {
            leave_expression_recursion(p);
            return NULL;
        }
        expr = build_unary_expression_node(p, op_tok, expr);
        leave_expression_recursion(p);
        return expr;
    }

    expr = parse_expression_ast_postfix(p);
    leave_expression_recursion(p);
    return expr;
}

static AstExpression *parse_expression_ast_postfix(Parser *p) {
    AstExpression *expr;

    expr = parse_expression_ast_primary_only(p);
    if (!expr) {
        return NULL;
    }

    while (1) {
        if (match(p, TOKEN_LPAREN)) {
            const Token *lparen_tok = previous(p);
            AstExpression **args = NULL;
            size_t arg_count = 0;
            size_t arg_capacity = 0;

            if (!ast_expression_is_callable_callee(expr)) {
                set_error(p,
                          lparen_tok,
                          "Expected callable expression before '('");
                ast_expression_free_internal(expr);
                free_ast_expression_array(args, arg_count);
                return NULL;
            }

            if (!check(p, TOKEN_RPAREN)) {
                do {
                    AstExpression *arg = parse_expression_ast_assignment(p);
                    if (!arg) {
                        ast_expression_free_internal(expr);
                        free_ast_expression_array(args, arg_count);
                        return NULL;
                    }
                    if (!append_ast_expression_arg(p, &args, &arg_count, &arg_capacity, arg)) {
                        ast_expression_free_internal(expr);
                        free_ast_expression_array(args, arg_count);
                        return NULL;
                    }
                } while (match(p, TOKEN_COMMA));
            }

            if (!consume(p, TOKEN_RPAREN, "')'")) {
                ast_expression_free_internal(expr);
                free_ast_expression_array(args, arg_count);
                return NULL;
            }

            expr = build_call_expression_node(p, lparen_tok, expr, args, arg_count);
            if (!expr) {
                return NULL;
            }
            continue;
        }

        if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS_MINUS)) {
            const Token *op_tok = previous(p);

            if (!ast_expression_is_identifier_lvalue(expr)) {
                set_error(p,
                          op_tok,
                          "Expected identifier lvalue before %s",
                          lexer_token_type_name(op_tok->type));
                ast_expression_free_internal(expr);
                return NULL;
            }

            expr = build_postfix_expression_node(p, op_tok, expr);
            if (!expr) {
                return NULL;
            }
            continue;
        }

        break;
    }

    return expr;
}

static AstExpression *parse_expression_ast_multiplicative(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_unary(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_unary(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_additive(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_multiplicative(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_multiplicative(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_relational(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_shift(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_LT) || match(p, TOKEN_LE) || match(p, TOKEN_GT) || match(p, TOKEN_GE)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_shift(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_shift(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_additive(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_SHIFT_LEFT) || match(p, TOKEN_SHIFT_RIGHT)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_additive(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_equality(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_relational(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_EQ) || match(p, TOKEN_NE)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_relational(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_bitwise_and(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_equality(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_AMP)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_equality(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_bitwise_xor(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_bitwise_and(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_CARET)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_bitwise_and(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_bitwise_or(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_bitwise_xor(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_PIPE)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_bitwise_xor(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_logical_and(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_bitwise_or(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_AND_AND)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_bitwise_or(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_logical_or(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_logical_and(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_OR_OR)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_logical_and(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static AstExpression *parse_expression_ast_conditional(Parser *p) {
    AstExpression *condition;
    AstExpression *then_expr;
    AstExpression *else_expr;
    const Token *question_tok;
    int entered = enter_expression_recursion(p, "conditional-expression-ast");

    if (!entered) {
        return NULL;
    }

    condition = parse_expression_ast_logical_or(p);
    if (!condition) {
        leave_expression_recursion(p);
        return NULL;
    }

    if (!match(p, TOKEN_QUESTION)) {
        leave_expression_recursion(p);
        return condition;
    }

    question_tok = previous(p);
    then_expr = parse_expression_ast_comma(p);
    if (!then_expr) {
        ast_expression_free_internal(condition);
        leave_expression_recursion(p);
        return NULL;
    }

    if (!consume(p, TOKEN_COLON, "':'")) {
        ast_expression_free_internal(condition);
        ast_expression_free_internal(then_expr);
        leave_expression_recursion(p);
        return NULL;
    }

    else_expr = parse_expression_ast_conditional(p);
    if (!else_expr) {
        ast_expression_free_internal(condition);
        ast_expression_free_internal(then_expr);
        leave_expression_recursion(p);
        return NULL;
    }

    condition = build_ternary_expression_node(p,
                                              question_tok,
                                              condition,
                                              then_expr,
                                              else_expr);
    leave_expression_recursion(p);
    return condition;
}

static AstExpression *parse_expression_ast_assignment(Parser *p) {
    AstExpression *left;
    AstExpression *right;
    const Token *op_tok;
    int entered = enter_expression_recursion(p, "assignment-expression-ast");

    if (!entered) {
        return NULL;
    }

    left = parse_expression_ast_conditional(p);
    if (!left) {
        leave_expression_recursion(p);
        return NULL;
    }

    if (!(left->kind == AST_EXPR_IDENTIFIER && match_assignment_operator(p, &op_tok))) {
        leave_expression_recursion(p);
        return left;
    }

    right = parse_expression_ast_assignment(p);
    if (!right) {
        ast_expression_free_internal(left);
        leave_expression_recursion(p);
        return NULL;
    }

    left = build_binary_expression_node(p, op_tok, left, right);
    leave_expression_recursion(p);
    return left;
}

static AstExpression *parse_expression_ast_comma(Parser *p) {
    AstExpression *left;

    left = parse_expression_ast_assignment(p);
    if (!left) {
        return NULL;
    }

    while (match(p, TOKEN_COMMA)) {
        const Token *op_tok = previous(p);
        AstExpression *right = parse_expression_ast_assignment(p);

        if (!right) {
            ast_expression_free_internal(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static int parse_primary_with_flags(Parser *p,
                                    int *out_is_identifier_lvalue,
                                    int *out_is_callable_target) {
    size_t expr_start_index;

    if (out_is_identifier_lvalue) {
        *out_is_identifier_lvalue = 0;
    }
    if (out_is_callable_target) {
        *out_is_callable_target = 0;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        if (out_is_identifier_lvalue) {
            *out_is_identifier_lvalue = 1;
        }
        if (out_is_callable_target) {
            *out_is_callable_target = 1;
        }
        return 1;
    }

    if (match(p, TOKEN_NUMBER)) {
        return 1;
    }

    if (match(p, TOKEN_LPAREN)) {
        expr_start_index = p->current;
        if (!parse_expression(p)) {
            return 0;
        }
        if (!consume(p, TOKEN_RPAREN, "')'")) {
            return 0;
        }

        if (out_is_identifier_lvalue) {
            *out_is_identifier_lvalue = token_slice_is_parenthesized_identifier_form(p->tokens,
                                                                                      expr_start_index,
                                                                                      p->current - 1);
        }
        if (out_is_callable_target) {
            *out_is_callable_target = token_slice_is_callable_form(p->tokens,
                                                                   expr_start_index,
                                                                   p->current - 1);
        }
        return 1;
    }

    set_error(p, peek(p), "Expected expression, got %s", lexer_token_type_name(peek(p)->type));
    return 0;
}

static int parse_postfix(Parser *p) {
    int is_identifier_lvalue = 0;
    int is_callable_target = 0;

    if (!parse_primary_with_flags(p, &is_identifier_lvalue, &is_callable_target)) {
        return 0;
    }

    while (1) {
        if (match(p, TOKEN_LPAREN)) {
            if (!is_callable_target) {
                set_error(p, previous(p), "Expected callable expression before '('");
                return 0;
            }
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    if (!parse_assignment(p)) {
                        return 0;
                    }
                } while (match(p, TOKEN_COMMA));
            }
            if (!consume(p, TOKEN_RPAREN, "')'")) {
                return 0;
            }
            is_identifier_lvalue = 0;
            is_callable_target = 1;
            continue;
        }

        if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS_MINUS)) {
            const Token *op_tok = previous(p);

            if (!is_identifier_lvalue) {
                set_error(p,
                          op_tok,
                          "Expected identifier lvalue before %s",
                          lexer_token_type_name(op_tok->type));
                return 0;
            }

            is_identifier_lvalue = 0;
            is_callable_target = 0;
            continue;
        }

        break;
    }

    return 1;
}

static int parse_unary(Parser *p) {
    int ok;
    size_t operand_start;

    if (!enter_expression_recursion(p, "unary-expression")) {
        return 0;
    }

    if (match(p, TOKEN_PLUS_PLUS) || match(p, TOKEN_MINUS_MINUS)) {
        const Token *op_tok = previous(p);
        operand_start = p->current;
        ok = parse_unary(p);
        if (ok && !token_slice_is_parenthesized_identifier_form(p->tokens, operand_start, p->current)) {
            set_error(p,
                      op_tok,
                      "Expected identifier lvalue for %s operand",
                      lexer_token_type_name(op_tok->type));
            ok = 0;
        }
        leave_expression_recursion(p);
        return ok;
    }

    if (match(p, TOKEN_MINUS) || match(p, TOKEN_PLUS) || match(p, TOKEN_BANG) ||
        match(p, TOKEN_TILDE)) {
        ok = parse_unary(p);
        leave_expression_recursion(p);
        return ok;
    }

    ok = parse_postfix(p);
    leave_expression_recursion(p);
    return ok;
}

static int parse_multiplicative(Parser *p) {
    if (!parse_unary(p)) {
        return 0;
    }

    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        if (!parse_unary(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_additive(Parser *p) {
    if (!parse_multiplicative(p)) {
        return 0;
    }

    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        if (!parse_multiplicative(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_shift(Parser *p) {
    if (!parse_additive(p)) {
        return 0;
    }

    while (match(p, TOKEN_SHIFT_LEFT) || match(p, TOKEN_SHIFT_RIGHT)) {
        if (!parse_additive(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_relational(Parser *p) {
    if (!parse_shift(p)) {
        return 0;
    }

    while (match(p, TOKEN_LT) || match(p, TOKEN_LE) || match(p, TOKEN_GT) || match(p, TOKEN_GE)) {
        if (!parse_shift(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_equality(Parser *p) {
    if (!parse_relational(p)) {
        return 0;
    }

    while (match(p, TOKEN_EQ) || match(p, TOKEN_NE)) {
        if (!parse_relational(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_bitwise_and(Parser *p) {
    if (!parse_equality(p)) {
        return 0;
    }

    while (match(p, TOKEN_AMP)) {
        if (!parse_equality(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_bitwise_xor(Parser *p) {
    if (!parse_bitwise_and(p)) {
        return 0;
    }

    while (match(p, TOKEN_CARET)) {
        if (!parse_bitwise_and(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_bitwise_or(Parser *p) {
    if (!parse_bitwise_xor(p)) {
        return 0;
    }

    while (match(p, TOKEN_PIPE)) {
        if (!parse_bitwise_xor(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_logical_and(Parser *p) {
    if (!parse_bitwise_or(p)) {
        return 0;
    }

    while (match(p, TOKEN_AND_AND)) {
        if (!parse_bitwise_or(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_logical_or(Parser *p) {
    if (!parse_logical_and(p)) {
        return 0;
    }

    while (match(p, TOKEN_OR_OR)) {
        if (!parse_logical_and(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_conditional(Parser *p) {
    int ok;

    if (!enter_expression_recursion(p, "conditional-expression")) {
        return 0;
    }

    if (!parse_logical_or(p)) {
        leave_expression_recursion(p);
        return 0;
    }

    if (!match(p, TOKEN_QUESTION)) {
        leave_expression_recursion(p);
        return 1;
    }

    if (!parse_comma(p)) {
        leave_expression_recursion(p);
        return 0;
    }

    if (!consume(p, TOKEN_COLON, "':'")) {
        leave_expression_recursion(p);
        return 0;
    }

    ok = parse_conditional(p);
    leave_expression_recursion(p);
    return ok;
}

static int parse_assignment(Parser *p) {
    int ok;
    size_t start = p->current;

    if (!enter_expression_recursion(p, "assignment-expression")) {
        return 0;
    }

    if (match(p, TOKEN_IDENTIFIER) && match_assignment_operator(p, NULL)) {
        ok = parse_assignment(p);
        leave_expression_recursion(p);
        return ok;
    }

    p->current = start;
    ok = parse_conditional(p);
    leave_expression_recursion(p);
    return ok;
}

static int parse_comma(Parser *p) {
    if (!parse_assignment(p)) {
        return 0;
    }

    while (match(p, TOKEN_COMMA)) {
        if (!parse_assignment(p)) {
            return 0;
        }
    }

    return 1;
}

static int parse_expression(Parser *p) {
    return parse_comma(p);
}

static int parse_expression_statement(Parser *p) {
    if (match(p, TOKEN_SEMICOLON)) {
        return 1;
    }
    if (!parse_expression(p)) {
        return 0;
    }
    return consume(p, TOKEN_SEMICOLON, "';'");
}

static int expression_slice_is_constant_true(const TokenArray *tokens,
                                             size_t start,
                                             size_t end) {
    const Token *tok;

    if (!tokens || !tokens->data || end <= start) {
        return 0;
    }

    if (end - start != 1) {
        return 0;
    }

    tok = &tokens->data[start];
    return tok->type == TOKEN_NUMBER && tok->number_value != 0;
}

static int parse_compound_statement_with_flow(Parser *p,
                                              int *out_guaranteed_return,
                                              int *out_may_fallthrough,
                                              int *out_may_break) {
    int block_guaranteed_return = 0;
    int block_may_fallthrough = 1;
    int block_may_break = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (!consume(p, TOKEN_LBRACE, "'{'") ) {
        return 0;
    }

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        if (check(p, TOKEN_KW_INT)) {
            if (p->track_function_declaration_count) {
                p->function_declaration_count++;
            }
            if (!parse_declaration(p, NULL)) {
                return 0;
            }
            if (block_may_fallthrough) {
                block_guaranteed_return = 0;
            }
        } else {
            int stmt_guaranteed_return = 0;
            int stmt_may_fallthrough = 1;
            int stmt_may_break = 0;

            if (!parse_statement_with_flow(p,
                                           &stmt_guaranteed_return,
                                           &stmt_may_fallthrough,
                                           &stmt_may_break)) {
                return 0;
            }

            if (block_may_fallthrough && stmt_may_break) {
                block_may_break = 1;
            }

            if (block_may_fallthrough) {
                if (stmt_guaranteed_return) {
                    block_guaranteed_return = 1;
                    block_may_fallthrough = 0;
                } else if (!stmt_may_fallthrough) {
                    block_guaranteed_return = 0;
                    block_may_fallthrough = 0;
                } else {
                    block_guaranteed_return = 0;
                }
            }
        }
    }

    if (!consume(p, TOKEN_RBRACE, "'}'")) {
        return 0;
    }

    if (out_guaranteed_return) {
        *out_guaranteed_return = block_guaranteed_return;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = block_may_fallthrough;
    }
    if (out_may_break) {
        *out_may_break = block_may_break;
    }
    return 1;
}

static int parse_if_statement_with_flow(Parser *p,
                                        int *out_guaranteed_return,
                                        int *out_may_fallthrough,
                                        int *out_may_break) {
    int then_guaranteed_return = 0;
    int then_may_fallthrough = 1;
    int then_may_break = 0;
    int else_guaranteed_return = 0;
    int else_may_fallthrough = 1;
    int else_may_break = 0;
    int has_else = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (p->track_function_if_count) {
        p->function_if_count++;
    }

    if (!consume(p, TOKEN_LPAREN, "'('") ) {
        return 0;
    }
    if (!parse_expression(p)) {
        return 0;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        return 0;
    }
    if (!parse_statement_with_flow(p,
                                   &then_guaranteed_return,
                                   &then_may_fallthrough,
                                   &then_may_break)) {
        return 0;
    }
    if (match(p, TOKEN_KW_ELSE)) {
        has_else = 1;
        if (!parse_statement_with_flow(p,
                                       &else_guaranteed_return,
                                       &else_may_fallthrough,
                                       &else_may_break)) {
            return 0;
        }
    }

    if (out_guaranteed_return) {
        *out_guaranteed_return = has_else && then_guaranteed_return && else_guaranteed_return;
    }
    if (out_may_fallthrough) {
        if (!has_else) {
            *out_may_fallthrough = 1;
        } else {
            *out_may_fallthrough = then_may_fallthrough || else_may_fallthrough;
        }
    }
    if (out_may_break) {
        if (!has_else) {
            *out_may_break = then_may_break;
        } else {
            *out_may_break = then_may_break || else_may_break;
        }
    }
    return 1;
}

static int parse_while_statement_with_flow(Parser *p,
                                           int *out_guaranteed_return,
                                           int *out_may_fallthrough,
                                           int *out_may_break) {
    size_t cond_start;
    int cond_always_true;
    int body_guaranteed_return = 0;
    int body_may_break = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (p->track_function_loop_count) {
        p->function_loop_count++;
    }

    if (!consume(p, TOKEN_LPAREN, "'('") ) {
        return 0;
    }
    cond_start = p->current;
    if (!parse_expression(p)) {
        return 0;
    }
    cond_always_true = expression_slice_is_constant_true(p->tokens, cond_start, p->current);
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        return 0;
    }

    p->loop_depth++;
    if (!parse_statement_with_flow(p, &body_guaranteed_return, NULL, &body_may_break)) {
        p->loop_depth--;
        return 0;
    }
    p->loop_depth--;

    if (cond_always_true) {
        if (body_may_break) {
            if (out_guaranteed_return) {
                *out_guaranteed_return = 0;
            }
            if (out_may_fallthrough) {
                *out_may_fallthrough = 1;
            }
        } else {
            if (out_guaranteed_return) {
                *out_guaranteed_return = body_guaranteed_return;
            }
            if (out_may_fallthrough) {
                *out_may_fallthrough = 0;
            }
        }
    } else {
        if (out_guaranteed_return) {
            *out_guaranteed_return = 0;
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
    }

    return 1;
}

static int parse_for_statement_with_flow(Parser *p,
                                         int *out_guaranteed_return,
                                         int *out_may_fallthrough,
                                         int *out_may_break) {
    int cond_always_true = 0;
    size_t cond_start = 0;
    int body_guaranteed_return = 0;
    int body_may_break = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (p->track_function_loop_count) {
        p->function_loop_count++;
    }

    if (!consume(p, TOKEN_LPAREN, "'('") ) {
        return 0;
    }

    if (match(p, TOKEN_KW_INT)) {
        p->current--;
        if (!parse_declaration(p, NULL)) {
            return 0;
        }
    } else {
        if (!parse_expression_statement(p)) {
            return 0;
        }
    }

    if (!check(p, TOKEN_SEMICOLON)) {
        cond_start = p->current;
        if (!parse_expression(p)) {
            return 0;
        }
        cond_always_true = expression_slice_is_constant_true(p->tokens, cond_start, p->current);
    } else {
        cond_always_true = 1;
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        return 0;
    }

    if (!check(p, TOKEN_RPAREN)) {
        if (!parse_expression(p)) {
            return 0;
        }
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        return 0;
    }

    p->loop_depth++;
    if (!parse_statement_with_flow(p, &body_guaranteed_return, NULL, &body_may_break)) {
        p->loop_depth--;
        return 0;
    }
    p->loop_depth--;

    if (cond_always_true) {
        if (body_may_break) {
            if (out_guaranteed_return) {
                *out_guaranteed_return = 0;
            }
            if (out_may_fallthrough) {
                *out_may_fallthrough = 1;
            }
        } else {
            if (out_guaranteed_return) {
                *out_guaranteed_return = body_guaranteed_return;
            }
            if (out_may_fallthrough) {
                *out_may_fallthrough = 0;
            }
        }
    } else {
        if (out_guaranteed_return) {
            *out_guaranteed_return = 0;
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
    }

    return 1;
}

static int parse_return_statement(Parser *p) {
    if (!check(p, TOKEN_SEMICOLON)) {
        if (!parse_expression(p)) {
            return 0;
        }
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        return 0;
    }
    if (p->track_function_return_count) {
        p->function_return_count++;
    }
    return 1;
}

static int parse_break_statement(Parser *p) {
    if (p->loop_depth == 0) {
        set_error(p, previous(p), "'break' is only allowed inside loops");
        return 0;
    }
    if (p->track_function_break_count) {
        p->function_break_count++;
    }
    return consume(p, TOKEN_SEMICOLON, "';'");
}

static int parse_continue_statement(Parser *p) {
    if (p->loop_depth == 0) {
        set_error(p, previous(p), "'continue' is only allowed inside loops");
        return 0;
    }
    if (p->track_function_continue_count) {
        p->function_continue_count++;
    }
    return consume(p, TOKEN_SEMICOLON, "';'");
}

static int parse_statement_with_flow(Parser *p,
                                     int *out_guaranteed_return,
                                     int *out_may_fallthrough,
                                     int *out_may_break) {
    int ok;
    int guaranteed_return = 0;
    int may_fallthrough = 1;
    int may_break = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (!enter_statement_recursion(p, "statement")) {
        return 0;
    }

    if (match(p, TOKEN_LBRACE)) {
        p->current--;
        ok = parse_compound_statement_with_flow(p,
                                                &guaranteed_return,
                                                &may_fallthrough,
                                                &may_break);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = guaranteed_return;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = may_fallthrough;
        }
        if (ok && out_may_break) {
            *out_may_break = may_break;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_IF)) {
        ok = parse_if_statement_with_flow(p,
                                          &guaranteed_return,
                                          &may_fallthrough,
                                          &may_break);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = guaranteed_return;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = may_fallthrough;
        }
        if (ok && out_may_break) {
            *out_may_break = may_break;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_WHILE)) {
        ok = parse_while_statement_with_flow(p,
                                             &guaranteed_return,
                                             &may_fallthrough,
                                             &may_break);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = guaranteed_return;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = may_fallthrough;
        }
        if (ok && out_may_break) {
            *out_may_break = may_break;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_FOR)) {
        ok = parse_for_statement_with_flow(p,
                                           &guaranteed_return,
                                           &may_fallthrough,
                                           &may_break);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = guaranteed_return;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = may_fallthrough;
        }
        if (ok && out_may_break) {
            *out_may_break = may_break;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_RETURN)) {
        ok = parse_return_statement(p);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = 1;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        if (ok && out_may_break) {
            *out_may_break = 0;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_BREAK)) {
        ok = parse_break_statement(p);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = 0;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        if (ok && out_may_break) {
            *out_may_break = 1;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_CONTINUE)) {
        ok = parse_continue_statement(p);
        leave_statement_recursion(p);
        if (ok && out_guaranteed_return) {
            *out_guaranteed_return = 0;
        }
        if (ok && out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        if (ok && out_may_break) {
            *out_may_break = 0;
        }
        return ok;
    }

    ok = parse_expression_statement(p);
    leave_statement_recursion(p);
    if (ok && out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (ok && out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (ok && out_may_break) {
        *out_may_break = 0;
    }
    return ok;
}

static int parse_init_declarator(Parser *p, const Token **out_ident, int *out_has_initializer) {
    if (out_has_initializer) {
        *out_has_initializer = 0;
    }

    if (!consume_token(p, TOKEN_IDENTIFIER, "identifier", out_ident)) {
        return 0;
    }
    if (match(p, TOKEN_ASSIGN)) {
        if (out_has_initializer) {
            *out_has_initializer = 1;
        }
        if (!parse_initializer_expression(p)) {
            return 0;
        }
    }
    return 1;
}

static int parse_declaration(Parser *p, AstProgram *top_level_program) {
    const Token *ident = NULL;
    int has_initializer = 0;

    if (!consume(p, TOKEN_KW_INT, "'int'")) {
        return 0;
    }
    if (!parse_init_declarator(p, &ident, &has_initializer)) {
        return 0;
    }
    if (top_level_program && !ast_program_append_external(top_level_program, AST_EXTERNAL_DECLARATION, ident)) {
        set_error(p, peek(p), "Out of memory while building AST");
        return 0;
    }
    if (top_level_program) {
        top_level_program->externals[top_level_program->count - 1].has_initializer = has_initializer;
    }

    while (match(p, TOKEN_COMMA)) {
        if (!parse_init_declarator(p, &ident, &has_initializer)) {
            return 0;
        }
        if (top_level_program &&
            !ast_program_append_external(top_level_program, AST_EXTERNAL_DECLARATION, ident)) {
            set_error(p, peek(p), "Out of memory while building AST");
            return 0;
        }
        if (top_level_program) {
            top_level_program->externals[top_level_program->count - 1].has_initializer = has_initializer;
        }
    }
    return consume(p, TOKEN_SEMICOLON, "';'");
}

static int parse_parameter_list(Parser *p,
                                size_t *out_param_count,
                                int *out_has_unnamed_parameter) {
    size_t count = 0;

    if (out_param_count) {
        *out_param_count = 0;
    }
    if (out_has_unnamed_parameter) {
        *out_has_unnamed_parameter = 0;
    }

    if (check(p, TOKEN_RPAREN)) {
        return 1;
    }

    do {
        if (!consume(p, TOKEN_KW_INT, "'int'")) {
            return 0;
        }
        /* C-style prototypes may omit parameter names: int f(int); */
        if (check(p, TOKEN_IDENTIFIER)) {
            advance(p);
        } else {
            if (out_has_unnamed_parameter) {
                *out_has_unnamed_parameter = 1;
            }
        }
        count++;
    } while (match(p, TOKEN_COMMA));

    if (out_param_count) {
        *out_param_count = count;
    }

    return 1;
}

static int parse_function_external(Parser *p,
                                   const Token **out_name_token,
                                   size_t *out_parameter_count,
                                   int *out_is_definition,
                                   size_t *out_return_statement_count,
                                   int *out_returns_on_all_paths,
                                   size_t *out_loop_statement_count,
                                   size_t *out_if_statement_count,
                                   size_t *out_break_statement_count,
                                   size_t *out_continue_statement_count,
                                   size_t *out_declaration_statement_count) {
    const Token *name_tok = NULL;
    size_t parameter_count = 0;
    int has_unnamed_parameter = 0;
    int body_ok;
    int function_returns_on_all_paths = 0;

    if (out_name_token) {
        *out_name_token = NULL;
    }
    if (out_parameter_count) {
        *out_parameter_count = 0;
    }
    if (out_is_definition) {
        *out_is_definition = 0;
    }
    if (out_return_statement_count) {
        *out_return_statement_count = 0;
    }
    if (out_returns_on_all_paths) {
        *out_returns_on_all_paths = 0;
    }
    if (out_loop_statement_count) {
        *out_loop_statement_count = 0;
    }
    if (out_if_statement_count) {
        *out_if_statement_count = 0;
    }
    if (out_break_statement_count) {
        *out_break_statement_count = 0;
    }
    if (out_continue_statement_count) {
        *out_continue_statement_count = 0;
    }
    if (out_declaration_statement_count) {
        *out_declaration_statement_count = 0;
    }

    if (!consume(p, TOKEN_KW_INT, "'int'")) {
        return 0;
    }
    if (!consume_token(p, TOKEN_IDENTIFIER, "identifier", &name_tok)) {
        return 0;
    }
    if (out_name_token) {
        *out_name_token = name_tok;
    }
    if (!consume(p, TOKEN_LPAREN, "'('") ) {
        return 0;
    }
    if (!parse_parameter_list(p, &parameter_count, &has_unnamed_parameter)) {
        return 0;
    }
    if (out_parameter_count) {
        *out_parameter_count = parameter_count;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        return 0;
    }

    if (match(p, TOKEN_SEMICOLON)) {
        if (out_is_definition) {
            *out_is_definition = 0;
        }
        return 1;
    }

    if (check(p, TOKEN_LBRACE)) {
        /*
         * Function definitions require named parameters so names are available
         * in function scope; unnamed parameters are only accepted in prototypes.
         */
        if (has_unnamed_parameter) {
            set_error(p,
                      peek(p),
                      "Function definition parameters must have names");
            return 0;
        }
        if (out_is_definition) {
            *out_is_definition = 1;
        }
        p->track_function_return_count = 1;
        p->function_return_count = 0;
        p->track_function_loop_count = 1;
        p->function_loop_count = 0;
        p->track_function_if_count = 1;
        p->function_if_count = 0;
        p->track_function_break_count = 1;
        p->function_break_count = 0;
        p->track_function_continue_count = 1;
        p->function_continue_count = 0;
        p->track_function_declaration_count = 1;
        p->function_declaration_count = 0;
        body_ok = parse_compound_statement_with_flow(p,
                                                     &function_returns_on_all_paths,
                                                     NULL,
                                                     NULL);
        if (out_return_statement_count) {
            *out_return_statement_count = p->function_return_count;
        }
        if (out_returns_on_all_paths) {
            *out_returns_on_all_paths = function_returns_on_all_paths;
        }
        if (out_loop_statement_count) {
            *out_loop_statement_count = p->function_loop_count;
        }
        if (out_if_statement_count) {
            *out_if_statement_count = p->function_if_count;
        }
        if (out_break_statement_count) {
            *out_break_statement_count = p->function_break_count;
        }
        if (out_continue_statement_count) {
            *out_continue_statement_count = p->function_continue_count;
        }
        if (out_declaration_statement_count) {
            *out_declaration_statement_count = p->function_declaration_count;
        }
        p->track_function_return_count = 0;
        p->track_function_loop_count = 0;
        p->track_function_if_count = 0;
        p->track_function_break_count = 0;
        p->track_function_continue_count = 0;
        p->track_function_declaration_count = 0;
        return body_ok;
    }

    set_error(p,
              peek(p),
              "Expected function body '{' or declaration terminator ';', got %s",
              lexer_token_type_name(peek(p)->type));
    return 0;
}

int parser_parse_translation_unit_ast(const TokenArray *tokens,
                                      AstProgram *out_program,
                                      ParserError *error) {
    Parser p;
    size_t start;
    const Token *external_name = NULL;
    size_t external_parameter_count = 0;
    int external_is_definition = 0;
    size_t external_return_statement_count = 0;
    int external_returns_on_all_paths = 0;
    size_t external_loop_statement_count = 0;
    size_t external_if_statement_count = 0;
    size_t external_break_statement_count = 0;
    size_t external_continue_statement_count = 0;
    size_t external_declaration_statement_count = 0;

    if (!out_program) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "Output AST is NULL");
        }
        return 0;
    }

    /* Keep state deterministic: on every call we first clear previous AST. */
    ast_program_clear_storage(out_program);

    if (!tokens || !tokens->data || tokens->size == 0) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "Empty token stream");
        }
        return 0;
    }

    if (tokens->data[tokens->size - 1].type != TOKEN_EOF) {
        if (error) {
            const Token *last = &tokens->data[tokens->size - 1];
            error->line = last->line;
            error->column = last->column;
            snprintf(error->message, sizeof(error->message), "Token stream missing EOF terminator");
        }
        return 0;
    }

    p.tokens = tokens;
    p.current = 0;
    p.error = error;
    p.has_error = 0;
    p.error_index = 0;
    p.expression_recursion_depth = 0;
    p.expression_recursion_limit = PARSER_EXPRESSION_RECURSION_LIMIT;
    p.statement_recursion_depth = 0;
    p.statement_recursion_limit = PARSER_STATEMENT_RECURSION_LIMIT;
    p.loop_depth = 0;
    p.track_function_return_count = 0;
    p.function_return_count = 0;
    p.track_function_loop_count = 0;
    p.function_loop_count = 0;
    p.track_function_if_count = 0;
    p.function_if_count = 0;
    p.track_function_break_count = 0;
    p.function_break_count = 0;
    p.track_function_continue_count = 0;
    p.function_continue_count = 0;
    p.track_function_declaration_count = 0;
    p.function_declaration_count = 0;

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    while (!is_at_end(&p)) {
        start = p.current;
        external_name = NULL;
        external_parameter_count = 0;
        external_is_definition = 0;
        external_return_statement_count = 0;
        external_returns_on_all_paths = 0;
        external_loop_statement_count = 0;
        external_if_statement_count = 0;
        external_break_statement_count = 0;
        external_continue_statement_count = 0;
        external_declaration_statement_count = 0;

        if (!parse_function_external(&p,
                                     &external_name,
                                     &external_parameter_count,
                                     &external_is_definition,
                                     &external_return_statement_count,
                                     &external_returns_on_all_paths,
                                     &external_loop_statement_count,
                                     &external_if_statement_count,
                                     &external_break_statement_count,
                                     &external_continue_statement_count,
                                     &external_declaration_statement_count)) {
            p.current = start;
            if (!parse_declaration(&p, out_program)) {
                ast_program_clear_storage(out_program);
                return 0;
            }
        } else {
            if (!ast_program_append_external(out_program, AST_EXTERNAL_FUNCTION, external_name)) {
                set_error(&p, peek(&p), "Out of memory while building AST");
                ast_program_clear_storage(out_program);
                return 0;
            }
            out_program->externals[out_program->count - 1].parameter_count = external_parameter_count;
            out_program->externals[out_program->count - 1].is_function_definition =
                external_is_definition;
            out_program->externals[out_program->count - 1].return_statement_count =
                external_return_statement_count;
            out_program->externals[out_program->count - 1].returns_on_all_paths =
                external_returns_on_all_paths;
            out_program->externals[out_program->count - 1].loop_statement_count =
                external_loop_statement_count;
            out_program->externals[out_program->count - 1].if_statement_count =
                external_if_statement_count;
            out_program->externals[out_program->count - 1].break_statement_count =
                external_break_statement_count;
            out_program->externals[out_program->count - 1].continue_statement_count =
                external_continue_statement_count;
            out_program->externals[out_program->count - 1].declaration_statement_count =
                external_declaration_statement_count;
        }
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    return 1;
}

int parser_parse_translation_unit(const TokenArray *tokens, ParserError *error) {
    Parser p;
    size_t start;

    if (!tokens || !tokens->data || tokens->size == 0) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "Empty token stream");
        }
        return 0;
    }

    if (tokens->data[tokens->size - 1].type != TOKEN_EOF) {
        if (error) {
            const Token *last = &tokens->data[tokens->size - 1];
            error->line = last->line;
            error->column = last->column;
            snprintf(error->message, sizeof(error->message), "Token stream missing EOF terminator");
        }
        return 0;
    }

    p.tokens = tokens;
    p.current = 0;
    p.error = error;
    p.has_error = 0;
    p.error_index = 0;
    p.expression_recursion_depth = 0;
    p.expression_recursion_limit = PARSER_EXPRESSION_RECURSION_LIMIT;
    p.statement_recursion_depth = 0;
    p.statement_recursion_limit = PARSER_STATEMENT_RECURSION_LIMIT;
    p.loop_depth = 0;
    p.track_function_return_count = 0;
    p.function_return_count = 0;
    p.track_function_loop_count = 0;
    p.function_loop_count = 0;
    p.track_function_if_count = 0;
    p.function_if_count = 0;
    p.track_function_break_count = 0;
    p.function_break_count = 0;
    p.track_function_continue_count = 0;
    p.function_continue_count = 0;
    p.track_function_declaration_count = 0;
    p.function_declaration_count = 0;

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    while (!is_at_end(&p)) {
        start = p.current;
        if (!parse_function_external(&p, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
            p.current = start;
            if (!parse_declaration(&p, NULL)) {
                return 0;
            }
        }
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    return 1;
}

int parser_parse_expression_ast_assignment(const TokenArray *tokens,
                                           AstExpression **out_expression,
                                           ParserError *error) {
    Parser p;
    AstExpression *expr;

    if (out_expression) {
        *out_expression = NULL;
    }

    if (!tokens || !tokens->data || tokens->size == 0) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "Empty token stream");
        }
        return 0;
    }

    if (tokens->data[tokens->size - 1].type != TOKEN_EOF) {
        if (error) {
            const Token *last = &tokens->data[tokens->size - 1];
            error->line = last->line;
            error->column = last->column;
            snprintf(error->message, sizeof(error->message), "Token stream missing EOF terminator");
        }
        return 0;
    }

    p.tokens = tokens;
    p.current = 0;
    p.error = error;
    p.has_error = 0;
    p.error_index = 0;
    p.expression_recursion_depth = 0;
    p.expression_recursion_limit = PARSER_EXPRESSION_RECURSION_LIMIT;
    p.statement_recursion_depth = 0;
    p.statement_recursion_limit = PARSER_STATEMENT_RECURSION_LIMIT;
    p.loop_depth = 0;
    p.track_function_return_count = 0;
    p.function_return_count = 0;
    p.track_function_loop_count = 0;
    p.function_loop_count = 0;
    p.track_function_if_count = 0;
    p.function_if_count = 0;
    p.track_function_break_count = 0;
    p.function_break_count = 0;
    p.track_function_continue_count = 0;
    p.function_continue_count = 0;
    p.track_function_declaration_count = 0;
    p.function_declaration_count = 0;

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    expr = parse_expression_ast_comma(&p);
    if (!expr) {
        return 0;
    }

    if (!is_at_end(&p)) {
        set_error(&p, peek(&p), "Expected EOF, got %s", lexer_token_type_name(peek(&p)->type));
        ast_expression_free_internal(expr);
        return 0;
    }

    if (out_expression) {
        *out_expression = expr;
    } else {
        ast_expression_free_internal(expr);
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }
    return 1;
}

int parser_parse_expression_ast_primary(const TokenArray *tokens,
                                        AstExpression **out_expression,
                                        ParserError *error) {
    /* Compatibility alias: forwards to comma-level parser by design. */
    return parser_parse_expression_ast_assignment(tokens, out_expression, error);
}

int parser_parse_expression_ast_additive(const TokenArray *tokens,
                                         AstExpression **out_expression,
                                         ParserError *error) {
    /* Compatibility alias: forwards to comma-level parser by design. */
    return parser_parse_expression_ast_assignment(tokens, out_expression, error);
}

int parser_parse_expression_ast_relational(const TokenArray *tokens,
                                           AstExpression **out_expression,
                                           ParserError *error) {
    /* Compatibility alias: forwards to comma-level parser by design. */
    return parser_parse_expression_ast_assignment(tokens, out_expression, error);
}

int parser_parse_expression_ast_equality(const TokenArray *tokens,
                                         AstExpression **out_expression,
                                         ParserError *error) {
    /* Compatibility alias: forwards to comma-level parser by design. */
    return parser_parse_expression_ast_assignment(tokens, out_expression, error);
}