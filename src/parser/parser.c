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
    int track_function_calls;
    char **function_called_names;
    int *function_called_lines;
    int *function_called_columns;
    size_t *function_called_arg_counts;
    int *function_called_kinds;
    size_t function_called_count;
    size_t function_called_capacity;
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
                                     int *out_may_break,
                                     AstStatement **out_statement);
static int parse_declaration(Parser *p, AstProgram *top_level_program);

static void parser_clear_function_called_names(Parser *p) {
    size_t i;

    if (!p) {
        return;
    }

    for (i = 0; i < p->function_called_count; ++i) {
        free(p->function_called_names[i]);
    }
    free(p->function_called_names);
    free(p->function_called_lines);
    free(p->function_called_columns);
    free(p->function_called_arg_counts);
    free(p->function_called_kinds);
    p->function_called_names = NULL;
    p->function_called_lines = NULL;
    p->function_called_columns = NULL;
    p->function_called_arg_counts = NULL;
    p->function_called_kinds = NULL;
    p->function_called_count = 0;
    p->function_called_capacity = 0;
}

static int parser_record_function_call_name(Parser *p,
                                            const Token *name_tok,
                                            const Token *call_site_tok,
                                            size_t arg_count,
                                            int callee_kind) {
    char **new_data;
    int *new_lines;
    int *new_columns;
    size_t *new_arg_counts;
    int *new_kinds;
    size_t next_capacity;
    char *name_copy;

    if (!p || !p->track_function_calls || !call_site_tok) {
        return 1;
    }

    if (p->function_called_count == p->function_called_capacity) {
        next_capacity = (p->function_called_capacity == 0) ? 8 : (p->function_called_capacity * 2);
        new_data = (char **)realloc(p->function_called_names, next_capacity * sizeof(char *));
        if (!new_data) {
            set_error(p, call_site_tok, "Out of memory while recording function call metadata");
            return 0;
        }
        p->function_called_names = new_data;

        new_lines = (int *)realloc(p->function_called_lines, next_capacity * sizeof(int));
        if (!new_lines) {
            set_error(p, call_site_tok, "Out of memory while recording function call metadata");
            return 0;
        }
        p->function_called_lines = new_lines;

        new_columns = (int *)realloc(p->function_called_columns, next_capacity * sizeof(int));
        if (!new_columns) {
            set_error(p, call_site_tok, "Out of memory while recording function call metadata");
            return 0;
        }
        p->function_called_columns = new_columns;

        new_arg_counts = (size_t *)realloc(p->function_called_arg_counts,
                                           next_capacity * sizeof(size_t));
        if (!new_arg_counts) {
            set_error(p, call_site_tok, "Out of memory while recording function call metadata");
            return 0;
        }
        p->function_called_arg_counts = new_arg_counts;

        new_kinds = (int *)realloc(p->function_called_kinds, next_capacity * sizeof(int));
        if (!new_kinds) {
            set_error(p, call_site_tok, "Out of memory while recording function call metadata");
            return 0;
        }
        p->function_called_kinds = new_kinds;
        p->function_called_capacity = next_capacity;
    }

    name_copy = NULL;
    if (name_tok && name_tok->lexeme && name_tok->length > 0) {
        name_copy = (char *)malloc(name_tok->length + 1);
        if (!name_copy) {
            set_error(p, call_site_tok, "Out of memory while recording function call metadata");
            return 0;
        }
        memcpy(name_copy, name_tok->lexeme, name_tok->length);
        name_copy[name_tok->length] = '\0';
    }

    p->function_called_names[p->function_called_count] = name_copy;
    p->function_called_lines[p->function_called_count] = call_site_tok->line;
    p->function_called_columns[p->function_called_count] = call_site_tok->column;
    p->function_called_arg_counts[p->function_called_count] = arg_count;
    p->function_called_kinds[p->function_called_count] = callee_kind;
    p->function_called_count++;
    return 1;
}

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
    int depth;
    size_t match_start;

    if (!tokens || !tokens->data || start >= end) {
        return 0;
    }

    trim_wrapping_parentheses(tokens, &start, &end);

    if (end == start + 1 && tokens->data[start].type == TOKEN_IDENTIFIER) {
        return 1;
    }

    if (end <= start + 1 || tokens->data[end - 1].type != TOKEN_RPAREN) {
        return 0;
    }

    depth = 0;
    match_start = end;
    for (i = end; i > start; --i) {
        TokenType type = tokens->data[i - 1].type;
        if (type == TOKEN_RPAREN) {
            depth++;
        } else if (type == TOKEN_LPAREN) {
            depth--;
            if (depth == 0) {
                match_start = i - 1;
                break;
            }
            if (depth < 0) {
                return 0;
            }
        }
    }

    if (depth != 0 || match_start <= start) {
        return 0;
    }

    return token_slice_is_callable_form(tokens, start, match_start);
}

static const Token *token_slice_callable_name_token(const TokenArray *tokens,
                                                    size_t start,
                                                    size_t end) {
    if (!tokens || !tokens->data || start >= end) {
        return NULL;
    }

    trim_wrapping_parentheses(tokens, &start, &end);

    /*
     * Semantic callable metadata only supports direct identifier callees.
     * Parenthesized call results like (f()) are callable syntactically, but
     * should not be attributed to a direct function name token.
     */
    if (end != start + 1 || tokens->data[start].type != TOKEN_IDENTIFIER) {
        return NULL;
    }

    return &tokens->data[start];
}

static int parse_initializer_expression(Parser *p) {
    /* C declarator initializers use assignment-expression, not comma-expression. */
    return parse_assignment(p);
}

static AstStatement *alloc_ast_statement(Parser *p,
                                         AstStatementKind kind,
                                         const Token *anchor_tok) {
    AstStatement *stmt = (AstStatement *)malloc(sizeof(AstStatement));

    if (!stmt) {
        set_error(p,
                  anchor_tok ? anchor_tok : peek(p),
                  "Out of memory while building statement AST");
        return NULL;
    }

    stmt->kind = kind;
    stmt->line = anchor_tok ? anchor_tok->line : 0;
    stmt->column = anchor_tok ? anchor_tok->column : 0;
    stmt->expressions = NULL;
    stmt->expression_count = 0;
    stmt->has_primary_expression = 0;
    stmt->primary_expression_index = 0;
    stmt->has_condition_expression = 0;
    stmt->condition_expression_index = 0;
    stmt->has_for_init_expression = 0;
    stmt->for_init_expression_index = 0;
    stmt->has_for_step_expression = 0;
    stmt->for_step_expression_index = 0;
    stmt->children = NULL;
    stmt->child_count = 0;
    return stmt;
}

static int build_expression_ast_from_slice(Parser *p,
                                           size_t start,
                                           size_t end,
                                           AstExpression **out_expr,
                                           const Token *anchor_tok) {
    ParserError nested_error;
    TokenArray slice;
    Token *owned_tokens;
    size_t len;
    size_t i;
    int ok;

    if (out_expr) {
        *out_expr = NULL;
    }

    if (!out_expr || end <= start) {
        return 1;
    }

    len = end - start;
    owned_tokens = (Token *)malloc((len + 1) * sizeof(Token));
    if (!owned_tokens) {
        set_error(p,
                  anchor_tok ? anchor_tok : peek(p),
                  "Out of memory while building expression AST");
        return 0;
    }

    for (i = 0; i < len; ++i) {
        owned_tokens[i] = p->tokens->data[start + i];
    }
    owned_tokens[len] = p->tokens->data[p->tokens->size - 1];

    slice.data = owned_tokens;
    slice.size = len + 1;
    slice.capacity = len + 1;

    nested_error.line = 0;
    nested_error.column = 0;
    nested_error.message[0] = '\0';
    ok = parser_parse_expression_ast_assignment(&slice, out_expr, &nested_error);
    free(owned_tokens);

    if (!ok) {
        set_error(p,
                  anchor_tok ? anchor_tok : peek(p),
                  "Failed to build expression AST slice at %d:%d: %s",
                  nested_error.line,
                  nested_error.column,
                  nested_error.message[0] ? nested_error.message : "unknown");
        return 0;
    }

    return 1;
}

static void free_ast_statement_array(AstStatement **items, size_t count) {
    size_t i;

    if (!items) {
        return;
    }

    for (i = 0; i < count; ++i) {
        ast_statement_free_internal(items[i]);
    }
    free(items);
}

static int append_ast_statement_child(Parser *p,
                                      AstStatement ***items,
                                      size_t *count,
                                      size_t *capacity,
                                      AstStatement *child,
                                      const Token *anchor_tok) {
    AstStatement **new_items;
    size_t next_capacity;

    if (!items || !count || !capacity || !child) {
        return 0;
    }

    if (*count == *capacity) {
        next_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        new_items = (AstStatement **)realloc(*items, next_capacity * sizeof(AstStatement *));
        if (!new_items) {
            set_error(p,
                      anchor_tok ? anchor_tok : peek(p),
                      "Out of memory while building statement AST");
            ast_statement_free_internal(child);
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[*count] = child;
    (*count)++;
    return 1;
}

static int append_ast_statement_expression(Parser *p,
                                           AstStatement *stmt,
                                           AstExpression *expr,
                                           const Token *anchor_tok) {
    AstExpression **new_items;

    if (!stmt || !expr) {
        return 0;
    }

    new_items = (AstExpression **)realloc(stmt->expressions,
                                          (stmt->expression_count + 1) *
                                              sizeof(AstExpression *));
    if (!new_items) {
        set_error(p,
                  anchor_tok ? anchor_tok : peek(p),
                  "Out of memory while building statement AST");
        ast_expression_free_internal(expr);
        return 0;
    }
    stmt->expressions = new_items;

    stmt->expressions[stmt->expression_count] = expr;
    stmt->expression_count++;
    return 1;
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

    if (expr->kind == AST_EXPR_IDENTIFIER || expr->kind == AST_EXPR_CALL ||
        expr->kind == AST_EXPR_PAREN) {
        return 1;
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
                                    int *out_is_callable_target,
                                    const Token **out_callable_name_token,
                                    int *out_callable_callee_kind) {
    size_t expr_start_index;

    if (out_is_identifier_lvalue) {
        *out_is_identifier_lvalue = 0;
    }
    if (out_is_callable_target) {
        *out_is_callable_target = 0;
    }
    if (out_callable_name_token) {
        *out_callable_name_token = NULL;
    }
    if (out_callable_callee_kind) {
        *out_callable_callee_kind = AST_CALL_CALLEE_NON_IDENTIFIER;
    }

    if (match(p, TOKEN_IDENTIFIER)) {
        if (out_is_identifier_lvalue) {
            *out_is_identifier_lvalue = 1;
        }
        if (out_is_callable_target) {
            *out_is_callable_target = 1;
        }
        if (out_callable_name_token) {
            *out_callable_name_token = previous(p);
        }
        if (out_callable_callee_kind) {
            *out_callable_callee_kind = AST_CALL_CALLEE_DIRECT_IDENTIFIER;
        }
        return 1;
    }

    if (match(p, TOKEN_NUMBER)) {
        return 1;
    }

    if (match(p, TOKEN_LPAREN)) {
        int is_callable_form;
        const Token *callable_name_tok;

        expr_start_index = p->current;
        if (!parse_expression(p)) {
            return 0;
        }
        if (!consume(p, TOKEN_RPAREN, "')'")) {
            return 0;
        }

        is_callable_form = token_slice_is_callable_form(p->tokens,
                                                        expr_start_index,
                                                        p->current - 1);
        callable_name_tok = token_slice_callable_name_token(p->tokens,
                                                            expr_start_index,
                                                            p->current - 1);

        if (out_is_identifier_lvalue) {
            *out_is_identifier_lvalue = token_slice_is_parenthesized_identifier_form(p->tokens,
                                                                                      expr_start_index,
                                                                                      p->current - 1);
        }
        if (out_is_callable_target) {
            /*
             * Any parenthesized expression can be a syntactic postfix call callee.
             * Semantic callable checks classify non-direct forms afterward.
             */
            *out_is_callable_target = 1;
        }
        if (out_callable_name_token) {
            *out_callable_name_token = callable_name_tok;
        }
        if (out_callable_callee_kind) {
            if (callable_name_tok) {
                *out_callable_callee_kind = AST_CALL_CALLEE_DIRECT_IDENTIFIER;
            } else if (is_callable_form) {
                *out_callable_callee_kind = AST_CALL_CALLEE_CALL_RESULT;
            } else {
                *out_callable_callee_kind = AST_CALL_CALLEE_NON_IDENTIFIER;
            }
        }
        return 1;
    }

    set_error(p, peek(p), "Expected expression, got %s", lexer_token_type_name(peek(p)->type));
    return 0;
}

static int parse_postfix(Parser *p) {
    int is_identifier_lvalue = 0;
    int is_callable_target = 0;
    int callable_callee_kind = AST_CALL_CALLEE_NON_IDENTIFIER;
    const Token *callable_name_token = NULL;

    if (!parse_primary_with_flags(p,
                                  &is_identifier_lvalue,
                                  &is_callable_target,
                                  &callable_name_token,
                                  &callable_callee_kind)) {
        return 0;
    }

    while (1) {
        if (match(p, TOKEN_LPAREN)) {
            const Token *call_site_tok = previous(p);
            size_t arg_count = 0;
            int callee_kind = callable_callee_kind;

            if (!is_callable_target) {
                set_error(p, previous(p), "Expected callable expression before '('");
                return 0;
            }
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    if (!parse_assignment(p)) {
                        return 0;
                    }
                    arg_count++;
                } while (match(p, TOKEN_COMMA));
            }
            if (!consume(p, TOKEN_RPAREN, "')'")) {
                return 0;
            }
            if (!parser_record_function_call_name(p,
                                                  callable_name_token,
                                                  call_site_tok,
                                                  arg_count,
                                                  callee_kind)) {
                return 0;
            }
            is_identifier_lvalue = 0;
            is_callable_target = 1;
            /* After a call, callee identity is no longer a direct identifier token. */
            callable_name_token = NULL;
            callable_callee_kind = AST_CALL_CALLEE_CALL_RESULT;
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
            callable_name_token = NULL;
            callable_callee_kind = AST_CALL_CALLEE_NON_IDENTIFIER;
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

static int parse_expression_statement(Parser *p, AstStatement **out_statement) {
    const Token *anchor_tok = peek(p);
    AstStatement *stmt = NULL;
    size_t expr_start = p->current;
    size_t expr_end = p->current;
    AstExpression *expr_ast = NULL;

    if (out_statement) {
        *out_statement = NULL;
    }

    if (match(p, TOKEN_SEMICOLON)) {
        if (out_statement) {
            stmt = alloc_ast_statement(p, AST_STMT_EXPRESSION, previous(p));
            if (!stmt) {
                return 0;
            }
            *out_statement = stmt;
        }
        return 1;
    }
    if (!parse_expression(p)) {
        return 0;
    }
    expr_end = p->current;
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        return 0;
    }

    if (out_statement) {
        stmt = alloc_ast_statement(p, AST_STMT_EXPRESSION, anchor_tok);
        if (!stmt) {
            return 0;
        }
        if (!build_expression_ast_from_slice(p,
                                             expr_start,
                                             expr_end,
                                             &expr_ast,
                                             anchor_tok)) {
            ast_statement_free_internal(stmt);
            return 0;
        }
        if (expr_ast && !append_ast_statement_expression(p, stmt, expr_ast, anchor_tok)) {
            ast_statement_free_internal(stmt);
            return 0;
        }
        if (expr_ast) {
            stmt->has_primary_expression = 1;
            stmt->primary_expression_index = stmt->expression_count - 1;
        }
        *out_statement = stmt;
    }

    return 1;
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
                                              int *out_may_break,
                                              AstStatement **out_statement) {
    int block_guaranteed_return = 0;
    int block_may_fallthrough = 1;
    int block_may_break = 0;
    AstStatement **children = NULL;
    size_t child_count = 0;
    size_t child_capacity = 0;
    const Token *lbrace_tok = NULL;

    if (out_statement) {
        *out_statement = NULL;
    }

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (!consume_token(p, TOKEN_LBRACE, "'{'", &lbrace_tok)) {
        return 0;
    }

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        if (check(p, TOKEN_KW_INT)) {
            const Token *decl_tok = peek(p);

            if (p->track_function_declaration_count) {
                p->function_declaration_count++;
            }
            if (!parse_declaration(p, NULL)) {
                free_ast_statement_array(children, child_count);
                return 0;
            }
            if (out_statement) {
                AstStatement *decl_stmt =
                    alloc_ast_statement(p, AST_STMT_DECLARATION, decl_tok);
                if (!decl_stmt) {
                    free_ast_statement_array(children, child_count);
                    return 0;
                }
                if (!append_ast_statement_child(p,
                                                &children,
                                                &child_count,
                                                &child_capacity,
                                                decl_stmt,
                                                decl_tok)) {
                    free_ast_statement_array(children, child_count);
                    return 0;
                }
            }
            if (block_may_fallthrough) {
                block_guaranteed_return = 0;
            }
        } else {
            int stmt_guaranteed_return = 0;
            int stmt_may_fallthrough = 1;
            int stmt_may_break = 0;
            AstStatement *child_stmt = NULL;

            if (!parse_statement_with_flow(p,
                                           &stmt_guaranteed_return,
                                           &stmt_may_fallthrough,
                                           &stmt_may_break,
                                           out_statement ? &child_stmt : NULL)) {
                free_ast_statement_array(children, child_count);
                return 0;
            }

            if (out_statement && child_stmt) {
                if (!append_ast_statement_child(p,
                                                &children,
                                                &child_count,
                                                &child_capacity,
                                                child_stmt,
                                                lbrace_tok)) {
                    free_ast_statement_array(children, child_count);
                    return 0;
                }
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
        free_ast_statement_array(children, child_count);
        return 0;
    }

    if (out_statement) {
        AstStatement *compound_stmt = alloc_ast_statement(p, AST_STMT_COMPOUND, lbrace_tok);
        if (!compound_stmt) {
            free_ast_statement_array(children, child_count);
            return 0;
        }
        compound_stmt->children = children;
        compound_stmt->child_count = child_count;
        *out_statement = compound_stmt;
    } else {
        free_ast_statement_array(children, child_count);
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
                                        int *out_may_break,
                                        AstStatement **out_statement) {
    int then_guaranteed_return = 0;
    int then_may_fallthrough = 1;
    int then_may_break = 0;
    int else_guaranteed_return = 0;
    int else_may_fallthrough = 1;
    int else_may_break = 0;
    int has_else = 0;
    const Token *if_tok = previous(p);
    AstStatement *then_stmt = NULL;
    AstStatement *else_stmt = NULL;
    AstExpression *cond_expr = NULL;
    AstStatement **children = NULL;
    size_t child_count = 0;
    size_t child_capacity = 0;

    if (out_statement) {
        *out_statement = NULL;
    }

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
    {
        size_t cond_start = p->current;
        size_t cond_end;

        if (!parse_expression(p)) {
            return 0;
        }
        cond_end = p->current;
        if (!build_expression_ast_from_slice(p,
                                             cond_start,
                                             cond_end,
                                             out_statement ? &cond_expr : NULL,
                                             if_tok)) {
            return 0;
        }
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        ast_expression_free_internal(cond_expr);
        return 0;
    }
    if (!parse_statement_with_flow(p,
                                   &then_guaranteed_return,
                                   &then_may_fallthrough,
                                   &then_may_break,
                                   out_statement ? &then_stmt : NULL)) {
        ast_expression_free_internal(cond_expr);
        return 0;
    }
    if (match(p, TOKEN_KW_ELSE)) {
        has_else = 1;
        if (!parse_statement_with_flow(p,
                                       &else_guaranteed_return,
                                       &else_may_fallthrough,
                                       &else_may_break,
                                       out_statement ? &else_stmt : NULL)) {
            ast_statement_free_internal(then_stmt);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
    }

    if (out_statement) {
        AstStatement *if_stmt;

        if (then_stmt &&
            !append_ast_statement_child(p,
                                        &children,
                                        &child_count,
                                        &child_capacity,
                                        then_stmt,
                                        if_tok)) {
            free_ast_statement_array(children, child_count);
            ast_statement_free_internal(else_stmt);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
        if (else_stmt &&
            !append_ast_statement_child(p,
                                        &children,
                                        &child_count,
                                        &child_capacity,
                                        else_stmt,
                                        if_tok)) {
            free_ast_statement_array(children, child_count);
            ast_expression_free_internal(cond_expr);
            return 0;
        }

        if_stmt = alloc_ast_statement(p, AST_STMT_IF, if_tok);
        if (!if_stmt) {
            free_ast_statement_array(children, child_count);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
        if (cond_expr && !append_ast_statement_expression(p, if_stmt, cond_expr, if_tok)) {
            ast_statement_free_internal(if_stmt);
            free_ast_statement_array(children, child_count);
            return 0;
        }
        if (cond_expr) {
            if_stmt->has_condition_expression = 1;
            if_stmt->condition_expression_index = if_stmt->expression_count - 1;
        }
        if_stmt->children = children;
        if_stmt->child_count = child_count;
        *out_statement = if_stmt;
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
                                           int *out_may_break,
                                           AstStatement **out_statement) {
    size_t cond_start;
    int cond_always_true;
    int body_guaranteed_return = 0;
    int body_may_break = 0;
    AstStatement *body_stmt = NULL;
    AstExpression *cond_expr = NULL;
    const Token *while_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }

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
    if (!build_expression_ast_from_slice(p,
                                         cond_start,
                                         p->current,
                                         out_statement ? &cond_expr : NULL,
                                         while_tok)) {
        return 0;
    }
    cond_always_true = expression_slice_is_constant_true(p->tokens, cond_start, p->current);
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        ast_expression_free_internal(cond_expr);
        return 0;
    }

    p->loop_depth++;
    if (!parse_statement_with_flow(p,
                                   &body_guaranteed_return,
                                   NULL,
                                   &body_may_break,
                                   out_statement ? &body_stmt : NULL)) {
        p->loop_depth--;
        ast_expression_free_internal(cond_expr);
        return 0;
    }
    p->loop_depth--;

    if (out_statement) {
        AstStatement *while_stmt = alloc_ast_statement(p, AST_STMT_WHILE, while_tok);
        if (!while_stmt) {
            ast_statement_free_internal(body_stmt);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
        while_stmt->children = (AstStatement **)malloc(sizeof(AstStatement *));
        if (!while_stmt->children) {
            set_error(p, while_tok, "Out of memory while building statement AST");
            ast_statement_free_internal(while_stmt);
            ast_statement_free_internal(body_stmt);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
        if (cond_expr && !append_ast_statement_expression(p, while_stmt, cond_expr, while_tok)) {
            ast_statement_free_internal(while_stmt);
            ast_statement_free_internal(body_stmt);
            return 0;
        }
        if (cond_expr) {
            while_stmt->has_condition_expression = 1;
            while_stmt->condition_expression_index = while_stmt->expression_count - 1;
        }
        while_stmt->children[0] = body_stmt;
        while_stmt->child_count = 1;
        *out_statement = while_stmt;
    }

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
                                         int *out_may_break,
                                         AstStatement **out_statement) {
    int cond_always_true = 0;
    size_t cond_start = 0;
    int body_guaranteed_return = 0;
    int body_may_break = 0;
    AstStatement *body_stmt = NULL;
    AstExpression *init_expr = NULL;
    AstExpression *cond_expr = NULL;
    AstExpression *step_expr = NULL;
    const Token *for_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }

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
        size_t init_start = p->current;
        size_t init_end = p->current;

        if (!check(p, TOKEN_SEMICOLON)) {
            if (!parse_expression(p)) {
                return 0;
            }
            init_end = p->current;
            if (!build_expression_ast_from_slice(p,
                                                 init_start,
                                                 init_end,
                                                 out_statement ? &init_expr : NULL,
                                                 for_tok)) {
                return 0;
            }
        }
        if (!consume(p, TOKEN_SEMICOLON, "';'")) {
            ast_expression_free_internal(init_expr);
            return 0;
        }
    }

    if (!check(p, TOKEN_SEMICOLON)) {
        cond_start = p->current;
        if (!parse_expression(p)) {
            ast_expression_free_internal(init_expr);
            return 0;
        }
        if (!build_expression_ast_from_slice(p,
                                             cond_start,
                                             p->current,
                                             out_statement ? &cond_expr : NULL,
                                             for_tok)) {
            ast_expression_free_internal(init_expr);
            return 0;
        }
        cond_always_true = expression_slice_is_constant_true(p->tokens, cond_start, p->current);
    } else {
        cond_always_true = 1;
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        ast_expression_free_internal(init_expr);
        ast_expression_free_internal(cond_expr);
        return 0;
    }

    if (!check(p, TOKEN_RPAREN)) {
        size_t step_start = p->current;
        size_t step_end;

        if (!parse_expression(p)) {
            ast_expression_free_internal(init_expr);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
        step_end = p->current;
        if (!build_expression_ast_from_slice(p,
                                             step_start,
                                             step_end,
                                             out_statement ? &step_expr : NULL,
                                             for_tok)) {
            ast_expression_free_internal(init_expr);
            ast_expression_free_internal(cond_expr);
            return 0;
        }
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        ast_expression_free_internal(init_expr);
        ast_expression_free_internal(cond_expr);
        ast_expression_free_internal(step_expr);
        return 0;
    }

    p->loop_depth++;
    if (!parse_statement_with_flow(p,
                                   &body_guaranteed_return,
                                   NULL,
                                   &body_may_break,
                                   out_statement ? &body_stmt : NULL)) {
        p->loop_depth--;
        ast_expression_free_internal(init_expr);
        ast_expression_free_internal(cond_expr);
        ast_expression_free_internal(step_expr);
        return 0;
    }
    p->loop_depth--;

    if (out_statement) {
        AstStatement *for_stmt = alloc_ast_statement(p, AST_STMT_FOR, for_tok);
        if (!for_stmt) {
            ast_statement_free_internal(body_stmt);
            ast_expression_free_internal(init_expr);
            ast_expression_free_internal(cond_expr);
            ast_expression_free_internal(step_expr);
            return 0;
        }
        for_stmt->children = (AstStatement **)malloc(sizeof(AstStatement *));
        if (!for_stmt->children) {
            set_error(p, for_tok, "Out of memory while building statement AST");
            ast_statement_free_internal(for_stmt);
            ast_statement_free_internal(body_stmt);
            ast_expression_free_internal(init_expr);
            ast_expression_free_internal(cond_expr);
            ast_expression_free_internal(step_expr);
            return 0;
        }
        if (init_expr && !append_ast_statement_expression(p, for_stmt, init_expr, for_tok)) {
            ast_statement_free_internal(for_stmt);
            ast_statement_free_internal(body_stmt);
            ast_expression_free_internal(cond_expr);
            ast_expression_free_internal(step_expr);
            return 0;
        }
        if (init_expr) {
            for_stmt->has_for_init_expression = 1;
            for_stmt->for_init_expression_index = for_stmt->expression_count - 1;
        }
        if (cond_expr && !append_ast_statement_expression(p, for_stmt, cond_expr, for_tok)) {
            ast_statement_free_internal(for_stmt);
            ast_statement_free_internal(body_stmt);
            ast_expression_free_internal(step_expr);
            return 0;
        }
        if (cond_expr) {
            for_stmt->has_condition_expression = 1;
            for_stmt->condition_expression_index = for_stmt->expression_count - 1;
        }
        if (step_expr && !append_ast_statement_expression(p, for_stmt, step_expr, for_tok)) {
            ast_statement_free_internal(for_stmt);
            ast_statement_free_internal(body_stmt);
            return 0;
        }
        if (step_expr) {
            for_stmt->has_for_step_expression = 1;
            for_stmt->for_step_expression_index = for_stmt->expression_count - 1;
        }
        for_stmt->children[0] = body_stmt;
        for_stmt->child_count = 1;
        *out_statement = for_stmt;
    }

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

static int parse_return_statement(Parser *p, AstStatement **out_statement) {
    const Token *return_tok = previous(p);
    size_t expr_start = p->current;
    size_t expr_end = p->current;
    AstExpression *expr_ast = NULL;

    if (out_statement) {
        *out_statement = NULL;
    }

    if (!check(p, TOKEN_SEMICOLON)) {
        if (!parse_expression(p)) {
            return 0;
        }
        expr_end = p->current;
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        return 0;
    }
    if (p->track_function_return_count) {
        p->function_return_count++;
    }

    if (out_statement) {
        AstStatement *stmt = alloc_ast_statement(p, AST_STMT_RETURN, return_tok);
        if (!stmt) {
            return 0;
        }
        if (!build_expression_ast_from_slice(p,
                                             expr_start,
                                             expr_end,
                                             &expr_ast,
                                             return_tok)) {
            ast_statement_free_internal(stmt);
            return 0;
        }
        if (expr_ast && !append_ast_statement_expression(p, stmt, expr_ast, return_tok)) {
            ast_statement_free_internal(stmt);
            return 0;
        }
        if (expr_ast) {
            stmt->has_primary_expression = 1;
            stmt->primary_expression_index = stmt->expression_count - 1;
        }
        *out_statement = stmt;
    }

    return 1;
}

static int parse_break_statement(Parser *p, AstStatement **out_statement) {
    const Token *break_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }

    if (p->loop_depth == 0) {
        set_error(p, previous(p), "'break' is only allowed inside loops");
        return 0;
    }
    if (p->track_function_break_count) {
        p->function_break_count++;
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        return 0;
    }

    if (out_statement) {
        AstStatement *stmt = alloc_ast_statement(p, AST_STMT_BREAK, break_tok);
        if (!stmt) {
            return 0;
        }
        *out_statement = stmt;
    }

    return 1;
}

static int parse_continue_statement(Parser *p, AstStatement **out_statement) {
    const Token *continue_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }

    if (p->loop_depth == 0) {
        set_error(p, previous(p), "'continue' is only allowed inside loops");
        return 0;
    }
    if (p->track_function_continue_count) {
        p->function_continue_count++;
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        return 0;
    }

    if (out_statement) {
        AstStatement *stmt = alloc_ast_statement(p, AST_STMT_CONTINUE, continue_tok);
        if (!stmt) {
            return 0;
        }
        *out_statement = stmt;
    }

    return 1;
}

static int parse_statement_with_flow(Parser *p,
                                     int *out_guaranteed_return,
                                     int *out_may_fallthrough,
                                     int *out_may_break,
                                     AstStatement **out_statement) {
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
    if (out_statement) {
        *out_statement = NULL;
    }

    if (!enter_statement_recursion(p, "statement")) {
        return 0;
    }

    if (match(p, TOKEN_LBRACE)) {
        AstStatement *stmt = NULL;

        p->current--;
        ok = parse_compound_statement_with_flow(p,
                                                &guaranteed_return,
                                                &may_fallthrough,
                                                &may_break,
                                                out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_IF)) {
        AstStatement *stmt = NULL;

        ok = parse_if_statement_with_flow(p,
                                          &guaranteed_return,
                                          &may_fallthrough,
                                          &may_break,
                                          out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_WHILE)) {
        AstStatement *stmt = NULL;

        ok = parse_while_statement_with_flow(p,
                                             &guaranteed_return,
                                             &may_fallthrough,
                                             &may_break,
                                             out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_FOR)) {
        AstStatement *stmt = NULL;

        ok = parse_for_statement_with_flow(p,
                                           &guaranteed_return,
                                           &may_fallthrough,
                                           &may_break,
                                           out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_RETURN)) {
        AstStatement *stmt = NULL;

        ok = parse_return_statement(p, out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_BREAK)) {
        AstStatement *stmt = NULL;

        ok = parse_break_statement(p, out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_CONTINUE)) {
        AstStatement *stmt = NULL;

        ok = parse_continue_statement(p, out_statement ? &stmt : NULL);
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
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }

    ok = parse_expression_statement(p, out_statement);
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
                                   AstStatement **out_function_body,
                                   size_t *out_return_statement_count,
                                   int *out_returns_on_all_paths,
                                   size_t *out_loop_statement_count,
                                   size_t *out_if_statement_count,
                                   size_t *out_break_statement_count,
                                   size_t *out_continue_statement_count,
                                   size_t *out_declaration_statement_count,
                                   char ***out_called_function_names,
                                   int **out_called_function_lines,
                                   int **out_called_function_columns,
                                   size_t **out_called_function_arg_counts,
                                   int **out_called_function_kinds,
                                   size_t *out_called_function_count) {
    const Token *name_tok = NULL;
    size_t parameter_count = 0;
    int has_unnamed_parameter = 0;
    int body_ok;
    int function_returns_on_all_paths = 0;
    AstStatement *function_body = NULL;

    if (out_name_token) {
        *out_name_token = NULL;
    }
    if (out_parameter_count) {
        *out_parameter_count = 0;
    }
    if (out_is_definition) {
        *out_is_definition = 0;
    }
    if (out_function_body) {
        *out_function_body = NULL;
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
    if (out_called_function_names) {
        *out_called_function_names = NULL;
    }
    if (out_called_function_lines) {
        *out_called_function_lines = NULL;
    }
    if (out_called_function_columns) {
        *out_called_function_columns = NULL;
    }
    if (out_called_function_arg_counts) {
        *out_called_function_arg_counts = NULL;
    }
    if (out_called_function_kinds) {
        *out_called_function_kinds = NULL;
    }
    if (out_called_function_count) {
        *out_called_function_count = 0;
    }

    p->track_function_calls = 0;
    parser_clear_function_called_names(p);

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
        p->track_function_calls = 1;
        body_ok = parse_compound_statement_with_flow(p,
                                                     &function_returns_on_all_paths,
                                                     NULL,
                                                     NULL,
                                                     out_function_body ? &function_body : NULL);
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
        p->track_function_calls = 0;
        if (body_ok && out_called_function_names && out_called_function_lines &&
            out_called_function_columns && out_called_function_arg_counts &&
            out_called_function_kinds &&
            out_called_function_count) {
            *out_called_function_names = p->function_called_names;
            *out_called_function_lines = p->function_called_lines;
            *out_called_function_columns = p->function_called_columns;
            *out_called_function_arg_counts = p->function_called_arg_counts;
            *out_called_function_kinds = p->function_called_kinds;
            *out_called_function_count = p->function_called_count;
            p->function_called_names = NULL;
            p->function_called_lines = NULL;
            p->function_called_columns = NULL;
            p->function_called_arg_counts = NULL;
            p->function_called_kinds = NULL;
            p->function_called_count = 0;
            p->function_called_capacity = 0;
        } else {
            parser_clear_function_called_names(p);
        }

        if (body_ok) {
            if (out_function_body) {
                *out_function_body = function_body;
            } else {
                ast_statement_free_internal(function_body);
            }
        } else {
            ast_statement_free_internal(function_body);
        }

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
    AstStatement *external_function_body = NULL;
    size_t external_return_statement_count = 0;
    int external_returns_on_all_paths = 0;
    size_t external_loop_statement_count = 0;
    size_t external_if_statement_count = 0;
    size_t external_break_statement_count = 0;
    size_t external_continue_statement_count = 0;
    size_t external_declaration_statement_count = 0;
    char **external_called_function_names = NULL;
    int *external_called_function_lines = NULL;
    int *external_called_function_columns = NULL;
    size_t *external_called_function_arg_counts = NULL;
    int *external_called_function_kinds = NULL;
    size_t external_called_function_count = 0;

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
    p.track_function_calls = 0;
    p.function_called_names = NULL;
    p.function_called_lines = NULL;
    p.function_called_columns = NULL;
    p.function_called_arg_counts = NULL;
    p.function_called_kinds = NULL;
    p.function_called_count = 0;
    p.function_called_capacity = 0;

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
        external_function_body = NULL;
        external_return_statement_count = 0;
        external_returns_on_all_paths = 0;
        external_loop_statement_count = 0;
        external_if_statement_count = 0;
        external_break_statement_count = 0;
        external_continue_statement_count = 0;
        external_declaration_statement_count = 0;
        external_called_function_names = NULL;
        external_called_function_lines = NULL;
        external_called_function_columns = NULL;
        external_called_function_arg_counts = NULL;
        external_called_function_kinds = NULL;
        external_called_function_count = 0;

        if (!parse_function_external(&p,
                                     &external_name,
                                     &external_parameter_count,
                                     &external_is_definition,
                                     &external_function_body,
                                     &external_return_statement_count,
                                     &external_returns_on_all_paths,
                                     &external_loop_statement_count,
                                     &external_if_statement_count,
                                     &external_break_statement_count,
                                     &external_continue_statement_count,
                                     &external_declaration_statement_count,
                                     &external_called_function_names,
                                     &external_called_function_lines,
                                     &external_called_function_columns,
                                     &external_called_function_arg_counts,
                                     &external_called_function_kinds,
                                     &external_called_function_count)) {
            p.current = start;
            if (!parse_declaration(&p, out_program)) {
                parser_clear_function_called_names(&p);
                ast_program_clear_storage(out_program);
                return 0;
            }
        } else {
            if (!ast_program_append_external(out_program, AST_EXTERNAL_FUNCTION, external_name)) {
                size_t i;

                ast_statement_free_internal(external_function_body);
                for (i = 0; i < external_called_function_count; ++i) {
                    free(external_called_function_names[i]);
                }
                free(external_called_function_names);
                free(external_called_function_lines);
                free(external_called_function_columns);
                free(external_called_function_arg_counts);
                free(external_called_function_kinds);
                set_error(&p, peek(&p), "Out of memory while building AST");
                ast_program_clear_storage(out_program);
                return 0;
            }
            out_program->externals[out_program->count - 1].parameter_count = external_parameter_count;
            out_program->externals[out_program->count - 1].is_function_definition =
                external_is_definition;
            out_program->externals[out_program->count - 1].function_body = external_function_body;
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
            out_program->externals[out_program->count - 1].called_function_names =
                external_called_function_names;
            out_program->externals[out_program->count - 1].called_function_lines =
                external_called_function_lines;
            out_program->externals[out_program->count - 1].called_function_columns =
                external_called_function_columns;
            out_program->externals[out_program->count - 1].called_function_arg_counts =
                external_called_function_arg_counts;
            out_program->externals[out_program->count - 1].called_function_kinds =
                external_called_function_kinds;
            out_program->externals[out_program->count - 1].called_function_count =
                external_called_function_count;
        }
    }

    parser_clear_function_called_names(&p);

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
    p.track_function_calls = 0;
    p.function_called_names = NULL;
    p.function_called_lines = NULL;
    p.function_called_columns = NULL;
    p.function_called_arg_counts = NULL;
    p.function_called_kinds = NULL;
    p.function_called_count = 0;
    p.function_called_capacity = 0;

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    while (!is_at_end(&p)) {
        start = p.current;
        if (!parse_function_external(&p,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL)) {
            p.current = start;
            if (!parse_declaration(&p, NULL)) {
                parser_clear_function_called_names(&p);
                return 0;
            }
        }
    }

    parser_clear_function_called_names(&p);

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
    p.track_function_calls = 0;
    p.function_called_names = NULL;
    p.function_called_lines = NULL;
    p.function_called_columns = NULL;
    p.function_called_arg_counts = NULL;
    p.function_called_kinds = NULL;
    p.function_called_count = 0;
    p.function_called_capacity = 0;

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