#include "ast.h"
#include "ast_internal.h"

static void ast_function_signature_view_init_from_signature(const AstFunctionSignature *signature,
    AstFunctionSignatureView *out_view) {
    if (!out_view) {
        return;
    }

    memset(out_view, 0, sizeof(*out_view));
    if (!signature) {
        return;
    }

    out_view->signature = signature;
    out_view->return_type = signature->return_type;
    out_view->return_type_name = signature->return_type_name;
    out_view->parameter_count = signature->parameter_count;
    out_view->parameter_value_kinds = signature->parameter_value_kinds;
    out_view->parameter_type_names = (const char *const *)signature->parameter_type_names;
    out_view->parameter_function_signatures =
        (const AstFunctionSignature *const *)signature->parameter_function_signatures;
    out_view->parameter_function_return_types = NULL;
    out_view->parameter_function_parameter_counts = NULL;
    out_view->parameter_function_parameter_value_kinds = NULL;
    out_view->parameter_function_parameter_return_types = NULL;
    out_view->parameter_function_parameter_parameter_counts = NULL;
}

static void ast_function_signature_free_shallow(AstFunctionSignature *signature) {
    size_t i;

    if (!signature) {
        return;
    }
    free(signature->return_type_name);
    if (signature->parameter_type_names) {
        for (i = 0; i < signature->parameter_count; ++i) {
            free(signature->parameter_type_names[i]);
        }
    }
    if (signature->parameter_function_signatures) {
        for (i = 0; i < signature->parameter_count; ++i) {
            ast_function_signature_free_shallow(signature->parameter_function_signatures[i]);
        }
    }
    free(signature->parameter_value_kinds);
    free(signature->parameter_type_names);
    free(signature->parameter_function_signatures);
    free(signature);
}

static char *ast_copy_string_or_null(const char *value) {
    size_t length;
    char *copy;

    if (!value) {
        return NULL;
    }
    length = strlen(value);
    copy = (char *)malloc(length + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, value, length + 1u);
    return copy;
}

static AstFunctionSignature *ast_build_signature_from_legacy(
    AstFunctionReturnType return_type,
    const char *return_type_name,
    size_t parameter_count,
    const int *parameter_value_kinds,
    const char *const *parameter_type_names,
    const AstFunctionReturnType *parameter_function_return_types,
    const size_t *parameter_function_parameter_counts,
    const int *const *parameter_function_parameter_value_kinds,
    const AstFunctionReturnType *const *parameter_function_parameter_return_types,
    const size_t *const *parameter_function_parameter_parameter_counts,
    const int *const *const *parameter_function_parameter_parameter_value_kinds,
    const AstFunctionReturnType *const *const *parameter_function_parameter_parameter_return_types,
    const size_t *const *const *parameter_function_parameter_parameter_parameter_counts) {
    AstFunctionSignature *signature;
    size_t i;

    signature = (AstFunctionSignature *)calloc(1u, sizeof(*signature));
    if (!signature) {
        return NULL;
    }

    signature->return_type = return_type;
    signature->return_type_name = ast_copy_string_or_null(return_type_name);
    if (return_type_name && !signature->return_type_name) {
        ast_function_signature_free_shallow(signature);
        return NULL;
    }

    signature->parameter_count = parameter_count;
    if (parameter_count == 0u) {
        return signature;
    }

    signature->parameter_value_kinds = (int *)calloc(parameter_count, sizeof(int));
    signature->parameter_type_names = (char **)calloc(parameter_count, sizeof(char *));
    signature->parameter_function_signatures =
        (AstFunctionSignature **)calloc(parameter_count, sizeof(AstFunctionSignature *));
    if (!signature->parameter_value_kinds ||
        !signature->parameter_type_names ||
        !signature->parameter_function_signatures) {
        ast_function_signature_free_shallow(signature);
        return NULL;
    }

    for (i = 0u; i < parameter_count; ++i) {
        int value_kind = parameter_value_kinds ? parameter_value_kinds[i] : AST_PARAMETER_VALUE_INT;

        signature->parameter_value_kinds[i] = value_kind;
        if (parameter_type_names && parameter_type_names[i]) {
            signature->parameter_type_names[i] = ast_copy_string_or_null(parameter_type_names[i]);
            if (!signature->parameter_type_names[i]) {
                ast_function_signature_free_shallow(signature);
                return NULL;
            }
        }

        if (value_kind == AST_PARAMETER_VALUE_FUNCTION) {
            signature->parameter_function_signatures[i] = ast_build_signature_from_legacy(
                parameter_function_return_types
                    ? parameter_function_return_types[i]
                    : AST_FUNCTION_RETURN_INT,
                NULL,
                parameter_function_parameter_counts
                    ? parameter_function_parameter_counts[i]
                    : 0u,
                parameter_function_parameter_value_kinds
                    ? parameter_function_parameter_value_kinds[i]
                    : NULL,
                NULL,
                parameter_function_parameter_return_types
                    ? parameter_function_parameter_return_types[i]
                    : NULL,
                parameter_function_parameter_parameter_counts
                    ? parameter_function_parameter_parameter_counts[i]
                    : NULL,
                parameter_function_parameter_parameter_value_kinds
                    ? parameter_function_parameter_parameter_value_kinds[i]
                    : NULL,
                parameter_function_parameter_parameter_return_types
                    ? parameter_function_parameter_parameter_return_types[i]
                    : NULL,
                parameter_function_parameter_parameter_parameter_counts
                    ? parameter_function_parameter_parameter_parameter_counts[i]
                    : NULL,
                NULL,
                NULL,
                NULL);
            if (!signature->parameter_function_signatures[i]) {
                ast_function_signature_free_shallow(signature);
                return NULL;
            }
        }
    }

    return signature;
}

static AstFunctionSignature *ast_build_signature_from_statement_legacy(
    const AstStatement *stmt,
    size_t declaration_index) {
    if (!stmt) {
        return NULL;
    }

    return ast_build_signature_from_legacy(
        stmt->declaration_function_return_types
            ? stmt->declaration_function_return_types[declaration_index]
            : AST_FUNCTION_RETURN_INT,
        NULL,
        stmt->declaration_function_parameter_counts
            ? stmt->declaration_function_parameter_counts[declaration_index]
            : 0u,
        stmt->declaration_function_parameter_value_kinds
            ? stmt->declaration_function_parameter_value_kinds[declaration_index]
            : NULL,
        NULL,
        stmt->declaration_function_parameter_return_types
            ? stmt->declaration_function_parameter_return_types[declaration_index]
            : NULL,
        stmt->declaration_function_parameter_parameter_counts
            ? stmt->declaration_function_parameter_parameter_counts[declaration_index]
            : NULL,
        stmt->declaration_function_parameter_parameter_value_kinds
            ? (const int *const *)stmt->declaration_function_parameter_parameter_value_kinds[declaration_index]
            : NULL,
        stmt->declaration_function_parameter_parameter_return_types
            ? (const AstFunctionReturnType *const *)
                stmt->declaration_function_parameter_parameter_return_types[declaration_index]
            : NULL,
        stmt->declaration_function_parameter_parameter_parameter_counts
            ? (const size_t *const *)
                stmt->declaration_function_parameter_parameter_parameter_counts[declaration_index]
            : NULL,
        NULL,
        NULL,
        NULL);
}

static AstFunctionSignature *ast_build_signature_from_external_parameter_legacy(
    const AstExternal *external,
    size_t parameter_index) {
    if (!external) {
        return NULL;
    }

    return ast_build_signature_from_legacy(
        external->parameter_function_return_types
            ? external->parameter_function_return_types[parameter_index]
            : AST_FUNCTION_RETURN_INT,
        NULL,
        external->parameter_function_parameter_counts
            ? external->parameter_function_parameter_counts[parameter_index]
            : 0u,
        external->parameter_function_parameter_value_kinds
            ? external->parameter_function_parameter_value_kinds[parameter_index]
            : NULL,
        NULL,
        external->parameter_function_parameter_return_types
            ? external->parameter_function_parameter_return_types[parameter_index]
            : NULL,
        external->parameter_function_parameter_parameter_counts
            ? external->parameter_function_parameter_parameter_counts[parameter_index]
            : NULL,
        external->parameter_function_parameter_parameter_value_kinds
            ? (const int *const *)
                external->parameter_function_parameter_parameter_value_kinds[parameter_index]
            : NULL,
        external->parameter_function_parameter_parameter_return_types
            ? (const AstFunctionReturnType *const *)
                external->parameter_function_parameter_parameter_return_types[parameter_index]
            : NULL,
        external->parameter_function_parameter_parameter_parameter_counts
            ? (const size_t *const *)
                external->parameter_function_parameter_parameter_parameter_counts[parameter_index]
            : NULL,
        NULL,
        NULL,
        NULL);
}

static int ast_attach_statement_function_signatures(AstStatement *stmt) {
    size_t i;
    int has_function_declaration = 0;

    if (!stmt) {
        return 1;
    }

    for (i = 0u; i < stmt->declaration_name_count; ++i) {
        if (stmt->declaration_value_kinds &&
            stmt->declaration_value_kinds[i] == AST_DECLARATION_VALUE_FUNCTION) {
            has_function_declaration = 1;
            break;
        }
    }
    if (has_function_declaration &&
        stmt->declaration_name_count > 0u &&
        !stmt->declaration_function_signatures) {
        stmt->declaration_function_signatures =
            (AstFunctionSignature **)calloc(stmt->declaration_name_count,
                sizeof(AstFunctionSignature *));
        if (!stmt->declaration_function_signatures) {
            return 0;
        }
    }

    for (i = 0u; i < stmt->declaration_name_count; ++i) {
        if (stmt->declaration_value_kinds &&
            stmt->declaration_value_kinds[i] == AST_DECLARATION_VALUE_FUNCTION &&
            stmt->declaration_function_signatures &&
            !stmt->declaration_function_signatures[i]) {
            stmt->declaration_function_signatures[i] =
                ast_build_signature_from_statement_legacy(stmt, i);
            if (!stmt->declaration_function_signatures[i]) {
                return 0;
            }
        }
    }

    for (i = 0u; i < stmt->expression_count; ++i) {
        AstExpression *expr = stmt->expressions ? stmt->expressions[i] : NULL;
        if (expr && expr->kind == AST_EXPR_CLOSURE && !expr->as.closure.signature) {
            expr->as.closure.signature = ast_build_signature_from_legacy(
                expr->as.closure.return_type,
                expr->as.closure.return_type_name,
                expr->as.closure.parameter_count,
                expr->as.closure.parameter_value_kinds,
                (const char *const *)expr->as.closure.parameter_type_names,
                expr->as.closure.parameter_function_return_types,
                expr->as.closure.parameter_function_parameter_counts,
                (const int *const *)expr->as.closure.parameter_function_parameter_value_kinds,
                (const AstFunctionReturnType *const *)
                    expr->as.closure.parameter_function_parameter_return_types,
                (const size_t *const *)
                    expr->as.closure.parameter_function_parameter_parameter_counts,
                NULL,
                NULL,
                NULL);
            if (!expr->as.closure.signature) {
                return 0;
            }
        }
    }

    for (i = 0u; i < stmt->child_count; ++i) {
        if (!ast_attach_statement_function_signatures(stmt->children[i])) {
            return 0;
        }
    }

    return 1;
}

void ast_program_init(AstProgram *program) {
    if (!program) {
        return;
    }
    program->struct_types = NULL;
    program->struct_type_count = 0;
    program->struct_type_capacity = 0;
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

int ast_program_attach_function_signature_nodes(AstProgram *program) {
    size_t i;

    if (!program) {
        return 0;
    }

    for (i = 0u; i < program->count; ++i) {
        AstExternal *external = &program->externals[i];
        size_t param_index;

        if (external->kind != AST_EXTERNAL_FUNCTION) {
            continue;
        }

        if (external->has_function_return_signature && !external->function_return_signature) {
            external->function_return_signature = ast_build_signature_from_legacy(
                external->function_return_function_return_type,
                external->function_return_function_return_type_name,
                external->function_return_function_parameter_count,
                external->function_return_function_parameter_value_kinds,
                (const char *const *)external->function_return_function_parameter_type_names,
                external->function_return_function_parameter_return_types,
                external->function_return_function_parameter_parameter_counts,
                (const int *const *)external->function_return_function_parameter_parameter_value_kinds,
                (const AstFunctionReturnType *const *)
                    external->function_return_function_parameter_parameter_return_types,
                (const size_t *const *)
                    external->function_return_function_parameter_parameter_parameter_counts,
                NULL,
                NULL,
                NULL);
            if (!external->function_return_signature) {
                return 0;
            }
        }

        if (external->parameter_count > 0u && !external->parameter_function_signatures) {
            external->parameter_function_signatures =
                (AstFunctionSignature **)calloc(external->parameter_count,
                    sizeof(AstFunctionSignature *));
            if (!external->parameter_function_signatures) {
                return 0;
            }
        }

        for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
            if (external->parameter_value_kinds &&
                external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
                !external->parameter_function_signatures[param_index]) {
                external->parameter_function_signatures[param_index] =
                    ast_build_signature_from_external_parameter_legacy(
                        external,
                        param_index);
                if (!external->parameter_function_signatures[param_index]) {
                    return 0;
                }
            }
        }

        if (external->function_body &&
            !ast_attach_statement_function_signatures(external->function_body)) {
            return 0;
        }
    }

    return 1;
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

int ast_external_get_function_signature_view(const AstExternal *external,
    AstFunctionSignatureView *out_view) {
    if (!external || !out_view || external->kind != AST_EXTERNAL_FUNCTION) {
        return 0;
    }

    out_view->return_type = external->function_return_type;
    out_view->return_type_name = external->function_return_type_name;
    out_view->parameter_count = external->parameter_count;
    out_view->parameter_value_kinds = external->parameter_value_kinds;
    out_view->parameter_type_names = (const char *const *)external->parameter_type_names;
    out_view->parameter_function_signatures =
        (const AstFunctionSignature *const *)external->parameter_function_signatures;
    out_view->parameter_function_return_types = external->parameter_function_return_types;
    out_view->parameter_function_parameter_counts =
        external->parameter_function_parameter_counts;
    out_view->parameter_function_parameter_value_kinds =
        (const int *const *)external->parameter_function_parameter_value_kinds;
    out_view->parameter_function_parameter_return_types =
        (const AstFunctionReturnType *const *)external->parameter_function_parameter_return_types;
    out_view->parameter_function_parameter_parameter_counts =
        (const size_t *const *)external->parameter_function_parameter_parameter_counts;
    return 1;
}

int ast_external_get_function_return_signature_view(const AstExternal *external,
    AstFunctionSignatureView *out_view) {
    if (!external || !out_view || external->kind != AST_EXTERNAL_FUNCTION ||
        !external->has_function_return_signature) {
        return 0;
    }

    ast_function_signature_view_init_from_signature(
        external->function_return_signature,
        out_view);
    if (!out_view->signature) {
        out_view->return_type = external->function_return_function_return_type;
        out_view->return_type_name = external->function_return_function_return_type_name;
        out_view->parameter_count = external->function_return_function_parameter_count;
        out_view->parameter_value_kinds =
            external->function_return_function_parameter_value_kinds;
        out_view->parameter_type_names =
            (const char *const *)external->function_return_function_parameter_type_names;
    }
    out_view->parameter_function_return_types =
        external->function_return_function_parameter_return_types;
    out_view->parameter_function_parameter_counts =
        external->function_return_function_parameter_parameter_counts;
    out_view->parameter_function_parameter_value_kinds =
        (const int *const *)external->function_return_function_parameter_parameter_value_kinds;
    out_view->parameter_function_parameter_return_types =
        (const AstFunctionReturnType *const *)
            external->function_return_function_parameter_parameter_return_types;
    out_view->parameter_function_parameter_parameter_counts =
        (const size_t *const *)
            external->function_return_function_parameter_parameter_parameter_counts;
    return 1;
}

int ast_external_get_parameter_function_signature_view(const AstExternal *external,
    size_t param_index,
    AstFunctionSignatureView *out_view) {
    if (!external || !out_view || external->kind != AST_EXTERNAL_FUNCTION ||
        param_index >= external->parameter_count ||
        !external->parameter_value_kinds ||
        external->parameter_value_kinds[param_index] != AST_PARAMETER_VALUE_FUNCTION) {
        return 0;
    }

    ast_function_signature_view_init_from_signature(
        external->parameter_function_signatures
            ? external->parameter_function_signatures[param_index]
            : NULL,
        out_view);
    if (!out_view->signature) {
        out_view->return_type =
            external->parameter_function_return_types
                ? external->parameter_function_return_types[param_index]
                : AST_FUNCTION_RETURN_INT;
        out_view->parameter_count =
            external->parameter_function_parameter_counts
                ? external->parameter_function_parameter_counts[param_index]
                : 0u;
        out_view->parameter_value_kinds =
            external->parameter_function_parameter_value_kinds
                ? external->parameter_function_parameter_value_kinds[param_index]
                : NULL;
    }
    out_view->parameter_function_return_types =
        external->parameter_function_parameter_return_types
            ? external->parameter_function_parameter_return_types[param_index]
            : NULL;
    out_view->parameter_function_parameter_counts =
        external->parameter_function_parameter_parameter_counts
            ? external->parameter_function_parameter_parameter_counts[param_index]
            : NULL;
    out_view->parameter_function_parameter_value_kinds =
        external->parameter_function_parameter_parameter_value_kinds
            ? (const int *const *)
                external->parameter_function_parameter_parameter_value_kinds[param_index]
            : NULL;
    out_view->parameter_function_parameter_return_types =
        external->parameter_function_parameter_parameter_return_types
            ? (const AstFunctionReturnType *const *)
                external->parameter_function_parameter_parameter_return_types[param_index]
            : NULL;
    out_view->parameter_function_parameter_parameter_counts =
        external->parameter_function_parameter_parameter_parameter_counts
            ? (const size_t *const *)
                external->parameter_function_parameter_parameter_parameter_counts[param_index]
            : NULL;
    return 1;
}

int ast_statement_get_declaration_function_signature_view(const AstStatement *stmt,
    size_t declaration_index,
    AstFunctionSignatureView *out_view) {
    if (!stmt || !out_view || stmt->kind != AST_STMT_DECLARATION ||
        !stmt->declaration_value_kinds ||
        declaration_index >= stmt->declaration_name_count ||
        stmt->declaration_value_kinds[declaration_index] != AST_DECLARATION_VALUE_FUNCTION) {
        return 0;
    }

    ast_function_signature_view_init_from_signature(
        stmt->declaration_function_signatures
            ? stmt->declaration_function_signatures[declaration_index]
            : NULL,
        out_view);
    if (!out_view->signature) {
        out_view->return_type =
            stmt->declaration_function_return_types
                ? stmt->declaration_function_return_types[declaration_index]
                : AST_FUNCTION_RETURN_INT;
        out_view->parameter_count =
            stmt->declaration_function_parameter_counts
                ? stmt->declaration_function_parameter_counts[declaration_index]
                : 0u;
        out_view->parameter_value_kinds =
            stmt->declaration_function_parameter_value_kinds
                ? stmt->declaration_function_parameter_value_kinds[declaration_index]
                : NULL;
    }
    out_view->parameter_function_return_types =
        stmt->declaration_function_parameter_return_types
            ? stmt->declaration_function_parameter_return_types[declaration_index]
            : NULL;
    out_view->parameter_function_parameter_counts =
        stmt->declaration_function_parameter_parameter_counts
            ? stmt->declaration_function_parameter_parameter_counts[declaration_index]
            : NULL;
    out_view->parameter_function_parameter_value_kinds =
        stmt->declaration_function_parameter_parameter_value_kinds
            ? (const int *const *)
                stmt->declaration_function_parameter_parameter_value_kinds[declaration_index]
            : NULL;
    out_view->parameter_function_parameter_return_types =
        stmt->declaration_function_parameter_parameter_return_types
            ? (const AstFunctionReturnType *const *)
                stmt->declaration_function_parameter_parameter_return_types[declaration_index]
            : NULL;
    out_view->parameter_function_parameter_parameter_counts =
        stmt->declaration_function_parameter_parameter_parameter_counts
            ? (const size_t *const *)
                stmt->declaration_function_parameter_parameter_parameter_counts[declaration_index]
            : NULL;
    return 1;
}

int ast_expression_get_closure_signature_view(const AstExpression *expr,
    AstFunctionSignatureView *out_view) {
    if (!expr || !out_view || expr->kind != AST_EXPR_CLOSURE) {
        return 0;
    }

    ast_function_signature_view_init_from_signature(expr->as.closure.signature, out_view);
    if (!out_view->signature) {
        out_view->return_type = expr->as.closure.return_type;
        out_view->return_type_name = expr->as.closure.return_type_name;
        out_view->parameter_count = expr->as.closure.parameter_count;
        out_view->parameter_value_kinds = expr->as.closure.parameter_value_kinds;
        out_view->parameter_type_names =
            (const char *const *)expr->as.closure.parameter_type_names;
    }
    out_view->parameter_function_return_types =
        expr->as.closure.parameter_function_return_types;
    out_view->parameter_function_parameter_counts =
        expr->as.closure.parameter_function_parameter_counts;
    out_view->parameter_function_parameter_value_kinds =
        (const int *const *)expr->as.closure.parameter_function_parameter_value_kinds;
    out_view->parameter_function_parameter_return_types =
        (const AstFunctionReturnType *const *)
            expr->as.closure.parameter_function_parameter_return_types;
    out_view->parameter_function_parameter_parameter_counts =
        (const size_t *const *)expr->as.closure.parameter_function_parameter_parameter_counts;
    return 1;
}

int ast_function_signature_view_get_parameter_function_signature_view(
    const AstFunctionSignatureView *view,
    size_t param_index,
    AstFunctionSignatureView *out_view) {
    if (!view || !out_view || param_index >= view->parameter_count ||
        !view->parameter_value_kinds ||
        view->parameter_value_kinds[param_index] != AST_PARAMETER_VALUE_FUNCTION) {
        return 0;
    }

    ast_function_signature_view_init_from_signature(
        view->parameter_function_signatures
            ? view->parameter_function_signatures[param_index]
            : NULL,
        out_view);
    out_view->parameter_function_return_types =
        view->parameter_function_parameter_return_types
            ? view->parameter_function_parameter_return_types[param_index]
            : NULL;
    out_view->parameter_function_parameter_counts =
        view->parameter_function_parameter_parameter_counts
            ? view->parameter_function_parameter_parameter_counts[param_index]
            : NULL;
    out_view->parameter_function_parameter_value_kinds = NULL;
    out_view->parameter_function_parameter_return_types = NULL;
    out_view->parameter_function_parameter_parameter_counts = NULL;
    return 1;
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
    case AST_EXPR_FLOAT_LITERAL:
        return "float_literal";
    case AST_EXPR_CONVERSION:
        return "conversion";
    case AST_EXPR_INIT_LIST:
        return "init_list";
    case AST_EXPR_PAREN:
        return "paren";
    case AST_EXPR_UNARY:
        return "unary";
    case AST_EXPR_POSTFIX:
        return "postfix";
    case AST_EXPR_SUBSCRIPT:
        return "subscript";
    case AST_EXPR_MEMBER:
        return "member";
    case AST_EXPR_CALL:
        return "call";
    case AST_EXPR_BINARY:
        return "binary";
    case AST_EXPR_TERNARY:
        return "ternary";
    case AST_EXPR_CLOSURE:
        return "closure";
    default:
        return "unknown";
    }
}

void ast_statement_free(AstStatement *stmt) {
    ast_statement_free_internal(stmt);
}

const char *ast_statement_kind_name(AstStatementKind kind) {
    switch (kind) {
    case AST_STMT_COMPOUND:
        return "compound";
    case AST_STMT_DECLARATION:
        return "declaration";
    case AST_STMT_EXPRESSION:
        return "expression";
    case AST_STMT_IF:
        return "if";
    case AST_STMT_WHILE:
        return "while";
    case AST_STMT_FOR:
        return "for";
    case AST_STMT_RETURN:
        return "return";
    case AST_STMT_BREAK:
        return "break";
    case AST_STMT_CONTINUE:
        return "continue";
    case AST_STMT_DEFER:
        return "defer";
    case AST_STMT_FNDEFER:
        return "fndefer";
    case AST_STMT_CAPDEFER:
        return "capdefer";
    default:
        return "unknown";
    }
}
