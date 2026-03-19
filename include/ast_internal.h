#ifndef AST_INTERNAL_H
#define AST_INTERNAL_H

#include <stdlib.h>
#include <string.h>

#include "ast.h"

/* Internal helper: free all owned AstProgram storage and reset to empty. */
static inline void ast_program_clear_storage(AstProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->count; ++i) {
        free(program->externals[i].name);
    }

    free(program->externals);
    program->externals = NULL;
    program->count = 0;
    program->capacity = 0;
}

/* Internal helper: append one top-level external node. */
static inline int ast_program_append_external(AstProgram *program,
                                              AstExternalKind kind,
                                              const Token *name_token) {
    AstExternal external;
    AstExternal *new_data;
    size_t next_capacity;

    if (!program) {
        return 0;
    }

    if (program->count == program->capacity) {
        next_capacity = (program->capacity == 0) ? 16 : (program->capacity * 2);
        new_data = (AstExternal *)realloc(program->externals, next_capacity * sizeof(AstExternal));
        if (!new_data) {
            return 0;
        }
        program->externals = new_data;
        program->capacity = next_capacity;
    }

    external.kind = kind;
    external.name = NULL;
    external.name_length = 0;
    external.has_initializer = 0;
    external.parameter_count = 0;
    external.is_function_definition = 0;
    external.line = 0;
    external.column = 0;

    if (name_token && name_token->lexeme && name_token->length > 0) {
        external.name = (char *)malloc(name_token->length + 1);
        if (!external.name) {
            return 0;
        }
        memcpy(external.name, name_token->lexeme, name_token->length);
        external.name[name_token->length] = '\0';
        external.name_length = name_token->length;
        external.line = name_token->line;
        external.column = name_token->column;
    }

    program->externals[program->count++] = external;
    return 1;
}

#endif