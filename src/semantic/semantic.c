#include "semantic.h"

#include <stdio.h>
#include <string.h>

#define CALLEE_PREVIEW_HEAD 24
#define CALLEE_PREVIEW_TAIL 16

static void format_callee_preview(const char *name, char *out, size_t out_size) {
    size_t len;

    if (!out || out_size == 0) {
        return;
    }

    if (!name) {
        snprintf(out, out_size, "(null)");
        return;
    }

    len = strlen(name);
    if (len <= (CALLEE_PREVIEW_HEAD + CALLEE_PREVIEW_TAIL + 3)) {
        snprintf(out, out_size, "%s", name);
        return;
    }

    snprintf(out,
             out_size,
             "%.*s...%.*s(len=%zu)",
             CALLEE_PREVIEW_HEAD,
             name,
             CALLEE_PREVIEW_TAIL,
             name + (len - CALLEE_PREVIEW_TAIL),
             len);
}

static void semantic_set_error(SemanticError *error,
                               int line,
                               int column,
                               const char *message) {
    if (!error) {
        return;
    }

    error->line = line;
    error->column = column;
    snprintf(error->message, sizeof(error->message), "%s", message);
}

static int names_equal(const AstExternal *a, const AstExternal *b) {
    if (!a->name || !b->name) {
        return 0;
    }
    if (a->name_length != b->name_length) {
        return 0;
    }
    return strncmp(a->name, b->name, a->name_length) == 0;
}

int semantic_analyze_program(const AstProgram *program, SemanticError *error) {
    size_t i;
    size_t j;
    size_t k;

    if (!program) {
        semantic_set_error(error, 0, 0, "Semantic input program is NULL");
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    for (i = 0; i < program->count; ++i) {
        const AstExternal *lhs = &program->externals[i];

        /* AST contract allows unnamed externals (name_token == NULL). */
        if (!lhs->name || lhs->name_length == 0) {
            continue;
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && lhs->is_function_definition &&
            !lhs->returns_on_all_paths) {
            if (error) {
                semantic_set_error(error,
                                   lhs->line,
                                   lhs->column,
                                   "Function definition must return on all control-flow paths");
            }
            return 0;
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && lhs->is_function_definition) {
            for (k = 0; k < lhs->called_function_count; ++k) {
                int found_decl = 0;
                int found_matching_param_count = 0;
                int found_non_function_symbol = 0;
                int has_later_function_decl = 0;
                int later_decl_line = 0;
                int later_decl_column = 0;
                int call_line = lhs->line;
                int call_column = lhs->column;
                int callee_kind = AST_CALL_CALLEE_NON_IDENTIFIER;
                size_t expected_arg_count = 0;
                size_t called_arg_count = 0;
                const char *called_name = lhs->called_function_names[k];
                char callee_preview[128];

                format_callee_preview(called_name, callee_preview, sizeof(callee_preview));

                if (lhs->called_function_lines && lhs->called_function_columns) {
                    call_line = lhs->called_function_lines[k];
                    call_column = lhs->called_function_columns[k];
                }
                if (lhs->called_function_arg_counts) {
                    called_arg_count = lhs->called_function_arg_counts[k];
                }
                if (lhs->called_function_kinds) {
                    callee_kind = lhs->called_function_kinds[k];
                }

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

                for (j = 0; j <= i; ++j) {
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
                    for (j = i + 1; j < program->count; ++j) {
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
                                 "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
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
                        semantic_set_error(error,
                                           rhs->line,
                                           rhs->column,
                                           "Conflicting function declarations: parameter count mismatch");
                    }
                    return 0;
                }

                if (lhs->is_function_definition && rhs->is_function_definition) {
                    if (error) {
                        semantic_set_error(error,
                                           rhs->line,
                                           rhs->column,
                                           "Duplicate function definition");
                    }
                    return 0;
                }

                /* Multiple compatible declarations are currently accepted. */
                continue;
            }

            if (lhs->kind != rhs->kind) {
                if (error) {
                    semantic_set_error(error,
                                       rhs->line,
                                       rhs->column,
                                       "Function/variable name conflict at top level");
                }
                return 0;
            }
        }
    }

    return 1;
}