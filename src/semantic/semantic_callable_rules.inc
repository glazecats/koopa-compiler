#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

static int semantic_check_single_callable_rule(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    const char *called_name,
    int call_line,
    int call_column,
    size_t called_arg_count,
    int callee_kind,
    SemanticError *error) {
    size_t j;
    int found_decl = 0;
    int found_matching_param_count = 0;
    int found_non_function_symbol = 0;
    int has_later_function_decl = 0;
    int matched_builtin = 0;
    int later_decl_line = 0;
    int later_decl_column = 0;
    size_t expected_arg_count = 0;
    size_t visible_external_count;
    char callee_preview[128];
    const SemanticBuiltinFunctionInfo *builtin = NULL;

    format_callee_preview(called_name, callee_preview, sizeof(callee_preview));

    if (!called_name || called_name[0] == '\0') {
        if (error) {
            if (callee_kind == AST_CALL_CALLEE_CALL_RESULT) {
                semantic_set_error(error,
                    call_line,
                    call_column,
                    "SEMA-CALL-005: call result is not callable in this semantic subset; callee_kind=call_result");
            } else {
                semantic_set_error(error,
                    call_line,
                    call_column,
                    "SEMA-CALL-006: non-identifier callee is not supported in this semantic subset; callee_kind=non_identifier");
            }
        }
        return 0;
    }

    if (!program) {
        return 1;
    }

    if (current_func &&
        semantic_function_parameter_name_is_function_valued(current_func, called_name)) {
        SemanticTypeDescriptor param_type;

        if (!semantic_find_named_function_parameter_type_descriptor(
                current_func,
                called_name,
                NULL,
                &param_type) ||
            param_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
            return 0;
        }

        if (called_arg_count != param_type.parameter_count) {
            if (error) {
                char msg[512];
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
                    callee_preview,
                    param_type.parameter_count,
                    called_arg_count);
                semantic_set_error(error, call_line, call_column, msg);
            }
            return 0;
        }
        return 1;
    }

    visible_external_count = include_current_external ? (func_index + 1) : func_index;
    if (visible_external_count > program->count) {
        visible_external_count = program->count;
    }

    for (j = 0; j < visible_external_count; ++j) {
        const AstExternal *candidate = &program->externals[j];

        if (!candidate->name) {
            continue;
        }

        if (strcmp(candidate->name, called_name) != 0) {
            continue;
        }

        if (candidate->kind == AST_EXTERNAL_FUNCTION) {
            found_decl = 1;
            expected_arg_count = candidate->parameter_count;
            if (candidate->parameter_count == called_arg_count) {
                found_matching_param_count = 1;
                break;
            }
            continue;
        }

        found_non_function_symbol = 1;
    }

    if (!found_decl) {
        builtin = semantic_find_builtin_function(called_name);
        if (builtin) {
            found_decl = 1;
            matched_builtin = 1;
            expected_arg_count = builtin->parameter_count;
            found_matching_param_count = builtin->parameter_count == called_arg_count;
        }
    }

    if (!found_decl) {
        for (j = func_index + 1; j < program->count; ++j) {
            const AstExternal *candidate = &program->externals[j];

            if (!candidate->name || candidate->kind != AST_EXTERNAL_FUNCTION) {
                continue;
            }

            if (strcmp(candidate->name, called_name) == 0) {
                has_later_function_decl = 1;
                later_decl_line = candidate->line;
                later_decl_column = candidate->column;
                break;
            }
        }
    }

    if (!found_decl && found_non_function_symbol) {
        if (error) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                "SEMA-CALL-003: callee=%s; symbol_kind=non_function; call to non-function symbol",
                callee_preview);
            semantic_set_error(error, call_line, call_column, msg);
        }
        return 0;
    }

    if (found_decl && !found_matching_param_count) {
        if (error) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                matched_builtin
                    ? "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; builtin call argument count mismatch"
                    : "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
                callee_preview,
                expected_arg_count,
                called_arg_count);
            semantic_set_error(error, call_line, call_column, msg);
        }
        return 0;
    }

    if (!found_decl) {
        if (error) {
            char msg[512];
            if (has_later_function_decl) {
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CALL-002: callee=%s; decl_line=%d; decl_col=%d; call before declaration",
                    callee_preview,
                    later_decl_line,
                    later_decl_column);
            } else {
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CALL-001: callee=%s; call to undeclared function",
                    callee_preview);
            }
            semantic_set_error(error, call_line, call_column, msg);
        }
        return 0;
    }

    return 1;
}

static int semantic_check_call_expr_postorder(const AstExpression *expr, void *ctx) {
    CallableRuleVisitCtx *visit_ctx;
    const char *called_name = NULL;
    int callee_kind;

    if (!expr || expr->kind != AST_EXPR_CALL) {
        return 1;
    }

    if (!ctx) {
        return 0;
    }

    visit_ctx = (CallableRuleVisitCtx *)ctx;
    callee_kind = classify_ast_call_callee(expr->as.call.callee, &called_name);
    if (callee_kind == AST_CALL_CALLEE_CALL_RESULT) {
        const AstExpression *callee_base = unwrap_paren_expression(expr->as.call.callee);
        const AstExternal *external = NULL;
        SemanticTypeDescriptor return_type;

        if (callee_base && callee_base->kind == AST_EXPR_CALL &&
            classify_ast_call_callee(callee_base->as.call.callee, &called_name) ==
                AST_CALL_CALLEE_DIRECT_IDENTIFIER &&
            called_name && called_name[0] != '\0') {
            external = semantic_find_visible_function_external(
                visit_ctx->program,
                visit_ctx->func_index,
                visit_ctx->include_current_external,
                called_name);
            if (external && external->has_function_return_signature &&
                semantic_build_function_return_type_descriptor(external, &return_type) &&
                return_type.kind == SEMANTIC_TYPE_KIND_FUNCTION &&
                expr->as.call.arg_count == return_type.parameter_count) {
                return 1;
            }
        }
    }

    return semantic_check_single_callable_rule(visit_ctx->program,
        visit_ctx->func_index,
        visit_ctx->include_current_external,
        visit_ctx->current_func,
        called_name,
        expr->line,
        expr->column,
        expr->as.call.arg_count,
        callee_kind,
        visit_ctx->error);
}

static int semantic_check_function_callable_rules(const AstProgram *program,
    size_t func_index,
    const AstExternal *func,
    SemanticError *error) {
    CallableRuleVisitCtx visit_ctx;

    if (!program || !func) {
        return 1;
    }

    /* S2-9: function-definition callable checks are AST-primary only. */
    if (!func->is_function_definition) {
        return 1;
    }

    if (!func->function_body) {
        semantic_set_error(error,
            func->line,
            func->column,
            "SEMA-INT-005: function definition missing statement AST body during AST-primary semantic phase");
        return 0;
    }

    visit_ctx.program = program;
    visit_ctx.func_index = func_index;
    visit_ctx.include_current_external = 1;
    visit_ctx.current_func = func;
    visit_ctx.error = error;
    return walk_statement_postorder(func->function_body,
        semantic_check_call_expr_postorder,
        &visit_ctx,
        error);
}

static int semantic_check_top_level_initializer_callable_rules(const AstProgram *program,
    size_t external_index,
    const AstExpression *expr,
    SemanticError *error) {
    CallableRuleVisitCtx visit_ctx;

    if (!program || !expr) {
        return 1;
    }

    visit_ctx.program = program;
    visit_ctx.func_index = external_index;
    visit_ctx.include_current_external = 0;
    visit_ctx.current_func = NULL;
    visit_ctx.error = error;
    return walk_expression_postorder(expr, semantic_check_call_expr_postorder, &visit_ctx, error);
}

#endif
