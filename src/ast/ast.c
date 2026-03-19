#include "ast.h"
#include "ast_internal.h"

void ast_program_init(AstProgram *program) {
    if (!program) {
        return;
    }
    program->externals = NULL;
    program->count = 0;
    program->capacity = 0;
}

void ast_program_free(AstProgram *program) {
    ast_program_clear_storage(program);
}

int ast_program_add_external(AstProgram *program, AstExternalKind kind, const Token *name_token) {
    return ast_program_append_external(program, kind, name_token);
}

const char *ast_external_kind_name(AstExternalKind kind) {
    switch (kind) {
    case AST_EXTERNAL_FUNCTION:
        return "function";
    case AST_EXTERNAL_DECLARATION:
        return "declaration";
    default:
        return "unknown";
    }
}