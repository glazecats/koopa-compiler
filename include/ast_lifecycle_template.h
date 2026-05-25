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
    if (stmt->declaration_array_extent_exprs) {
        for (i = 0; i < stmt->declaration_name_count; ++i) {
            size_t j;
            AstExpression **extent_exprs = stmt->declaration_array_extent_exprs[i];
            size_t rank = stmt->declaration_array_ranks ? stmt->declaration_array_ranks[i] : 0u;

            if (!extent_exprs) {
                continue;
            }
            for (j = 0; j < rank; ++j) {
                AST_LIFECYCLE_EXPRESSION_FREE_FN(extent_exprs[j]);
            }
            free(extent_exprs);
        }
    }
    free(stmt->declaration_names);
    free(stmt->declaration_value_kinds);
    if (stmt->declaration_type_names) {
        for (i = 0; i < stmt->declaration_name_count; ++i) {
            free(stmt->declaration_type_names[i]);
        }
    }
    free(stmt->declaration_type_names);
    free(stmt->declaration_array_ranks);
    free(stmt->declaration_array_extent_exprs);
    free(stmt->declaration_function_return_types);
    free(stmt->declaration_function_parameter_counts);
    if (stmt->capture_names) {
        for (i = 0; i < stmt->capture_name_count; ++i) {
            free(stmt->capture_names[i]);
        }
    }
    free(stmt->capture_names);
    if (stmt->capture_expressions) {
        for (i = 0; i < stmt->capture_expression_count; ++i) {
            AST_LIFECYCLE_EXPRESSION_FREE_FN(stmt->capture_expressions[i]);
        }
    }
    free(stmt->capture_expressions);
    free(stmt->expressions);
    free(stmt->children);
    free(stmt);
}

AST_LIFECYCLE_STATIC void AST_LIFECYCLE_PROGRAM_CLEAR_FN(AstProgram *program) {
    size_t i;

    if (!program) {
        return;
    }

    for (i = 0; i < program->struct_type_count; ++i) {
        size_t j;

        free(program->struct_types[i].name);
        if (program->struct_types[i].field_names) {
            for (j = 0; j < program->struct_types[i].field_count; ++j) {
                free(program->struct_types[i].field_names[j]);
            }
        }
        free(program->struct_types[i].field_names);
    }
    free(program->struct_types);
    program->struct_types = NULL;
    program->struct_type_count = 0;
    program->struct_type_capacity = 0;

    for (i = 0; i < program->count; ++i) {
        size_t j;
        free(program->externals[i].name);
        free(program->externals[i].declaration_type_name);
        if (program->externals[i].declaration_array_extent_exprs) {
            for (j = 0; j < program->externals[i].declaration_array_rank; ++j) {
                AST_LIFECYCLE_EXPRESSION_FREE_FN(program->externals[i].declaration_array_extent_exprs[j]);
            }
            free(program->externals[i].declaration_array_extent_exprs);
        }
        if (program->externals[i].parameter_names) {
            for (j = 0; j < program->externals[i].parameter_count; ++j) {
                free(program->externals[i].parameter_names[j]);
            }
        }
        free(program->externals[i].parameter_names);
        free(program->externals[i].parameter_value_kinds);
        if (program->externals[i].parameter_array_extent_exprs) {
            for (j = 0; j < program->externals[i].parameter_count; ++j) {
                size_t k;
                AstExpression **extent_exprs = program->externals[i].parameter_array_extent_exprs[j];
                size_t rank = program->externals[i].parameter_array_ranks
                    ? program->externals[i].parameter_array_ranks[j]
                    : 0u;

                if (!extent_exprs) {
                    continue;
                }
                for (k = 0; k < rank; ++k) {
                    AST_LIFECYCLE_EXPRESSION_FREE_FN(extent_exprs[k]);
                }
                free(extent_exprs);
            }
            free(program->externals[i].parameter_array_extent_exprs);
        }
        free(program->externals[i].parameter_array_ranks);
        free(program->externals[i].parameter_function_return_types);
        free(program->externals[i].parameter_function_parameter_counts);
        free(program->externals[i].parameter_is_const);
        free(program->externals[i].parameter_name_lines);
        free(program->externals[i].parameter_name_columns);
        AST_LIFECYCLE_EXPRESSION_FREE_FN(program->externals[i].declaration_initializer);
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

    if (!program->struct_types) {
        program->struct_type_count = 0;
        program->struct_type_capacity = 0;
    }

    if (program->count == program->capacity) {
        if (program->capacity == 0) {
            next_capacity = 16;
        } else {
            if (program->capacity > ((size_t)-1) / 2) {
                return 0;
            }
            next_capacity = program->capacity * 2;
        }
        if (next_capacity > ((size_t)-1) / sizeof(AstExternal)) {
            return 0;
        }
        new_data = (AstExternal *)realloc(program->externals, next_capacity * sizeof(AstExternal));
        if (!new_data) {
            return 0;
        }
        program->externals = new_data;
        program->capacity = next_capacity;
    }

    external.kind = kind;
    external.function_return_type = AST_FUNCTION_RETURN_INT;
    external.declaration_value_kind = AST_DECLARATION_VALUE_INT;
    external.declaration_type_name = NULL;
    external.name = NULL;
    external.name_length = 0;
    external.declaration_array_rank = 0;
    external.declaration_array_extent_exprs = NULL;
    external.is_const_qualified = 0;
    external.has_initializer = 0;
    external.declaration_initializer = NULL;
    external.parameter_count = 0;
    external.parameter_names = NULL;
    external.parameter_value_kinds = NULL;
    external.parameter_array_ranks = NULL;
    external.parameter_array_extent_exprs = NULL;
    external.parameter_function_return_types = NULL;
    external.parameter_function_parameter_counts = NULL;
    external.parameter_is_const = NULL;
    external.parameter_name_lines = NULL;
    external.parameter_name_columns = NULL;
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
    case AST_EXPR_INIT_LIST: {
        size_t i;
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.init_list.items[i]);
        }
        free(expr->as.init_list.items);
        break;
    }
    case AST_EXPR_CONVERSION:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.conversion.operand);
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
    case AST_EXPR_SUBSCRIPT:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.subscript.base);
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.subscript.index);
        break;
    case AST_EXPR_MEMBER:
        AST_LIFECYCLE_EXPRESSION_FREE_FN(expr->as.member.base);
        free(expr->as.member.field_name);
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
    case AST_EXPR_FLOAT_LITERAL:
    default:
        break;
    }

    free(expr);
}
