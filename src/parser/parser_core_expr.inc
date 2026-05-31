#if defined(__INTELLISENSE__) && !defined(PARSER_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(PARSER_SPLIT_AGGREGATOR)
#define PARSER_SPLIT_INTELLISENSE_CONTEXT
#include "parser.c"
#else

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
            "Parser expression recursion limit (%zu call-depth frames) exceeded while parsing %s",
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

static AstExpression *parse_expression_ast_primary_only(Parser *p);
static AstExpression *try_parse_expression_ast_conversion(Parser *p);
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

static int check_assignment_operator(const Parser *p) {
    TokenType type;

    if (!p) {
        return 0;
    }

    type = peek((Parser *)p)->type;
    return type == TOKEN_ASSIGN || type == TOKEN_PLUS_ASSIGN || type == TOKEN_MINUS_ASSIGN ||
        type == TOKEN_STAR_ASSIGN || type == TOKEN_SLASH_ASSIGN ||
        type == TOKEN_PERCENT_ASSIGN || type == TOKEN_AMP_ASSIGN ||
        type == TOKEN_CARET_ASSIGN || type == TOKEN_PIPE_ASSIGN ||
        type == TOKEN_SHIFT_LEFT_ASSIGN || type == TOKEN_SHIFT_RIGHT_ASSIGN;
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

static int token_slice_is_parenthesized_lvalue_form(const TokenArray *tokens,
    size_t start,
    size_t end) {
    size_t i;

    if (!tokens || !tokens->data || end <= start) {
        return 0;
    }

    trim_wrapping_parentheses(tokens, &start, &end);
    if (end <= start || tokens->data[start].type != TOKEN_IDENTIFIER) {
        return 0;
    }

    i = start + 1u;
    while (i < end) {
        size_t bracket_depth = 0u;
        size_t paren_depth = 0u;

        if (tokens->data[i].type == TOKEN_DOT) {
            if (i + 1u >= end || tokens->data[i + 1u].type != TOKEN_IDENTIFIER) {
                return 0;
            }
            i += 2u;
            continue;
        }

        if (tokens->data[i].type != TOKEN_LBRACKET) {
            return 0;
        }
        ++i;
        ++bracket_depth;
        while (i < end && bracket_depth > 0u) {
            if (tokens->data[i].type == TOKEN_LPAREN) {
                ++paren_depth;
            } else if (tokens->data[i].type == TOKEN_RPAREN) {
                if (paren_depth > 0u) {
                    --paren_depth;
                }
            } else if (tokens->data[i].type == TOKEN_LBRACKET) {
                ++bracket_depth;
            } else if (tokens->data[i].type == TOKEN_RBRACKET) {
                --bracket_depth;
                if (bracket_depth == 0u) {
                    ++i;
                    break;
                }
            }
            ++i;
        }
        if (bracket_depth != 0u || paren_depth != 0u) {
            return 0;
        }
    }

    return i == end;
}

static int token_slice_is_assignment_lvalue_form(const TokenArray *tokens,
    size_t start,
    size_t end) {
    size_t i;

    if (!tokens || !tokens->data || end <= start) {
        return 0;
    }

    if (tokens->data[start].type != TOKEN_IDENTIFIER) {
        return 0;
    }

    i = start + 1u;
    while (i < end) {
        size_t bracket_depth = 0u;
        size_t paren_depth = 0u;

        if (tokens->data[i].type == TOKEN_DOT) {
            if (i + 1u >= end || tokens->data[i + 1u].type != TOKEN_IDENTIFIER) {
                return 0;
            }
            i += 2u;
            continue;
        }

        if (tokens->data[i].type != TOKEN_LBRACKET) {
            return 0;
        }
        ++i;
        ++bracket_depth;
        while (i < end && bracket_depth > 0u) {
            if (tokens->data[i].type == TOKEN_LPAREN) {
                ++paren_depth;
            } else if (tokens->data[i].type == TOKEN_RPAREN) {
                if (paren_depth > 0u) {
                    --paren_depth;
                }
            } else if (tokens->data[i].type == TOKEN_LBRACKET) {
                ++bracket_depth;
            } else if (tokens->data[i].type == TOKEN_RBRACKET) {
                --bracket_depth;
                if (bracket_depth == 0u) {
                    ++i;
                    break;
                }
            }
            ++i;
        }
        if (bracket_depth != 0u || paren_depth != 0u) {
            return 0;
        }
    }

    return i == end;
}

static AstExpression *alloc_ast_expression(AstExpressionKind kind,
    int line,
    int column);
static int append_ast_expression_slot(Parser *p,
    AstExpression ***items,
    size_t *count,
    size_t *capacity,
    AstExpression *expr,
    const Token *anchor_tok);

static int parse_initializer_expression(Parser *p, AstExpression **out_expr, const Token *anchor_tok) {
    AstExpression *expr = NULL;
    size_t start = 0u;

    if (out_expr) {
        *out_expr = NULL;
    }

    if (match(p, TOKEN_LBRACE)) {
        const Token *lbrace_tok = previous(p);
        AstExpression **items = NULL;
        size_t item_count = 0u;
        size_t item_capacity = 0u;

        expr = alloc_ast_expression(AST_EXPR_INIT_LIST,
            lbrace_tok ? lbrace_tok->line : 0,
            lbrace_tok ? lbrace_tok->column : 0);
        if (!expr) {
            set_error(p,
                lbrace_tok ? lbrace_tok : (anchor_tok ? anchor_tok : peek(p)),
                "Out of memory while building expression AST");
            return 0;
        }

        if (!check(p, TOKEN_RBRACE)) {
            do {
                AstExpression *item = NULL;

                if (!parse_initializer_expression(p, &item, lbrace_tok)) {
                    free(items);
                    parser_ast_expression_free(expr);
                    return 0;
                }
                if (!append_ast_expression_slot(
                        p,
                        &items,
                        &item_count,
                        &item_capacity,
                        item,
                        lbrace_tok)) {
                    free(items);
                    parser_ast_expression_free(expr);
                    return 0;
                }
            } while (match(p, TOKEN_COMMA) && !check(p, TOKEN_RBRACE));
        }
        if (!consume(p, TOKEN_RBRACE, "'}'")) {
            free(items);
            parser_ast_expression_free(expr);
            return 0;
        }

        expr->as.init_list.items = items;
        expr->as.init_list.item_count = item_count;
        if (out_expr) {
            *out_expr = expr;
        } else {
            parser_ast_expression_free(expr);
        }
        return 1;
    }

    start = p->current;
    if (!parse_assignment(p)) {
        return 0;
    }
    return build_expression_ast_from_slice(
        p,
        start,
        p->current,
        out_expr,
        anchor_tok);
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
    stmt->extension_origin = AST_EXTENSION_ORIGIN_NONE;
    stmt->line = anchor_tok ? anchor_tok->line : 0;
    stmt->column = anchor_tok ? anchor_tok->column : 0;
    stmt->declaration_is_const = 0;
    stmt->declaration_names = NULL;
    stmt->declaration_name_count = 0;
    stmt->declaration_value_kinds = NULL;
    stmt->declaration_array_ranks = NULL;
    stmt->declaration_array_extent_exprs = NULL;
    stmt->declaration_function_return_types = NULL;
    stmt->declaration_function_parameter_counts = NULL;
    stmt->declaration_function_parameter_value_kinds = NULL;
    stmt->declaration_type_names = NULL;
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
    stmt->capture_names = NULL;
    stmt->capture_name_count = 0u;
    stmt->capture_expressions = NULL;
    stmt->capture_expression_count = 0u;
    stmt->children = NULL;
    stmt->child_count = 0;
    return stmt;
}

static char *copy_token_identifier_lexeme(const Token *tok) {
    char *name_copy;

    if (!tok || !tok->lexeme || tok->length == 0) {
        return NULL;
    }

    name_copy = (char *)malloc(tok->length + 1);
    if (!name_copy) {
        return NULL;
    }
    memcpy(name_copy, tok->lexeme, tok->length);
    name_copy[tok->length] = '\0';
    return name_copy;
}

static void free_name_array(char **names, size_t count) {
    size_t i;

    if (!names) {
        return;
    }

    for (i = 0; i < count; ++i) {
        free(names[i]);
    }
    free(names);
}

static int parser_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity) {
    size_t next_capacity;

    if (!out_next_capacity || initial == 0 || elem_size == 0) {
        return 0;
    }

    if (current == 0) {
        next_capacity = initial;
    } else {
        if (current > ((size_t)-1) / 2) {
            return 0;
        }
        next_capacity = current * 2;
    }

    if (next_capacity > ((size_t)-1) / elem_size) {
        return 0;
    }

    *out_next_capacity = next_capacity;
    return 1;
}

static int append_name_array_item(Parser *p,
    char ***names,
    size_t *count,
    size_t *capacity,
    char *name,
    const Token *anchor_tok) {
    char **new_items;
    size_t next_capacity;

    if (!names || !count || !capacity) {
        free(name);
        return 0;
    }

    if (*count == *capacity) {
        if (!parser_next_growth_capacity(*capacity, 4, sizeof(char *), &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST identifier metadata");
            free(name);
            return 0;
        }
        new_items = (char **)realloc(*names, next_capacity * sizeof(char *));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST identifier metadata");
            free(name);
            return 0;
        }
        *names = new_items;
        *capacity = next_capacity;
    }

    (*names)[*count] = name;
    (*count)++;
    return 1;
}

static int append_nullable_name_array_item(Parser *p,
    char ***names,
    size_t index,
    size_t *capacity,
    char *name,
    const Token *anchor_tok) {
    char **new_items;
    size_t old_capacity;
    size_t next_capacity;

    if (!names || !capacity) {
        free(name);
        return 0;
    }

    if (index >= *capacity) {
        old_capacity = *capacity;
        if (!parser_next_growth_capacity(*capacity, 4, sizeof(char *), &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST identifier metadata");
            free(name);
            return 0;
        }
        new_items = (char **)realloc(*names, next_capacity * sizeof(char *));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST identifier metadata");
            free(name);
            return 0;
        }
        *names = new_items;
        *capacity = next_capacity;
        while (old_capacity < next_capacity) {
            (*names)[old_capacity++] = NULL;
        }
    }
    (*names)[index] = name;
    return 1;
}

static int append_size_t_array_item(Parser *p,
    size_t **items,
    size_t count,
    size_t *capacity,
    size_t value,
    const Token *anchor_tok) {
    size_t *new_items;
    size_t next_capacity;

    if (!items || !capacity) {
        return 0;
    }

    if (count == *capacity) {
        if (!parser_next_growth_capacity(*capacity, 4, sizeof(size_t), &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST array metadata");
            return 0;
        }
        new_items = (size_t *)realloc(*items, next_capacity * sizeof(size_t));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST array metadata");
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[count] = value;
    return 1;
}

static int append_function_return_type_array_item(Parser *p,
    AstFunctionReturnType **items,
    size_t count,
    size_t *capacity,
    AstFunctionReturnType value,
    const Token *anchor_tok) {
    AstFunctionReturnType *new_items;
    size_t next_capacity;

    if (!items || !capacity) {
        return 0;
    }

    if (count == *capacity) {
        if (!parser_next_growth_capacity(*capacity,
                4,
                sizeof(AstFunctionReturnType),
                &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST function metadata");
            return 0;
        }
        new_items = (AstFunctionReturnType *)realloc(
            *items,
            next_capacity * sizeof(AstFunctionReturnType));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST function metadata");
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[count] = value;
    return 1;
}

static int append_int_array_item(Parser *p,
    int **items,
    size_t count,
    size_t *capacity,
    int value,
    const Token *anchor_tok) {
    int *new_items;
    size_t next_capacity;

    if (!items || !capacity) {
        return 0;
    }

    if (count == *capacity) {
        if (!parser_next_growth_capacity(*capacity, 4, sizeof(int), &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST declaration metadata");
            return 0;
        }
        new_items = (int *)realloc(*items, next_capacity * sizeof(int));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST declaration metadata");
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[count] = value;
    return 1;
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
        parser_ast_statement_free(items[i]);
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
        if (!parser_next_growth_capacity(*capacity,
            4,
            sizeof(AstStatement *),
            &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building statement AST");
            parser_ast_statement_free(child);
            return 0;
        }
        new_items = (AstStatement **)realloc(*items, next_capacity * sizeof(AstStatement *));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building statement AST");
            parser_ast_statement_free(child);
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
        parser_ast_expression_free(expr);
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
    memset(expr, 0, sizeof(*expr));
    expr->kind = kind;
    expr->line = line;
    expr->column = column;
    return expr;
}

static AstExpression *build_binary_expression_node(Parser *p,
    const Token *op_tok,
    AstExpression *left,
    AstExpression *right) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_BINARY, op_tok->line, op_tok->column);
    if (!expr) {
        set_error(p, op_tok, "Out of memory while building expression AST");
        parser_ast_expression_free(left);
        parser_ast_expression_free(right);
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
        parser_ast_expression_free(operand);
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
        parser_ast_expression_free(operand);
        return NULL;
    }

    expr->as.postfix.op = op_tok->type;
    expr->as.postfix.operand = operand;
    return expr;
}

static AstExpression *build_subscript_expression_node(Parser *p,
    const Token *lbracket_tok,
    AstExpression *base,
    AstExpression *index) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_SUBSCRIPT, lbracket_tok->line, lbracket_tok->column);
    if (!expr) {
        set_error(p, lbracket_tok, "Out of memory while building expression AST");
        parser_ast_expression_free(base);
        parser_ast_expression_free(index);
        return NULL;
    }

    expr->as.subscript.base = base;
    expr->as.subscript.index = index;
    return expr;
}

static AstExpression *build_member_expression_node(Parser *p,
    const Token *dot_tok,
    AstExpression *base,
    const Token *field_tok) {
    AstExpression *expr = alloc_ast_expression(AST_EXPR_MEMBER, dot_tok->line, dot_tok->column);

    if (!expr) {
        set_error(p, dot_tok, "Out of memory while building expression AST");
        parser_ast_expression_free(base);
        return NULL;
    }

    expr->as.member.base = base;
    expr->as.member.field_name = NULL;
    if (field_tok && field_tok->lexeme && field_tok->length > 0u) {
        expr->as.member.field_name = (char *)malloc(field_tok->length + 1u);
    }
    if (!expr->as.member.field_name) {
        set_error(p, field_tok, "Out of memory while building expression AST");
        parser_ast_expression_free(expr);
        return NULL;
    }
    memcpy(expr->as.member.field_name, field_tok->lexeme, field_tok->length);
    expr->as.member.field_name[field_tok->length] = '\0';
    expr->as.member.field_name_length = field_tok ? field_tok->length : 0u;
    return expr;
}

static void free_ast_expression_array(AstExpression **items, size_t count) {
    size_t i;

    if (!items) {
        return;
    }

    for (i = 0; i < count; ++i) {
        parser_ast_expression_free(items[i]);
    }

    free(items);
}

static void free_ast_expression_matrix(AstExpression ***items,
    const size_t *ranks,
    size_t count) {
    size_t i;

    if (!items) {
        return;
    }

    for (i = 0; i < count; ++i) {
        size_t j;
        AstExpression **row = items[i];
        size_t rank = ranks ? ranks[i] : 0u;

        if (!row) {
            continue;
        }
        for (j = 0; j < rank; ++j) {
            parser_ast_expression_free(row[j]);
        }
        free(row);
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
        if (!parser_next_growth_capacity(*capacity,
            4,
            sizeof(AstExpression *),
            &next_capacity)) {
            set_error(p, peek(p), "Out of memory while building expression AST");
            parser_ast_expression_free(arg);
            return 0;
        }
        new_items = (AstExpression **)realloc(*items, next_capacity * sizeof(AstExpression *));
        if (!new_items) {
            set_error(p, peek(p), "Out of memory while building expression AST");
            parser_ast_expression_free(arg);
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[*count] = arg;
    (*count)++;
    return 1;
}

static int append_ast_expression_slot(Parser *p,
    AstExpression ***items,
    size_t *count,
    size_t *capacity,
    AstExpression *expr,
    const Token *anchor_tok) {
    AstExpression **new_items;
    size_t next_capacity;

    if (!items || !count || !capacity) {
        parser_ast_expression_free(expr);
        return 0;
    }

    if (*count == *capacity) {
        if (!parser_next_growth_capacity(*capacity,
            4,
            sizeof(AstExpression *),
            &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building expression AST");
            parser_ast_expression_free(expr);
            return 0;
        }
        new_items = (AstExpression **)realloc(*items, next_capacity * sizeof(AstExpression *));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building expression AST");
            parser_ast_expression_free(expr);
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    (*items)[*count] = expr;
    (*count)++;
    return 1;
}

static int append_ast_expression_list_slot(Parser *p,
    AstExpression ****items,
    size_t index,
    size_t *capacity,
    AstExpression **list,
    const Token *anchor_tok) {
    AstExpression ***new_items;
    size_t next_capacity;

    if (!items || !capacity) {
        free(list);
        return 0;
    }

    if (index >= *capacity) {
        if (!parser_next_growth_capacity(*capacity,
                4,
                sizeof(AstExpression **),
                &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building expression AST");
            free(list);
            return 0;
        }
        new_items = (AstExpression ***)realloc(*items, next_capacity * sizeof(AstExpression **));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building expression AST");
            free(list);
            return 0;
        }
        while (*capacity < next_capacity) {
            new_items[*capacity] = NULL;
            (*capacity)++;
        }
        *items = new_items;
    }

    (*items)[index] = list;
    return 1;
}

static int append_int_list_slot(Parser *p,
    int ***items,
    size_t index,
    size_t *capacity,
    int *list,
    const Token *anchor_tok) {
    int **new_items;
    size_t next_capacity;

    if (!items || !capacity) {
        free(list);
        return 0;
    }

    if (index >= *capacity) {
        if (!parser_next_growth_capacity(*capacity, 4, sizeof(int *), &next_capacity)) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST function metadata");
            free(list);
            return 0;
        }
        new_items = (int **)realloc(*items, next_capacity * sizeof(int *));
        if (!new_items) {
            set_error(p,
                anchor_tok ? anchor_tok : peek(p),
                "Out of memory while building AST function metadata");
            free(list);
            return 0;
        }
        while (*capacity < next_capacity) {
            new_items[*capacity] = NULL;
            (*capacity)++;
        }
        *items = new_items;
    }

    (*items)[index] = list;
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
        parser_ast_expression_free(callee);
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

    if (expr->kind == AST_EXPR_SUBSCRIPT) {
        return 1;
    }

    if (expr->kind == AST_EXPR_MEMBER) {
        return 1;
    }

    return 0;
}

static int ast_expression_is_assignment_lvalue(const AstExpression *expr) {
    if (!expr) {
        return 0;
    }

    return expr->kind == AST_EXPR_IDENTIFIER ||
        expr->kind == AST_EXPR_SUBSCRIPT ||
        expr->kind == AST_EXPR_MEMBER;
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
        parser_ast_expression_free(condition);
        parser_ast_expression_free(then_expr);
        parser_ast_expression_free(else_expr);
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
        parser_ast_expression_free(inner);
        return NULL;
    }

    paren_expr = alloc_ast_expression(AST_EXPR_PAREN, lparen->line, lparen->column);
    if (!paren_expr) {
        set_error(p, lparen, "Out of memory while building expression AST");
        parser_ast_expression_free(inner);
        return NULL;
    }
    paren_expr->as.inner = inner;
    return paren_expr;
}

static AstExpression *try_parse_expression_ast_conversion(Parser *p) {
    size_t saved_current;
    const Token *type_tok;
    AstDeclarationValueKind target_type_kind;
    AstExpression *operand = NULL;
    AstExpression *expr = NULL;

    if (!check(p, TOKEN_KW_INT) && !check(p, TOKEN_KW_FLOAT)) {
        return NULL;
    }

    saved_current = p->current;
    advance(p);
    type_tok = previous(p);
    if (!match(p, TOKEN_LPAREN)) {
        p->current = saved_current;
        return NULL;
    }

    target_type_kind = type_tok->type == TOKEN_KW_FLOAT
        ? AST_DECLARATION_VALUE_FLOAT
        : AST_DECLARATION_VALUE_INT;

    operand = parse_expression_ast_comma(p);
    if (!operand) {
        p->current = saved_current;
        return NULL;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        parser_ast_expression_free(operand);
        p->current = saved_current;
        return NULL;
    }

    expr = alloc_ast_expression(AST_EXPR_CONVERSION, type_tok->line, type_tok->column);
    if (!expr) {
        set_error(p, type_tok, "Out of memory while building expression AST");
        parser_ast_expression_free(operand);
        return NULL;
    }
    expr->as.conversion.target_type_kind = target_type_kind;
    expr->as.conversion.operand = operand;
    return expr;
}

static AstExpression *parse_expression_ast_closure_literal(Parser *p, const Token *closure_tok) {
    AstExpression *expr = NULL;
    char **capture_names = NULL;
    size_t capture_count = 0u;
    size_t capture_capacity = 0u;
    AstFunctionReturnType return_type = AST_FUNCTION_RETURN_INT;
    char *return_type_name = NULL;
    size_t parameter_count = 0u;
    int has_unnamed_parameter = 0;
    char **parameter_names = NULL;
    int *parameter_value_kinds = NULL;
    char **parameter_type_names = NULL;
    AstStatement *body = NULL;
    const Token *capture_tok = NULL;

    if (!consume(p, TOKEN_LBRACKET, "'['")) {
        return NULL;
    }

    while (!check(p, TOKEN_RBRACKET)) {
        char *capture_name = NULL;

        if (!consume_token(p, TOKEN_IDENTIFIER, "capture name", &capture_tok)) {
            free_name_array(capture_names, capture_count);
            return NULL;
        }
        capture_name = copy_token_identifier_lexeme(capture_tok);
        if (!capture_name ||
            !append_name_array_item(p,
                &capture_names,
                &capture_count,
                &capture_capacity,
                capture_name,
                capture_tok)) {
            free_name_array(capture_names, capture_count);
            return NULL;
        }
        if (!match(p, TOKEN_COMMA)) {
            break;
        }
    }

    if (!consume(p, TOKEN_RBRACKET, "']'")) {
        free_name_array(capture_names, capture_count);
        return NULL;
    }

    if (match(p, TOKEN_KW_INT)) {
        return_type = AST_FUNCTION_RETURN_INT;
    } else if (match(p, TOKEN_KW_VOID)) {
        return_type = AST_FUNCTION_RETURN_VOID;
    } else {
        set_error(p, peek(p), "Expected closure return type 'int' or 'void'");
        free_name_array(capture_names, capture_count);
        return NULL;
    }

    if (!consume(p, TOKEN_LPAREN, "'('")) {
        free_name_array(capture_names, capture_count);
        return NULL;
    }
    if (!parse_parameter_list(p,
            &parameter_count,
            &has_unnamed_parameter,
            &parameter_names,
            &parameter_value_kinds,
            &parameter_type_names,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL)) {
        free_name_array(capture_names, capture_count);
        return NULL;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        free_name_array(capture_names, capture_count);
        free_name_array(parameter_names, parameter_count);
        free(parameter_value_kinds);
        free_name_array(parameter_type_names, parameter_count);
        return NULL;
    }
    if (has_unnamed_parameter) {
        set_error(p,
            closure_tok,
            "Closure parameters must have names in the first closure slice");
        free_name_array(capture_names, capture_count);
        free_name_array(parameter_names, parameter_count);
        free(parameter_value_kinds);
        free_name_array(parameter_type_names, parameter_count);
        return NULL;
    }

    if (!parse_compound_statement_with_local_control(p, NULL, &body)) {
        free_name_array(capture_names, capture_count);
        free_name_array(parameter_names, parameter_count);
        free(parameter_value_kinds);
        free_name_array(parameter_type_names, parameter_count);
        return NULL;
    }

    expr = alloc_ast_expression(AST_EXPR_CLOSURE, closure_tok->line, closure_tok->column);
    if (!expr) {
        set_error(p, closure_tok, "Out of memory while building closure expression AST");
        free_name_array(capture_names, capture_count);
        free_name_array(parameter_names, parameter_count);
        free(parameter_value_kinds);
        free_name_array(parameter_type_names, parameter_count);
        parser_ast_statement_free(body);
        free(return_type_name);
        return NULL;
    }

    expr->as.closure.capture_names = capture_names;
    expr->as.closure.capture_name_count = capture_count;
    expr->as.closure.return_type = return_type;
    expr->as.closure.return_type_name = return_type_name;
    expr->as.closure.parameter_count = parameter_count;
    expr->as.closure.parameter_names = parameter_names;
    expr->as.closure.parameter_value_kinds = parameter_value_kinds;
    expr->as.closure.parameter_type_names = parameter_type_names;
    expr->as.closure.body = body;
    return expr;
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
            parser_ast_expression_free(expr);
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

    if (match(p, TOKEN_FLOAT_NUMBER)) {
        tok = previous(p);
        expr = alloc_ast_expression(AST_EXPR_FLOAT_LITERAL, tok->line, tok->column);
        if (!expr) {
            set_error(p, tok, "Out of memory while building expression AST");
            leave_expression_recursion(p);
            return NULL;
        }
        expr->as.float_number_value = tok->float_number_value;
        leave_expression_recursion(p);
        return expr;
    }

    expr = try_parse_expression_ast_conversion(p);
    if (expr) {
        leave_expression_recursion(p);
        return expr;
    }

    if (match(p, TOKEN_KW_CLOSURE)) {
        tok = previous(p);
        expr = parse_expression_ast_closure_literal(p, tok);
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
            parser_ast_expression_free(expr);
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
                parser_ast_expression_free(expr);
                free_ast_expression_array(args, arg_count);
                return NULL;
            }

            if (!check(p, TOKEN_RPAREN)) {
                do {
                    AstExpression *arg = parse_expression_ast_assignment(p);
                    if (!arg) {
                        parser_ast_expression_free(expr);
                        free_ast_expression_array(args, arg_count);
                        return NULL;
                    }
                    if (!append_ast_expression_arg(p, &args, &arg_count, &arg_capacity, arg)) {
                        parser_ast_expression_free(expr);
                        free_ast_expression_array(args, arg_count);
                        return NULL;
                    }
                } while (match(p, TOKEN_COMMA));
            }

            if (!consume(p, TOKEN_RPAREN, "')'")) {
                parser_ast_expression_free(expr);
                free_ast_expression_array(args, arg_count);
                return NULL;
            }

            expr = build_call_expression_node(p, lparen_tok, expr, args, arg_count);
            if (!expr) {
                return NULL;
            }
            continue;
        }

        if (match(p, TOKEN_LBRACKET)) {
            const Token *lbracket_tok = previous(p);
            AstExpression *index_expr = parse_expression_ast_assignment(p);

            if (!index_expr) {
                parser_ast_expression_free(expr);
                return NULL;
            }
            if (!consume(p, TOKEN_RBRACKET, "']'")) {
                parser_ast_expression_free(expr);
                parser_ast_expression_free(index_expr);
                return NULL;
            }

            expr = build_subscript_expression_node(p, lbracket_tok, expr, index_expr);
            if (!expr) {
                return NULL;
            }
            continue;
        }

        if (match(p, TOKEN_DOT)) {
            const Token *dot_tok = previous(p);
            const Token *field_tok = NULL;

            if (!consume_token(p, TOKEN_IDENTIFIER, "field name", &field_tok)) {
                parser_ast_expression_free(expr);
                return NULL;
            }

            expr = build_member_expression_node(p, dot_tok, expr, field_tok);
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
                parser_ast_expression_free(expr);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
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
        parser_ast_expression_free(condition);
        leave_expression_recursion(p);
        return NULL;
    }

    if (!consume(p, TOKEN_COLON, "':'")) {
        parser_ast_expression_free(condition);
        parser_ast_expression_free(then_expr);
        leave_expression_recursion(p);
        return NULL;
    }

    else_expr = parse_expression_ast_conditional(p);
    if (!else_expr) {
        parser_ast_expression_free(condition);
        parser_ast_expression_free(then_expr);
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

    if (!ast_expression_is_assignment_lvalue(left) || !check_assignment_operator(p)) {
        leave_expression_recursion(p);
        return left;
    }

    (void)match_assignment_operator(p, &op_tok);
    right = parse_expression_ast_assignment(p);
    if (!right) {
        parser_ast_expression_free(left);
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
            parser_ast_expression_free(left);
            return NULL;
        }

        left = build_binary_expression_node(p, op_tok, left, right);
        if (!left) {
            return NULL;
        }
    }

    return left;
}

static int parse_closure_literal_syntax(Parser *p, const Token *closure_tok) {
    int has_unnamed_parameter = 0;

    if (!consume(p, TOKEN_LBRACKET, "'['")) {
        return 0;
    }
    while (!check(p, TOKEN_RBRACKET)) {
        if (!consume(p, TOKEN_IDENTIFIER, "capture name")) {
            return 0;
        }
        if (!match(p, TOKEN_COMMA)) {
            break;
        }
    }
    if (!consume(p, TOKEN_RBRACKET, "']'")) {
        return 0;
    }

    if (!match(p, TOKEN_KW_INT) && !match(p, TOKEN_KW_VOID)) {
        set_error(p, peek(p), "Expected closure return type 'int' or 'void'");
        return 0;
    }
    if (!consume(p, TOKEN_LPAREN, "'('")) {
        return 0;
    }
    if (!parse_parameter_list(p,
            NULL,
            &has_unnamed_parameter,
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
        return 0;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        return 0;
    }
    if (has_unnamed_parameter) {
        set_error(p,
            closure_tok ? closure_tok : previous(p),
            "Closure parameters must have names in the first closure slice");
        return 0;
    }

    return parse_compound_statement_with_local_control(p, NULL, NULL);
}

static int parse_primary_with_flags(Parser *p,
    int *out_is_identifier_lvalue,
    int *out_is_callable_target) {
    size_t expr_start_index;
    size_t saved_current;

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

    if (match(p, TOKEN_FLOAT_NUMBER)) {
        return 1;
    }

    if (match(p, TOKEN_KW_CLOSURE)) {
        if (!parse_closure_literal_syntax(p, previous(p))) {
            return 0;
        }
        return 1;
    }

    if (check(p, TOKEN_KW_INT) || check(p, TOKEN_KW_FLOAT)) {
        saved_current = p->current;
        advance(p);
        if (match(p, TOKEN_LPAREN)) {
            if (!parse_expression(p)) {
                p->current = saved_current;
                return 0;
            }
            if (!consume(p, TOKEN_RPAREN, "')'")) {
                p->current = saved_current;
                return 0;
            }
            return 1;
        }
        p->current = saved_current;
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
            /*
             * Any parenthesized expression can be a syntactic postfix call callee.
             * Semantic callable checks classify non-direct forms afterward.
             */
            *out_is_callable_target = 1;
        }
        return 1;
    }

    set_error(p, peek(p), "Expected expression, got %s", lexer_token_type_name(peek(p)->type));
    return 0;
}

static int parse_postfix(Parser *p) {
    int is_identifier_lvalue = 0;
    int is_callable_target = 0;

    if (!parse_primary_with_flags(p,
        &is_identifier_lvalue,
        &is_callable_target)) {
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

        if (match(p, TOKEN_LBRACKET)) {
            if (!parse_assignment(p)) {
                return 0;
            }
            if (!consume(p, TOKEN_RBRACKET, "']'")) {
                return 0;
            }
            is_identifier_lvalue = 1;
            is_callable_target = 0;
            continue;
        }

        if (match(p, TOKEN_DOT)) {
            if (!consume(p, TOKEN_IDENTIFIER, "field name")) {
                return 0;
            }
            is_identifier_lvalue = 1;
            is_callable_target = 0;
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
        if (ok && !token_slice_is_parenthesized_lvalue_form(p->tokens, operand_start, p->current)) {
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
    size_t lhs_end = 0u;
    const Token *op_tok = NULL;

    if (!enter_expression_recursion(p, "assignment-expression")) {
        return 0;
    }
    ok = parse_conditional(p);
    lhs_end = p->current;
    if (ok && check_assignment_operator(p)) {
        if (!token_slice_is_assignment_lvalue_form(p->tokens, start, lhs_end)) {
            leave_expression_recursion(p);
            return 1;
        }
        (void)match_assignment_operator(p, &op_tok);
        ok = parse_assignment(p);
    }
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

#endif
