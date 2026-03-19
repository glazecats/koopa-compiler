#include "semantic.h"

#include <stdio.h>
#include <string.h>

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