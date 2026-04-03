#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

int semantic_analyze_program(const AstProgram *program, SemanticError *error) {
    size_t i;
    size_t j;

    if (!program) {
        semantic_set_error(error, 0, 0, "SEMA-INT-013: semantic input program is NULL");
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    for (i = 0; i < program->count; ++i) {
        const AstExternal *lhs = &program->externals[i];
        int returns_all_paths = 0;

        /* AST contract allows unnamed externals (name_token == NULL). */
        if (!lhs->name || lhs->name_length == 0) {
            continue;
        }

        if (lhs->kind == AST_EXTERNAL_DECLARATION && lhs->has_initializer) {
            if (!lhs->declaration_initializer) {
                semantic_set_error(
                    error,
                    lhs->line,
                    lhs->column,
                    "SEMA-INT-011: top-level declaration initializer metadata missing AST expression");
                return 0;
            }

            if (!semantic_check_top_level_initializer_callable_rules(program,
                i,
                lhs->declaration_initializer,
                error)) {
                return 0;
            }

            if (!semantic_scope_check_top_level_initializer_expression(lhs->declaration_initializer,
                program,
                i,
                error)) {
                return 0;
            }
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && lhs->is_function_definition) {
            if (!semantic_check_function_callable_scope_shadow_rules(lhs, error)) {
                return 0;
            }

            if (!semantic_check_function_callable_rules(program, i, lhs, error)) {
                return 0;
            }

            if (!semantic_check_function_scope_rules(program, i, lhs, error)) {
                return 0;
            }

            if (!semantic_compute_function_returns_all_paths(lhs, &returns_all_paths, error)) {
                return 0;
            }

            if (!returns_all_paths) {
                if (error) {
                    semantic_set_error(error,
                        lhs->line,
                        lhs->column,
                        "SEMA-CF-001: Function definition must return on all control-flow paths");
                }
                return 0;
            }
        }

        for (j = i + 1; j < program->count; ++j) {
            const AstExternal *rhs = &program->externals[j];

            if (!rhs->name || rhs->name_length == 0) {
                continue;
            }

            if (!names_equal(lhs, rhs)) {
                continue;
            }

            if (lhs->kind == AST_EXTERNAL_FUNCTION && rhs->kind == AST_EXTERNAL_FUNCTION) {
                if (lhs->parameter_count != rhs->parameter_count) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-002: identifier=%s; prior_params=%zu; current_params=%zu; conflicting function declarations: parameter count mismatch",
                            preview,
                            lhs->parameter_count,
                            rhs->parameter_count);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                if (lhs->is_function_definition && rhs->is_function_definition) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-003: identifier=%s; duplicate function definition",
                            preview);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                /* Multiple compatible declarations are currently accepted. */
                continue;
            }

            if (lhs->kind == AST_EXTERNAL_DECLARATION && rhs->kind == AST_EXTERNAL_DECLARATION) {
                if (lhs->has_initializer && rhs->has_initializer) {
                    if (error) {
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            "SEMA-TOP-001: duplicate top-level variable definition");
                    }
                    return 0;
                }

                /* Repeated top-level declarations without duplicate definition are accepted. */
                continue;
            }

            if (lhs->kind != rhs->kind) {
                if (error) {
                    char preview[128];
                    char msg[512];

                    format_callee_preview(lhs->name, preview, sizeof(preview));
                    snprintf(msg,
                        sizeof(msg),
                        "SEMA-TOP-004: identifier=%s; function/variable name conflict at top level",
                        preview);
                    semantic_set_error(error,
                        rhs->line,
                        rhs->column,
                        msg);
                }
                return 0;
            }
        }
    }

    return 1;
}
#endif
