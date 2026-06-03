#if defined(__INTELLISENSE__) && !defined(PARSER_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(PARSER_SPLIT_AGGREGATOR)
#define PARSER_SPLIT_INTELLISENSE_CONTEXT
#include "parser.c"
#else

static int parse_capture_list(Parser *p,
    char ***out_capture_names,
    size_t *out_capture_name_count,
    AstExpression ***out_capture_expressions,
    size_t *out_capture_expression_count);

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
            parser_ast_statement_free(stmt);
            return 0;
        }
        if (expr_ast && !append_ast_statement_expression(p, stmt, expr_ast, anchor_tok)) {
            parser_ast_statement_free(stmt);
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

static int parser_check_const_int_declaration_start(const Parser *p) {
    return p &&
        check(p, TOKEN_KW_CONST) &&
        p->current + 1u < p->tokens->size &&
        p->tokens->data[p->current + 1u].type == TOKEN_KW_INT;
}

static void parser_consume_optional_const_qualifier(Parser *p) {
    if (!p) {
        return;
    }
    (void)match(p, TOKEN_KW_CONST);
}

/*
 * Parser-side flow values are used only for local parsing constraints and
 * statement-shape bookkeeping. Semantic return-path authority is implemented
 * in semantic flow analysis, not in parser flow outputs.
 */

static void parser_local_control_set(ParserLocalControl *out_local_control,
    int may_fallthrough,
    int may_break) {
    if (!out_local_control) {
        return;
    }

    out_local_control->may_fallthrough = may_fallthrough;
    out_local_control->may_break = may_break;
}

static int parse_compound_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    int block_may_fallthrough = 1;
    int block_may_break = 0;
    AstStatement **children = NULL;
    size_t child_count = 0;
    size_t child_capacity = 0;
    const Token *lbrace_tok = NULL;

    if (out_statement) {
        *out_statement = NULL;
    }
    parser_local_control_set(out_local_control, 1, 0);

    if (!consume_token(p, TOKEN_LBRACE, "'{'", &lbrace_tok)) {
        return 0;
    }

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        if (check(p, TOKEN_KW_INT) || check(p, TOKEN_KW_VOID) || check(p, TOKEN_KW_FLOAT) || check(p, TOKEN_KW_PAIR) || check(p, TOKEN_KW_STRUCT) ||
            parser_check_const_int_declaration_start(p) ||
            (check(p, TOKEN_KW_CONST) &&
                p->current + 1u < p->tokens->size &&
                (p->tokens->data[p->current + 1u].type == TOKEN_KW_VOID ||
                    p->tokens->data[p->current + 1u].type == TOKEN_KW_FLOAT ||
                    p->tokens->data[p->current + 1u].type == TOKEN_KW_PAIR ||
                    p->tokens->data[p->current + 1u].type == TOKEN_KW_STRUCT))) {
            const Token *decl_tok = peek(p);
            int decl_is_const = 0;
            AstStatement *decl_stmt = NULL;
            char **decl_names = NULL;
            size_t decl_name_count = 0;
            int *decl_value_kinds = NULL;
            AstFunctionReturnType *decl_function_return_types = NULL;
            size_t *decl_function_parameter_counts = NULL;
            int **decl_function_parameter_value_kinds = NULL;
            AstFunctionReturnType **decl_function_parameter_return_types = NULL;
            size_t **decl_function_parameter_parameter_counts = NULL;
            int ***decl_function_parameter_parameter_value_kinds = NULL;
            AstFunctionReturnType ***decl_function_parameter_parameter_return_types = NULL;
            size_t ***decl_function_parameter_parameter_parameter_counts = NULL;
            char **decl_type_names = NULL;
            size_t *decl_array_ranks = NULL;
            AstExpression ***decl_array_extent_exprs = NULL;
            AstExpression **decl_init_exprs = NULL;
            size_t decl_init_expr_count = 0;

            if (!parse_declaration(p,
                NULL,
                &decl_is_const,
                out_statement ? &decl_names : NULL,
                out_statement ? &decl_name_count : NULL,
                out_statement ? &decl_value_kinds : NULL,
                out_statement ? &decl_function_return_types : NULL,
                out_statement ? &decl_function_parameter_counts : NULL,
                out_statement ? &decl_function_parameter_value_kinds : NULL,
                out_statement ? &decl_function_parameter_return_types : NULL,
                out_statement ? &decl_function_parameter_parameter_counts : NULL,
                out_statement ? &decl_function_parameter_parameter_value_kinds : NULL,
                out_statement ? &decl_function_parameter_parameter_return_types : NULL,
                out_statement ? &decl_function_parameter_parameter_parameter_counts : NULL,
                out_statement ? &decl_type_names : NULL,
                out_statement ? &decl_array_ranks : NULL,
                out_statement ? &decl_array_extent_exprs : NULL,
                out_statement ? &decl_init_exprs : NULL,
                out_statement ? &decl_init_expr_count : NULL)) {
                free_ast_statement_array(children, child_count);
                return 0;
            }
            if (out_statement) {
                decl_stmt = alloc_ast_statement(p, AST_STMT_DECLARATION, decl_tok);
                if (!decl_stmt) {
                    free_name_array(decl_names, decl_name_count);
                    free(decl_value_kinds);
                    free(decl_function_return_types);
                    free(decl_function_parameter_counts);
                    if (decl_function_parameter_value_kinds) {
                        size_t decl_index;
                        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
                            free(decl_function_parameter_value_kinds[decl_index]);
                        }
                    }
                    free(decl_function_parameter_value_kinds);
                    free_name_array(decl_type_names, decl_name_count);
                    free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                    free(decl_array_ranks);
                    free_ast_expression_array(decl_init_exprs, decl_init_expr_count);
                    free_ast_statement_array(children, child_count);
                    return 0;
                }
                decl_stmt->declaration_is_const = decl_is_const;
                decl_stmt->declaration_names = decl_names;
                decl_stmt->declaration_name_count = decl_name_count;
                decl_stmt->declaration_value_kinds = decl_value_kinds;
                decl_stmt->declaration_function_return_types = decl_function_return_types;
                decl_stmt->declaration_function_parameter_counts = decl_function_parameter_counts;
                decl_stmt->declaration_function_parameter_value_kinds =
                    decl_function_parameter_value_kinds;
                decl_stmt->declaration_function_parameter_return_types =
                    decl_function_parameter_return_types;
                decl_stmt->declaration_function_parameter_parameter_counts =
                    decl_function_parameter_parameter_counts;
                decl_stmt->declaration_function_parameter_parameter_value_kinds =
                    decl_function_parameter_parameter_value_kinds;
                decl_stmt->declaration_function_parameter_parameter_return_types =
                    decl_function_parameter_parameter_return_types;
                decl_stmt->declaration_function_parameter_parameter_parameter_counts =
                    decl_function_parameter_parameter_parameter_counts;
                decl_stmt->declaration_type_names = decl_type_names;
                decl_stmt->declaration_array_ranks = decl_array_ranks;
                decl_stmt->declaration_array_extent_exprs = decl_array_extent_exprs;
                decl_stmt->expressions = decl_init_exprs;
                decl_stmt->expression_count = decl_init_expr_count;
                if (decl_value_kinds &&
                    decl_name_count > 0u &&
                    decl_value_kinds[0] == AST_DECLARATION_VALUE_PAIR) {
                    decl_stmt->extension_origin = AST_EXTENSION_ORIGIN_PAIR;
                } else if (decl_value_kinds &&
                    decl_name_count > 0u &&
                    decl_value_kinds[0] == AST_DECLARATION_VALUE_STRUCT) {
                    decl_stmt->extension_origin = AST_EXTENSION_ORIGIN_STRUCT;
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
        } else {
            ParserLocalControl stmt_local_control;
            AstStatement *child_stmt = NULL;

            parser_local_control_set(&stmt_local_control, 1, 0);

            if (!parse_statement_with_local_control(p,
                &stmt_local_control,
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

            if (block_may_fallthrough && stmt_local_control.may_break) {
                block_may_break = 1;
            }

            if (block_may_fallthrough) {
                if (!stmt_local_control.may_fallthrough) {
                    block_may_fallthrough = 0;
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

    parser_local_control_set(out_local_control, block_may_fallthrough, block_may_break);
    return 1;
}

static int parse_if_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    ParserLocalControl then_local_control;
    ParserLocalControl else_local_control;
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
    parser_local_control_set(out_local_control, 1, 0);
    parser_local_control_set(&then_local_control, 1, 0);
    parser_local_control_set(&else_local_control, 1, 0);

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
        parser_ast_expression_free(cond_expr);
        return 0;
    }
    if (!parse_statement_with_local_control(p,
        &then_local_control,
        out_statement ? &then_stmt : NULL)) {
        parser_ast_expression_free(cond_expr);
        return 0;
    }
    if (match(p, TOKEN_KW_ELSE)) {
        has_else = 1;
        if (!parse_statement_with_local_control(p,
            &else_local_control,
            out_statement ? &else_stmt : NULL)) {
            parser_ast_statement_free(then_stmt);
            parser_ast_expression_free(cond_expr);
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
            parser_ast_statement_free(else_stmt);
            parser_ast_expression_free(cond_expr);
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
            parser_ast_expression_free(cond_expr);
            return 0;
        }

        if_stmt = alloc_ast_statement(p, AST_STMT_IF, if_tok);
        if (!if_stmt) {
            free_ast_statement_array(children, child_count);
            parser_ast_expression_free(cond_expr);
            return 0;
        }
        if (cond_expr && !append_ast_statement_expression(p, if_stmt, cond_expr, if_tok)) {
            parser_ast_statement_free(if_stmt);
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

    if (!has_else) {
        parser_local_control_set(out_local_control, 1, then_local_control.may_break);
    } else {
        parser_local_control_set(out_local_control,
            then_local_control.may_fallthrough ||
                else_local_control.may_fallthrough,
            then_local_control.may_break || else_local_control.may_break);
    }
    return 1;
}

static int parse_unless_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    ParserLocalControl then_local_control;
    const Token *unless_tok = previous(p);
    AstStatement *then_stmt = NULL;
    AstExpression *cond_expr = NULL;
    AstExpression *negated_cond_expr = NULL;
    AstStatement **children = NULL;
    size_t child_count = 0;
    size_t child_capacity = 0;

    if (out_statement) {
        *out_statement = NULL;
    }
    parser_local_control_set(out_local_control, 1, 0);
    parser_local_control_set(&then_local_control, 1, 0);

    if (!consume(p, TOKEN_LPAREN, "'('")) {
        return 0;
    }
    {
        size_t cond_start = p->current;
        size_t cond_end;
        Token bang_tok;

        if (!parse_expression(p)) {
            return 0;
        }
        cond_end = p->current;
        if (!build_expression_ast_from_slice(p,
            cond_start,
            cond_end,
            out_statement ? &cond_expr : NULL,
            unless_tok)) {
            return 0;
        }
        if (out_statement && cond_expr) {
            bang_tok.type = TOKEN_BANG;
            bang_tok.lexeme = "!";
            bang_tok.length = 1u;
            bang_tok.number_value = 0;
            bang_tok.line = unless_tok->line;
            bang_tok.column = unless_tok->column;
            negated_cond_expr = build_unary_expression_node(p, &bang_tok, cond_expr);
            if (!negated_cond_expr) {
                return 0;
            }
            cond_expr = NULL;
        }
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        parser_ast_expression_free(cond_expr);
        parser_ast_expression_free(negated_cond_expr);
        return 0;
    }
    if (!parse_statement_with_local_control(p,
        &then_local_control,
        out_statement ? &then_stmt : NULL)) {
        parser_ast_expression_free(cond_expr);
        parser_ast_expression_free(negated_cond_expr);
        return 0;
    }

    if (out_statement) {
        AstStatement *if_stmt;

        if (then_stmt &&
            !append_ast_statement_child(p,
                &children,
                &child_count,
                &child_capacity,
                then_stmt,
                unless_tok)) {
            free_ast_statement_array(children, child_count);
            parser_ast_expression_free(negated_cond_expr);
            return 0;
        }

        if_stmt = alloc_ast_statement(p, AST_STMT_IF, unless_tok);
        if (!if_stmt) {
            free_ast_statement_array(children, child_count);
            parser_ast_expression_free(negated_cond_expr);
            return 0;
        }
        if (negated_cond_expr &&
            !append_ast_statement_expression(p, if_stmt, negated_cond_expr, unless_tok)) {
            parser_ast_statement_free(if_stmt);
            free_ast_statement_array(children, child_count);
            return 0;
        }
        if (negated_cond_expr) {
            if_stmt->has_condition_expression = 1;
            if_stmt->condition_expression_index = if_stmt->expression_count - 1;
        }
        if_stmt->extension_origin = AST_EXTENSION_ORIGIN_UNLESS;
        if_stmt->children = children;
        if_stmt->child_count = child_count;
        *out_statement = if_stmt;
    }

    parser_local_control_set(out_local_control, 1, then_local_control.may_break);
    return 1;
}

static int parse_while_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    size_t cond_start;
    int cond_always_true;
    ParserLocalControl body_local_control;
    AstStatement *body_stmt = NULL;
    AstExpression *cond_expr = NULL;
    const Token *while_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }
    parser_local_control_set(out_local_control, 1, 0);
    parser_local_control_set(&body_local_control, 1, 0);

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
        parser_ast_expression_free(cond_expr);
        return 0;
    }

    p->loop_depth++;
    if (!parse_statement_with_local_control(p,
        &body_local_control,
        out_statement ? &body_stmt : NULL)) {
        p->loop_depth--;
        parser_ast_expression_free(cond_expr);
        return 0;
    }
    p->loop_depth--;

    if (out_statement) {
        AstStatement *while_stmt = alloc_ast_statement(p, AST_STMT_WHILE, while_tok);
        if (!while_stmt) {
            parser_ast_statement_free(body_stmt);
            parser_ast_expression_free(cond_expr);
            return 0;
        }
        while_stmt->children = (AstStatement **)malloc(sizeof(AstStatement *));
        if (!while_stmt->children) {
            set_error(p, while_tok, "Out of memory while building statement AST");
            parser_ast_statement_free(while_stmt);
            parser_ast_statement_free(body_stmt);
            parser_ast_expression_free(cond_expr);
            return 0;
        }
        if (cond_expr && !append_ast_statement_expression(p, while_stmt, cond_expr, while_tok)) {
            parser_ast_statement_free(while_stmt);
            parser_ast_statement_free(body_stmt);
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
        if (body_local_control.may_break) {
            parser_local_control_set(out_local_control, 1, 0);
        } else {
            parser_local_control_set(out_local_control, 0, 0);
        }
    } else {
        parser_local_control_set(out_local_control, 1, 0);
    }

    return 1;
}

static int parse_for_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    int cond_always_true = 0;
    size_t cond_start = 0;
    ParserLocalControl body_local_control;
    AstStatement *body_stmt = NULL;
    AstExpression *init_expr = NULL;
    AstExpression *cond_expr = NULL;
    AstExpression *step_expr = NULL;
    int for_init_decl_is_const = 0;
    AstExpression **for_init_decl_exprs = NULL;
    size_t for_init_decl_expr_count = 0;
    char **for_init_decl_names = NULL;
    size_t for_init_decl_name_count = 0;
    int *for_init_decl_value_kinds = NULL;
    AstFunctionReturnType *for_init_decl_function_return_types = NULL;
            size_t *for_init_decl_function_parameter_counts = NULL;
            int **for_init_decl_function_parameter_value_kinds = NULL;
            AstFunctionReturnType **for_init_decl_function_parameter_return_types = NULL;
            size_t **for_init_decl_function_parameter_parameter_counts = NULL;
            int ***for_init_decl_function_parameter_parameter_value_kinds = NULL;
            AstFunctionReturnType ***for_init_decl_function_parameter_parameter_return_types = NULL;
            size_t ***for_init_decl_function_parameter_parameter_parameter_counts = NULL;
            char **for_init_decl_type_names = NULL;
    size_t *for_init_decl_array_ranks = NULL;
    AstExpression ***for_init_decl_array_extent_exprs = NULL;
    const Token *for_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }
    parser_local_control_set(out_local_control, 1, 0);
    parser_local_control_set(&body_local_control, 1, 0);

    if (!consume(p, TOKEN_LPAREN, "'('") ) {
        return 0;
    }

    if (check(p, TOKEN_KW_INT) || check(p, TOKEN_KW_VOID) || check(p, TOKEN_KW_PAIR) || check(p, TOKEN_KW_STRUCT) ||
        parser_check_const_int_declaration_start(p) ||
        (check(p, TOKEN_KW_CONST) &&
            p->current + 1u < p->tokens->size &&
            (p->tokens->data[p->current + 1u].type == TOKEN_KW_VOID ||
                p->tokens->data[p->current + 1u].type == TOKEN_KW_PAIR ||
                p->tokens->data[p->current + 1u].type == TOKEN_KW_STRUCT))) {
        if (!parse_declaration(p,
            NULL,
            out_statement ? &for_init_decl_is_const : NULL,
            out_statement ? &for_init_decl_names : NULL,
            out_statement ? &for_init_decl_name_count : NULL,
            out_statement ? &for_init_decl_value_kinds : NULL,
            out_statement ? &for_init_decl_function_return_types : NULL,
            out_statement ? &for_init_decl_function_parameter_counts : NULL,
            out_statement ? &for_init_decl_function_parameter_value_kinds : NULL,
            out_statement ? &for_init_decl_function_parameter_return_types : NULL,
            out_statement ? &for_init_decl_function_parameter_parameter_counts : NULL,
            out_statement ? &for_init_decl_function_parameter_parameter_value_kinds : NULL,
            out_statement ? &for_init_decl_function_parameter_parameter_return_types : NULL,
            out_statement ? &for_init_decl_function_parameter_parameter_parameter_counts : NULL,
            out_statement ? &for_init_decl_type_names : NULL,
            out_statement ? &for_init_decl_array_ranks : NULL,
            out_statement ? &for_init_decl_array_extent_exprs : NULL,
            out_statement ? &for_init_decl_exprs : NULL,
            out_statement ? &for_init_decl_expr_count : NULL)) {
            return 0;
        }
    } else {
        size_t init_start = p->current;
        size_t init_end = p->current;

        if (!check(p, TOKEN_SEMICOLON)) {
            if (!parse_expression(p)) {
                free_name_array(for_init_decl_names, for_init_decl_name_count);
                free_ast_expression_matrix(
                    for_init_decl_array_extent_exprs,
                    for_init_decl_array_ranks,
                    for_init_decl_name_count);
                free(for_init_decl_value_kinds);
                free(for_init_decl_function_return_types);
                free(for_init_decl_function_parameter_counts);
                free_name_array(for_init_decl_type_names, for_init_decl_name_count);
                free(for_init_decl_array_ranks);
                return 0;
            }
            init_end = p->current;
            if (!build_expression_ast_from_slice(p,
                init_start,
                init_end,
                out_statement ? &init_expr : NULL,
                for_tok)) {
                free_name_array(for_init_decl_names, for_init_decl_name_count);
                free_ast_expression_matrix(
                    for_init_decl_array_extent_exprs,
                    for_init_decl_array_ranks,
                    for_init_decl_name_count);
                free(for_init_decl_value_kinds);
                free(for_init_decl_function_return_types);
                free(for_init_decl_function_parameter_counts);
                free_name_array(for_init_decl_type_names, for_init_decl_name_count);
                free(for_init_decl_array_ranks);
                return 0;
            }
        }
        if (!consume(p, TOKEN_SEMICOLON, "';'")) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free(for_init_decl_function_return_types);
            free(for_init_decl_function_parameter_counts);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            return 0;
        }
    }

    if (!check(p, TOKEN_SEMICOLON)) {
        cond_start = p->current;
        if (!parse_expression(p)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free(for_init_decl_function_return_types);
            free(for_init_decl_function_parameter_counts);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            return 0;
        }
        if (!build_expression_ast_from_slice(p,
            cond_start,
            p->current,
            out_statement ? &cond_expr : NULL,
            for_tok)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free(for_init_decl_function_return_types);
            free(for_init_decl_function_parameter_counts);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            return 0;
        }
        cond_always_true = expression_slice_is_constant_true(p->tokens, cond_start, p->current);
    } else {
        cond_always_true = 1;
    }
    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        free_name_array(for_init_decl_names, for_init_decl_name_count);
        free_ast_expression_matrix(
            for_init_decl_array_extent_exprs,
            for_init_decl_array_ranks,
            for_init_decl_name_count);
        free(for_init_decl_value_kinds);
        free(for_init_decl_function_return_types);
        free(for_init_decl_function_parameter_counts);
        free_name_array(for_init_decl_type_names, for_init_decl_name_count);
        free(for_init_decl_array_ranks);
        free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
        parser_ast_expression_free(init_expr);
        parser_ast_expression_free(cond_expr);
        return 0;
    }

    if (!check(p, TOKEN_RPAREN)) {
        size_t step_start = p->current;
        size_t step_end;

        if (!parse_expression(p)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free(for_init_decl_function_return_types);
            free(for_init_decl_function_parameter_counts);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            parser_ast_expression_free(cond_expr);
            return 0;
        }
        step_end = p->current;
        if (!build_expression_ast_from_slice(p,
            step_start,
            step_end,
            out_statement ? &step_expr : NULL,
            for_tok)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            parser_ast_expression_free(cond_expr);
            return 0;
        }
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        free_name_array(for_init_decl_names, for_init_decl_name_count);
        free_ast_expression_matrix(
            for_init_decl_array_extent_exprs,
            for_init_decl_array_ranks,
            for_init_decl_name_count);
        free(for_init_decl_value_kinds);
        free(for_init_decl_function_return_types);
        free(for_init_decl_function_parameter_counts);
        free_name_array(for_init_decl_type_names, for_init_decl_name_count);
        free(for_init_decl_array_ranks);
        free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
        parser_ast_expression_free(init_expr);
        parser_ast_expression_free(cond_expr);
        parser_ast_expression_free(step_expr);
        return 0;
    }

    p->loop_depth++;
    if (!parse_statement_with_local_control(p,
        &body_local_control,
        out_statement ? &body_stmt : NULL)) {
        p->loop_depth--;
        free_name_array(for_init_decl_names, for_init_decl_name_count);
        free_ast_expression_matrix(
            for_init_decl_array_extent_exprs,
            for_init_decl_array_ranks,
            for_init_decl_name_count);
        free(for_init_decl_value_kinds);
        free(for_init_decl_function_return_types);
        free(for_init_decl_function_parameter_counts);
        free_name_array(for_init_decl_type_names, for_init_decl_name_count);
        free(for_init_decl_array_ranks);
        free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
        parser_ast_expression_free(init_expr);
        parser_ast_expression_free(cond_expr);
        parser_ast_expression_free(step_expr);
        return 0;
    }
    p->loop_depth--;

    if (out_statement) {
        AstStatement *for_stmt = alloc_ast_statement(p, AST_STMT_FOR, for_tok);
        if (!for_stmt) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_statement_free(body_stmt);
            parser_ast_expression_free(init_expr);
            parser_ast_expression_free(cond_expr);
            parser_ast_expression_free(step_expr);
            return 0;
        }
        for_stmt->children = (AstStatement **)malloc(sizeof(AstStatement *));
        if (!for_stmt->children) {
            set_error(p, for_tok, "Out of memory while building statement AST");
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free(for_init_decl_function_return_types);
            free(for_init_decl_function_parameter_counts);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_statement_free(for_stmt);
            parser_ast_statement_free(body_stmt);
            parser_ast_expression_free(init_expr);
            parser_ast_expression_free(cond_expr);
            parser_ast_expression_free(step_expr);
            return 0;
        }
        for_stmt->expressions = for_init_decl_exprs;
        for_stmt->expression_count = for_init_decl_expr_count;
        if (for_init_decl_expr_count > 0) {
            for_stmt->has_for_init_expression = 1;
            for_stmt->for_init_expression_index = 0;
            for_init_decl_exprs = NULL;
            for_init_decl_expr_count = 0;
        }
        if (init_expr && !append_ast_statement_expression(p, for_stmt, init_expr, for_tok)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            parser_ast_statement_free(for_stmt);
            parser_ast_statement_free(body_stmt);
            parser_ast_expression_free(cond_expr);
            parser_ast_expression_free(step_expr);
            return 0;
        }
        if (init_expr) {
            for_stmt->has_for_init_expression = 1;
            for_stmt->for_init_expression_index = for_stmt->expression_count - 1;
        }
        if (cond_expr && !append_ast_statement_expression(p, for_stmt, cond_expr, for_tok)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_matrix(
                for_init_decl_array_extent_exprs,
                for_init_decl_array_ranks,
                for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            parser_ast_statement_free(for_stmt);
            parser_ast_statement_free(body_stmt);
            parser_ast_expression_free(step_expr);
            return 0;
        }
        if (cond_expr) {
            for_stmt->has_condition_expression = 1;
            for_stmt->condition_expression_index = for_stmt->expression_count - 1;
        }
        if (step_expr && !append_ast_statement_expression(p, for_stmt, step_expr, for_tok)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free(for_init_decl_value_kinds);
            free(for_init_decl_function_return_types);
            free(for_init_decl_function_parameter_counts);
            free_name_array(for_init_decl_type_names, for_init_decl_name_count);
            free(for_init_decl_array_ranks);
            parser_ast_statement_free(for_stmt);
            parser_ast_statement_free(body_stmt);
            return 0;
        }
        if (step_expr) {
            for_stmt->has_for_step_expression = 1;
            for_stmt->for_step_expression_index = for_stmt->expression_count - 1;
        }
        for_stmt->declaration_is_const = for_init_decl_is_const;
        for_stmt->declaration_names = for_init_decl_names;
        for_stmt->declaration_name_count = for_init_decl_name_count;
        for_stmt->declaration_value_kinds = for_init_decl_value_kinds;
        for_stmt->declaration_function_return_types = for_init_decl_function_return_types;
        for_stmt->declaration_function_parameter_counts = for_init_decl_function_parameter_counts;
        for_stmt->declaration_function_parameter_value_kinds =
            for_init_decl_function_parameter_value_kinds;
        for_stmt->declaration_function_parameter_return_types =
            for_init_decl_function_parameter_return_types;
        for_stmt->declaration_function_parameter_parameter_counts =
            for_init_decl_function_parameter_parameter_counts;
        for_stmt->declaration_function_parameter_parameter_value_kinds =
            for_init_decl_function_parameter_parameter_value_kinds;
        for_stmt->declaration_function_parameter_parameter_return_types =
            for_init_decl_function_parameter_parameter_return_types;
        for_stmt->declaration_function_parameter_parameter_parameter_counts =
            for_init_decl_function_parameter_parameter_parameter_counts;
        for_stmt->declaration_type_names = for_init_decl_type_names;
        for_stmt->declaration_array_ranks = for_init_decl_array_ranks;
        for_stmt->declaration_array_extent_exprs = for_init_decl_array_extent_exprs;
        if (for_init_decl_value_kinds &&
            for_init_decl_name_count > 0u &&
            for_init_decl_value_kinds[0] == AST_DECLARATION_VALUE_PAIR) {
            for_stmt->extension_origin = AST_EXTENSION_ORIGIN_PAIR;
        } else if (for_init_decl_value_kinds &&
            for_init_decl_name_count > 0u &&
            for_init_decl_value_kinds[0] == AST_DECLARATION_VALUE_STRUCT) {
            for_stmt->extension_origin = AST_EXTENSION_ORIGIN_STRUCT;
        }
        for_stmt->children[0] = body_stmt;
        for_stmt->child_count = 1;
        *out_statement = for_stmt;
    } else {
        free_name_array(for_init_decl_names, for_init_decl_name_count);
        free_ast_expression_matrix(
            for_init_decl_array_extent_exprs,
            for_init_decl_array_ranks,
            for_init_decl_name_count);
        free(for_init_decl_value_kinds);
        free(for_init_decl_function_return_types);
        free(for_init_decl_function_parameter_counts);
        free_name_array(for_init_decl_type_names, for_init_decl_name_count);
        free(for_init_decl_array_ranks);
        free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
    }

    if (cond_always_true) {
        if (body_local_control.may_break) {
            parser_local_control_set(out_local_control, 1, 0);
        } else {
            parser_local_control_set(out_local_control, 0, 0);
        }
    } else {
        parser_local_control_set(out_local_control, 1, 0);
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
            parser_ast_statement_free(stmt);
            return 0;
        }
        if (expr_ast && !append_ast_statement_expression(p, stmt, expr_ast, return_tok)) {
            parser_ast_statement_free(stmt);
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

static size_t count_return_statements_in_ast(const AstStatement *stmt) {
    size_t total = 0;
    size_t i;

    if (!stmt) {
        return 0;
    }

    if (stmt->kind == AST_STMT_RETURN) {
        total = 1;
    }

    for (i = 0; i < stmt->child_count; ++i) {
        total += count_return_statements_in_ast(stmt->children[i]);
    }

    return total;
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

static int parse_defer_statement(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    const Token *defer_tok = previous(p);
    AstStatement *payload_stmt = NULL;
    AstStatement *defer_stmt = NULL;
    ParserLocalControl payload_control;

    parser_local_control_set(out_local_control, 1, 0);
    if (out_statement) {
        *out_statement = NULL;
    }

    if (!parse_statement_with_local_control(p, &payload_control, out_statement ? &payload_stmt : NULL)) {
        return 0;
    }

    if (out_statement) {
        defer_stmt = alloc_ast_statement(p, AST_STMT_DEFER, defer_tok);
        if (!defer_stmt) {
            ast_statement_free(payload_stmt);
            return 0;
        }
        defer_stmt->children = (AstStatement **)calloc(1u, sizeof(AstStatement *));
        if (!defer_stmt->children) {
            ast_statement_free(defer_stmt);
            ast_statement_free(payload_stmt);
            set_error(p, defer_tok, "out of memory allocating defer statement");
            return 0;
        }
        defer_stmt->children[0] = payload_stmt;
        defer_stmt->child_count = 1u;
        defer_stmt->extension_origin = AST_EXTENSION_ORIGIN_DEFER;
        *out_statement = defer_stmt;
    }

    /*
     * Registering a defer does not itself change local control flow. The
     * deferred payload will run later when lowering rewrites scope exits.
     */
    return 1;
}

static int parse_capdefer_statement(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    const Token *capdefer_tok = previous(p);
    AstStatement *payload_stmt = NULL;
    AstStatement *capdefer_stmt = NULL;
    ParserLocalControl payload_control;

    parser_local_control_set(out_local_control, 1, 0);
    if (out_statement) {
        *out_statement = NULL;
    }

    if (out_statement) {
        capdefer_stmt = alloc_ast_statement(p, AST_STMT_CAPDEFER, capdefer_tok);
        if (!capdefer_stmt) {
            return 0;
        }
    }

    if (!parse_capture_list(p,
            out_statement ? &capdefer_stmt->capture_names : NULL,
            out_statement ? &capdefer_stmt->capture_name_count : NULL,
            out_statement ? &capdefer_stmt->capture_expressions : NULL,
            out_statement ? &capdefer_stmt->capture_expression_count : NULL)) {
        ast_statement_free(capdefer_stmt);
        return 0;
    }
    if (!parse_statement_with_local_control(p, &payload_control, out_statement ? &payload_stmt : NULL)) {
        ast_statement_free(capdefer_stmt);
        return 0;
    }

    if (out_statement) {
        capdefer_stmt->children = (AstStatement **)calloc(1u, sizeof(AstStatement *));
        if (!capdefer_stmt->children) {
            ast_statement_free(capdefer_stmt);
            ast_statement_free(payload_stmt);
            set_error(p, capdefer_tok, "out of memory allocating capture defer statement");
            return 0;
        }
        capdefer_stmt->children[0] = payload_stmt;
        capdefer_stmt->child_count = 1u;
        capdefer_stmt->extension_origin = AST_EXTENSION_ORIGIN_CAPDEFER;
        *out_statement = capdefer_stmt;
    }

    return 1;
}

static int parse_capture_list(Parser *p,
    char ***out_capture_names,
    size_t *out_capture_name_count,
    AstExpression ***out_capture_expressions,
    size_t *out_capture_expression_count) {
    char **capture_names = NULL;
    AstExpression **capture_exprs = NULL;
    size_t capture_count = 0u;
    size_t capture_capacity = 0u;

    if (out_capture_names) {
        *out_capture_names = NULL;
    }
    if (out_capture_name_count) {
        *out_capture_name_count = 0u;
    }
    if (out_capture_expressions) {
        *out_capture_expressions = NULL;
    }
    if (out_capture_expression_count) {
        *out_capture_expression_count = 0u;
    }

    if (!consume(p, TOKEN_LPAREN, "'('")) {
        return 0;
    }
    if (check(p, TOKEN_RPAREN)) {
        advance(p);
        return 1;
    }

    for (;;) {
        const Token *name_tok;
        AstExpression *expr = NULL;
        char *name_copy = NULL;
        size_t name_length;
        char **new_names;
        AstExpression **new_exprs;
        size_t next_capacity;

        if (!consume_token(p, TOKEN_IDENTIFIER, "capture name", &name_tok)) {
            goto fail;
        }
        if (!consume(p, TOKEN_ASSIGN, "'='")) {
            goto fail;
        }
        if (!parse_initializer_expression(p, &expr, name_tok)) {
            goto fail;
        }

        if (capture_count == capture_capacity) {
            if (!parser_next_growth_capacity(capture_capacity, 4u, sizeof(char *), &next_capacity)) {
                set_error(p, name_tok, "out of memory growing capture list");
                goto fail;
            }
            new_names = (char **)realloc(capture_names, next_capacity * sizeof(char *));
            if (!new_names) {
                set_error(p, name_tok, "out of memory growing capture list");
                goto fail;
            }
            new_exprs = (AstExpression **)realloc(capture_exprs, next_capacity * sizeof(AstExpression *));
            if (!new_exprs) {
                set_error(p, name_tok, "out of memory growing capture list");
                goto fail;
            }
            capture_names = new_names;
            capture_exprs = new_exprs;
            capture_capacity = next_capacity;
        }

        name_length = name_tok->length;
        name_copy = (char *)malloc(name_length + 1u);
        if (!name_copy) {
            set_error(p, name_tok, "out of memory copying capture name");
            goto fail;
        }
        memcpy(name_copy, name_tok->lexeme, name_length);
        name_copy[name_length] = '\0';
        capture_names[capture_count] = name_copy;
        capture_exprs[capture_count] = expr;
        capture_count++;

        if (match(p, TOKEN_COMMA)) {
            continue;
        }
        break;
    }

    if (!consume(p, TOKEN_RPAREN, "')'")) {
        goto fail;
    }

    if (out_capture_names) {
        *out_capture_names = capture_names;
    } else {
        free_name_array(capture_names, capture_count);
    }
    if (out_capture_name_count) {
        *out_capture_name_count = capture_count;
    }
    if (out_capture_expressions) {
        *out_capture_expressions = capture_exprs;
    } else {
        free_ast_expression_array(capture_exprs, capture_count);
    }
    if (out_capture_expression_count) {
        *out_capture_expression_count = capture_count;
    }
    return 1;

fail:
    free_name_array(capture_names, capture_count);
    free_ast_expression_array(capture_exprs, capture_count);
    return 0;
}

static int parse_fndefer_statement(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    const Token *defer_tok = previous(p);
    AstStatement *payload_stmt = NULL;
    AstStatement *defer_stmt = NULL;
    ParserLocalControl payload_control;

    parser_local_control_set(out_local_control, 1, 0);
    if (out_statement) {
        *out_statement = NULL;
    }

    if (!parse_statement_with_local_control(p, &payload_control, out_statement ? &payload_stmt : NULL)) {
        return 0;
    }

    if (out_statement) {
        defer_stmt = alloc_ast_statement(p, AST_STMT_FNDEFER, defer_tok);
        if (!defer_stmt) {
            ast_statement_free(payload_stmt);
            return 0;
        }
        defer_stmt->children = (AstStatement **)calloc(1u, sizeof(AstStatement *));
        if (!defer_stmt->children) {
            ast_statement_free(defer_stmt);
            ast_statement_free(payload_stmt);
            set_error(p, defer_tok, "out of memory allocating function-exit defer statement");
            return 0;
        }
        defer_stmt->children[0] = payload_stmt;
        defer_stmt->child_count = 1u;
        defer_stmt->extension_origin = AST_EXTENSION_ORIGIN_FNDEFER;
        *out_statement = defer_stmt;
    }

    return 1;
}

static int parse_unless_statement(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    const Token *unless_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }

    if (!parse_unless_statement_with_local_control(p, out_local_control, out_statement)) {
        return 0;
    }

    if (out_statement && *out_statement) {
        (*out_statement)->line = unless_tok->line;
        (*out_statement)->column = unless_tok->column;
    }

    return 1;
}

static int parse_statement_with_local_control(Parser *p,
    ParserLocalControl *out_local_control,
    AstStatement **out_statement) {
    int ok;
    ParserLocalControl local_control;

    parser_local_control_set(&local_control, 1, 0);
    parser_local_control_set(out_local_control, 1, 0);
    if (out_statement) {
        *out_statement = NULL;
    }

    if (!enter_statement_recursion(p, "statement")) {
        return 0;
    }

    if (match(p, TOKEN_LBRACE)) {
        AstStatement *stmt = NULL;

        p->current--;
        ok = parse_compound_statement_with_local_control(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control,
                local_control.may_fallthrough,
                local_control.may_break);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_IF)) {
        AstStatement *stmt = NULL;

        ok = parse_if_statement_with_local_control(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control,
                local_control.may_fallthrough,
                local_control.may_break);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_UNLESS)) {
        AstStatement *stmt = NULL;

        ok = parse_unless_statement_with_local_control(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control,
                local_control.may_fallthrough,
                local_control.may_break);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_WHILE)) {
        AstStatement *stmt = NULL;

        ok = parse_while_statement_with_local_control(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control,
                local_control.may_fallthrough,
                local_control.may_break);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_FOR)) {
        AstStatement *stmt = NULL;

        ok = parse_for_statement_with_local_control(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control,
                local_control.may_fallthrough,
                local_control.may_break);
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
        if (ok) {
            parser_local_control_set(out_local_control, 0, 0);
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
        if (ok) {
            parser_local_control_set(out_local_control, 0, 1);
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
        if (ok) {
            parser_local_control_set(out_local_control, 0, 0);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_DEFER)) {
        AstStatement *stmt = NULL;

        ok = parse_defer_statement(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control, 1, 0);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_FNDEFER)) {
        AstStatement *stmt = NULL;

        ok = parse_fndefer_statement(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control, 1, 0);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_CAPDEFER)) {
        AstStatement *stmt = NULL;

        ok = parse_capdefer_statement(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control, 1, 0);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }
    if (match(p, TOKEN_KW_UNLESS)) {
        AstStatement *stmt = NULL;

        ok = parse_unless_statement(p,
            &local_control,
            out_statement ? &stmt : NULL);
        leave_statement_recursion(p);
        if (ok) {
            parser_local_control_set(out_local_control,
                local_control.may_fallthrough,
                local_control.may_break);
        }
        if (ok && out_statement) {
            *out_statement = stmt;
        }
        return ok;
    }

    ok = parse_expression_statement(p, out_statement);
    leave_statement_recursion(p);
    if (ok) {
        parser_local_control_set(out_local_control, 1, 0);
    }
    return ok;
}

static int parse_init_declarator(Parser *p,
    const Token **out_ident,
    size_t *out_array_rank,
    AstExpression ***out_array_extent_exprs,
    AstFunctionReturnType base_function_return_type,
    AstFunctionReturnType *out_function_return_type,
    int *out_is_function_declarator,
    int **out_function_parameter_value_kinds,
    size_t *out_function_parameter_count,
    AstFunctionReturnType **out_function_parameter_return_types,
    size_t **out_function_parameter_parameter_counts,
    int ***out_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ***out_function_parameter_parameter_return_types,
    size_t ***out_function_parameter_parameter_parameter_counts,
    int *out_has_initializer,
    AstExpression **out_initializer_expr) {
    AstExpression **array_extent_exprs = NULL;
    size_t array_rank = 0u;
    size_t array_extent_capacity = 0u;
    AstFunctionReturnType function_return_type = base_function_return_type;
    int *function_parameter_value_kinds = NULL;
    size_t function_parameter_count = 0u;
    AstFunctionReturnType *function_parameter_return_types = NULL;
    size_t *function_parameter_parameter_counts = NULL;
    int **function_parameter_parameter_value_kinds = NULL;
    AstFunctionReturnType **function_parameter_parameter_return_types = NULL;
    size_t **function_parameter_parameter_parameter_counts = NULL;

    if (out_array_rank) {
        *out_array_rank = 0u;
    }
    if (out_array_extent_exprs) {
        *out_array_extent_exprs = NULL;
    }
    if (out_function_return_type) {
        *out_function_return_type = base_function_return_type;
    }
    if (out_is_function_declarator) {
        *out_is_function_declarator = 0;
    }
    if (out_function_parameter_value_kinds) {
        *out_function_parameter_value_kinds = NULL;
    }
    if (out_function_parameter_count) {
        *out_function_parameter_count = 0u;
    }
    if (out_function_parameter_return_types) {
        *out_function_parameter_return_types = NULL;
    }
    if (out_function_parameter_parameter_counts) {
        *out_function_parameter_parameter_counts = NULL;
    }
    if (out_function_parameter_parameter_value_kinds) {
        *out_function_parameter_parameter_value_kinds = NULL;
    }
    if (out_function_parameter_parameter_return_types) {
        *out_function_parameter_parameter_return_types = NULL;
    }
    if (out_function_parameter_parameter_parameter_counts) {
        *out_function_parameter_parameter_parameter_counts = NULL;
    }
    if (out_has_initializer) {
        *out_has_initializer = 0;
    }
    if (out_initializer_expr) {
        *out_initializer_expr = NULL;
    }

    if (!consume_token(p, TOKEN_IDENTIFIER, "identifier", out_ident)) {
        return 0;
    }
    if (match(p, TOKEN_LPAREN)) {
        size_t nested_param_count = 0u;
        if (out_is_function_declarator) {
            *out_is_function_declarator = 1;
        }

        if (!parse_parameter_list(p,
                0u,
                &nested_param_count,
                NULL,
                NULL,
                out_function_parameter_value_kinds ? &function_parameter_value_kinds : NULL,
                NULL,
                NULL,
                NULL,
                out_function_parameter_return_types ? &function_parameter_return_types : NULL,
                out_function_parameter_parameter_counts ? &function_parameter_parameter_counts : NULL,
                out_function_parameter_parameter_value_kinds
                    ? &function_parameter_parameter_value_kinds
                    : NULL,
                out_function_parameter_parameter_return_types
                    ? &function_parameter_parameter_return_types
                    : NULL,
                out_function_parameter_parameter_parameter_counts
                    ? &function_parameter_parameter_parameter_counts
                    : NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL)) {
            free(function_parameter_value_kinds);
            free(function_parameter_return_types);
            free(function_parameter_parameter_counts);
            free(function_parameter_parameter_value_kinds);
            free(function_parameter_parameter_return_types);
            free(function_parameter_parameter_parameter_counts);
            return 0;
        }
        if (!consume(p, TOKEN_RPAREN, "')'")) {
            free(function_parameter_value_kinds);
            free(function_parameter_return_types);
            free(function_parameter_parameter_counts);
            free(function_parameter_parameter_value_kinds);
            free(function_parameter_parameter_return_types);
            free(function_parameter_parameter_parameter_counts);
            return 0;
        }
        function_parameter_count = nested_param_count;
    }
    while (match(p, TOKEN_LBRACKET)) {
        size_t extent_start = p->current;
        AstExpression *extent_expr = NULL;
        if (!check(p, TOKEN_RBRACKET)) {
            if (!parse_expression(p)) {
                free_ast_expression_array(array_extent_exprs, array_rank);
                free(function_parameter_value_kinds);
                return 0;
            }
            if (!build_expression_ast_from_slice(p,
                    extent_start,
                    p->current,
                    &extent_expr,
                    out_ident ? *out_ident : NULL)) {
                free_ast_expression_array(array_extent_exprs, array_rank);
                free(function_parameter_value_kinds);
                return 0;
            }
        }
        if (!consume(p, TOKEN_RBRACKET, "']'")) {
            parser_ast_expression_free(extent_expr);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free(function_parameter_value_kinds);
            return 0;
        }
        if (!append_ast_expression_slot(p,
                &array_extent_exprs,
                &array_rank,
                &array_extent_capacity,
                extent_expr,
                out_ident ? *out_ident : NULL)) {
            free_ast_expression_array(array_extent_exprs, array_rank);
            free(function_parameter_value_kinds);
            return 0;
        }
    }
    if (out_array_rank) {
        *out_array_rank = array_rank;
    }
    if (out_array_extent_exprs) {
        *out_array_extent_exprs = array_extent_exprs;
    } else {
        free_ast_expression_array(array_extent_exprs, array_rank);
    }
    if (out_function_return_type) {
        *out_function_return_type = function_return_type;
    }
    if (out_function_parameter_value_kinds) {
        *out_function_parameter_value_kinds = function_parameter_value_kinds;
    } else {
        free(function_parameter_value_kinds);
    }
    if (out_function_parameter_count) {
        *out_function_parameter_count = function_parameter_count;
    }
    if (out_function_parameter_return_types) {
        *out_function_parameter_return_types = function_parameter_return_types;
    } else {
        free(function_parameter_return_types);
    }
    if (out_function_parameter_parameter_counts) {
        *out_function_parameter_parameter_counts = function_parameter_parameter_counts;
    } else {
        free(function_parameter_parameter_counts);
    }
    if (out_function_parameter_parameter_value_kinds) {
        *out_function_parameter_parameter_value_kinds = function_parameter_parameter_value_kinds;
    } else {
        if (function_parameter_parameter_value_kinds) {
            size_t i;
            for (i = 0u; i < function_parameter_count; ++i) {
                free(function_parameter_parameter_value_kinds[i]);
            }
        }
        free(function_parameter_parameter_value_kinds);
    }
    if (out_function_parameter_parameter_return_types) {
        *out_function_parameter_parameter_return_types = function_parameter_parameter_return_types;
    } else if (function_parameter_parameter_return_types) {
        size_t i;
        for (i = 0u; i < function_parameter_count; ++i) {
            free(function_parameter_parameter_return_types[i]);
        }
        free(function_parameter_parameter_return_types);
    }
    if (out_function_parameter_parameter_parameter_counts) {
        *out_function_parameter_parameter_parameter_counts =
            function_parameter_parameter_parameter_counts;
    } else if (function_parameter_parameter_parameter_counts) {
        size_t i;
        for (i = 0u; i < function_parameter_count; ++i) {
            free(function_parameter_parameter_parameter_counts[i]);
        }
        free(function_parameter_parameter_parameter_counts);
    }
    if (match(p, TOKEN_ASSIGN)) {
        AstExpression *init_expr = NULL;

        if (out_has_initializer) {
            *out_has_initializer = 1;
        }
        if (!parse_initializer_expression(p, &init_expr, out_ident ? *out_ident : NULL)) {
            if (out_array_extent_exprs) {
                *out_array_extent_exprs = NULL;
            }
            free_ast_expression_array(array_extent_exprs, array_rank);
            if (out_function_parameter_value_kinds) {
                *out_function_parameter_value_kinds = NULL;
            }
            free(function_parameter_value_kinds);
            return 0;
        }
        if (out_initializer_expr) {
            *out_initializer_expr = init_expr;
        } else {
            parser_ast_expression_free(init_expr);
        }
    }
    return 1;
}

static int parse_declaration(Parser *p,
    AstProgram *top_level_program,
    int *out_declaration_is_const,
    char ***out_decl_names,
    size_t *out_decl_name_count,
    int **out_decl_value_kinds,
    AstFunctionReturnType **out_decl_function_return_types,
    size_t **out_decl_function_parameter_counts,
    int ***out_decl_function_parameter_value_kinds,
    AstFunctionReturnType ***out_decl_function_parameter_return_types,
    size_t ***out_decl_function_parameter_parameter_counts,
    int ****out_decl_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ****out_decl_function_parameter_parameter_return_types,
    size_t ****out_decl_function_parameter_parameter_parameter_counts,
    char ***out_decl_type_names,
    size_t **out_decl_array_ranks,
    AstExpression ****out_decl_array_extent_exprs,
    AstExpression ***out_decl_initializer_exprs,
    size_t *out_decl_initializer_expr_count) {
    const Token *ident = NULL;
    const Token *struct_type_tok = NULL;
    AstExpression **array_extent_exprs = NULL;
    size_t array_rank = 0u;
    int declaration_is_const = 0;
    int has_initializer = 0;
    AstExpression *external_initializer_expr = NULL;
    char **decl_names = NULL;
    size_t decl_name_count = 0;
    size_t decl_name_capacity = 0;
    int *decl_value_kinds = NULL;
    size_t decl_value_kind_capacity = 0;
    AstFunctionReturnType *decl_function_return_types = NULL;
    size_t *decl_function_parameter_counts = NULL;
    int **decl_function_parameter_value_kinds = NULL;
    AstFunctionReturnType **decl_function_parameter_return_types = NULL;
    size_t **decl_function_parameter_parameter_counts = NULL;
    int ***decl_function_parameter_parameter_value_kinds = NULL;
    AstFunctionReturnType ***decl_function_parameter_parameter_return_types = NULL;
    size_t ***decl_function_parameter_parameter_parameter_counts = NULL;
    size_t decl_function_return_type_capacity = 0;
    size_t decl_function_parameter_count_capacity = 0;
    size_t decl_function_parameter_value_kind_capacity = 0;
    char **decl_type_names = NULL;
    size_t decl_type_name_capacity = 0;
    size_t *decl_array_ranks = NULL;
    size_t decl_array_rank_capacity = 0;
    AstExpression ***decl_array_extent_exprs = NULL;
    size_t decl_array_extent_capacity = 0;
    AstExpression **decl_initializer_exprs = NULL;
    size_t decl_initializer_expr_count = 0;
    size_t decl_initializer_expr_capacity = 0;
    int declaration_value_kind = AST_DECLARATION_VALUE_INT;
    int declaration_is_function_declarator = 0;
    AstFunctionReturnType declaration_function_return_type = AST_FUNCTION_RETURN_INT;
    int *declaration_function_parameter_value_kinds = NULL;
    size_t declaration_function_parameter_count = 0u;
    AstFunctionReturnType *declaration_function_parameter_return_types = NULL;
    size_t *declaration_function_parameter_parameter_counts = NULL;
    int **declaration_function_parameter_parameter_value_kinds = NULL;
    AstFunctionReturnType **declaration_function_parameter_parameter_return_types = NULL;
    size_t **declaration_function_parameter_parameter_parameter_counts = NULL;
    char *declaration_type_name = NULL;

    if (out_decl_names) {
        *out_decl_names = NULL;
    }
    if (out_decl_name_count) {
        *out_decl_name_count = 0;
    }
    if (out_decl_value_kinds) {
        *out_decl_value_kinds = NULL;
    }
    if (out_decl_function_return_types) {
        *out_decl_function_return_types = NULL;
    }
    if (out_decl_function_parameter_counts) {
        *out_decl_function_parameter_counts = NULL;
    }
    if (out_decl_function_parameter_value_kinds) {
        *out_decl_function_parameter_value_kinds = NULL;
    }
    if (out_decl_function_parameter_return_types) {
        *out_decl_function_parameter_return_types = NULL;
    }
    if (out_decl_function_parameter_parameter_counts) {
        *out_decl_function_parameter_parameter_counts = NULL;
    }
    if (out_decl_function_parameter_parameter_value_kinds) {
        *out_decl_function_parameter_parameter_value_kinds = NULL;
    }
    if (out_decl_function_parameter_parameter_return_types) {
        *out_decl_function_parameter_parameter_return_types = NULL;
    }
    if (out_decl_function_parameter_parameter_parameter_counts) {
        *out_decl_function_parameter_parameter_parameter_counts = NULL;
    }
    if (out_decl_type_names) {
        *out_decl_type_names = NULL;
    }
    if (out_decl_array_ranks) {
        *out_decl_array_ranks = NULL;
    }
    if (out_decl_array_extent_exprs) {
        *out_decl_array_extent_exprs = NULL;
    }
    if (out_declaration_is_const) {
        *out_declaration_is_const = 0;
    }
    if (out_decl_initializer_exprs) {
        *out_decl_initializer_exprs = NULL;
    }
    if (out_decl_initializer_expr_count) {
        *out_decl_initializer_expr_count = 0;
    }

    if (match(p, TOKEN_KW_CONST)) {
        declaration_is_const = 1;
    }
    if (match(p, TOKEN_KW_INT)) {
        declaration_value_kind = AST_DECLARATION_VALUE_INT;
        declaration_function_return_type = AST_FUNCTION_RETURN_INT;
    } else if (match(p, TOKEN_KW_VOID)) {
        declaration_value_kind = AST_DECLARATION_VALUE_INT;
        declaration_function_return_type = AST_FUNCTION_RETURN_VOID;
    } else if (match(p, TOKEN_KW_FLOAT)) {
        declaration_value_kind = AST_DECLARATION_VALUE_FLOAT;
        declaration_function_return_type = AST_FUNCTION_RETURN_FLOAT;
    } else if (match(p, TOKEN_KW_PAIR)) {
        declaration_value_kind = AST_DECLARATION_VALUE_PAIR;
        declaration_function_return_type = AST_FUNCTION_RETURN_PAIR;
    } else if (match(p, TOKEN_KW_STRUCT)) {
        declaration_value_kind = AST_DECLARATION_VALUE_STRUCT;
        declaration_function_return_type = AST_FUNCTION_RETURN_STRUCT;
        if (!consume_token(p, TOKEN_IDENTIFIER, "struct type name", &struct_type_tok)) {
            return 0;
        }
        declaration_type_name = copy_token_identifier_lexeme(struct_type_tok);
        if (!declaration_type_name) {
            set_error(p,
                struct_type_tok ? struct_type_tok : peek(p),
                "Out of memory while building AST identifier metadata");
            return 0;
        }
    } else {
        if (!consume(p, TOKEN_KW_INT, "'int', 'void', 'float', 'pair', or 'struct <name>'")) {
            return 0;
        }
        declaration_value_kind = AST_DECLARATION_VALUE_INT;
    }
    if (top_level_program &&
        (declaration_value_kind == AST_DECLARATION_VALUE_PAIR ||
            declaration_value_kind == AST_DECLARATION_VALUE_STRUCT)) {
        set_error(p,
            ident ? ident : previous(p),
            declaration_value_kind == AST_DECLARATION_VALUE_PAIR
                ? "'pair' top-level declarations are not supported in this extension slice"
                : "'struct' object top-level declarations are not supported in this extension slice");
        free(declaration_type_name);
        return 0;
    }
        if (!parse_init_declarator(p,
            &ident,
            &array_rank,
            &array_extent_exprs,
            declaration_function_return_type,
        &declaration_function_return_type,
        &declaration_is_function_declarator,
        &declaration_function_parameter_value_kinds,
        &declaration_function_parameter_count,
            &declaration_function_parameter_return_types,
            &declaration_function_parameter_parameter_counts,
            &declaration_function_parameter_parameter_value_kinds,
            &declaration_function_parameter_parameter_return_types,
            &declaration_function_parameter_parameter_parameter_counts,
            &has_initializer,
            &external_initializer_expr)) {
        free_name_array(decl_names, decl_name_count);
        free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
        free(decl_array_ranks);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }
    if (declaration_function_return_type == AST_FUNCTION_RETURN_VOID &&
        !declaration_is_function_declarator) {
        set_error(p,
            ident ? ident : peek(p),
            "Local 'void' declarations must use a function declarator in the current extension slice");
        free_name_array(decl_names, decl_name_count);
        free(declaration_function_parameter_value_kinds);
        free(declaration_function_parameter_return_types);
        free(declaration_function_parameter_parameter_counts);
        free(declaration_function_parameter_parameter_value_kinds);
        free(declaration_function_parameter_parameter_return_types);
        free(declaration_function_parameter_parameter_parameter_counts);
        free_ast_expression_array(array_extent_exprs, array_rank);
        free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
        free(decl_array_ranks);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }
    if (out_decl_initializer_exprs) {
        if (!append_ast_expression_slot(p,
            &decl_initializer_exprs,
            &decl_initializer_expr_count,
            &decl_initializer_expr_capacity,
            external_initializer_expr,
            ident)) {
            free_name_array(decl_names, decl_name_count);
            free(declaration_type_name);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        external_initializer_expr = NULL;
    }
    if (out_decl_names) {
        char *name_copy = copy_token_identifier_lexeme(ident);
        if (!name_copy || !append_name_array_item(p,
            &decl_names,
            &decl_name_count,
            &decl_name_capacity,
            name_copy,
            ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (!append_int_array_item(p,
                &decl_value_kinds,
                decl_name_count - 1u,
                &decl_value_kind_capacity,
                declaration_is_function_declarator
                    ? AST_DECLARATION_VALUE_FUNCTION
                    : declaration_value_kind,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free(decl_value_kinds);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (!append_function_return_type_array_item(p,
                &decl_function_return_types,
                decl_name_count - 1u,
                &decl_function_return_type_capacity,
                declaration_is_function_declarator
                    ? declaration_function_return_type
                    : AST_FUNCTION_RETURN_INT,
                ident) ||
            !append_size_t_array_item(p,
                &decl_function_parameter_counts,
                decl_name_count - 1u,
                &decl_function_parameter_count_capacity,
                declaration_function_parameter_count,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free(decl_value_kinds);
            free(decl_function_return_types);
                free(decl_function_parameter_counts);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
        }
        if (!append_int_list_slot(p,
                &decl_function_parameter_value_kinds,
                decl_name_count - 1u,
                &decl_function_parameter_value_kind_capacity,
                declaration_function_parameter_value_kinds,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free(decl_value_kinds);
            free(decl_function_return_types);
            free(decl_function_parameter_counts);
            free(decl_function_parameter_value_kinds);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        declaration_function_parameter_value_kinds = NULL;
        if (!append_pointer_list_slot(p,
                (void ***)&decl_function_parameter_return_types,
                decl_name_count - 1u,
                &decl_function_parameter_value_kind_capacity,
                (void *)declaration_function_parameter_return_types,
                ident) ||
            !append_pointer_list_slot(p,
                (void ***)&decl_function_parameter_parameter_counts,
                decl_name_count - 1u,
                &decl_function_parameter_value_kind_capacity,
                (void *)declaration_function_parameter_parameter_counts,
                ident) ||
            !append_pointer_list_slot(p,
                (void ***)&decl_function_parameter_parameter_value_kinds,
                decl_name_count - 1u,
                &decl_function_parameter_value_kind_capacity,
                (void *)declaration_function_parameter_parameter_value_kinds,
                ident) ||
            !append_pointer_list_slot(p,
                (void ***)&decl_function_parameter_parameter_return_types,
                decl_name_count - 1u,
                &decl_function_parameter_value_kind_capacity,
                (void *)declaration_function_parameter_parameter_return_types,
                ident) ||
            !append_pointer_list_slot(p,
                (void ***)&decl_function_parameter_parameter_parameter_counts,
                decl_name_count - 1u,
                &decl_function_parameter_value_kind_capacity,
                (void *)declaration_function_parameter_parameter_parameter_counts,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free(decl_value_kinds);
            free(decl_function_return_types);
            free(decl_function_parameter_counts);
            free(decl_function_parameter_value_kinds);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        declaration_function_parameter_return_types = NULL;
        declaration_function_parameter_parameter_counts = NULL;
        declaration_function_parameter_parameter_value_kinds = NULL;
        declaration_function_parameter_parameter_return_types = NULL;
        declaration_function_parameter_parameter_parameter_counts = NULL;
        if (!append_nullable_name_array_item(p,
                &decl_type_names,
                decl_name_count - 1u,
                &decl_type_name_capacity,
                declaration_type_name,
                struct_type_tok ? struct_type_tok : ident)) {
            free_name_array(decl_names, decl_name_count);
            free(decl_value_kinds);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        declaration_type_name = NULL;
        if (!append_size_t_array_item(p,
                &decl_array_ranks,
                decl_name_count - 1u,
                &decl_array_rank_capacity,
                array_rank,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free(declaration_type_name);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (!append_ast_expression_list_slot(p,
                &decl_array_extent_exprs,
                decl_name_count - 1u,
                &decl_array_extent_capacity,
                array_extent_exprs,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        array_extent_exprs = NULL;
    }
    if (top_level_program && !parser_ast_program_add_external(top_level_program, AST_EXTERNAL_DECLARATION, ident)) {
        set_error(p, peek(p), "Out of memory while building AST");
        parser_ast_expression_free(external_initializer_expr);
        free_name_array(decl_names, decl_name_count);
        free_ast_expression_array(array_extent_exprs, array_rank);
        free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
        free(decl_array_ranks);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }
    if (top_level_program) {
        top_level_program->externals[top_level_program->count - 1].declaration_array_rank = array_rank;
        top_level_program->externals[top_level_program->count - 1].declaration_array_extent_exprs =
            array_extent_exprs;
        top_level_program->externals[top_level_program->count - 1].declaration_value_kind =
            declaration_value_kind;
        top_level_program->externals[top_level_program->count - 1].declaration_type_name =
            declaration_type_name;
        declaration_type_name = NULL;
        top_level_program->externals[top_level_program->count - 1].is_const_qualified = declaration_is_const;
        top_level_program->externals[top_level_program->count - 1].has_initializer = has_initializer;
        top_level_program->externals[top_level_program->count - 1].declaration_initializer =
            external_initializer_expr;
        external_initializer_expr = NULL;
        array_extent_exprs = NULL;
    }

    while (match(p, TOKEN_COMMA)) {
        if (!parse_init_declarator(p,
            &ident,
            &array_rank,
            &array_extent_exprs,
            declaration_function_return_type,
        &declaration_function_return_type,
        &declaration_is_function_declarator,
        &declaration_function_parameter_value_kinds,
        &declaration_function_parameter_count,
            &declaration_function_parameter_return_types,
            &declaration_function_parameter_parameter_counts,
            &declaration_function_parameter_parameter_value_kinds,
            &declaration_function_parameter_parameter_return_types,
            &declaration_function_parameter_parameter_parameter_counts,
            &has_initializer,
            &external_initializer_expr)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (declaration_function_return_type == AST_FUNCTION_RETURN_VOID &&
            !declaration_is_function_declarator) {
            set_error(p,
                ident ? ident : peek(p),
                "Local 'void' declarations must use a function declarator in the current extension slice");
            free_name_array(decl_names, decl_name_count);
            free(declaration_function_parameter_value_kinds);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (out_decl_initializer_exprs) {
            if (!append_ast_expression_slot(p,
                &decl_initializer_exprs,
                &decl_initializer_expr_count,
                &decl_initializer_expr_capacity,
                external_initializer_expr,
                ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            external_initializer_expr = NULL;
        }
        if (out_decl_names) {
            char *name_copy = copy_token_identifier_lexeme(ident);
        if (!name_copy || !append_name_array_item(p,
            &decl_names,
            &decl_name_count,
            &decl_name_capacity,
            name_copy,
                ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            if (!append_int_array_item(p,
                    &decl_value_kinds,
                    decl_name_count - 1u,
                    &decl_value_kind_capacity,
                    declaration_is_function_declarator
                        ? AST_DECLARATION_VALUE_FUNCTION
                        : declaration_value_kind,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free(decl_value_kinds);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            if (!append_function_return_type_array_item(p,
                    &decl_function_return_types,
                    decl_name_count - 1u,
                    &decl_function_return_type_capacity,
                    declaration_is_function_declarator
                        ? declaration_function_return_type
                        : AST_FUNCTION_RETURN_INT,
                    ident) ||
                !append_size_t_array_item(p,
                    &decl_function_parameter_counts,
                    decl_name_count - 1u,
                    &decl_function_parameter_count_capacity,
                    declaration_function_parameter_count,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free(decl_value_kinds);
                free(decl_function_return_types);
                free(decl_function_parameter_counts);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            if (!append_int_list_slot(p,
                    &decl_function_parameter_value_kinds,
                    decl_name_count - 1u,
                    &decl_function_parameter_value_kind_capacity,
                    declaration_function_parameter_value_kinds,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free(decl_value_kinds);
                free(decl_function_return_types);
                free(decl_function_parameter_counts);
                free(decl_function_parameter_value_kinds);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            declaration_function_parameter_value_kinds = NULL;
            if (!append_pointer_list_slot(p,
                    (void ***)&decl_function_parameter_return_types,
                    decl_name_count - 1u,
                    &decl_function_parameter_value_kind_capacity,
                    (void *)declaration_function_parameter_return_types,
                    ident) ||
                !append_pointer_list_slot(p,
                    (void ***)&decl_function_parameter_parameter_counts,
                    decl_name_count - 1u,
                    &decl_function_parameter_value_kind_capacity,
                    (void *)declaration_function_parameter_parameter_counts,
                    ident) ||
                !append_pointer_list_slot(p,
                    (void ***)&decl_function_parameter_parameter_value_kinds,
                    decl_name_count - 1u,
                    &decl_function_parameter_value_kind_capacity,
                    (void *)declaration_function_parameter_parameter_value_kinds,
                    ident) ||
                !append_pointer_list_slot(p,
                    (void ***)&decl_function_parameter_parameter_return_types,
                    decl_name_count - 1u,
                    &decl_function_parameter_value_kind_capacity,
                    (void *)declaration_function_parameter_parameter_return_types,
                    ident) ||
                !append_pointer_list_slot(p,
                    (void ***)&decl_function_parameter_parameter_parameter_counts,
                    decl_name_count - 1u,
                    &decl_function_parameter_value_kind_capacity,
                    (void *)declaration_function_parameter_parameter_parameter_counts,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free(decl_value_kinds);
                free(decl_function_return_types);
                free(decl_function_parameter_counts);
                free(decl_function_parameter_value_kinds);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            declaration_function_parameter_return_types = NULL;
            declaration_function_parameter_parameter_counts = NULL;
            declaration_function_parameter_parameter_value_kinds = NULL;
            declaration_function_parameter_parameter_return_types = NULL;
            declaration_function_parameter_parameter_parameter_counts = NULL;
            if (!append_nullable_name_array_item(p,
                    &decl_type_names,
                    decl_name_count - 1u,
                    &decl_type_name_capacity,
                    declaration_type_name
                        ? declaration_type_name
                        : (declaration_value_kind == AST_DECLARATION_VALUE_STRUCT && struct_type_tok
                                ? copy_token_identifier_lexeme(struct_type_tok)
                                : NULL),
                    struct_type_tok ? struct_type_tok : ident)) {
                free_name_array(decl_names, decl_name_count);
                free(decl_value_kinds);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            declaration_type_name = NULL;
            if (!append_size_t_array_item(p,
                    &decl_array_ranks,
                    decl_name_count - 1u,
                    &decl_array_rank_capacity,
                    array_rank,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(array_extent_exprs, array_rank);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            if (!append_ast_expression_list_slot(p,
                    &decl_array_extent_exprs,
                    decl_name_count - 1u,
                    &decl_array_extent_capacity,
                    array_extent_exprs,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
                free(decl_array_ranks);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            array_extent_exprs = NULL;
        }
        if (top_level_program &&
            !parser_ast_program_add_external(top_level_program, AST_EXTERNAL_DECLARATION, ident)) {
            set_error(p, peek(p), "Out of memory while building AST");
            parser_ast_expression_free(external_initializer_expr);
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(array_extent_exprs, array_rank);
            free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
            free(decl_array_ranks);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (top_level_program) {
            top_level_program->externals[top_level_program->count - 1].declaration_array_rank = array_rank;
            top_level_program->externals[top_level_program->count - 1].declaration_array_extent_exprs =
                array_extent_exprs;
            top_level_program->externals[top_level_program->count - 1].declaration_value_kind =
                declaration_value_kind;
            top_level_program->externals[top_level_program->count - 1].declaration_type_name =
                declaration_type_name;
            declaration_type_name = NULL;
            top_level_program->externals[top_level_program->count - 1].is_const_qualified = declaration_is_const;
            top_level_program->externals[top_level_program->count - 1].has_initializer = has_initializer;
            top_level_program->externals[top_level_program->count - 1].declaration_initializer =
                external_initializer_expr;
            external_initializer_expr = NULL;
            array_extent_exprs = NULL;
        }
    }

    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        free_name_array(decl_names, decl_name_count);
        free_ast_expression_array(array_extent_exprs, array_rank);
        free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
        free(decl_array_ranks);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }

    if (out_decl_names) {
        *out_decl_names = decl_names;
        if (out_decl_name_count) {
            *out_decl_name_count = decl_name_count;
        }
    } else {
        free_name_array(decl_names, decl_name_count);
    }
    if (out_decl_value_kinds) {
        *out_decl_value_kinds = decl_value_kinds;
    } else {
        free(decl_value_kinds);
    }
    if (out_decl_function_return_types) {
        *out_decl_function_return_types = decl_function_return_types;
    } else {
        free(decl_function_return_types);
    }
    if (out_decl_function_parameter_counts) {
        *out_decl_function_parameter_counts = decl_function_parameter_counts;
    } else {
        free(decl_function_parameter_counts);
    }
    if (out_decl_function_parameter_value_kinds) {
        *out_decl_function_parameter_value_kinds = decl_function_parameter_value_kinds;
    } else if (decl_function_parameter_value_kinds) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
            free(decl_function_parameter_value_kinds[decl_index]);
        }
        free(decl_function_parameter_value_kinds);
    }
    if (out_decl_function_parameter_return_types) {
        *out_decl_function_parameter_return_types = decl_function_parameter_return_types;
    } else if (decl_function_parameter_return_types) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
            free(decl_function_parameter_return_types[decl_index]);
        }
        free(decl_function_parameter_return_types);
    }
    if (out_decl_function_parameter_parameter_counts) {
        *out_decl_function_parameter_parameter_counts = decl_function_parameter_parameter_counts;
    } else if (decl_function_parameter_parameter_counts) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
            free(decl_function_parameter_parameter_counts[decl_index]);
        }
        free(decl_function_parameter_parameter_counts);
    }
    if (out_decl_function_parameter_parameter_value_kinds) {
        *out_decl_function_parameter_parameter_value_kinds =
            decl_function_parameter_parameter_value_kinds;
    } else if (decl_function_parameter_parameter_value_kinds) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
            size_t param_index;
            int **nested_lists = decl_function_parameter_parameter_value_kinds[decl_index];
            size_t param_count =
                decl_function_parameter_counts ? decl_function_parameter_counts[decl_index] : 0u;
            if (!nested_lists) {
                continue;
            }
            for (param_index = 0u; param_index < param_count; ++param_index) {
                free(nested_lists[param_index]);
            }
            free(nested_lists);
        }
        free(decl_function_parameter_parameter_value_kinds);
    }
    if (out_decl_function_parameter_parameter_return_types) {
        *out_decl_function_parameter_parameter_return_types =
            decl_function_parameter_parameter_return_types;
    } else if (decl_function_parameter_parameter_return_types) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
            size_t param_index;
            AstFunctionReturnType **nested_lists = decl_function_parameter_parameter_return_types[decl_index];
            size_t param_count =
                decl_function_parameter_counts ? decl_function_parameter_counts[decl_index] : 0u;
            if (!nested_lists) {
                continue;
            }
            for (param_index = 0u; param_index < param_count; ++param_index) {
                free(nested_lists[param_index]);
            }
            free(nested_lists);
        }
        free(decl_function_parameter_parameter_return_types);
    }
    if (out_decl_function_parameter_parameter_parameter_counts) {
        *out_decl_function_parameter_parameter_parameter_counts =
            decl_function_parameter_parameter_parameter_counts;
    } else if (decl_function_parameter_parameter_parameter_counts) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < decl_name_count; ++decl_index) {
            size_t param_index;
            size_t **nested_lists = decl_function_parameter_parameter_parameter_counts[decl_index];
            size_t param_count =
                decl_function_parameter_counts ? decl_function_parameter_counts[decl_index] : 0u;
            if (!nested_lists) {
                continue;
            }
            for (param_index = 0u; param_index < param_count; ++param_index) {
                free(nested_lists[param_index]);
            }
            free(nested_lists);
        }
        free(decl_function_parameter_parameter_parameter_counts);
    }
    if (out_decl_type_names) {
        *out_decl_type_names = decl_type_names;
    } else {
        free_name_array(decl_type_names, decl_name_count);
    }
    if (out_decl_array_extent_exprs) {
        *out_decl_array_extent_exprs = decl_array_extent_exprs;
    } else {
        free_ast_expression_matrix(decl_array_extent_exprs, decl_array_ranks, decl_name_count);
    }
    if (out_decl_array_ranks) {
        *out_decl_array_ranks = decl_array_ranks;
    } else {
        free(decl_array_ranks);
    }

    if (out_decl_initializer_exprs) {
        *out_decl_initializer_exprs = decl_initializer_exprs;
        if (out_decl_initializer_expr_count) {
            *out_decl_initializer_expr_count = decl_initializer_expr_count;
        }
    } else {
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
    }
    if (out_declaration_is_const) {
        *out_declaration_is_const = declaration_is_const;
    }

    return 1;
}

static int parse_parameter_list(Parser *p,
    size_t function_type_depth,
    size_t *out_param_count,
    int *out_has_unnamed_parameter,
    char ***out_parameter_names,
    int **out_parameter_value_kinds,
    char ***out_parameter_type_names,
    size_t **out_parameter_array_ranks,
    AstExpression ****out_parameter_array_extent_exprs,
    AstFunctionReturnType **out_parameter_function_return_types,
    size_t **out_parameter_function_parameter_counts,
    int ***out_parameter_function_parameter_value_kinds,
    AstFunctionReturnType ***out_parameter_function_parameter_return_types,
    size_t ***out_parameter_function_parameter_parameter_counts,
    int ****out_parameter_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ****out_parameter_function_parameter_parameter_return_types,
    size_t ****out_parameter_function_parameter_parameter_parameter_counts,
    int **out_parameter_is_const,
    int **out_parameter_name_lines,
    int **out_parameter_name_columns) {
    size_t count = 0;
    size_t name_capacity = 0;
    size_t array_rank_capacity = 0;
    size_t type_name_capacity = 0;
    size_t *parameter_array_ranks = NULL;
    AstExpression ***parameter_array_extent_exprs = NULL;
    size_t parameter_array_extent_capacity = 0;
    int *parameter_value_kinds = NULL;
    char **parameter_type_names = NULL;
    AstFunctionReturnType *parameter_function_return_types = NULL;
    size_t *parameter_function_parameter_counts = NULL;
    int **parameter_function_parameter_value_kinds = NULL;
    AstFunctionReturnType **parameter_function_parameter_return_types = NULL;
    size_t **parameter_function_parameter_parameter_counts = NULL;
    int ***parameter_function_parameter_parameter_value_kinds = NULL;
    AstFunctionReturnType ***parameter_function_parameter_parameter_return_types = NULL;
    size_t ***parameter_function_parameter_parameter_parameter_counts = NULL;
    int *parameter_is_const = NULL;
    int capture_parameter_metadata =
        (out_parameter_names != NULL) ||
        (out_parameter_value_kinds != NULL) ||
        (out_parameter_type_names != NULL) ||
        (out_parameter_array_ranks != NULL) ||
        (out_parameter_function_return_types != NULL) ||
        (out_parameter_function_parameter_counts != NULL) ||
        (out_parameter_function_parameter_value_kinds != NULL) ||
        (out_parameter_function_parameter_return_types != NULL) ||
        (out_parameter_function_parameter_parameter_counts != NULL) ||
        (out_parameter_is_const != NULL) ||
        (out_parameter_name_lines != NULL) ||
        (out_parameter_name_columns != NULL);
    char **parameter_names = NULL;
    int *parameter_name_lines = NULL;
    int *parameter_name_columns = NULL;

    if (out_param_count) {
        *out_param_count = 0;
    }
    if (out_has_unnamed_parameter) {
        *out_has_unnamed_parameter = 0;
    }
    if (out_parameter_names) {
        *out_parameter_names = NULL;
    }
    if (out_parameter_value_kinds) {
        *out_parameter_value_kinds = NULL;
    }
    if (out_parameter_type_names) {
        *out_parameter_type_names = NULL;
    }
    if (out_parameter_array_ranks) {
        *out_parameter_array_ranks = NULL;
    }
    if (out_parameter_array_extent_exprs) {
        *out_parameter_array_extent_exprs = NULL;
    }
    if (out_parameter_function_return_types) {
        *out_parameter_function_return_types = NULL;
    }
    if (out_parameter_function_parameter_counts) {
        *out_parameter_function_parameter_counts = NULL;
    }
    if (out_parameter_function_parameter_value_kinds) {
        *out_parameter_function_parameter_value_kinds = NULL;
    }
    if (out_parameter_function_parameter_return_types) {
        *out_parameter_function_parameter_return_types = NULL;
    }
    if (out_parameter_function_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_counts = NULL;
    }
    if (out_parameter_function_parameter_parameter_value_kinds) {
        *out_parameter_function_parameter_parameter_value_kinds = NULL;
    }
    if (out_parameter_function_parameter_parameter_return_types) {
        *out_parameter_function_parameter_parameter_return_types = NULL;
    }
    if (out_parameter_function_parameter_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_parameter_counts = NULL;
    }
    if (out_parameter_is_const) {
        *out_parameter_is_const = NULL;
    }
    if (out_parameter_name_lines) {
        *out_parameter_name_lines = NULL;
    }
    if (out_parameter_name_columns) {
        *out_parameter_name_columns = NULL;
    }

    if (check(p, TOKEN_RPAREN)) {
        if (out_parameter_names) {
            *out_parameter_names = parameter_names;
        }
        if (out_parameter_value_kinds) {
            *out_parameter_value_kinds = parameter_value_kinds;
        }
        if (out_parameter_type_names) {
            *out_parameter_type_names = parameter_type_names;
        }
        if (out_parameter_array_ranks) {
            *out_parameter_array_ranks = parameter_array_ranks;
        }
        if (out_parameter_array_extent_exprs) {
            *out_parameter_array_extent_exprs = parameter_array_extent_exprs;
        }
        if (out_parameter_function_return_types) {
            *out_parameter_function_return_types = parameter_function_return_types;
        }
        if (out_parameter_function_parameter_counts) {
            *out_parameter_function_parameter_counts = parameter_function_parameter_counts;
        }
        if (out_parameter_function_parameter_value_kinds) {
            *out_parameter_function_parameter_value_kinds = parameter_function_parameter_value_kinds;
        }
        if (out_parameter_function_parameter_return_types) {
            *out_parameter_function_parameter_return_types = parameter_function_parameter_return_types;
        }
        if (out_parameter_function_parameter_parameter_counts) {
            *out_parameter_function_parameter_parameter_counts = parameter_function_parameter_parameter_counts;
        }
        if (out_parameter_function_parameter_parameter_value_kinds) {
            *out_parameter_function_parameter_parameter_value_kinds =
                parameter_function_parameter_parameter_value_kinds;
        }
        if (out_parameter_function_parameter_parameter_return_types) {
            *out_parameter_function_parameter_parameter_return_types =
                parameter_function_parameter_parameter_return_types;
        }
        if (out_parameter_function_parameter_parameter_parameter_counts) {
            *out_parameter_function_parameter_parameter_parameter_counts =
                parameter_function_parameter_parameter_parameter_counts;
        }
        if (out_parameter_is_const) {
            *out_parameter_is_const = parameter_is_const;
        }
        if (out_parameter_name_lines) {
            *out_parameter_name_lines = parameter_name_lines;
        }
        if (out_parameter_name_columns) {
            *out_parameter_name_columns = parameter_name_columns;
        }
        return 1;
    }

    do {
        const Token *parameter_name_tok = NULL;
        int parameter_is_const_flag = 0;
        int parameter_line = 0;
        int parameter_column = 0;
        AstExpression **parameter_extent_exprs = NULL;
        size_t parameter_extent_capacity = 0u;
        size_t parameter_array_rank = 0u;
        const Token *struct_type_tok = NULL;
        AstFunctionReturnType parameter_decl_type = AST_FUNCTION_RETURN_INT;
        int parameter_value_kind = AST_PARAMETER_VALUE_INT;
        AstFunctionReturnType parameter_function_return_type = AST_FUNCTION_RETURN_INT;
        size_t parameter_function_parameter_count = 0u;
        int *parameter_function_parameter_value_kind_list = NULL;
        AstFunctionReturnType *parameter_function_parameter_return_type_list = NULL;
        size_t *parameter_function_parameter_parameter_count_list = NULL;
        int **nested_parameter_function_parameter_value_kinds = NULL;
        AstFunctionReturnType **nested_parameter_function_parameter_return_types = NULL;
        size_t **nested_parameter_function_parameter_parameter_counts = NULL;
        char *parameter_type_name = NULL;

        if (match(p, TOKEN_KW_CONST)) {
            parameter_is_const_flag = 1;
        }
        if (match(p, TOKEN_KW_INT)) {
            parameter_decl_type = AST_FUNCTION_RETURN_INT;
        } else if (match(p, TOKEN_KW_FLOAT)) {
            parameter_decl_type = AST_FUNCTION_RETURN_FLOAT;
        } else if (match(p, TOKEN_KW_PAIR)) {
            parameter_value_kind = AST_PARAMETER_VALUE_PAIR;
        } else if (match(p, TOKEN_KW_STRUCT)) {
            parameter_value_kind = AST_PARAMETER_VALUE_STRUCT;
            if (!consume_token(p, TOKEN_IDENTIFIER, "struct parameter type name", &struct_type_tok)) {
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            parameter_type_name = copy_token_identifier_lexeme(struct_type_tok);
            if (!parameter_type_name) {
                set_error(p,
                    struct_type_tok,
                    "Out of memory while building function parameter metadata");
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
        } else if (match(p, TOKEN_KW_VOID)) {
            parameter_decl_type = AST_FUNCTION_RETURN_VOID;
        } else {
            consume(p, TOKEN_KW_INT, "'int', 'float', 'pair', 'struct <name>', or 'void'");
            free_name_array(parameter_names, count);
            free(parameter_value_kinds);
            free_name_array(parameter_type_names, count);
            free(parameter_is_const);
            free(parameter_name_lines);
            free(parameter_name_columns);
            free(parameter_function_return_types);
            free(parameter_function_parameter_counts);
            return 0;
        }
        /* C-style prototypes may omit parameter names: int f(int); */
        if (check(p, TOKEN_IDENTIFIER)) {
            consume_token(p, TOKEN_IDENTIFIER, "identifier", &parameter_name_tok);
            parameter_line = parameter_name_tok->line;
            parameter_column = parameter_name_tok->column;
            if (check(p, TOKEN_LPAREN)) {
                size_t nested_param_count = 0u;
                int *nested_parameter_value_kinds = NULL;
                AstFunctionReturnType *nested_parameter_function_return_types = NULL;
                size_t *nested_parameter_function_parameter_counts = NULL;
                int ***nested_parameter_function_parameter_parameter_value_kinds = NULL;
                AstFunctionReturnType ***nested_parameter_function_parameter_parameter_return_types = NULL;
                size_t ***nested_parameter_function_parameter_parameter_parameter_counts = NULL;

                advance(p);
                if (!parse_parameter_list(p,
                        function_type_depth + 1u,
                        &nested_param_count,
                        NULL,
                        NULL,
                        &nested_parameter_value_kinds,
                        NULL,
                        NULL,
                        NULL,
                        &nested_parameter_function_return_types,
                        &nested_parameter_function_parameter_counts,
                        &nested_parameter_function_parameter_value_kinds,
                        &nested_parameter_function_parameter_return_types,
                        &nested_parameter_function_parameter_parameter_counts,
                        &nested_parameter_function_parameter_parameter_value_kinds,
                        &nested_parameter_function_parameter_parameter_return_types,
                        &nested_parameter_function_parameter_parameter_parameter_counts,
                        NULL,
                        NULL,
                        NULL)) {
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(nested_parameter_value_kinds);
                    free(nested_parameter_function_return_types);
                    free(nested_parameter_function_parameter_counts);
                    if (nested_parameter_function_parameter_value_kinds) {
                        size_t nested_index;
                        for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                            free(nested_parameter_function_parameter_value_kinds[nested_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_value_kinds);
                    if (nested_parameter_function_parameter_return_types) {
                        size_t nested_index;
                        for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                            free(nested_parameter_function_parameter_return_types[nested_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_return_types);
                    if (nested_parameter_function_parameter_parameter_counts) {
                        size_t nested_index;
                        for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                            free(nested_parameter_function_parameter_parameter_counts[nested_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_parameter_counts);
                    if (nested_parameter_function_parameter_parameter_value_kinds) {
                        size_t nested_index;
                        for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                            free(nested_parameter_function_parameter_parameter_value_kinds[nested_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_parameter_value_kinds);
                    if (nested_parameter_function_parameter_parameter_return_types) {
                        size_t nested_index;
                        for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                            free(nested_parameter_function_parameter_parameter_return_types[nested_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_parameter_return_types);
                    if (nested_parameter_function_parameter_parameter_parameter_counts) {
                        size_t nested_index;
                        for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                            free(nested_parameter_function_parameter_parameter_parameter_counts[nested_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_parameter_parameter_counts);
                    return 0;
                }
                if (nested_parameter_value_kinds) {
                    size_t nested_index;

                    for (nested_index = 0u; nested_index < nested_param_count; ++nested_index) {
                        if (nested_parameter_value_kinds[nested_index] == AST_PARAMETER_VALUE_FUNCTION) {
                            if (function_type_depth >= 2u) {
                                set_error(p,
                                    parameter_name_tok,
                                    "Nested function-typed parameters currently support only three levels of function signature");
                                free_name_array(parameter_names, count);
                                free(parameter_value_kinds);
                                free_name_array(parameter_type_names, count);
                                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                                free(parameter_array_ranks);
                                free(parameter_is_const);
                                free(parameter_name_lines);
                                free(parameter_name_columns);
                            free(parameter_function_return_types);
                            free(parameter_function_parameter_counts);
                            free(nested_parameter_value_kinds);
                            free(nested_parameter_function_return_types);
                            free(nested_parameter_function_parameter_counts);
                            if (nested_parameter_function_parameter_value_kinds) {
                                size_t free_index;
                                for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                                    free(nested_parameter_function_parameter_value_kinds[free_index]);
                                }
                            }
                            free(nested_parameter_function_parameter_value_kinds);
                            if (nested_parameter_function_parameter_return_types) {
                                size_t free_index;
                                for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                                    free(nested_parameter_function_parameter_return_types[free_index]);
                                }
                            }
                            free(nested_parameter_function_parameter_return_types);
                            if (nested_parameter_function_parameter_parameter_counts) {
                                size_t free_index;
                                for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                                    free(nested_parameter_function_parameter_parameter_counts[free_index]);
                                }
                            }
                            free(nested_parameter_function_parameter_parameter_counts);
                            return 0;
                        }
                        continue;
                        }
                        if (nested_parameter_value_kinds[nested_index] != AST_PARAMETER_VALUE_INT) {
                            set_error(p,
                                parameter_name_tok,
                                "Nested function-typed parameters currently support only plain int parameter lists in the current slice");
                            free_name_array(parameter_names, count);
                            free(parameter_value_kinds);
                            free_name_array(parameter_type_names, count);
                            free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                            free(parameter_array_ranks);
                            free(parameter_is_const);
                            free(parameter_name_lines);
                            free(parameter_name_columns);
                            free(parameter_function_return_types);
                            free(parameter_function_parameter_counts);
                            free(nested_parameter_value_kinds);
                            free(nested_parameter_function_return_types);
                            free(nested_parameter_function_parameter_counts);
                            if (nested_parameter_function_parameter_value_kinds) {
                                size_t free_index;
                                for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                                    free(nested_parameter_function_parameter_value_kinds[free_index]);
                                }
                            }
                            free(nested_parameter_function_parameter_value_kinds);
                            if (nested_parameter_function_parameter_return_types) {
                                size_t free_index;
                                for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                                    free(nested_parameter_function_parameter_return_types[free_index]);
                                }
                            }
                            free(nested_parameter_function_parameter_return_types);
                            if (nested_parameter_function_parameter_parameter_counts) {
                                size_t free_index;
                                for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                                    free(nested_parameter_function_parameter_parameter_counts[free_index]);
                                }
                            }
                            free(nested_parameter_function_parameter_parameter_counts);
                            return 0;
                        }
                    }
                }
                if (!consume(p, TOKEN_RPAREN, "')'")) {
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(nested_parameter_value_kinds);
                    free(nested_parameter_function_return_types);
                    free(nested_parameter_function_parameter_counts);
                    if (nested_parameter_function_parameter_value_kinds) {
                        size_t free_index;
                        for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                            free(nested_parameter_function_parameter_value_kinds[free_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_value_kinds);
                    if (nested_parameter_function_parameter_return_types) {
                        size_t free_index;
                        for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                            free(nested_parameter_function_parameter_return_types[free_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_return_types);
                    if (nested_parameter_function_parameter_parameter_counts) {
                        size_t free_index;
                        for (free_index = 0u; free_index < nested_param_count; ++free_index) {
                            free(nested_parameter_function_parameter_parameter_counts[free_index]);
                        }
                    }
                    free(nested_parameter_function_parameter_parameter_counts);
                    return 0;
                }
                parameter_value_kind = AST_PARAMETER_VALUE_FUNCTION;
                parameter_function_return_type = parameter_decl_type;
                parameter_function_parameter_count = nested_param_count;
                parameter_function_parameter_value_kind_list = nested_parameter_value_kinds;
                parameter_function_parameter_return_type_list = nested_parameter_function_return_types;
                parameter_function_parameter_parameter_count_list =
                    nested_parameter_function_parameter_counts;
                free(parameter_type_name);
                parameter_type_name = NULL;
            } else if (parameter_decl_type == AST_FUNCTION_RETURN_FLOAT) {
                parameter_value_kind = AST_PARAMETER_VALUE_FLOAT;
            } else if (parameter_decl_type == AST_FUNCTION_RETURN_VOID) {
                set_error(p,
                    parameter_name_tok,
                    "Expected function declarator '(...)' after void-typed parameter name");
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
        } else {
            if (parameter_decl_type == AST_FUNCTION_RETURN_VOID) {
                set_error(p, peek(p), "Expected identifier after 'void' in parameter list");
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            if (parameter_decl_type == AST_FUNCTION_RETURN_FLOAT) {
                parameter_value_kind = AST_PARAMETER_VALUE_FLOAT;
            }
            if (out_has_unnamed_parameter) {
                *out_has_unnamed_parameter = 1;
            }
        }
        while (match(p, TOKEN_LBRACKET)) {
            size_t extent_start = p->current;
            AstExpression *extent_expr = NULL;

            if (!check(p, TOKEN_RBRACKET)) {
                if (!parse_expression(p)) {
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                if (!build_expression_ast_from_slice(p,
                        extent_start,
                        p->current,
                        &extent_expr,
                        parameter_name_tok ? parameter_name_tok : peek(p))) {
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
            }
            if (!consume(p, TOKEN_RBRACKET, "']'")) {
                free_name_array(parameter_names, count);
                parser_ast_expression_free(extent_expr);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            if (!append_ast_expression_slot(p,
                    &parameter_extent_exprs,
                    &parameter_array_rank,
                    &parameter_extent_capacity,
                    extent_expr,
                    parameter_name_tok ? parameter_name_tok : peek(p))) {
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
        }

        if (capture_parameter_metadata) {
            char *parameter_name_copy = NULL;
            if (parameter_name_tok) {
                parameter_name_copy = copy_token_identifier_lexeme(parameter_name_tok);
                if (!parameter_name_copy) {
                    set_error(p,
                        parameter_name_tok,
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
            }
            if (!append_name_array_item(p,
                &parameter_names,
                &count,
                &name_capacity,
                parameter_name_copy,
                parameter_name_tok)) {
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            if (out_parameter_value_kinds) {
                int *new_parameter_value_kinds;

                if (!parameter_value_kinds) {
                    new_parameter_value_kinds = (int *)malloc(name_capacity * sizeof(int));
                } else {
                    new_parameter_value_kinds = (int *)realloc(parameter_value_kinds, name_capacity * sizeof(int));
                }
                if (!new_parameter_value_kinds) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                parameter_value_kinds = new_parameter_value_kinds;
                parameter_value_kinds[count - 1] = parameter_value_kind;
            }
            if (!append_nullable_name_array_item(p,
                    &parameter_type_names,
                    count - 1u,
                    &type_name_capacity,
                    parameter_type_name,
                    struct_type_tok ? struct_type_tok : (parameter_name_tok ? parameter_name_tok : peek(p)))) {
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            parameter_type_name = NULL;
            if (!append_size_t_array_item(p,
                    &parameter_array_ranks,
                    count - 1u,
                    &array_rank_capacity,
                    parameter_array_rank,
                    parameter_name_tok)) {
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            if (!append_ast_expression_list_slot(p,
                    &parameter_array_extent_exprs,
                    count - 1u,
                    &parameter_array_extent_capacity,
                    parameter_extent_exprs,
                    parameter_name_tok ? parameter_name_tok : peek(p))) {
                free_name_array(parameter_names, count);
                free(parameter_value_kinds);
                free_name_array(parameter_type_names, count);
                free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                free(parameter_array_ranks);
                free(parameter_is_const);
                free(parameter_name_lines);
                free(parameter_name_columns);
                free(parameter_function_return_types);
                free(parameter_function_parameter_counts);
                return 0;
            }
            parameter_extent_exprs = NULL;
            if (out_parameter_function_return_types) {
                AstFunctionReturnType *new_parameter_function_return_types;

                if (!parameter_function_return_types) {
                    new_parameter_function_return_types =
                        (AstFunctionReturnType *)malloc(name_capacity * sizeof(AstFunctionReturnType));
                } else {
                    new_parameter_function_return_types =
                        (AstFunctionReturnType *)realloc(parameter_function_return_types,
                            name_capacity * sizeof(AstFunctionReturnType));
                }
                if (!new_parameter_function_return_types) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                parameter_function_return_types = new_parameter_function_return_types;
                parameter_function_return_types[count - 1] = parameter_function_return_type;
            }
            if (out_parameter_function_parameter_counts) {
                size_t *new_parameter_function_parameter_counts;

                if (!parameter_function_parameter_counts) {
                    new_parameter_function_parameter_counts = (size_t *)malloc(name_capacity * sizeof(size_t));
                } else {
                    new_parameter_function_parameter_counts =
                        (size_t *)realloc(parameter_function_parameter_counts, name_capacity * sizeof(size_t));
                }
                if (!new_parameter_function_parameter_counts) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                parameter_function_parameter_counts = new_parameter_function_parameter_counts;
                parameter_function_parameter_counts[count - 1] = parameter_function_parameter_count;
            }
            if (out_parameter_function_parameter_value_kinds) {
                int ***new_parameter_function_parameter_value_kinds;

                if (!parameter_function_parameter_value_kinds) {
                    new_parameter_function_parameter_value_kinds =
                        (int ***)malloc(name_capacity * sizeof(int **));
                } else {
                    new_parameter_function_parameter_value_kinds =
                        (int ***)realloc(parameter_function_parameter_value_kinds,
                            name_capacity * sizeof(int **));
                }
                if (!new_parameter_function_parameter_value_kinds) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(parameter_function_parameter_value_kind_list);
                    free(parameter_function_parameter_return_type_list);
                    free(parameter_function_parameter_parameter_count_list);
                    return 0;
                }
                if (name_capacity > count) {
                    size_t init_index;
                    for (init_index = count; init_index < name_capacity; ++init_index) {
                        new_parameter_function_parameter_value_kinds[init_index] = NULL;
                    }
                }
                parameter_function_parameter_value_kinds =
                    (int **)new_parameter_function_parameter_value_kinds;
                parameter_function_parameter_value_kinds[count - 1] =
                    parameter_function_parameter_value_kind_list;
                parameter_function_parameter_value_kind_list = NULL;
            }
            if (out_parameter_function_parameter_return_types) {
                AstFunctionReturnType ***new_parameter_function_parameter_return_types;

                if (!parameter_function_parameter_return_types) {
                    new_parameter_function_parameter_return_types =
                        (AstFunctionReturnType ***)malloc(name_capacity * sizeof(AstFunctionReturnType **));
                } else {
                    new_parameter_function_parameter_return_types =
                        (AstFunctionReturnType ***)realloc(parameter_function_parameter_return_types,
                            name_capacity * sizeof(AstFunctionReturnType **));
                }
                if (!new_parameter_function_parameter_return_types) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(parameter_function_parameter_value_kind_list);
                    free(parameter_function_parameter_return_type_list);
                    free(parameter_function_parameter_parameter_count_list);
                    return 0;
                }
                if (name_capacity > count) {
                    size_t init_index;
                    for (init_index = count; init_index < name_capacity; ++init_index) {
                        new_parameter_function_parameter_return_types[init_index] = NULL;
                    }
                }
                parameter_function_parameter_return_types =
                    (AstFunctionReturnType **)new_parameter_function_parameter_return_types;
                parameter_function_parameter_return_types[count - 1] =
                    parameter_function_parameter_return_type_list;
                parameter_function_parameter_return_type_list = NULL;
            }
            if (out_parameter_function_parameter_parameter_counts) {
                size_t ***new_parameter_function_parameter_parameter_counts;

                if (!parameter_function_parameter_parameter_counts) {
                    new_parameter_function_parameter_parameter_counts =
                        (size_t ***)malloc(name_capacity * sizeof(size_t **));
                } else {
                    new_parameter_function_parameter_parameter_counts =
                        (size_t ***)realloc(parameter_function_parameter_parameter_counts,
                            name_capacity * sizeof(size_t **));
                }
                if (!new_parameter_function_parameter_parameter_counts) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(parameter_function_parameter_value_kind_list);
                    free(parameter_function_parameter_return_type_list);
                    free(parameter_function_parameter_parameter_count_list);
                    return 0;
                }
                if (name_capacity > count) {
                    size_t init_index;
                    for (init_index = count; init_index < name_capacity; ++init_index) {
                        new_parameter_function_parameter_parameter_counts[init_index] = NULL;
                    }
                }
                parameter_function_parameter_parameter_counts =
                    (size_t **)new_parameter_function_parameter_parameter_counts;
                parameter_function_parameter_parameter_counts[count - 1] =
                    parameter_function_parameter_parameter_count_list;
                parameter_function_parameter_parameter_count_list = NULL;
            }
            if (out_parameter_function_parameter_parameter_value_kinds) {
                int ****new_parameter_function_parameter_parameter_value_kinds;

                if (!parameter_function_parameter_parameter_value_kinds) {
                    new_parameter_function_parameter_parameter_value_kinds =
                        (int ****)malloc(name_capacity * sizeof(int ***));
                } else {
                    new_parameter_function_parameter_parameter_value_kinds =
                        (int ****)realloc(parameter_function_parameter_parameter_value_kinds,
                            name_capacity * sizeof(int ***));
                }
                if (!new_parameter_function_parameter_parameter_value_kinds) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(parameter_function_parameter_value_kind_list);
                    free(parameter_function_parameter_return_type_list);
                    free(parameter_function_parameter_parameter_count_list);
                    return 0;
                }
                if (name_capacity > count) {
                    size_t init_index;
                    for (init_index = count; init_index < name_capacity; ++init_index) {
                        new_parameter_function_parameter_parameter_value_kinds[init_index] = NULL;
                    }
                }
                parameter_function_parameter_parameter_value_kinds =
                    (int ***)new_parameter_function_parameter_parameter_value_kinds;
                parameter_function_parameter_parameter_value_kinds[count - 1] =
                    nested_parameter_function_parameter_value_kinds;
                nested_parameter_function_parameter_value_kinds = NULL;
            }
            if (out_parameter_function_parameter_parameter_return_types) {
                AstFunctionReturnType ****new_parameter_function_parameter_parameter_return_types;

                if (!parameter_function_parameter_parameter_return_types) {
                    new_parameter_function_parameter_parameter_return_types =
                        (AstFunctionReturnType ****)malloc(name_capacity * sizeof(AstFunctionReturnType ***));
                } else {
                    new_parameter_function_parameter_parameter_return_types =
                        (AstFunctionReturnType ****)realloc(parameter_function_parameter_parameter_return_types,
                            name_capacity * sizeof(AstFunctionReturnType ***));
                }
                if (!new_parameter_function_parameter_parameter_return_types) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(parameter_function_parameter_value_kind_list);
                    free(parameter_function_parameter_return_type_list);
                    free(parameter_function_parameter_parameter_count_list);
                    return 0;
                }
                if (name_capacity > count) {
                    size_t init_index;
                    for (init_index = count; init_index < name_capacity; ++init_index) {
                        new_parameter_function_parameter_parameter_return_types[init_index] = NULL;
                    }
                }
                parameter_function_parameter_parameter_return_types =
                    (AstFunctionReturnType ***)new_parameter_function_parameter_parameter_return_types;
                parameter_function_parameter_parameter_return_types[count - 1] =
                    nested_parameter_function_parameter_return_types;
                nested_parameter_function_parameter_return_types = NULL;
            }
            if (out_parameter_function_parameter_parameter_parameter_counts) {
                size_t ****new_parameter_function_parameter_parameter_parameter_counts;

                if (!parameter_function_parameter_parameter_parameter_counts) {
                    new_parameter_function_parameter_parameter_parameter_counts =
                        (size_t ****)malloc(name_capacity * sizeof(size_t ***));
                } else {
                    new_parameter_function_parameter_parameter_parameter_counts =
                        (size_t ****)realloc(parameter_function_parameter_parameter_parameter_counts,
                            name_capacity * sizeof(size_t ***));
                }
                if (!new_parameter_function_parameter_parameter_parameter_counts) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    free(parameter_function_parameter_value_kind_list);
                    free(parameter_function_parameter_return_type_list);
                    free(parameter_function_parameter_parameter_count_list);
                    return 0;
                }
                if (name_capacity > count) {
                    size_t init_index;
                    for (init_index = count; init_index < name_capacity; ++init_index) {
                        new_parameter_function_parameter_parameter_parameter_counts[init_index] = NULL;
                    }
                }
                parameter_function_parameter_parameter_parameter_counts =
                    (size_t ***)new_parameter_function_parameter_parameter_parameter_counts;
                parameter_function_parameter_parameter_parameter_counts[count - 1] =
                    nested_parameter_function_parameter_parameter_counts;
                nested_parameter_function_parameter_parameter_counts = NULL;
            }
            if (out_parameter_is_const) {
                int *new_parameter_is_const;

                if (!parameter_is_const) {
                    new_parameter_is_const = (int *)malloc(name_capacity * sizeof(int));
                } else {
                    new_parameter_is_const = (int *)realloc(parameter_is_const, name_capacity * sizeof(int));
                }
                if (!new_parameter_is_const) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                parameter_is_const = new_parameter_is_const;
                parameter_is_const[count - 1] = parameter_is_const_flag;
            }
            if (out_parameter_name_lines) {
                int *new_lines;

                if (!parameter_name_lines) {
                    new_lines = (int *)malloc(name_capacity * sizeof(int));
                } else {
                    new_lines = (int *)realloc(parameter_name_lines, name_capacity * sizeof(int));
                }
                if (!new_lines) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                parameter_name_lines = new_lines;
                parameter_name_lines[count - 1] = parameter_line;
            }
            if (out_parameter_name_columns) {
                int *new_columns;

                if (!parameter_name_columns) {
                    new_columns = (int *)malloc(name_capacity * sizeof(int));
                } else {
                    new_columns = (int *)realloc(parameter_name_columns,
                        name_capacity * sizeof(int));
                }
                if (!new_columns) {
                    set_error(p,
                        parameter_name_tok ? parameter_name_tok : peek(p),
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
                    free(parameter_value_kinds);
                    free_name_array(parameter_type_names, count);
                    free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
                    free(parameter_array_ranks);
                    free(parameter_is_const);
                    free(parameter_name_lines);
                    free(parameter_name_columns);
                    free(parameter_function_return_types);
                    free(parameter_function_parameter_counts);
                    return 0;
                }
                parameter_name_columns = new_columns;
                parameter_name_columns[count - 1] = parameter_column;
            }
            if (out_parameter_name_lines && !parameter_name_tok) {
                parameter_name_lines[count - 1] = 0;
            }
            if (out_parameter_name_columns && !parameter_name_tok) {
                parameter_name_columns[count - 1] = 0;
            }
        } else {
            free(parameter_type_name);
            free_ast_expression_array(parameter_extent_exprs, parameter_array_rank);
            count++;
        }
    } while (match(p, TOKEN_COMMA));

    if (out_param_count) {
        *out_param_count = count;
    }

    if (out_parameter_names) {
        *out_parameter_names = parameter_names;
    } else {
        free_name_array(parameter_names, count);
    }
    if (out_parameter_value_kinds) {
        *out_parameter_value_kinds = parameter_value_kinds;
    } else {
        free(parameter_value_kinds);
    }
    if (out_parameter_type_names) {
        *out_parameter_type_names = parameter_type_names;
    } else {
        free_name_array(parameter_type_names, count);
    }
    if (out_parameter_array_ranks) {
        *out_parameter_array_ranks = parameter_array_ranks;
    }
    if (out_parameter_array_extent_exprs) {
        *out_parameter_array_extent_exprs = parameter_array_extent_exprs;
    } else {
        free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, count);
    }
    if (!out_parameter_array_ranks) {
        free(parameter_array_ranks);
    }
    if (out_parameter_function_return_types) {
        *out_parameter_function_return_types = parameter_function_return_types;
    } else {
        free(parameter_function_return_types);
    }
    if (out_parameter_function_parameter_counts) {
        *out_parameter_function_parameter_counts = parameter_function_parameter_counts;
    } else {
        free(parameter_function_parameter_counts);
    }
    if (out_parameter_function_parameter_value_kinds) {
        *out_parameter_function_parameter_value_kinds = parameter_function_parameter_value_kinds;
    } else if (parameter_function_parameter_value_kinds) {
        size_t free_index;
        for (free_index = 0u; free_index < count; ++free_index) {
            free(parameter_function_parameter_value_kinds[free_index]);
        }
        free(parameter_function_parameter_value_kinds);
    }
    if (out_parameter_function_parameter_return_types) {
        *out_parameter_function_parameter_return_types = parameter_function_parameter_return_types;
    } else if (parameter_function_parameter_return_types) {
        size_t free_index;
        for (free_index = 0u; free_index < count; ++free_index) {
            free(parameter_function_parameter_return_types[free_index]);
        }
        free(parameter_function_parameter_return_types);
    }
    if (out_parameter_function_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_counts = parameter_function_parameter_parameter_counts;
    } else if (parameter_function_parameter_parameter_counts) {
        size_t free_index;
        for (free_index = 0u; free_index < count; ++free_index) {
            free(parameter_function_parameter_parameter_counts[free_index]);
        }
        free(parameter_function_parameter_parameter_counts);
    }
    if (out_parameter_function_parameter_parameter_value_kinds) {
        *out_parameter_function_parameter_parameter_value_kinds =
            parameter_function_parameter_parameter_value_kinds;
    } else if (parameter_function_parameter_parameter_value_kinds) {
        size_t free_index;
        for (free_index = 0u; free_index < count; ++free_index) {
            free(parameter_function_parameter_parameter_value_kinds[free_index]);
        }
        free(parameter_function_parameter_parameter_value_kinds);
    }
    if (out_parameter_function_parameter_parameter_return_types) {
        *out_parameter_function_parameter_parameter_return_types =
            parameter_function_parameter_parameter_return_types;
    } else if (parameter_function_parameter_parameter_return_types) {
        size_t free_index;
        for (free_index = 0u; free_index < count; ++free_index) {
            free(parameter_function_parameter_parameter_return_types[free_index]);
        }
        free(parameter_function_parameter_parameter_return_types);
    }
    if (out_parameter_function_parameter_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_parameter_counts =
            parameter_function_parameter_parameter_parameter_counts;
    } else if (parameter_function_parameter_parameter_parameter_counts) {
        size_t free_index;
        for (free_index = 0u; free_index < count; ++free_index) {
            free(parameter_function_parameter_parameter_parameter_counts[free_index]);
        }
        free(parameter_function_parameter_parameter_parameter_counts);
    }
    if (out_parameter_is_const) {
        *out_parameter_is_const = parameter_is_const;
    } else {
        free(parameter_is_const);
    }
    if (out_parameter_name_lines) {
        *out_parameter_name_lines = parameter_name_lines;
    } else {
        free(parameter_name_lines);
    }
    if (out_parameter_name_columns) {
        *out_parameter_name_columns = parameter_name_columns;
    } else {
        free(parameter_name_columns);
    }

    return 1;
}

static void parsed_function_signature_init(ParsedFunctionSignature *signature) {
    if (!signature) {
        return;
    }

    signature->return_type = AST_FUNCTION_RETURN_INT;
    signature->return_type_name = NULL;
    signature->has_function_return_signature = 0;
    signature->function_return_function_return_type = AST_FUNCTION_RETURN_INT;
    signature->function_return_function_return_type_name = NULL;
    signature->function_return_function_parameter_count = 0u;
    signature->function_return_function_parameter_value_kinds = NULL;
    signature->function_return_function_parameter_type_names = NULL;
    signature->function_return_function_parameter_return_types = NULL;
    signature->function_return_function_parameter_parameter_counts = NULL;
    signature->function_return_function_parameter_parameter_value_kinds = NULL;
    signature->function_return_function_parameter_parameter_return_types = NULL;
    signature->function_return_function_parameter_parameter_parameter_counts = NULL;
    signature->name_tok = NULL;
    signature->parameter_count = 0;
    signature->parameter_names = NULL;
    signature->parameter_value_kinds = NULL;
    signature->parameter_type_names = NULL;
    signature->parameter_array_ranks = NULL;
    signature->parameter_array_extent_exprs = NULL;
    signature->parameter_function_return_types = NULL;
    signature->parameter_function_parameter_counts = NULL;
    signature->parameter_function_parameter_value_kinds = NULL;
    signature->parameter_function_parameter_return_types = NULL;
    signature->parameter_function_parameter_parameter_counts = NULL;
    signature->parameter_function_parameter_parameter_value_kinds = NULL;
    signature->parameter_function_parameter_parameter_return_types = NULL;
    signature->parameter_function_parameter_parameter_parameter_counts = NULL;
    signature->parameter_is_const = NULL;
    signature->parameter_name_lines = NULL;
    signature->parameter_name_columns = NULL;
    signature->has_unnamed_parameter = 0;
}

static void parsed_function_signature_release(ParsedFunctionSignature *signature) {
    if (!signature) {
        return;
    }

    free(signature->return_type_name);
    free(signature->function_return_function_return_type_name);
    free(signature->function_return_function_parameter_value_kinds);
    free_name_array(signature->function_return_function_parameter_type_names,
        signature->function_return_function_parameter_count);
    free(signature->function_return_function_parameter_return_types);
    free(signature->function_return_function_parameter_parameter_counts);
    if (signature->function_return_function_parameter_parameter_value_kinds) {
        size_t i;
        for (i = 0u; i < signature->function_return_function_parameter_count; ++i) {
            free(signature->function_return_function_parameter_parameter_value_kinds[i]);
        }
    }
    free(signature->function_return_function_parameter_parameter_value_kinds);
    if (signature->function_return_function_parameter_parameter_return_types) {
        size_t i;
        for (i = 0u; i < signature->function_return_function_parameter_count; ++i) {
            free(signature->function_return_function_parameter_parameter_return_types[i]);
        }
    }
    free(signature->function_return_function_parameter_parameter_return_types);
    if (signature->function_return_function_parameter_parameter_parameter_counts) {
        size_t i;
        for (i = 0u; i < signature->function_return_function_parameter_count; ++i) {
            free(signature->function_return_function_parameter_parameter_parameter_counts[i]);
        }
    }
    free(signature->function_return_function_parameter_parameter_parameter_counts);
    free_name_array(signature->parameter_names, signature->parameter_count);
    free(signature->parameter_value_kinds);
    free_name_array(signature->parameter_type_names, signature->parameter_count);
    free_ast_expression_matrix(signature->parameter_array_extent_exprs,
        signature->parameter_array_ranks,
        signature->parameter_count);
    free(signature->parameter_array_ranks);
    free(signature->parameter_function_return_types);
    free(signature->parameter_function_parameter_counts);
    if (signature->parameter_function_parameter_value_kinds) {
        size_t i;
        for (i = 0u; i < signature->parameter_count; ++i) {
            free(signature->parameter_function_parameter_value_kinds[i]);
        }
    }
    free(signature->parameter_function_parameter_value_kinds);
    if (signature->parameter_function_parameter_return_types) {
        size_t i;
        for (i = 0u; i < signature->parameter_count; ++i) {
            free(signature->parameter_function_parameter_return_types[i]);
        }
    }
    free(signature->parameter_function_parameter_return_types);
    if (signature->parameter_function_parameter_parameter_counts) {
        size_t i;
        for (i = 0u; i < signature->parameter_count; ++i) {
            free(signature->parameter_function_parameter_parameter_counts[i]);
        }
    }
    free(signature->parameter_function_parameter_parameter_counts);
    if (signature->parameter_function_parameter_parameter_value_kinds) {
        size_t i;
        for (i = 0u; i < signature->parameter_count; ++i) {
            free(signature->parameter_function_parameter_parameter_value_kinds[i]);
        }
    }
    free(signature->parameter_function_parameter_parameter_value_kinds);
    if (signature->parameter_function_parameter_parameter_return_types) {
        size_t i;
        for (i = 0u; i < signature->parameter_count; ++i) {
            free(signature->parameter_function_parameter_parameter_return_types[i]);
        }
    }
    free(signature->parameter_function_parameter_parameter_return_types);
    if (signature->parameter_function_parameter_parameter_parameter_counts) {
        size_t i;
        for (i = 0u; i < signature->parameter_count; ++i) {
            free(signature->parameter_function_parameter_parameter_parameter_counts[i]);
        }
    }
    free(signature->parameter_function_parameter_parameter_parameter_counts);
    free(signature->parameter_is_const);
    free(signature->parameter_name_lines);
    free(signature->parameter_name_columns);
    parsed_function_signature_init(signature);
}

static void parser_commit_parameter_names(char ***out_parameter_names,
    int **out_parameter_value_kinds,
    char ***out_parameter_type_names,
    size_t **out_parameter_array_ranks,
    AstExpression ****out_parameter_array_extent_exprs,
    AstFunctionReturnType **out_parameter_function_return_types,
    size_t **out_parameter_function_parameter_counts,
    int ***out_parameter_function_parameter_value_kinds,
    AstFunctionReturnType ***out_parameter_function_parameter_return_types,
    size_t ***out_parameter_function_parameter_parameter_counts,
    int ****out_parameter_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ****out_parameter_function_parameter_parameter_return_types,
    size_t ****out_parameter_function_parameter_parameter_parameter_counts,
    int **out_parameter_is_const,
    int **out_parameter_name_lines,
    int **out_parameter_name_columns,
    char **parameter_names,
    int *parameter_value_kinds,
    char **parameter_type_names,
    size_t *parameter_array_ranks,
    AstExpression ***parameter_array_extent_exprs,
    AstFunctionReturnType *parameter_function_return_types,
    size_t *parameter_function_parameter_counts,
    int **parameter_function_parameter_value_kinds,
    AstFunctionReturnType **parameter_function_parameter_return_types,
    size_t **parameter_function_parameter_parameter_counts,
    int ***parameter_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ***parameter_function_parameter_parameter_return_types,
    size_t ***parameter_function_parameter_parameter_parameter_counts,
    int *parameter_is_const,
    int *parameter_name_lines,
    int *parameter_name_columns,
    size_t parameter_count) {
    if (out_parameter_names) {
        *out_parameter_names = parameter_names;
    } else {
        free_name_array(parameter_names, parameter_count);
    }
    if (out_parameter_value_kinds) {
        *out_parameter_value_kinds = parameter_value_kinds;
    } else {
        free(parameter_value_kinds);
    }
    if (out_parameter_type_names) {
        *out_parameter_type_names = parameter_type_names;
    } else {
        free_name_array(parameter_type_names, parameter_count);
    }
    if (out_parameter_array_ranks) {
        *out_parameter_array_ranks = parameter_array_ranks;
    }
    if (out_parameter_array_extent_exprs) {
        *out_parameter_array_extent_exprs = parameter_array_extent_exprs;
    } else {
        free_ast_expression_matrix(parameter_array_extent_exprs, parameter_array_ranks, parameter_count);
    }
    if (!out_parameter_array_ranks) {
        free(parameter_array_ranks);
    }
    if (out_parameter_function_return_types) {
        *out_parameter_function_return_types = parameter_function_return_types;
    } else {
        free(parameter_function_return_types);
    }
    if (out_parameter_function_parameter_counts) {
        *out_parameter_function_parameter_counts = parameter_function_parameter_counts;
    } else {
        free(parameter_function_parameter_counts);
    }
    if (out_parameter_function_parameter_value_kinds) {
        *out_parameter_function_parameter_value_kinds = parameter_function_parameter_value_kinds;
    } else if (parameter_function_parameter_value_kinds) {
        size_t i;
        for (i = 0u; i < parameter_count; ++i) {
            free(parameter_function_parameter_value_kinds[i]);
        }
        free(parameter_function_parameter_value_kinds);
    }
    if (out_parameter_function_parameter_return_types) {
        *out_parameter_function_parameter_return_types = parameter_function_parameter_return_types;
    } else if (parameter_function_parameter_return_types) {
        size_t i;
        for (i = 0u; i < parameter_count; ++i) {
            free(parameter_function_parameter_return_types[i]);
        }
        free(parameter_function_parameter_return_types);
    }
    if (out_parameter_function_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_counts = parameter_function_parameter_parameter_counts;
    } else if (parameter_function_parameter_parameter_counts) {
        size_t i;
        for (i = 0u; i < parameter_count; ++i) {
            free(parameter_function_parameter_parameter_counts[i]);
        }
        free(parameter_function_parameter_parameter_counts);
    }
    if (out_parameter_function_parameter_parameter_value_kinds) {
        *out_parameter_function_parameter_parameter_value_kinds =
            parameter_function_parameter_parameter_value_kinds;
    } else if (parameter_function_parameter_parameter_value_kinds) {
        size_t i;
        for (i = 0u; i < parameter_count; ++i) {
            free(parameter_function_parameter_parameter_value_kinds[i]);
        }
        free(parameter_function_parameter_parameter_value_kinds);
    }
    if (out_parameter_function_parameter_parameter_return_types) {
        *out_parameter_function_parameter_parameter_return_types =
            parameter_function_parameter_parameter_return_types;
    } else if (parameter_function_parameter_parameter_return_types) {
        size_t i;
        for (i = 0u; i < parameter_count; ++i) {
            free(parameter_function_parameter_parameter_return_types[i]);
        }
        free(parameter_function_parameter_parameter_return_types);
    }
    if (out_parameter_function_parameter_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_parameter_counts =
            parameter_function_parameter_parameter_parameter_counts;
    } else if (parameter_function_parameter_parameter_parameter_counts) {
        size_t i;
        for (i = 0u; i < parameter_count; ++i) {
            free(parameter_function_parameter_parameter_parameter_counts[i]);
        }
        free(parameter_function_parameter_parameter_parameter_counts);
    }
    if (out_parameter_is_const) {
        *out_parameter_is_const = parameter_is_const;
    } else {
        free(parameter_is_const);
    }
    if (out_parameter_name_lines) {
        *out_parameter_name_lines = parameter_name_lines;
    } else {
        free(parameter_name_lines);
    }
    if (out_parameter_name_columns) {
        *out_parameter_name_columns = parameter_name_columns;
    } else {
        free(parameter_name_columns);
    }
}

static int parse_function_external_signature(Parser *p,
    int capture_parameter_names,
    ParsedFunctionSignature *out_signature) {
    parsed_function_signature_init(out_signature);

    parser_consume_optional_const_qualifier(p);
    if (match(p, TOKEN_KW_INT)) {
        out_signature->return_type = AST_FUNCTION_RETURN_INT;
    } else if (match(p, TOKEN_KW_FLOAT)) {
        out_signature->return_type = AST_FUNCTION_RETURN_FLOAT;
    } else if (match(p, TOKEN_KW_PAIR)) {
        out_signature->return_type = AST_FUNCTION_RETURN_PAIR;
    } else if (match(p, TOKEN_KW_STRUCT)) {
        const Token *return_type_name_tok = NULL;

        out_signature->return_type = AST_FUNCTION_RETURN_STRUCT;
        if (!consume_token(p, TOKEN_IDENTIFIER, "struct return type name", &return_type_name_tok)) {
            return 0;
        }
        out_signature->return_type_name = copy_token_identifier_lexeme(return_type_name_tok);
        if (!out_signature->return_type_name) {
            set_error(p, return_type_name_tok, "Out of memory while building AST");
            return 0;
        }
    } else if (match(p, TOKEN_KW_VOID)) {
        out_signature->return_type = AST_FUNCTION_RETURN_VOID;
    } else {
        consume(p, TOKEN_KW_INT, "'int', 'float', 'pair', 'struct <name>', or 'void'");
        return 0;
    }
    if (!consume_token(p, TOKEN_IDENTIFIER, "identifier", &out_signature->name_tok)) {
        return 0;
    }
    if (!consume(p, TOKEN_LPAREN, "'('")) {
        return 0;
    }
    if (!parse_parameter_list(p,
        0u,
        &out_signature->parameter_count,
        &out_signature->has_unnamed_parameter,
        capture_parameter_names ? &out_signature->parameter_names : NULL,
        capture_parameter_names ? &out_signature->parameter_value_kinds : NULL,
        capture_parameter_names ? &out_signature->parameter_type_names : NULL,
        capture_parameter_names ? &out_signature->parameter_array_ranks : NULL,
        capture_parameter_names ? &out_signature->parameter_array_extent_exprs : NULL,
        capture_parameter_names ? &out_signature->parameter_function_return_types : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_counts : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_value_kinds : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_return_types : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_parameter_counts : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_parameter_value_kinds : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_parameter_return_types : NULL,
        capture_parameter_names ? &out_signature->parameter_function_parameter_parameter_parameter_counts : NULL,
        capture_parameter_names ? &out_signature->parameter_is_const : NULL,
        capture_parameter_names ? &out_signature->parameter_name_lines : NULL,
        capture_parameter_names ? &out_signature->parameter_name_columns : NULL)) {
        return 0;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        parsed_function_signature_release(out_signature);
        return 0;
    }

    if (match(p, TOKEN_LPAREN)) {
        size_t nested_parameter_count = 0u;
        int *nested_parameter_value_kinds = NULL;
        char **nested_parameter_type_names = NULL;
        AstFunctionReturnType *nested_parameter_function_return_types = NULL;
        size_t *nested_parameter_function_parameter_counts = NULL;
        int **nested_parameter_function_parameter_value_kinds = NULL;
        AstFunctionReturnType **nested_parameter_function_parameter_return_types = NULL;
        size_t **nested_parameter_function_parameter_parameter_counts = NULL;

        out_signature->has_function_return_signature = 1;
        out_signature->function_return_function_return_type = out_signature->return_type;
        out_signature->function_return_function_return_type_name = out_signature->return_type_name;
        out_signature->return_type = AST_FUNCTION_RETURN_INT;
        out_signature->return_type_name = NULL;

        if (!parse_parameter_list(p,
                0u,
                &nested_parameter_count,
                NULL,
                NULL,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_value_kinds
                    : &nested_parameter_value_kinds,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_type_names
                    : &nested_parameter_type_names,
                NULL,
                NULL,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_return_types
                    : &nested_parameter_function_return_types,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_parameter_counts
                    : &nested_parameter_function_parameter_counts,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_parameter_value_kinds
                    : &nested_parameter_function_parameter_value_kinds,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_parameter_return_types
                    : &nested_parameter_function_parameter_return_types,
                capture_parameter_names
                    ? &out_signature->function_return_function_parameter_parameter_parameter_counts
                    : &nested_parameter_function_parameter_parameter_counts,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL)) {
            free(nested_parameter_value_kinds);
            free_name_array(nested_parameter_type_names, nested_parameter_count);
            free(nested_parameter_function_return_types);
            free(nested_parameter_function_parameter_counts);
            if (nested_parameter_function_parameter_value_kinds) {
                size_t nested_index;
                for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                    free(nested_parameter_function_parameter_value_kinds[nested_index]);
                }
            }
            free(nested_parameter_function_parameter_value_kinds);
            if (nested_parameter_function_parameter_return_types) {
                size_t nested_index;
                for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                    free(nested_parameter_function_parameter_return_types[nested_index]);
                }
            }
            free(nested_parameter_function_parameter_return_types);
            if (nested_parameter_function_parameter_parameter_counts) {
                size_t nested_index;
                for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                    free(nested_parameter_function_parameter_parameter_counts[nested_index]);
                }
            }
            free(nested_parameter_function_parameter_parameter_counts);
            parsed_function_signature_release(out_signature);
            return 0;
        }
        if (!consume(p, TOKEN_RPAREN, "')'")) {
            free(nested_parameter_value_kinds);
            free_name_array(nested_parameter_type_names, nested_parameter_count);
            free(nested_parameter_function_return_types);
            free(nested_parameter_function_parameter_counts);
            if (nested_parameter_function_parameter_value_kinds) {
                size_t nested_index;
                for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                    free(nested_parameter_function_parameter_value_kinds[nested_index]);
                }
            }
            free(nested_parameter_function_parameter_value_kinds);
            if (nested_parameter_function_parameter_return_types) {
                size_t nested_index;
                for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                    free(nested_parameter_function_parameter_return_types[nested_index]);
                }
            }
            free(nested_parameter_function_parameter_return_types);
            if (nested_parameter_function_parameter_parameter_counts) {
                size_t nested_index;
                for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                    free(nested_parameter_function_parameter_parameter_counts[nested_index]);
                }
            }
            free(nested_parameter_function_parameter_parameter_counts);
            parsed_function_signature_release(out_signature);
            return 0;
        }
        out_signature->function_return_function_parameter_count = nested_parameter_count;
        free(nested_parameter_value_kinds);
        free_name_array(nested_parameter_type_names, nested_parameter_count);
        free(nested_parameter_function_return_types);
        free(nested_parameter_function_parameter_counts);
        if (nested_parameter_function_parameter_value_kinds) {
            size_t nested_index;
            for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                free(nested_parameter_function_parameter_value_kinds[nested_index]);
            }
        }
        free(nested_parameter_function_parameter_value_kinds);
        if (nested_parameter_function_parameter_return_types) {
            size_t nested_index;
            for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                free(nested_parameter_function_parameter_return_types[nested_index]);
            }
        }
        free(nested_parameter_function_parameter_return_types);
        if (nested_parameter_function_parameter_parameter_counts) {
            size_t nested_index;
            for (nested_index = 0u; nested_index < nested_parameter_count; ++nested_index) {
                free(nested_parameter_function_parameter_parameter_counts[nested_index]);
            }
        }
        free(nested_parameter_function_parameter_parameter_counts);
    }

    return 1;
}

static int parse_function_external_definition_body(Parser *p,
    const ParsedFunctionSignature *signature,
    AstStatement **out_function_body,
    size_t *out_return_statement_count) {
    int needs_function_body_ast;
    AstStatement *function_body = NULL;

    if (signature->has_unnamed_parameter) {
        set_error(p,
            peek(p),
            "Function definition parameters must have names");
        return 0;
    }

    needs_function_body_ast = (out_function_body != NULL) || (out_return_statement_count != NULL);
    if (!parse_compound_statement_with_local_control(p,
            NULL,
            needs_function_body_ast ? &function_body : NULL)) {
        parser_ast_statement_free(function_body);
        return 0;
    }

    if (out_return_statement_count) {
        *out_return_statement_count = count_return_statements_in_ast(function_body);
    }

    if (out_function_body) {
        *out_function_body = function_body;
    } else {
        parser_ast_statement_free(function_body);
    }

    return 1;
}

static int parse_function_external(Parser *p,
    const Token **out_name_token,
    AstFunctionReturnType *out_return_type,
    char **out_return_type_name,
    int *out_has_function_return_signature,
    AstFunctionReturnType *out_function_return_function_return_type,
    char **out_function_return_function_return_type_name,
    size_t *out_function_return_function_parameter_count,
    int **out_function_return_function_parameter_value_kinds,
    char ***out_function_return_function_parameter_type_names,
    AstFunctionReturnType **out_function_return_function_parameter_return_types,
    size_t **out_function_return_function_parameter_parameter_counts,
    int ***out_function_return_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ***out_function_return_function_parameter_parameter_return_types,
    size_t ***out_function_return_function_parameter_parameter_parameter_counts,
    size_t *out_parameter_count,
    char ***out_parameter_names,
    int **out_parameter_value_kinds,
    char ***out_parameter_type_names,
    size_t **out_parameter_array_ranks,
    AstExpression ****out_parameter_array_extent_exprs,
    AstFunctionReturnType **out_parameter_function_return_types,
    size_t **out_parameter_function_parameter_counts,
    int ***out_parameter_function_parameter_value_kinds,
    AstFunctionReturnType ***out_parameter_function_parameter_return_types,
    size_t ***out_parameter_function_parameter_parameter_counts,
    int ****out_parameter_function_parameter_parameter_value_kinds,
    AstFunctionReturnType ****out_parameter_function_parameter_parameter_return_types,
    size_t ****out_parameter_function_parameter_parameter_parameter_counts,
    int **out_parameter_is_const,
    int **out_parameter_name_lines,
    int **out_parameter_name_columns,
    int *out_is_definition,
    AstStatement **out_function_body,
    size_t *out_return_statement_count) {
    ParsedFunctionSignature signature;
    int capture_parameter_metadata =
        (out_parameter_names != NULL) ||
        (out_parameter_value_kinds != NULL) ||
        (out_parameter_type_names != NULL) ||
        (out_parameter_array_ranks != NULL) ||
        (out_parameter_function_return_types != NULL) ||
        (out_parameter_function_parameter_counts != NULL) ||
        (out_parameter_is_const != NULL) ||
        (out_parameter_name_lines != NULL) ||
        (out_parameter_name_columns != NULL);
    int body_ok;

    if (out_name_token) {
        *out_name_token = NULL;
    }
    if (out_return_type) {
        *out_return_type = AST_FUNCTION_RETURN_INT;
    }
    if (out_return_type_name) {
        *out_return_type_name = NULL;
    }
    if (out_has_function_return_signature) {
        *out_has_function_return_signature = 0;
    }
    if (out_function_return_function_return_type) {
        *out_function_return_function_return_type = AST_FUNCTION_RETURN_INT;
    }
    if (out_function_return_function_return_type_name) {
        *out_function_return_function_return_type_name = NULL;
    }
    if (out_function_return_function_parameter_count) {
        *out_function_return_function_parameter_count = 0u;
    }
    if (out_function_return_function_parameter_value_kinds) {
        *out_function_return_function_parameter_value_kinds = NULL;
    }
    if (out_function_return_function_parameter_type_names) {
        *out_function_return_function_parameter_type_names = NULL;
    }
    if (out_function_return_function_parameter_return_types) {
        *out_function_return_function_parameter_return_types = NULL;
    }
    if (out_function_return_function_parameter_parameter_counts) {
        *out_function_return_function_parameter_parameter_counts = NULL;
    }
    if (out_function_return_function_parameter_parameter_value_kinds) {
        *out_function_return_function_parameter_parameter_value_kinds = NULL;
    }
    if (out_function_return_function_parameter_parameter_return_types) {
        *out_function_return_function_parameter_parameter_return_types = NULL;
    }
    if (out_function_return_function_parameter_parameter_parameter_counts) {
        *out_function_return_function_parameter_parameter_parameter_counts = NULL;
    }
    if (out_function_return_function_parameter_parameter_return_types) {
        *out_function_return_function_parameter_parameter_return_types = NULL;
    }
    if (out_function_return_function_parameter_parameter_parameter_counts) {
        *out_function_return_function_parameter_parameter_parameter_counts = NULL;
    }
    if (out_parameter_count) {
        *out_parameter_count = 0;
    }
    if (out_parameter_names) {
        *out_parameter_names = NULL;
    }
    if (out_parameter_value_kinds) {
        *out_parameter_value_kinds = NULL;
    }
    if (out_parameter_type_names) {
        *out_parameter_type_names = NULL;
    }
    if (out_parameter_array_ranks) {
        *out_parameter_array_ranks = NULL;
    }
    if (out_parameter_array_extent_exprs) {
        *out_parameter_array_extent_exprs = NULL;
    }
    if (out_parameter_function_return_types) {
        *out_parameter_function_return_types = NULL;
    }
    if (out_parameter_function_parameter_counts) {
        *out_parameter_function_parameter_counts = NULL;
    }
    if (out_parameter_function_parameter_value_kinds) {
        *out_parameter_function_parameter_value_kinds = NULL;
    }
    if (out_parameter_function_parameter_return_types) {
        *out_parameter_function_parameter_return_types = NULL;
    }
    if (out_parameter_function_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_counts = NULL;
    }
    if (out_parameter_function_parameter_parameter_value_kinds) {
        *out_parameter_function_parameter_parameter_value_kinds = NULL;
    }
    if (out_parameter_function_parameter_parameter_return_types) {
        *out_parameter_function_parameter_parameter_return_types = NULL;
    }
    if (out_parameter_function_parameter_parameter_parameter_counts) {
        *out_parameter_function_parameter_parameter_parameter_counts = NULL;
    }
    if (out_parameter_is_const) {
        *out_parameter_is_const = NULL;
    }
    if (out_parameter_name_lines) {
        *out_parameter_name_lines = NULL;
    }
    if (out_parameter_name_columns) {
        *out_parameter_name_columns = NULL;
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

    if (!parse_function_external_signature(p,
        capture_parameter_metadata,
        &signature)) {
        return 0;
    }

    if (out_name_token) {
        *out_name_token = signature.name_tok;
    }
    if (out_return_type) {
        *out_return_type = signature.return_type;
    }
    if (out_return_type_name) {
        *out_return_type_name = signature.return_type_name;
    } else {
        free(signature.return_type_name);
    }
    signature.return_type_name = NULL;
    if (out_has_function_return_signature) {
        *out_has_function_return_signature = signature.has_function_return_signature;
    }
    if (out_function_return_function_return_type) {
        *out_function_return_function_return_type = signature.function_return_function_return_type;
    }
    if (out_function_return_function_return_type_name) {
        *out_function_return_function_return_type_name =
            signature.function_return_function_return_type_name;
    } else {
        free(signature.function_return_function_return_type_name);
    }
    signature.function_return_function_return_type_name = NULL;
    if (out_function_return_function_parameter_count) {
        *out_function_return_function_parameter_count =
            signature.function_return_function_parameter_count;
    }
    if (out_function_return_function_parameter_value_kinds) {
        *out_function_return_function_parameter_value_kinds =
            signature.function_return_function_parameter_value_kinds;
    } else {
        free(signature.function_return_function_parameter_value_kinds);
    }
    signature.function_return_function_parameter_value_kinds = NULL;
    if (out_function_return_function_parameter_type_names) {
        *out_function_return_function_parameter_type_names =
            signature.function_return_function_parameter_type_names;
    } else {
        free_name_array(signature.function_return_function_parameter_type_names,
            signature.function_return_function_parameter_count);
    }
    signature.function_return_function_parameter_type_names = NULL;
    if (out_function_return_function_parameter_return_types) {
        *out_function_return_function_parameter_return_types =
            signature.function_return_function_parameter_return_types;
    } else {
        free(signature.function_return_function_parameter_return_types);
    }
    signature.function_return_function_parameter_return_types = NULL;
    if (out_function_return_function_parameter_parameter_counts) {
        *out_function_return_function_parameter_parameter_counts =
            signature.function_return_function_parameter_parameter_counts;
    } else {
        free(signature.function_return_function_parameter_parameter_counts);
    }
    signature.function_return_function_parameter_parameter_counts = NULL;
    if (out_function_return_function_parameter_parameter_value_kinds) {
        *out_function_return_function_parameter_parameter_value_kinds =
            signature.function_return_function_parameter_parameter_value_kinds;
    } else {
        if (signature.function_return_function_parameter_parameter_value_kinds) {
            size_t i;
            for (i = 0u; i < signature.function_return_function_parameter_count; ++i) {
                free(signature.function_return_function_parameter_parameter_value_kinds[i]);
            }
        }
        free(signature.function_return_function_parameter_parameter_value_kinds);
    }
    signature.function_return_function_parameter_parameter_value_kinds = NULL;
    if (out_function_return_function_parameter_parameter_return_types) {
        *out_function_return_function_parameter_parameter_return_types =
            signature.function_return_function_parameter_parameter_return_types;
    } else {
        if (signature.function_return_function_parameter_parameter_return_types) {
            size_t i;
            for (i = 0u; i < signature.function_return_function_parameter_count; ++i) {
                free(signature.function_return_function_parameter_parameter_return_types[i]);
            }
        }
        free(signature.function_return_function_parameter_parameter_return_types);
    }
    signature.function_return_function_parameter_parameter_return_types = NULL;
    if (out_function_return_function_parameter_parameter_parameter_counts) {
        *out_function_return_function_parameter_parameter_parameter_counts =
            signature.function_return_function_parameter_parameter_parameter_counts;
    } else {
        if (signature.function_return_function_parameter_parameter_parameter_counts) {
            size_t i;
            for (i = 0u; i < signature.function_return_function_parameter_count; ++i) {
                free(signature.function_return_function_parameter_parameter_parameter_counts[i]);
            }
        }
        free(signature.function_return_function_parameter_parameter_parameter_counts);
    }
    signature.function_return_function_parameter_parameter_parameter_counts = NULL;
    if (out_parameter_count) {
        *out_parameter_count = signature.parameter_count;
    }

    if (match(p, TOKEN_SEMICOLON)) {
        parser_commit_parameter_names(out_parameter_names,
            out_parameter_value_kinds,
            out_parameter_type_names,
            out_parameter_array_ranks,
            out_parameter_array_extent_exprs,
            out_parameter_function_return_types,
            out_parameter_function_parameter_counts,
            out_parameter_function_parameter_value_kinds,
            out_parameter_function_parameter_return_types,
            out_parameter_function_parameter_parameter_counts,
            out_parameter_function_parameter_parameter_value_kinds,
            out_parameter_function_parameter_parameter_return_types,
            out_parameter_function_parameter_parameter_parameter_counts,
            out_parameter_is_const,
            out_parameter_name_lines,
            out_parameter_name_columns,
            signature.parameter_names,
            signature.parameter_value_kinds,
            signature.parameter_type_names,
            signature.parameter_array_ranks,
            signature.parameter_array_extent_exprs,
            signature.parameter_function_return_types,
            signature.parameter_function_parameter_counts,
            signature.parameter_function_parameter_value_kinds,
            signature.parameter_function_parameter_return_types,
            signature.parameter_function_parameter_parameter_counts,
            signature.parameter_function_parameter_parameter_value_kinds,
            signature.parameter_function_parameter_parameter_return_types,
            signature.parameter_function_parameter_parameter_parameter_counts,
            signature.parameter_is_const,
            signature.parameter_name_lines,
            signature.parameter_name_columns,
            signature.parameter_count);
        signature.parameter_names = NULL;
        signature.parameter_value_kinds = NULL;
        signature.parameter_type_names = NULL;
        signature.parameter_array_ranks = NULL;
        signature.parameter_array_extent_exprs = NULL;
        signature.parameter_function_return_types = NULL;
        signature.parameter_function_parameter_counts = NULL;
        signature.parameter_function_parameter_value_kinds = NULL;
        signature.parameter_function_parameter_return_types = NULL;
        signature.parameter_function_parameter_parameter_counts = NULL;
        signature.parameter_function_parameter_parameter_value_kinds = NULL;
        signature.parameter_function_parameter_parameter_return_types = NULL;
        signature.parameter_function_parameter_parameter_parameter_counts = NULL;
        signature.parameter_is_const = NULL;
        signature.parameter_name_lines = NULL;
        signature.parameter_name_columns = NULL;
        if (out_is_definition) {
            *out_is_definition = 0;
        }
        return 1;
    }

    if (check(p, TOKEN_LBRACE)) {
        if (out_is_definition) {
            *out_is_definition = 1;
        }
        body_ok = parse_function_external_definition_body(p,
            &signature,
            out_function_body,
            out_return_statement_count);
        if (body_ok) {
            parser_commit_parameter_names(out_parameter_names,
                out_parameter_value_kinds,
                out_parameter_type_names,
                out_parameter_array_ranks,
                out_parameter_array_extent_exprs,
                out_parameter_function_return_types,
                out_parameter_function_parameter_counts,
                out_parameter_function_parameter_value_kinds,
                out_parameter_function_parameter_return_types,
                out_parameter_function_parameter_parameter_counts,
                out_parameter_function_parameter_parameter_value_kinds,
                out_parameter_function_parameter_parameter_return_types,
                out_parameter_function_parameter_parameter_parameter_counts,
                out_parameter_is_const,
                out_parameter_name_lines,
                out_parameter_name_columns,
                signature.parameter_names,
                signature.parameter_value_kinds,
                signature.parameter_type_names,
                signature.parameter_array_ranks,
                signature.parameter_array_extent_exprs,
                signature.parameter_function_return_types,
                signature.parameter_function_parameter_counts,
                signature.parameter_function_parameter_value_kinds,
                signature.parameter_function_parameter_return_types,
                signature.parameter_function_parameter_parameter_counts,
                signature.parameter_function_parameter_parameter_value_kinds,
                signature.parameter_function_parameter_parameter_return_types,
                signature.parameter_function_parameter_parameter_parameter_counts,
                signature.parameter_is_const,
                signature.parameter_name_lines,
                signature.parameter_name_columns,
                signature.parameter_count);
            signature.parameter_names = NULL;
            signature.parameter_value_kinds = NULL;
            signature.parameter_type_names = NULL;
            signature.parameter_array_ranks = NULL;
            signature.parameter_array_extent_exprs = NULL;
            signature.parameter_function_return_types = NULL;
            signature.parameter_function_parameter_counts = NULL;
            signature.parameter_function_parameter_value_kinds = NULL;
            signature.parameter_function_parameter_return_types = NULL;
            signature.parameter_function_parameter_parameter_counts = NULL;
            signature.parameter_function_parameter_parameter_value_kinds = NULL;
            signature.parameter_function_parameter_parameter_return_types = NULL;
            signature.parameter_function_parameter_parameter_parameter_counts = NULL;
            signature.parameter_is_const = NULL;
            signature.parameter_name_lines = NULL;
            signature.parameter_name_columns = NULL;
        } else {
            parsed_function_signature_release(&signature);
        }

        return body_ok;
    }

    set_error(p,
        peek(p),
        "Expected function body '{' or declaration terminator ';', got %s",
        lexer_token_type_name(peek(p)->type));
    parsed_function_signature_release(&signature);
    return 0;
}

static int parser_ast_program_add_struct_type(Parser *p,
    AstProgram *program,
    const Token *name_tok,
    char **field_names,
    int *field_value_kinds,
    char **field_type_names,
    size_t field_count) {
    AstStructType *new_types;
    AstStructType *entry;
    size_t next_capacity;

    if (!program || !name_tok) {
        free_name_array(field_names, field_count);
        free(field_value_kinds);
        free_name_array(field_type_names, field_count);
        return 0;
    }

    if (program->struct_type_count == program->struct_type_capacity) {
        if (!parser_next_growth_capacity(program->struct_type_capacity,
                8,
                sizeof(AstStructType),
                &next_capacity)) {
            set_error(p, name_tok, "Out of memory while building AST");
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            return 0;
        }
        new_types = (AstStructType *)realloc(program->struct_types, next_capacity * sizeof(AstStructType));
        if (!new_types) {
            set_error(p, name_tok, "Out of memory while building AST");
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            return 0;
        }
        program->struct_types = new_types;
        program->struct_type_capacity = next_capacity;
    }

    entry = &program->struct_types[program->struct_type_count++];
    entry->name = copy_token_identifier_lexeme(name_tok);
    if (!entry->name) {
        set_error(p, name_tok, "Out of memory while building AST");
        --program->struct_type_count;
        free_name_array(field_names, field_count);
        free(field_value_kinds);
        free_name_array(field_type_names, field_count);
        return 0;
    }
    entry->name_length = name_tok->length;
    entry->field_names = field_names;
    entry->field_value_kinds = field_value_kinds;
    entry->field_type_names = field_type_names;
    entry->field_count = field_count;
    return 1;
}

static int parse_struct_type_definition(Parser *p, AstProgram *out_program) {
    const Token *struct_name_tok = NULL;
    char **field_names = NULL;
    int *field_value_kinds = NULL;
    char **field_type_names = NULL;
    size_t field_count = 0u;
    size_t field_capacity = 0u;

    if (!consume(p, TOKEN_KW_STRUCT, "'struct'")) {
        return 0;
    }
    if (!consume_token(p, TOKEN_IDENTIFIER, "struct type name", &struct_name_tok)) {
        return 0;
    }
    if (!consume(p, TOKEN_LBRACE, "'{'")) {
        return 0;
    }

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        int field_value_kind = AST_DECLARATION_VALUE_INT;
        const Token *field_type_name_tok = NULL;
        const Token *field_name_tok = NULL;
        char *field_name_copy = NULL;
        char *field_type_name_copy = NULL;
        int *new_field_value_kinds = NULL;
        char **new_field_type_names = NULL;
        size_t next_capacity = 0u;

        if (match(p, TOKEN_KW_INT)) {
            field_value_kind = AST_DECLARATION_VALUE_INT;
        } else if (match(p, TOKEN_KW_PAIR)) {
            field_value_kind = AST_DECLARATION_VALUE_PAIR;
        } else if (match(p, TOKEN_KW_STRUCT)) {
            field_value_kind = AST_DECLARATION_VALUE_STRUCT;
            if (!consume_token(p, TOKEN_IDENTIFIER, "struct field type name", &field_type_name_tok)) {
                free_name_array(field_names, field_count);
                free(field_value_kinds);
                free_name_array(field_type_names, field_count);
                return 0;
            }
            field_type_name_copy = copy_token_identifier_lexeme(field_type_name_tok);
            if (!field_type_name_copy) {
                free_name_array(field_names, field_count);
                free(field_value_kinds);
                free_name_array(field_type_names, field_count);
                set_error(p, field_type_name_tok, "Out of memory while building AST");
                return 0;
            }
        } else {
            set_error(p, peek(p), "Expected struct field type 'int', 'pair', or 'struct'");
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            return 0;
        }
        if (!consume_token(p, TOKEN_IDENTIFIER, "field name", &field_name_tok)) {
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            free(field_type_name_copy);
            return 0;
        }
        field_name_copy = copy_token_identifier_lexeme(field_name_tok);
        if (!field_name_copy || !append_name_array_item(
                p,
                &field_names,
                &field_count,
                &field_capacity,
                field_name_copy,
                field_name_tok)) {
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            free(field_type_name_copy);
            return 0;
        }
        next_capacity = field_capacity > 0u ? field_capacity : field_count;
        new_field_value_kinds = field_value_kinds
            ? (int *)realloc(field_value_kinds, next_capacity * sizeof(int))
            : (int *)malloc(next_capacity * sizeof(int));
        new_field_type_names = field_type_names
            ? (char **)realloc(field_type_names, next_capacity * sizeof(char *))
            : (char **)malloc(next_capacity * sizeof(char *));
        if (!new_field_value_kinds || !new_field_type_names) {
            if (new_field_value_kinds && !field_value_kinds) {
                free(new_field_value_kinds);
            }
            if (new_field_type_names && !field_type_names) {
                free(new_field_type_names);
            }
            set_error(p, field_name_tok, "Out of memory while building AST");
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            free(field_type_name_copy);
            return 0;
        }
        field_value_kinds = new_field_value_kinds;
        field_type_names = new_field_type_names;
        field_value_kinds[field_count - 1u] = field_value_kind;
        field_type_names[field_count - 1u] = field_type_name_copy;
        if (!consume(p, TOKEN_SEMICOLON, "';'")) {
            free_name_array(field_names, field_count);
            free(field_value_kinds);
            free_name_array(field_type_names, field_count);
            return 0;
        }
    }

    if (!consume(p, TOKEN_RBRACE, "'}'") || !consume(p, TOKEN_SEMICOLON, "';'")) {
        free_name_array(field_names, field_count);
        free(field_value_kinds);
        free_name_array(field_type_names, field_count);
        return 0;
    }

    if (out_program) {
        return parser_ast_program_add_struct_type(
            p,
            out_program,
            struct_name_tok,
            field_names,
            field_value_kinds,
            field_type_names,
            field_count);
    }

    free_name_array(field_names, field_count);
    free(field_value_kinds);
    free_name_array(field_type_names, field_count);
    return 1;
}

static int parser_prepare(Parser *p, const TokenArray *tokens, ParserError *error) {
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

    p->tokens = tokens;
    p->current = 0;
    p->error = error;
    p->has_error = 0;
    p->error_index = 0;
    p->expression_recursion_depth = 0;
    p->expression_recursion_limit = PARSER_EXPRESSION_RECURSION_LIMIT;
    p->statement_recursion_depth = 0;
    p->statement_recursion_limit = PARSER_STATEMENT_RECURSION_LIMIT;
    p->loop_depth = 0;

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    return 1;
}

static int parser_translation_unit_starts_like_function_external(const Parser *p, size_t start) {
    size_t index;

    if (!p || !p->tokens || !p->tokens->data || start >= p->tokens->size) {
        return 0;
    }

    index = start;
    if (p->tokens->data[index].type == TOKEN_KW_CONST) {
        ++index;
    }
    if (index >= p->tokens->size) {
        return 0;
    }

    if (p->tokens->data[index].type == TOKEN_KW_STRUCT) {
        if (index + 2u >= p->tokens->size) {
            return 0;
        }
        index += 2u;
    } else if (p->tokens->data[index].type == TOKEN_KW_INT ||
        p->tokens->data[index].type == TOKEN_KW_FLOAT ||
        p->tokens->data[index].type == TOKEN_KW_PAIR) {
        ++index;
    } else {
        return 0;
    }

    if (index + 1u >= p->tokens->size) {
        return 0;
    }

    return p->tokens->data[index].type == TOKEN_IDENTIFIER &&
        p->tokens->data[index + 1u].type == TOKEN_LPAREN;
}

static int parse_translation_unit_external_or_declaration(Parser *p, AstProgram *out_program) {
    size_t start = p->current;
    const Token *external_name = NULL;
    AstFunctionReturnType external_return_type = AST_FUNCTION_RETURN_INT;
    char *external_return_type_name = NULL;
    int external_has_function_return_signature = 0;
    AstFunctionReturnType external_function_return_function_return_type = AST_FUNCTION_RETURN_INT;
    char *external_function_return_function_return_type_name = NULL;
    size_t external_function_return_function_parameter_count = 0u;
    int *external_function_return_function_parameter_value_kinds = NULL;
    char **external_function_return_function_parameter_type_names = NULL;
    AstFunctionReturnType *external_function_return_function_parameter_return_types = NULL;
    size_t *external_function_return_function_parameter_parameter_counts = NULL;
    int **external_function_return_function_parameter_parameter_value_kinds = NULL;
    AstFunctionReturnType **external_function_return_function_parameter_parameter_return_types = NULL;
    size_t **external_function_return_function_parameter_parameter_parameter_counts = NULL;
    size_t external_parameter_count = 0;
    char **external_parameter_names = NULL;
    int *external_parameter_value_kinds = NULL;
    char **external_parameter_type_names = NULL;
    size_t *external_parameter_array_ranks = NULL;
    AstExpression ***external_parameter_array_extent_exprs = NULL;
    AstFunctionReturnType *external_parameter_function_return_types = NULL;
    size_t *external_parameter_function_parameter_counts = NULL;
    int **external_parameter_function_parameter_value_kinds = NULL;
    AstFunctionReturnType **external_parameter_function_parameter_return_types = NULL;
    size_t **external_parameter_function_parameter_parameter_counts = NULL;
    int ***external_parameter_function_parameter_parameter_value_kinds = NULL;
    AstFunctionReturnType ***external_parameter_function_parameter_parameter_return_types = NULL;
    size_t ***external_parameter_function_parameter_parameter_parameter_counts = NULL;
    int *external_parameter_is_const = NULL;
    int *external_parameter_name_lines = NULL;
    int *external_parameter_name_columns = NULL;
    int external_is_definition = 0;
    AstStatement *external_function_body = NULL;
    size_t external_return_statement_count = 0;

    if (check(p, TOKEN_KW_STRUCT) &&
        p->current + 2u < p->tokens->size &&
        p->tokens->data[p->current + 1u].type == TOKEN_IDENTIFIER &&
        p->tokens->data[p->current + 2u].type == TOKEN_LBRACE) {
        return parse_struct_type_definition(p, out_program);
    }

    if (!parse_function_external(p,
        out_program ? &external_name : NULL,
        out_program ? &external_return_type : NULL,
        out_program ? &external_return_type_name : NULL,
        out_program ? &external_has_function_return_signature : NULL,
        out_program ? &external_function_return_function_return_type : NULL,
        out_program ? &external_function_return_function_return_type_name : NULL,
        out_program ? &external_function_return_function_parameter_count : NULL,
        out_program ? &external_function_return_function_parameter_value_kinds : NULL,
        out_program ? &external_function_return_function_parameter_type_names : NULL,
        out_program ? &external_function_return_function_parameter_return_types : NULL,
        out_program ? &external_function_return_function_parameter_parameter_counts : NULL,
        out_program ? &external_function_return_function_parameter_parameter_value_kinds : NULL,
        out_program ? &external_function_return_function_parameter_parameter_return_types : NULL,
        out_program ? &external_function_return_function_parameter_parameter_parameter_counts : NULL,
        out_program ? &external_parameter_count : NULL,
        out_program ? &external_parameter_names : NULL,
        out_program ? &external_parameter_value_kinds : NULL,
        out_program ? &external_parameter_type_names : NULL,
        out_program ? &external_parameter_array_ranks : NULL,
        out_program ? &external_parameter_array_extent_exprs : NULL,
        out_program ? &external_parameter_function_return_types : NULL,
        out_program ? &external_parameter_function_parameter_counts : NULL,
        out_program ? &external_parameter_function_parameter_value_kinds : NULL,
        out_program ? &external_parameter_function_parameter_return_types : NULL,
        out_program ? &external_parameter_function_parameter_parameter_counts : NULL,
        out_program ? &external_parameter_function_parameter_parameter_value_kinds : NULL,
        out_program ? &external_parameter_function_parameter_parameter_return_types : NULL,
        out_program ? &external_parameter_function_parameter_parameter_parameter_counts : NULL,
        out_program ? &external_parameter_is_const : NULL,
        out_program ? &external_parameter_name_lines : NULL,
        out_program ? &external_parameter_name_columns : NULL,
        out_program ? &external_is_definition : NULL,
        out_program ? &external_function_body : NULL,
        out_program ? &external_return_statement_count : NULL)) {
        if (check(p, TOKEN_KW_FLOAT)) {
            set_error(p, peek(p), "'float' declarations are not supported in this extension slice");
            return 0;
        }
        if (parser_translation_unit_starts_like_function_external(p, start)) {
            return 0;
        }
        p->current = start;
        return parse_declaration(
            p,
            out_program,
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
            NULL);
    }

    if (!out_program) {
        return 1;
    }

    if (!parser_ast_program_add_external(out_program, AST_EXTERNAL_FUNCTION, external_name)) {
        free_name_array(external_parameter_names, external_parameter_count);
        free(external_parameter_value_kinds);
        free_name_array(external_parameter_type_names, external_parameter_count);
        free(external_return_type_name);
        free(external_function_return_function_return_type_name);
        free(external_function_return_function_parameter_value_kinds);
        free_name_array(external_function_return_function_parameter_type_names,
            external_function_return_function_parameter_count);
        free(external_function_return_function_parameter_return_types);
        free(external_function_return_function_parameter_parameter_counts);
        if (external_function_return_function_parameter_parameter_value_kinds) {
            size_t i;
            for (i = 0u; i < external_function_return_function_parameter_count; ++i) {
                free(external_function_return_function_parameter_parameter_value_kinds[i]);
            }
        }
        free(external_function_return_function_parameter_parameter_value_kinds);
        if (external_function_return_function_parameter_parameter_return_types) {
            size_t i;
            for (i = 0u; i < external_function_return_function_parameter_count; ++i) {
                free(external_function_return_function_parameter_parameter_return_types[i]);
            }
        }
        free(external_function_return_function_parameter_parameter_return_types);
        if (external_function_return_function_parameter_parameter_parameter_counts) {
            size_t i;
            for (i = 0u; i < external_function_return_function_parameter_count; ++i) {
                free(external_function_return_function_parameter_parameter_parameter_counts[i]);
            }
        }
        free(external_function_return_function_parameter_parameter_parameter_counts);
        free(external_parameter_array_ranks);
        free(external_parameter_array_extent_exprs);
        free(external_parameter_function_return_types);
        free(external_parameter_function_parameter_counts);
        if (external_parameter_function_parameter_value_kinds) {
            size_t i;
            for (i = 0u; i < external_parameter_count; ++i) {
                free(external_parameter_function_parameter_value_kinds[i]);
            }
        }
        free(external_parameter_function_parameter_value_kinds);
        if (external_parameter_function_parameter_return_types) {
            size_t i;
            for (i = 0u; i < external_parameter_count; ++i) {
                free(external_parameter_function_parameter_return_types[i]);
            }
        }
        free(external_parameter_function_parameter_return_types);
        if (external_parameter_function_parameter_parameter_counts) {
            size_t i;
            for (i = 0u; i < external_parameter_count; ++i) {
                free(external_parameter_function_parameter_parameter_counts[i]);
            }
        }
        free(external_parameter_function_parameter_parameter_counts);
        free(external_parameter_is_const);
        free(external_parameter_name_lines);
        free(external_parameter_name_columns);
        parser_ast_statement_free(external_function_body);
        set_error(p, peek(p), "Out of memory while building AST");
        return 0;
    }

    out_program->externals[out_program->count - 1].parameter_count = external_parameter_count;
    out_program->externals[out_program->count - 1].function_return_type = external_return_type;
    out_program->externals[out_program->count - 1].function_return_type_name = external_return_type_name;
    out_program->externals[out_program->count - 1].has_function_return_signature =
        external_has_function_return_signature;
    out_program->externals[out_program->count - 1].function_return_function_return_type =
        external_function_return_function_return_type;
    out_program->externals[out_program->count - 1].function_return_function_return_type_name =
        external_function_return_function_return_type_name;
    out_program->externals[out_program->count - 1].function_return_function_parameter_count =
        external_function_return_function_parameter_count;
    out_program->externals[out_program->count - 1].function_return_function_parameter_value_kinds =
        external_function_return_function_parameter_value_kinds;
    out_program->externals[out_program->count - 1].function_return_function_parameter_type_names =
        external_function_return_function_parameter_type_names;
    out_program->externals[out_program->count - 1].function_return_function_parameter_return_types =
        external_function_return_function_parameter_return_types;
    out_program->externals[out_program->count - 1].function_return_function_parameter_parameter_counts =
        external_function_return_function_parameter_parameter_counts;
    out_program->externals[out_program->count - 1]
        .function_return_function_parameter_parameter_value_kinds =
        external_function_return_function_parameter_parameter_value_kinds;
    out_program->externals[out_program->count - 1]
        .function_return_function_parameter_parameter_return_types =
        external_function_return_function_parameter_parameter_return_types;
    out_program->externals[out_program->count - 1]
        .function_return_function_parameter_parameter_parameter_counts =
        external_function_return_function_parameter_parameter_parameter_counts;
    out_program->externals[out_program->count - 1].parameter_names = external_parameter_names;
    out_program->externals[out_program->count - 1].parameter_value_kinds = external_parameter_value_kinds;
    out_program->externals[out_program->count - 1].parameter_type_names = external_parameter_type_names;
    out_program->externals[out_program->count - 1].parameter_array_ranks = external_parameter_array_ranks;
    out_program->externals[out_program->count - 1].parameter_array_extent_exprs =
        external_parameter_array_extent_exprs;
    out_program->externals[out_program->count - 1].parameter_function_return_types =
        external_parameter_function_return_types;
    out_program->externals[out_program->count - 1].parameter_function_parameter_counts =
        external_parameter_function_parameter_counts;
    out_program->externals[out_program->count - 1].parameter_function_parameter_value_kinds =
        external_parameter_function_parameter_value_kinds;
    out_program->externals[out_program->count - 1].parameter_function_parameter_return_types =
        external_parameter_function_parameter_return_types;
    out_program->externals[out_program->count - 1].parameter_function_parameter_parameter_counts =
        external_parameter_function_parameter_parameter_counts;
    out_program->externals[out_program->count - 1].parameter_function_parameter_parameter_value_kinds =
        external_parameter_function_parameter_parameter_value_kinds;
    out_program->externals[out_program->count - 1].parameter_function_parameter_parameter_return_types =
        external_parameter_function_parameter_parameter_return_types;
    out_program->externals[out_program->count - 1].parameter_function_parameter_parameter_parameter_counts =
        external_parameter_function_parameter_parameter_parameter_counts;
    out_program->externals[out_program->count - 1].parameter_is_const = external_parameter_is_const;
    out_program->externals[out_program->count - 1].parameter_name_lines =
        external_parameter_name_lines;
    out_program->externals[out_program->count - 1].parameter_name_columns =
        external_parameter_name_columns;
    out_program->externals[out_program->count - 1].is_function_definition = external_is_definition;
    out_program->externals[out_program->count - 1].function_body = external_function_body;
    out_program->externals[out_program->count - 1].return_statement_count =
        external_return_statement_count;
    return 1;
}

int parser_parse_translation_unit_ast(const TokenArray *tokens,
    AstProgram *out_program,
    ParserError *error) {
    Parser p;

    if (!out_program) {
        if (error) {
            error->line = 0;
            error->column = 0;
            snprintf(error->message, sizeof(error->message), "Output AST is NULL");
        }
        return 0;
    }

    /* Keep state deterministic: on every call we first clear previous AST. */
    parser_ast_program_free(out_program);

    if (!parser_prepare(&p, tokens, error)) {
        return 0;
    }

    while (!is_at_end(&p)) {
        if (!parse_translation_unit_external_or_declaration(&p, out_program)) {
            parser_ast_program_free(out_program);
            return 0;
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

    if (!parser_prepare(&p, tokens, error)) {
        return 0;
    }

    while (!is_at_end(&p)) {
        if (!parse_translation_unit_external_or_declaration(&p, NULL)) {
            return 0;
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

    if (!parser_prepare(&p, tokens, error)) {
        return 0;
    }

    expr = parse_expression_ast_comma(&p);
    if (!expr) {
        return 0;
    }

    if (!is_at_end(&p)) {
        set_error(&p, peek(&p), "Expected EOF, got %s", lexer_token_type_name(peek(&p)->type));
        parser_ast_expression_free(expr);
        return 0;
    }

    if (out_expression) {
        *out_expression = expr;
    } else {
        parser_ast_expression_free(expr);
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }
    return 1;
}

#endif
