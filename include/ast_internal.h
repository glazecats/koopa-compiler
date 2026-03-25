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
        size_t j;
        free(program->externals[i].name);
        for (j = 0; j < program->externals[i].called_function_count; ++j) {
            free(program->externals[i].called_function_names[j]);
        }
        free(program->externals[i].called_function_names);
        free(program->externals[i].called_function_lines);
        free(program->externals[i].called_function_columns);
        free(program->externals[i].called_function_arg_counts);
        free(program->externals[i].called_function_kinds);
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
    external.return_statement_count = 0;
    external.returns_on_all_paths = 0;
    external.loop_statement_count = 0;
    external.if_statement_count = 0;
    external.break_statement_count = 0;
    external.continue_statement_count = 0;
    external.declaration_statement_count = 0;
    external.called_function_names = NULL;
    external.called_function_lines = NULL;
    external.called_function_columns = NULL;
    external.called_function_arg_counts = NULL;
    external.called_function_kinds = NULL;
    external.called_function_count = 0;
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

/* Internal helper: recursively free an expression subtree. */
static inline void ast_expression_free_internal(AstExpression *expr) {
    if (!expr) {
        return;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        free(expr->as.identifier.name);
        break;
    case AST_EXPR_PAREN:
        ast_expression_free_internal(expr->as.inner);
        break;
    case AST_EXPR_UNARY:
        ast_expression_free_internal(expr->as.unary.operand);
        break;
    case AST_EXPR_POSTFIX:
        ast_expression_free_internal(expr->as.postfix.operand);
        break;
    case AST_EXPR_CALL: {
        size_t i;
        ast_expression_free_internal(expr->as.call.callee);
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            ast_expression_free_internal(expr->as.call.args[i]);
        }
        free(expr->as.call.args);
        break;
    }
    case AST_EXPR_BINARY:
        ast_expression_free_internal(expr->as.binary.left);
        ast_expression_free_internal(expr->as.binary.right);
        break;
    case AST_EXPR_TERNARY:
        ast_expression_free_internal(expr->as.ternary.condition);
        ast_expression_free_internal(expr->as.ternary.then_expr);
        ast_expression_free_internal(expr->as.ternary.else_expr);
        break;
    case AST_EXPR_NUMBER:
    default:
        break;
    }

    free(expr);
}

#endif