#if defined(__INTELLISENSE__) && !defined(PARSER_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(PARSER_SPLIT_AGGREGATOR)
#define PARSER_SPLIT_INTELLISENSE_CONTEXT
#include "parser.c"
#else

/*
 * Keep parser link-compatible without requiring src/ast/ast.c for legacy
 * parser-only builds.
 *
 * Maintenance checklist: when AstProgram/AstExternal/AstStatement/AstExpression
 * ownership fields change, update these parser_ast_* helpers together with
 * include/ast_internal.h lifecycle helpers to prevent legacy-link drift.
 */
static void parser_ast_expression_free(AstExpression *expr);

static void parser_ast_statement_free(AstStatement *stmt) {
    size_t i;

    if (!stmt) {
        return;
    }

    for (i = 0; i < stmt->child_count; ++i) {
        parser_ast_statement_free(stmt->children[i]);
    }
    for (i = 0; i < stmt->expression_count; ++i) {
        parser_ast_expression_free(stmt->expressions[i]);
    }
    for (i = 0; i < stmt->declaration_name_count; ++i) {
        free(stmt->declaration_names[i]);
    }
    free(stmt->declaration_names);
    free(stmt->expressions);
    free(stmt->children);
    free(stmt);
}

static void parser_ast_program_free(AstProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->count; ++i) {
        size_t j;
        free(program->externals[i].name);
        if (program->externals[i].parameter_names) {
            for (j = 0; j < program->externals[i].parameter_count; ++j) {
                free(program->externals[i].parameter_names[j]);
            }
        }
        free(program->externals[i].parameter_names);
        parser_ast_statement_free(program->externals[i].function_body);
    }

    free(program->externals);
    program->externals = NULL;
    program->count = 0;
    program->capacity = 0;
}

static int parser_ast_program_add_external(AstProgram *program,
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
    external.parameter_names = NULL;
    external.is_function_definition = 0;
    external.function_body = NULL;
    external.return_statement_count = 0;
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

static void parser_ast_expression_free(AstExpression *expr) {
    size_t i;

    if (!expr) {
        return;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        free(expr->as.identifier.name);
        break;
    case AST_EXPR_NUMBER:
        break;
    case AST_EXPR_PAREN:
        parser_ast_expression_free(expr->as.inner);
        break;
    case AST_EXPR_UNARY:
        parser_ast_expression_free(expr->as.unary.operand);
        break;
    case AST_EXPR_POSTFIX:
        parser_ast_expression_free(expr->as.postfix.operand);
        break;
    case AST_EXPR_CALL:
        parser_ast_expression_free(expr->as.call.callee);
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            parser_ast_expression_free(expr->as.call.args[i]);
        }
        free(expr->as.call.args);
        break;
    case AST_EXPR_BINARY:
        parser_ast_expression_free(expr->as.binary.left);
        parser_ast_expression_free(expr->as.binary.right);
        break;
    case AST_EXPR_TERNARY:
        parser_ast_expression_free(expr->as.ternary.condition);
        parser_ast_expression_free(expr->as.ternary.then_expr);
        parser_ast_expression_free(expr->as.ternary.else_expr);
        break;
    default:
        break;
    }

    free(expr);
}

#endif
