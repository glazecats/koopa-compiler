#if defined(__INTELLISENSE__) && !defined(PARSER_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(PARSER_SPLIT_AGGREGATOR)
#define PARSER_SPLIT_INTELLISENSE_CONTEXT
#include "parser.c"
#else

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
        if (check(p, TOKEN_KW_INT)) {
            const Token *decl_tok = peek(p);
            AstStatement *decl_stmt = NULL;
            char **decl_names = NULL;
            size_t decl_name_count = 0;
            AstExpression **decl_init_exprs = NULL;
            size_t decl_init_expr_count = 0;

            if (!parse_declaration(p,
                NULL,
                out_statement ? &decl_names : NULL,
                out_statement ? &decl_name_count : NULL,
                out_statement ? &decl_init_exprs : NULL,
                out_statement ? &decl_init_expr_count : NULL)) {
                free_ast_statement_array(children, child_count);
                return 0;
            }
            if (out_statement) {
                decl_stmt = alloc_ast_statement(p, AST_STMT_DECLARATION, decl_tok);
                if (!decl_stmt) {
                    free_name_array(decl_names, decl_name_count);
                    free_ast_expression_array(decl_init_exprs, decl_init_expr_count);
                    free_ast_statement_array(children, child_count);
                    return 0;
                }
                decl_stmt->declaration_names = decl_names;
                decl_stmt->declaration_name_count = decl_name_count;
                decl_stmt->expressions = decl_init_exprs;
                decl_stmt->expression_count = decl_init_expr_count;
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
    AstExpression **for_init_decl_exprs = NULL;
    size_t for_init_decl_expr_count = 0;
    char **for_init_decl_names = NULL;
    size_t for_init_decl_name_count = 0;
    const Token *for_tok = previous(p);

    if (out_statement) {
        *out_statement = NULL;
    }
    parser_local_control_set(out_local_control, 1, 0);
    parser_local_control_set(&body_local_control, 1, 0);

    if (!consume(p, TOKEN_LPAREN, "'('") ) {
        return 0;
    }

    if (match(p, TOKEN_KW_INT)) {
        p->current--;
        if (!parse_declaration(p,
            NULL,
            out_statement ? &for_init_decl_names : NULL,
            out_statement ? &for_init_decl_name_count : NULL,
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
                return 0;
            }
            init_end = p->current;
            if (!build_expression_ast_from_slice(p,
                init_start,
                init_end,
                out_statement ? &init_expr : NULL,
                for_tok)) {
                free_name_array(for_init_decl_names, for_init_decl_name_count);
                return 0;
            }
        }
        if (!consume(p, TOKEN_SEMICOLON, "';'")) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            return 0;
        }
    }

    if (!check(p, TOKEN_SEMICOLON)) {
        cond_start = p->current;
        if (!parse_expression(p)) {
            free_name_array(for_init_decl_names, for_init_decl_name_count);
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
            free_ast_expression_array(for_init_decl_exprs, for_init_decl_expr_count);
            parser_ast_expression_free(init_expr);
            parser_ast_expression_free(cond_expr);
            return 0;
        }
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        free_name_array(for_init_decl_names, for_init_decl_name_count);
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
            parser_ast_statement_free(for_stmt);
            parser_ast_statement_free(body_stmt);
            return 0;
        }
        if (step_expr) {
            for_stmt->has_for_step_expression = 1;
            for_stmt->for_step_expression_index = for_stmt->expression_count - 1;
        }
        for_stmt->declaration_names = for_init_decl_names;
        for_stmt->declaration_name_count = for_init_decl_name_count;
        for_stmt->children[0] = body_stmt;
        for_stmt->child_count = 1;
        *out_statement = for_stmt;
    } else {
        free_name_array(for_init_decl_names, for_init_decl_name_count);
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

    ok = parse_expression_statement(p, out_statement);
    leave_statement_recursion(p);
    if (ok) {
        parser_local_control_set(out_local_control, 1, 0);
    }
    return ok;
}

static int parse_init_declarator(Parser *p,
    const Token **out_ident,
    int *out_has_initializer,
    size_t *out_initializer_start,
    size_t *out_initializer_end) {
    if (out_has_initializer) {
        *out_has_initializer = 0;
    }
    if (out_initializer_start) {
        *out_initializer_start = 0;
    }
    if (out_initializer_end) {
        *out_initializer_end = 0;
    }

    if (!consume_token(p, TOKEN_IDENTIFIER, "identifier", out_ident)) {
        return 0;
    }
    if (match(p, TOKEN_ASSIGN)) {
        size_t init_start = p->current;

        if (out_has_initializer) {
            *out_has_initializer = 1;
        }
        if (!parse_initializer_expression(p)) {
            return 0;
        }
        if (out_initializer_start) {
            *out_initializer_start = init_start;
        }
        if (out_initializer_end) {
            *out_initializer_end = p->current;
        }
    }
    return 1;
}

static int parse_declaration(Parser *p,
    AstProgram *top_level_program,
    char ***out_decl_names,
    size_t *out_decl_name_count,
    AstExpression ***out_decl_initializer_exprs,
    size_t *out_decl_initializer_expr_count) {
    const Token *ident = NULL;
    int has_initializer = 0;
    size_t initializer_start = 0;
    size_t initializer_end = 0;
    AstExpression *external_initializer_expr = NULL;
    char **decl_names = NULL;
    size_t decl_name_count = 0;
    size_t decl_name_capacity = 0;
    AstExpression **decl_initializer_exprs = NULL;
    size_t decl_initializer_expr_count = 0;
    size_t decl_initializer_expr_capacity = 0;

    if (out_decl_names) {
        *out_decl_names = NULL;
    }
    if (out_decl_name_count) {
        *out_decl_name_count = 0;
    }
    if (out_decl_initializer_exprs) {
        *out_decl_initializer_exprs = NULL;
    }
    if (out_decl_initializer_expr_count) {
        *out_decl_initializer_expr_count = 0;
    }

    if (!consume(p, TOKEN_KW_INT, "'int'")) {
        return 0;
    }
    if (!parse_init_declarator(p,
        &ident,
        &has_initializer,
        &initializer_start,
        &initializer_end)) {
        free_name_array(decl_names, decl_name_count);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }
    if (top_level_program && has_initializer &&
        !build_expression_ast_from_slice(p,
            initializer_start,
            initializer_end,
            &external_initializer_expr,
            ident)) {
        free_name_array(decl_names, decl_name_count);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }
    if (out_decl_initializer_exprs) {
        AstExpression *initializer_expr = NULL;
        if (has_initializer &&
            !build_expression_ast_from_slice(p,
                initializer_start,
                initializer_end,
                &initializer_expr,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (!append_ast_expression_slot(p,
            &decl_initializer_exprs,
            &decl_initializer_expr_count,
            &decl_initializer_expr_capacity,
            initializer_expr,
            ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
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
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
    }
    if (top_level_program && !parser_ast_program_add_external(top_level_program, AST_EXTERNAL_DECLARATION, ident)) {
        set_error(p, peek(p), "Out of memory while building AST");
        parser_ast_expression_free(external_initializer_expr);
        free_name_array(decl_names, decl_name_count);
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
        return 0;
    }
    if (top_level_program) {
        top_level_program->externals[top_level_program->count - 1].has_initializer = has_initializer;
        top_level_program->externals[top_level_program->count - 1].declaration_initializer =
            external_initializer_expr;
        external_initializer_expr = NULL;
    }

    while (match(p, TOKEN_COMMA)) {
        if (!parse_init_declarator(p,
            &ident,
            &has_initializer,
            &initializer_start,
            &initializer_end)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (top_level_program && has_initializer &&
            !build_expression_ast_from_slice(p,
                initializer_start,
                initializer_end,
                &external_initializer_expr,
                ident)) {
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (out_decl_initializer_exprs) {
            AstExpression *initializer_expr = NULL;
            if (has_initializer &&
                !build_expression_ast_from_slice(p,
                    initializer_start,
                    initializer_end,
                    &initializer_expr,
                    ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
            if (!append_ast_expression_slot(p,
                &decl_initializer_exprs,
                &decl_initializer_expr_count,
                &decl_initializer_expr_capacity,
                initializer_expr,
                ident)) {
                free_name_array(decl_names, decl_name_count);
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
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
                free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
                return 0;
            }
        }
        if (top_level_program &&
            !parser_ast_program_add_external(top_level_program, AST_EXTERNAL_DECLARATION, ident)) {
            set_error(p, peek(p), "Out of memory while building AST");
            parser_ast_expression_free(external_initializer_expr);
            free_name_array(decl_names, decl_name_count);
            free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
            return 0;
        }
        if (top_level_program) {
            top_level_program->externals[top_level_program->count - 1].has_initializer = has_initializer;
            top_level_program->externals[top_level_program->count - 1].declaration_initializer =
                external_initializer_expr;
            external_initializer_expr = NULL;
        }
    }

    if (!consume(p, TOKEN_SEMICOLON, "';'")) {
        free_name_array(decl_names, decl_name_count);
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

    if (out_decl_initializer_exprs) {
        *out_decl_initializer_exprs = decl_initializer_exprs;
        if (out_decl_initializer_expr_count) {
            *out_decl_initializer_expr_count = decl_initializer_expr_count;
        }
    } else {
        free_ast_expression_array(decl_initializer_exprs, decl_initializer_expr_count);
    }

    return 1;
}

static int parse_parameter_list(Parser *p,
    size_t *out_param_count,
    int *out_has_unnamed_parameter,
    char ***out_parameter_names) {
    size_t count = 0;
    size_t name_capacity = 0;
    char **parameter_names = NULL;

    if (out_param_count) {
        *out_param_count = 0;
    }
    if (out_has_unnamed_parameter) {
        *out_has_unnamed_parameter = 0;
    }
    if (out_parameter_names) {
        *out_parameter_names = NULL;
    }

    if (check(p, TOKEN_RPAREN)) {
        if (out_parameter_names) {
            *out_parameter_names = parameter_names;
        }
        return 1;
    }

    do {
        const Token *parameter_name_tok = NULL;

        if (!consume(p, TOKEN_KW_INT, "'int'")) {
            free_name_array(parameter_names, count);
            return 0;
        }
        /* C-style prototypes may omit parameter names: int f(int); */
        if (check(p, TOKEN_IDENTIFIER)) {
            consume_token(p, TOKEN_IDENTIFIER, "identifier", &parameter_name_tok);
        } else {
            if (out_has_unnamed_parameter) {
                *out_has_unnamed_parameter = 1;
            }
        }

        if (out_parameter_names) {
            char *parameter_name_copy = NULL;
            if (parameter_name_tok) {
                parameter_name_copy = copy_token_identifier_lexeme(parameter_name_tok);
                if (!parameter_name_copy) {
                    set_error(p,
                        parameter_name_tok,
                        "Out of memory while building function parameter metadata");
                    free_name_array(parameter_names, count);
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
                return 0;
            }
        } else {
            count++;
        }
    } while (match(p, TOKEN_COMMA));

    if (out_param_count) {
        *out_param_count = count;
    }

    if (out_parameter_names) {
        *out_parameter_names = parameter_names;
    }

    return 1;
}

typedef struct {
    const Token *name_tok;
    size_t parameter_count;
    char **parameter_names;
    int has_unnamed_parameter;
} ParsedFunctionSignature;

static void parsed_function_signature_init(ParsedFunctionSignature *signature) {
    if (!signature) {
        return;
    }

    signature->name_tok = NULL;
    signature->parameter_count = 0;
    signature->parameter_names = NULL;
    signature->has_unnamed_parameter = 0;
}

static void parsed_function_signature_release(ParsedFunctionSignature *signature) {
    if (!signature) {
        return;
    }

    free_name_array(signature->parameter_names, signature->parameter_count);
    parsed_function_signature_init(signature);
}

static void parser_commit_parameter_names(char ***out_parameter_names,
    char **parameter_names,
    size_t parameter_count) {
    if (out_parameter_names) {
        *out_parameter_names = parameter_names;
    } else {
        free_name_array(parameter_names, parameter_count);
    }
}

static int parse_function_external_signature(Parser *p,
    int capture_parameter_names,
    ParsedFunctionSignature *out_signature) {
    parsed_function_signature_init(out_signature);

    if (!consume(p, TOKEN_KW_INT, "'int'")) {
        return 0;
    }
    if (!consume_token(p, TOKEN_IDENTIFIER, "identifier", &out_signature->name_tok)) {
        return 0;
    }
    if (!consume(p, TOKEN_LPAREN, "'('")) {
        return 0;
    }
    if (!parse_parameter_list(p,
        &out_signature->parameter_count,
        &out_signature->has_unnamed_parameter,
        capture_parameter_names ? &out_signature->parameter_names : NULL)) {
        return 0;
    }
    if (!consume(p, TOKEN_RPAREN, "')'")) {
        parsed_function_signature_release(out_signature);
        return 0;
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
    size_t *out_parameter_count,
    char ***out_parameter_names,
    int *out_is_definition,
    AstStatement **out_function_body,
    size_t *out_return_statement_count) {
    ParsedFunctionSignature signature;
    int body_ok;

    if (out_name_token) {
        *out_name_token = NULL;
    }
    if (out_parameter_count) {
        *out_parameter_count = 0;
    }
    if (out_parameter_names) {
        *out_parameter_names = NULL;
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
        out_parameter_names != NULL,
        &signature)) {
        return 0;
    }

    if (out_name_token) {
        *out_name_token = signature.name_tok;
    }
    if (out_parameter_count) {
        *out_parameter_count = signature.parameter_count;
    }

    if (match(p, TOKEN_SEMICOLON)) {
        parser_commit_parameter_names(out_parameter_names,
            signature.parameter_names,
            signature.parameter_count);
        signature.parameter_names = NULL;
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
                signature.parameter_names,
                signature.parameter_count);
            signature.parameter_names = NULL;
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

static int parse_translation_unit_external_or_declaration(Parser *p, AstProgram *out_program) {
    size_t start = p->current;
    const Token *external_name = NULL;
    size_t external_parameter_count = 0;
    char **external_parameter_names = NULL;
    int external_is_definition = 0;
    AstStatement *external_function_body = NULL;
    size_t external_return_statement_count = 0;

    if (!parse_function_external(p,
        out_program ? &external_name : NULL,
        out_program ? &external_parameter_count : NULL,
        out_program ? &external_parameter_names : NULL,
        out_program ? &external_is_definition : NULL,
        out_program ? &external_function_body : NULL,
        out_program ? &external_return_statement_count : NULL)) {
        p->current = start;
        return parse_declaration(p, out_program, NULL, NULL, NULL, NULL);
    }

    if (!out_program) {
        return 1;
    }

    if (!parser_ast_program_add_external(out_program, AST_EXTERNAL_FUNCTION, external_name)) {
        free_name_array(external_parameter_names, external_parameter_count);
        parser_ast_statement_free(external_function_body);
        set_error(p, peek(p), "Out of memory while building AST");
        return 0;
    }

    out_program->externals[out_program->count - 1].parameter_count = external_parameter_count;
    out_program->externals[out_program->count - 1].parameter_names = external_parameter_names;
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