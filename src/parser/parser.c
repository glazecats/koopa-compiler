#include "parser.h"
#include "ast_internal.h"

#include <stdarg.h>
#include <stdio.h>

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

static int parse_primary(Parser *p) {
    if (match(p, TOKEN_NUMBER) || match(p, TOKEN_IDENTIFIER)) {
        return 1;
    }

    if (match(p, TOKEN_LPAREN)) {
        if (!parse_expression(p)) {
            return 0;
        }
        return consume(p, TOKEN_RPAREN, "')'");
    }

    set_error(p, peek(p), "Expected expression, got %s", lexer_token_type_name(peek(p)->type));
    return 0;
}

static int parse_unary(Parser *p) {
    int ok;

    if (!enter_expression_recursion(p, "unary-expression")) {
        return 0;
    }

    if (match(p, TOKEN_MINUS) || match(p, TOKEN_PLUS)) {
        ok = parse_unary(p);
        leave_expression_recursion(p);
        return ok;
    }

    ok = parse_primary(p);
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

static int parse_relational(Parser *p) {
    if (!parse_additive(p)) {
        return 0;
    }

    while (match(p, TOKEN_LT) || match(p, TOKEN_LE) || match(p, TOKEN_GT) || match(p, TOKEN_GE)) {
        if (!parse_additive(p)) {
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

static int parse_assignment(Parser *p) {
    int ok;
    size_t start = p->current;

    if (!enter_expression_recursion(p, "assignment-expression")) {
        return 0;
    }

    if (match(p, TOKEN_IDENTIFIER) && match(p, TOKEN_ASSIGN)) {
        ok = parse_assignment(p);
        leave_expression_recursion(p);
        return ok;
    }

    p->current = start;
    ok = parse_equality(p);
    leave_expression_recursion(p);
    return ok;
}

static int parse_expression(Parser *p) {
    return parse_assignment(p);
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
    return consume(p, TOKEN_SEMICOLON, "';'");
}

static int parse_continue_statement(Parser *p) {
    if (p->loop_depth == 0) {
        set_error(p, previous(p), "'continue' is only allowed inside loops");
        return 0;
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
        if (!parse_expression(p)) {
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
                                   size_t *out_if_statement_count) {
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
        p->track_function_return_count = 0;
        p->track_function_loop_count = 0;
        p->track_function_if_count = 0;
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

        if (!parse_function_external(&p,
                                     &external_name,
                                     &external_parameter_count,
                                     &external_is_definition,
                                     &external_return_statement_count,
                                     &external_returns_on_all_paths,
                                     &external_loop_statement_count,
                                     &external_if_statement_count)) {
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

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    while (!is_at_end(&p)) {
        start = p.current;
        if (!parse_function_external(&p, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
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