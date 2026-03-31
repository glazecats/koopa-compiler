/*
 * Shared AST lifecycle helper template.
 *
 * This file is intentionally include-guard free because it is instantiated
 * multiple times with different symbol mappings.
 */

#if !defined(AST_LIFECYCLE_STATIC) || !defined(AST_LIFECYCLE_STATEMENT_FREE_FN) || \
    !defined(AST_LIFECYCLE_PROGRAM_CLEAR_FN) || !defined(AST_LIFECYCLE_PROGRAM_ADD_EXTERNAL_FN) || \
    !defined(AST_LIFECYCLE_EXPRESSION_FREE_FN)
#error "ast_lifecycle_template.h requires lifecycle mapping macros"
#endif

AST_LIFECYCLE_STATIC void AST_LIFECYCLE_EXPRESSION_FREE_FN(AstExpression *expr);

AST_LIFECYCLE_STATIC void AST_LIFECYCLE_STATEMENT_FREE_FN(AstStatement *stmt) {
    size_t i;

    if (!stmt) {
        return;
    }

    for (i = 0; i < stmt->child_count; ++i) {
        AST_LIFECYCLE_STATEMENT_FREE_FN(stmt->children[i]);
    }
    for (i = 0; i < stmt->expression_count; ++i) {
        AST_LIFECYCLE_EXPRESSION_FREE_FN(stmt->expressions[i]);
    }
    for (i = 0; i < stmt->declaration_name_count; ++i) {
        free(stmt->declaration_names[i]);
    }
    free(stmt->declaration_names);
    free(stmt->expressions);
    free(stmt->children);
    free(stmt);
}

AST_LIFECYCLE_STATIC void AST_LIFECYCLE_PROGRAM_CLEAR_FN(AstProgram *program) {
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
        AST_LIFECYCLE_STATEMENT_FREE_FN(program->externals[i].function_body);
    }

    free(program->externals);
    program->externals = NULL;
    program->count = 0;
    program->capacity = 0;
}

AST_LIFECYCLE_STATIC int AST_LIFECYCLE_PROGRAM_ADD_EXTERNAL_FN(AstProgram *program,
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

AST_LIFECYCLE_STATIC void AST_LIFECYCLE_EXPRESSION_FREE_FN(AstExpression *expr) {
    if (!expr) {
        return;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        free(expr->as.identifier.name);
        break;
    case AST_EXPR_PAREN:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.inner);
        break;
    case AST_EXPR_UNARY:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.unary.operand);
        break;
    case AST_EXPR_POSTFIX:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.postfix.operand);
        break;
    case AST_EXPR_CALL: {
        size_t i;
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.call.callee);
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.call.args[i]);
        }
        free(expr->as.call.args);
        break;
    }
    case AST_EXPR_BINARY:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.binary.left);
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.binary.right);
        break;
    case AST_EXPR_TERNARY:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.ternary.condition);
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.ternary.then_expr);
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.ternary.else_expr);
        break;
    case AST_EXPR_NUMBER:
    default:
        break;
    }

    free(expr);
}
