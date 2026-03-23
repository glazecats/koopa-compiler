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

void ast_expression_free(AstExpression *expr) {
    ast_expression_free_internal(expr);
}

const char *ast_expression_kind_name(AstExpressionKind kind) {
    switch (kind) {
    case AST_EXPR_IDENTIFIER:
        return "identifier";
    case AST_EXPR_NUMBER:
        return "number";
    case AST_EXPR_PAREN:
        return "paren";
    case AST_EXPR_UNARY:
        return "unary";
    case AST_EXPR_POSTFIX:
        return "postfix";
    case AST_EXPR_BINARY:
        return "binary";
    case AST_EXPR_TERNARY:
        return "ternary";
    default:
        return "unknown";
    }
}