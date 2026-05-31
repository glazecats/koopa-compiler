#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

static int semantic_build_function_parameter_type_descriptor(const AstExternal *external,
    size_t param_index,
    SemanticTypeDescriptor *out_type);
static int semantic_validate_scalar_initializer_type(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    int allow_extension_features,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_find_named_function_parameter_type_descriptor(const AstExternal *func,
    const char *name,
    size_t *out_index,
    SemanticTypeDescriptor *out_type);
static int semantic_extension_local_function_value_argument_is_allowed(
    const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func);
static int semantic_extension_function_value_initializer_expr_matches_expected(
    const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    const SemanticTypeDescriptor *expected_type,
    int *out_is_closure_backed);
static int semantic_extension_resolve_local_function_value_binding_with_expected_type(
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    const char *name,
    size_t max_parameter_count,
    const SemanticTypeDescriptor *expected_type,
    int *out_is_closure_backed,
    const char **out_resolved_name);
static int semantic_current_function_has_local_function_value_name(
    const AstExternal *current_func,
    const char *name);
static int semantic_build_local_function_value_declaration_type_descriptor(
    const AstStatement *stmt,
    size_t declaration_index,
    SemanticTypeDescriptor *out_type);
static int semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
    const AstExternal *current_func,
    const char *name,
    SemanticTypeDescriptor *out_type);

static int semantic_external_has_function_valued_parameters(const AstExternal *external) {
    size_t param_index;
    SemanticTypeDescriptor param_type;

    if (!external) {
        return 0;
    }

    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
        if (semantic_build_function_parameter_type_descriptor(external, param_index, &param_type) &&
            param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
            return 1;
        }
    }

    return 0;
}

static int semantic_find_named_function_parameter_type_descriptor(const AstExternal *func,
    const char *name,
    size_t *out_index,
    SemanticTypeDescriptor *out_type) {
    size_t param_index;
    SemanticTypeDescriptor param_type;

    if (out_index) {
        *out_index = 0u;
    }
    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!func || !name || name[0] == '\0' || !func->parameter_names) {
        return 0;
    }

    for (param_index = 0u; param_index < func->parameter_count; ++param_index) {
        if (!func->parameter_names[param_index] ||
            strcmp(func->parameter_names[param_index], name) != 0) {
            continue;
        }
        if (!semantic_build_function_parameter_type_descriptor(func, param_index, &param_type)) {
            return 0;
        }
        if (out_index) {
            *out_index = param_index;
        }
        if (out_type) {
            *out_type = param_type;
        }
        return 1;
    }

    return 0;
}

static int semantic_type_descriptor_array_extents_match(const SemanticTypeDescriptor *lhs,
    const SemanticTypeDescriptor *rhs) {
    size_t extent_index;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->array_rank != rhs->array_rank) {
        return 0;
    }

    for (extent_index = 0u; extent_index < lhs->array_rank; ++extent_index) {
        const AstExpression *lhs_extent =
            lhs->array_extent_exprs ? lhs->array_extent_exprs[extent_index] : NULL;
        const AstExpression *rhs_extent =
            rhs->array_extent_exprs ? rhs->array_extent_exprs[extent_index] : NULL;
        long long lhs_value = 0;
        long long rhs_value = 0;

        if (!lhs_extent || !rhs_extent) {
            return 0;
        }
        if (!flow_try_eval_constant_int(lhs_extent, &lhs_value) ||
            !flow_try_eval_constant_int(rhs_extent, &rhs_value) ||
            lhs_value != rhs_value) {
            return 0;
        }
    }

    return 1;
}

static int semantic_parameter_value_kind_maps_to_type_descriptor_kind(int parameter_value_kind,
    SemanticTypeKind *out_kind) {
    if (!out_kind) {
        return 0;
    }

    switch (parameter_value_kind) {
    case AST_PARAMETER_VALUE_INT:
        *out_kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    case AST_PARAMETER_VALUE_FLOAT:
        *out_kind = SEMANTIC_TYPE_KIND_FLOAT;
        return 1;
    case AST_PARAMETER_VALUE_PAIR:
    case AST_PARAMETER_VALUE_STRUCT:
        *out_kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        return 1;
    case AST_PARAMETER_VALUE_FUNCTION:
        *out_kind = SEMANTIC_TYPE_KIND_FUNCTION;
        return 1;
    default:
        *out_kind = SEMANTIC_TYPE_KIND_INVALID;
        return 0;
    }
}

static SemanticTypeKind semantic_declaration_value_kind_to_type_kind(int declaration_value_kind) {
    switch (declaration_value_kind) {
    case AST_DECLARATION_VALUE_INT:
        return SEMANTIC_TYPE_KIND_INT;
    case AST_DECLARATION_VALUE_FLOAT:
        return SEMANTIC_TYPE_KIND_FLOAT;
    case AST_DECLARATION_VALUE_PAIR:
    case AST_DECLARATION_VALUE_STRUCT:
        return SEMANTIC_TYPE_KIND_AGGREGATE;
    default:
        return SEMANTIC_TYPE_KIND_INVALID;
    }
}

static int semantic_build_top_level_declaration_type_descriptor(const AstExternal *external,
    SemanticTypeDescriptor *out_type) {
    if (!external || !out_type || external->kind != AST_EXTERNAL_DECLARATION) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    out_type->kind = semantic_declaration_value_kind_to_type_kind(external->declaration_value_kind);
    out_type->aggregate_kind =
        semantic_aggregate_kind_from_declaration_value_kind(external->declaration_value_kind);
    out_type->named_type_name = external->declaration_type_name;
    out_type->array_rank = external->declaration_array_rank;
    out_type->array_extent_exprs = external->declaration_array_extent_exprs;

    return out_type->kind != SEMANTIC_TYPE_KIND_INVALID;
}

static int semantic_build_function_return_type_descriptor(const AstExternal *external,
    SemanticTypeDescriptor *out_type) {
    if (!external || !out_type || external->kind != AST_EXTERNAL_FUNCTION) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    if (external->has_function_return_signature) {
        out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
        out_type->function_return_type = external->function_return_function_return_type;
        out_type->named_type_name = external->function_return_function_return_type_name;
        out_type->parameter_count = external->function_return_function_parameter_count;
        out_type->parameter_value_kinds = external->function_return_function_parameter_value_kinds;
        out_type->parameter_function_return_types =
            external->function_return_function_parameter_return_types;
        out_type->parameter_function_parameter_counts =
            external->function_return_function_parameter_parameter_counts;
        return 1;
    }

    switch (external->function_return_type) {
    case AST_FUNCTION_RETURN_INT:
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    case AST_FUNCTION_RETURN_FLOAT:
        out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
        return 1;
    case AST_FUNCTION_RETURN_PAIR:
        out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        out_type->aggregate_kind = SEMANTIC_AGGREGATE_KIND_PAIR;
        return 1;
    case AST_FUNCTION_RETURN_STRUCT:
        out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        out_type->aggregate_kind = SEMANTIC_AGGREGATE_KIND_STRUCT;
        out_type->named_type_name = external->function_return_type_name;
        return out_type->named_type_name != NULL;
    case AST_FUNCTION_RETURN_VOID:
        out_type->kind = SEMANTIC_TYPE_KIND_VOID;
        return 1;
    default:
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
        return 0;
    }
}

static int semantic_build_function_parameter_type_descriptor(const AstExternal *external,
    size_t param_index,
    SemanticTypeDescriptor *out_type) {
    SemanticTypeKind kind = SEMANTIC_TYPE_KIND_INVALID;

    if (!external || !out_type || external->kind != AST_EXTERNAL_FUNCTION ||
        param_index >= external->parameter_count) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    if (!semantic_parameter_value_kind_maps_to_type_descriptor_kind(
            external->parameter_value_kinds
                ? external->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT,
            &kind)) {
        return 0;
    }

    out_type->kind = kind;
    if (kind == SEMANTIC_TYPE_KIND_AGGREGATE) {
        out_type->aggregate_kind =
            external->parameter_value_kinds && external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_STRUCT
                ? SEMANTIC_AGGREGATE_KIND_STRUCT
                : SEMANTIC_AGGREGATE_KIND_PAIR;
        out_type->named_type_name =
            external->parameter_type_names
                ? external->parameter_type_names[param_index]
                : NULL;
    }
    out_type->array_rank =
        external->parameter_array_ranks
            ? external->parameter_array_ranks[param_index]
            : 0u;
    if (kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        out_type->function_return_type =
            external->parameter_function_return_types
                ? external->parameter_function_return_types[param_index]
                : AST_FUNCTION_RETURN_INT;
        out_type->parameter_count =
            external->parameter_function_parameter_counts
                ? external->parameter_function_parameter_counts[param_index]
                : 0u;
    }

    return 1;
}

static int semantic_type_descriptors_are_compatible(const SemanticTypeDescriptor *lhs,
    const SemanticTypeDescriptor *rhs) {
    if (!lhs || !rhs || lhs->kind != rhs->kind) {
        return 0;
    }

    if (lhs->array_rank != rhs->array_rank) {
        return 0;
    }
    if (lhs->array_rank > 0u &&
        !semantic_type_descriptor_array_extents_match(lhs, rhs)) {
        return 0;
    }

    if (lhs->kind == SEMANTIC_TYPE_KIND_AGGREGATE) {
        const char *lhs_type_name = lhs->named_type_name ? lhs->named_type_name : "";
        const char *rhs_type_name = rhs->named_type_name ? rhs->named_type_name : "";

        return lhs->aggregate_kind == rhs->aggregate_kind &&
            strcmp(lhs_type_name, rhs_type_name) == 0;
    }

    if (lhs->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        size_t param_index;

        if (lhs->function_return_type != rhs->function_return_type ||
            lhs->parameter_count != rhs->parameter_count) {
            return 0;
        }

        for (param_index = 0u; param_index < lhs->parameter_count; ++param_index) {
            int lhs_param_kind =
                lhs->parameter_value_kinds
                    ? lhs->parameter_value_kinds[param_index]
                    : AST_PARAMETER_VALUE_INT;
            int rhs_param_kind =
                rhs->parameter_value_kinds
                    ? rhs->parameter_value_kinds[param_index]
                    : AST_PARAMETER_VALUE_INT;

            if (lhs_param_kind != rhs_param_kind) {
                return 0;
            }
            if (lhs_param_kind == AST_PARAMETER_VALUE_FUNCTION) {
                AstFunctionReturnType lhs_nested_return_type =
                    lhs->parameter_function_return_types
                        ? lhs->parameter_function_return_types[param_index]
                        : AST_FUNCTION_RETURN_INT;
                AstFunctionReturnType rhs_nested_return_type =
                    rhs->parameter_function_return_types
                        ? rhs->parameter_function_return_types[param_index]
                        : AST_FUNCTION_RETURN_INT;
                size_t lhs_nested_param_count =
                    lhs->parameter_function_parameter_counts
                        ? lhs->parameter_function_parameter_counts[param_index]
                        : 0u;
                size_t rhs_nested_param_count =
                    rhs->parameter_function_parameter_counts
                        ? rhs->parameter_function_parameter_counts[param_index]
                        : 0u;

                if (lhs_nested_return_type != rhs_nested_return_type ||
                    lhs_nested_param_count != rhs_nested_param_count) {
                    return 0;
                }
            }
        }

        return 1;
    }

    return 1;
}

static int semantic_function_signature_matches_type_descriptor(
    const AstExternal *external,
    const SemanticTypeDescriptor *expected_type) {
    SemanticTypeDescriptor actual_type;

    if (!external || !expected_type || expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    memset(&actual_type, 0, sizeof(actual_type));
    actual_type.kind = SEMANTIC_TYPE_KIND_FUNCTION;
    actual_type.function_return_type = external->function_return_type;
    actual_type.named_type_name = external->function_return_type_name;
    actual_type.parameter_count = external->parameter_count;
    actual_type.parameter_value_kinds = external->parameter_value_kinds;
    actual_type.parameter_function_return_types = external->parameter_function_return_types;
    actual_type.parameter_function_parameter_counts =
        external->parameter_function_parameter_counts;
    return semantic_type_descriptors_are_compatible(expected_type, &actual_type);
}

static int semantic_builtin_signature_matches_type_descriptor(
    const SemanticBuiltinFunctionInfo *builtin,
    const SemanticTypeDescriptor *expected_type) {
    SemanticTypeDescriptor actual_type;

    if (!builtin || !expected_type || expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    memset(&actual_type, 0, sizeof(actual_type));
    actual_type.kind = SEMANTIC_TYPE_KIND_FUNCTION;
    actual_type.function_return_type = builtin->return_type;
    actual_type.parameter_count = builtin->parameter_count;
    return semantic_type_descriptors_are_compatible(expected_type, &actual_type);
}

static int semantic_function_value_return_matches_type_descriptor(
    const AstExternal *external,
    const SemanticTypeDescriptor *expected_type) {
    SemanticTypeDescriptor actual_type;

    if (!external || !expected_type || expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION ||
        !external->has_function_return_signature) {
        return 0;
    }
    if (!semantic_build_function_return_type_descriptor(external, &actual_type) ||
        actual_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }
    return semantic_type_descriptors_are_compatible(expected_type, &actual_type);
}

static int semantic_build_closure_type_descriptor(
    const AstExpression *closure_expr,
    SemanticTypeDescriptor *out_type) {
    if (!closure_expr || closure_expr->kind != AST_EXPR_CLOSURE || !out_type) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
    out_type->function_return_type = closure_expr->as.closure.return_type;
    out_type->named_type_name = closure_expr->as.closure.return_type_name;
    out_type->parameter_count = closure_expr->as.closure.parameter_count;
    out_type->parameter_value_kinds = closure_expr->as.closure.parameter_value_kinds;
    return 1;
}

static int semantic_make_function_type_descriptor(
    AstFunctionReturnType return_type,
    size_t parameter_count,
    const int *parameter_value_kinds,
    SemanticTypeDescriptor *out_type) {
    if (!out_type) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
    out_type->function_return_type = return_type;
    out_type->parameter_count = parameter_count;
    out_type->parameter_value_kinds = parameter_value_kinds;
    return 1;
}

static int semantic_function_return_type_requires_value(const AstExternal *external) {
    SemanticTypeDescriptor return_type;

    if (!semantic_build_function_return_type_descriptor(external, &return_type)) {
        return 0;
    }

    return return_type.kind != SEMANTIC_TYPE_KIND_VOID;
}

static int semantic_top_level_declarations_have_compatible_types(const AstExternal *lhs,
    const AstExternal *rhs) {
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;

    if (!semantic_build_top_level_declaration_type_descriptor(lhs, &lhs_type) ||
        !semantic_build_top_level_declaration_type_descriptor(rhs, &rhs_type)) {
        return 0;
    }

    return semantic_type_descriptors_are_compatible(&lhs_type, &rhs_type);
}

static SemanticFunctionSignatureMatchKind semantic_function_declaration_signature_match_kind(
    const AstExternal *lhs,
    const AstExternal *rhs) {
    size_t param_index;
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;

    if (!lhs || !rhs ||
        lhs->kind != AST_EXTERNAL_FUNCTION ||
        rhs->kind != AST_EXTERNAL_FUNCTION) {
        return SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_RETURN_TYPE;
    }

    if (!semantic_build_function_return_type_descriptor(lhs, &lhs_type) ||
        !semantic_build_function_return_type_descriptor(rhs, &rhs_type) ||
        !semantic_type_descriptors_are_compatible(&lhs_type, &rhs_type)) {
        return SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_RETURN_TYPE;
    }

    if (lhs->parameter_count != rhs->parameter_count) {
        return SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_COUNT;
    }

    for (param_index = 0u; param_index < lhs->parameter_count; ++param_index) {
        if (!semantic_build_function_parameter_type_descriptor(lhs, param_index, &lhs_type) ||
            !semantic_build_function_parameter_type_descriptor(rhs, param_index, &rhs_type)) {
            return SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_KIND;
        }

        if (lhs_type.kind != rhs_type.kind) {
            return SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_KIND;
        }

        if ((lhs_type.kind == SEMANTIC_TYPE_KIND_FUNCTION ||
                lhs_type.kind == SEMANTIC_TYPE_KIND_AGGREGATE) &&
            !semantic_type_descriptors_are_compatible(&lhs_type, &rhs_type)) {
            return lhs_type.kind == SEMANTIC_TYPE_KIND_FUNCTION
                ? SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_NESTED_SIGNATURE
                : SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_KIND;
        }
    }

    return SEMANTIC_FUNCTION_SIGNATURE_MATCH;
}

static int semantic_function_parameter_name_is_function_valued(const AstExternal *func, const char *name) {
    SemanticTypeDescriptor param_type;

    return semantic_find_named_function_parameter_type_descriptor(func, name, NULL, &param_type) &&
        param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION;
}

static int semantic_extension_function_value_arg_matches_parameter(const AstExpression *arg_expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    const AstExternal *callee_external,
    size_t param_index) {
    const char *arg_name = NULL;
    const AstExternal *arg_function = NULL;
    const SemanticBuiltinFunctionInfo *builtin = NULL;
    SemanticTypeDescriptor expected_param_type;

    if (!arg_expr || !program || !callee_external ||
        !semantic_build_function_parameter_type_descriptor(callee_external, param_index, &expected_param_type) ||
        expected_param_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    {
        const AstExpression *arg_base = semantic_unwrap_function_value_wrapper_expression(arg_expr);
        if (arg_base && arg_base->kind == AST_EXPR_TERNARY) {
            const AstExpression *then_base =
                unwrap_paren_expression(arg_base->as.ternary.then_expr);
            const AstExpression *else_base =
                unwrap_paren_expression(arg_base->as.ternary.else_expr);

            if (!then_base || !else_base ||
                then_base->kind != AST_EXPR_IDENTIFIER ||
                else_base->kind != AST_EXPR_IDENTIFIER) {
                return 0;
            }
            return semantic_extension_function_value_arg_matches_parameter(
                    then_base,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    callee_external,
                    param_index) &&
                semantic_extension_function_value_arg_matches_parameter(
                    else_base,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    callee_external,
                    param_index);
        }
        if (arg_base && arg_base->kind == AST_EXPR_CLOSURE) {
            return semantic_closure_signature_matches_expected(arg_base, &expected_param_type);
        }
        if (arg_base && arg_base->kind == AST_EXPR_CALL) {
            const char *called_name = NULL;
            const AstExternal *visible_function = NULL;

            if (classify_ast_call_callee(arg_base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
                !called_name || called_name[0] == '\0') {
                return 0;
            }
            visible_function = semantic_find_visible_function_external(
                program,
                func_index,
                include_current_external,
                called_name);
            if (!visible_function ||
                !semantic_function_value_return_matches_type_descriptor(
                    visible_function,
                    &expected_param_type)) {
                return 0;
            }
            return 1;
        }
    }

    if (classify_ast_call_callee(arg_expr, &arg_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !arg_name || arg_name[0] == '\0') {
        return 0;
    }

    if (current_func) {
        SemanticTypeDescriptor current_param_type;

        if (semantic_find_named_function_parameter_type_descriptor(
                current_func,
                arg_name,
                NULL,
                &current_param_type) &&
            current_param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
            return semantic_type_descriptors_are_compatible(
                &expected_param_type,
                &current_param_type);
        }
    }

    if (current_func && program) {
        int closure_backed = 0;
        const char *resolved_name = NULL;

        if (semantic_extension_resolve_local_function_value_binding_with_expected_type(
                program,
                func_index,
                include_current_external,
                current_func,
                arg_name,
                expected_param_type.parameter_count,
                &expected_param_type,
                &closure_backed,
                &resolved_name)) {
            if (closure_backed) {
                return 1;
            }
            if (resolved_name) {
                arg_name = resolved_name;
            }
        }
    }

    arg_function = semantic_find_visible_function_external(
        program,
        func_index,
        include_current_external,
        arg_name);
    if (!arg_function) {
        builtin = semantic_find_builtin_function(arg_name);
        if (!builtin) {
            return 0;
        }
        return semantic_builtin_signature_matches_type_descriptor(
            builtin,
            &expected_param_type);
    }

    return semantic_function_signature_matches_type_descriptor(
        arg_function,
        &expected_param_type);
}

static int semantic_current_function_has_local_function_value_name(
    const AstExternal *current_func,
    const char *name) {
    const AstStatement *body = NULL;
    size_t child_index;

    if (!current_func || !name || name[0] == '\0' ||
        !current_func->function_body ||
        current_func->function_body->kind != AST_STMT_COMPOUND) {
        return 0;
    }

    body = current_func->function_body;
    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children ? body->children[child_index] : NULL;
        size_t decl_index;

        if (!child || child->kind != AST_STMT_DECLARATION ||
            !child->declaration_names || !child->declaration_value_kinds) {
            continue;
        }
        for (decl_index = 0u; decl_index < child->declaration_name_count; ++decl_index) {
            if (child->declaration_names[decl_index] &&
                strcmp(child->declaration_names[decl_index], name) == 0 &&
                child->declaration_value_kinds[decl_index] == AST_DECLARATION_VALUE_FUNCTION) {
                return 1;
            }
        }
    }

    return 0;
}

static int semantic_build_local_function_value_declaration_type_descriptor(
    const AstStatement *stmt,
    size_t declaration_index,
    SemanticTypeDescriptor *out_type) {
    if (!out_type) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    if (!stmt ||
        stmt->kind != AST_STMT_DECLARATION ||
        !stmt->declaration_value_kinds ||
        declaration_index >= stmt->declaration_name_count ||
        stmt->declaration_value_kinds[declaration_index] != AST_DECLARATION_VALUE_FUNCTION) {
        return 0;
    }

    out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
    out_type->function_return_type =
        stmt->declaration_function_return_types
            ? stmt->declaration_function_return_types[declaration_index]
            : AST_FUNCTION_RETURN_INT;
    out_type->parameter_count =
        stmt->declaration_function_parameter_counts
            ? stmt->declaration_function_parameter_counts[declaration_index]
            : 0u;
    out_type->parameter_value_kinds =
        stmt->declaration_function_parameter_value_kinds
            ? stmt->declaration_function_parameter_value_kinds[declaration_index]
            : NULL;
    return 1;
}

static int semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
    const AstExternal *current_func,
    const char *name,
    SemanticTypeDescriptor *out_type) {
    const AstStatement *body = NULL;
    size_t child_index;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!current_func || !name || name[0] == '\0' ||
        !current_func->function_body ||
        current_func->function_body->kind != AST_STMT_COMPOUND) {
        return 0;
    }

    body = current_func->function_body;
    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children ? body->children[child_index] : NULL;
        size_t decl_index;

        if (!child || child->kind != AST_STMT_DECLARATION ||
            !child->declaration_names || !child->declaration_value_kinds) {
            continue;
        }
        for (decl_index = 0u; decl_index < child->declaration_name_count; ++decl_index) {
            if (child->declaration_names[decl_index] &&
                strcmp(child->declaration_names[decl_index], name) == 0 &&
                child->declaration_value_kinds[decl_index] == AST_DECLARATION_VALUE_FUNCTION) {
                return semantic_build_local_function_value_declaration_type_descriptor(
                    child,
                    decl_index,
                    out_type);
            }
        }
    }

    return 0;
}

static int semantic_extension_resolve_local_function_value_binding_with_expected_type(
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    const char *name,
    size_t max_parameter_count,
    const SemanticTypeDescriptor *expected_type,
    int *out_is_closure_backed,
    const char **out_resolved_name) {
    const AstStatement *body = NULL;
    size_t child_index;

    if (out_is_closure_backed) {
        *out_is_closure_backed = 0;
    }
    if (out_resolved_name) {
        *out_resolved_name = NULL;
    }
    if (!current_func || !name || name[0] == '\0' ||
        !expected_type ||
        expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION ||
        !current_func->function_body ||
        current_func->function_body->kind != AST_STMT_COMPOUND) {
        return 0;
    }

    body = current_func->function_body;
    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children ? body->children[child_index] : NULL;
        size_t decl_index;

        if (!child || child->kind != AST_STMT_DECLARATION ||
            !child->declaration_names || !child->declaration_value_kinds ||
            !child->expressions) {
            continue;
        }
        for (decl_index = 0u; decl_index < child->declaration_name_count &&
                decl_index < child->expression_count; ++decl_index) {
            const AstExpression *init_expr;
            const AstExpression *init_base;
            SemanticTypeDescriptor local_type;

            if (!child->declaration_names[decl_index] ||
                strcmp(child->declaration_names[decl_index], name) != 0 ||
                child->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_FUNCTION) {
                continue;
            }
            if (!semantic_build_local_function_value_declaration_type_descriptor(
                    child,
                    decl_index,
                    &local_type) ||
                !semantic_type_descriptors_are_compatible(expected_type, &local_type) ||
                local_type.parameter_count > max_parameter_count) {
                return 0;
            }

            init_expr = child->expressions[decl_index];
            init_base = unwrap_paren_expression(init_expr);
            if (!init_base) {
                return 0;
            }
            if (init_base->kind == AST_EXPR_CLOSURE) {
                if (out_is_closure_backed) {
                    *out_is_closure_backed = 1;
                }
                if (out_resolved_name) {
                    *out_resolved_name = child->declaration_names[decl_index];
                }
                return 1;
            }
            if (init_base->kind == AST_EXPR_CALL) {
                const char *called_name = NULL;
                const AstExternal *visible_function = NULL;

                if (classify_ast_call_callee(init_base->as.call.callee, &called_name) !=
                        AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
                    !called_name || called_name[0] == '\0') {
                    return 0;
                }
                visible_function = semantic_find_visible_function_external(
                    program,
                    func_index,
                    1,
                    called_name);
                if (!visible_function ||
                    !semantic_function_value_return_matches_type_descriptor(
                        visible_function,
                        expected_type)) {
                    return 0;
                }
                if (out_is_closure_backed) {
                    *out_is_closure_backed = 1;
                }
                if (out_resolved_name) {
                    *out_resolved_name = child->declaration_names[decl_index];
                }
                return 1;
            }
            if (init_base->kind == AST_EXPR_IDENTIFIER && init_base->as.identifier.name) {
                const char *resolved_name = init_base->as.identifier.name;
                const AstExternal *visible_function = semantic_find_visible_function_external(
                    program,
                    func_index,
                    include_current_external,
                    resolved_name);
                if (visible_function &&
                    semantic_function_signature_matches_type_descriptor(
                        visible_function,
                        expected_type)) {
                    if (out_resolved_name) {
                        *out_resolved_name = visible_function->name;
                    }
                    return 1;
                }
                if (semantic_builtin_signature_matches_type_descriptor(
                        semantic_find_builtin_function(resolved_name),
                        expected_type)) {
                    if (out_resolved_name) {
                        *out_resolved_name = resolved_name;
                    }
                    return 1;
                }
                return semantic_extension_resolve_local_function_value_binding_with_expected_type(
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    resolved_name,
                    max_parameter_count,
                    expected_type,
                    out_is_closure_backed,
                    out_resolved_name);
            }
            return 0;
        }
    }

    return 0;
}

static int semantic_extension_local_function_value_assignment_statement_is_allowed(
    const AstStatement *stmt,
    const AstProgram *program,
    size_t func_index,
    const AstExternal *current_func) {
    const AstExpression *expr = NULL;
    const AstExpression *base = NULL;
    const AstExpression *lhs_base = NULL;
    const AstExpression *rhs_base = NULL;
    const char *rhs_name = NULL;
    SemanticTypeDescriptor expected_type;
    int lhs_closure_backed = 0;

    if (!stmt || stmt->kind != AST_STMT_EXPRESSION ||
        stmt->expression_count != 1u || !stmt->expressions ||
        !current_func || !current_func->function_body ||
        current_func->function_body->kind != AST_STMT_COMPOUND) {
        return 0;
    }

    expr = stmt->expressions[0];
    base = unwrap_paren_expression(expr);
    if (!base || base->kind != AST_EXPR_BINARY || base->as.binary.op != TOKEN_ASSIGN) {
        return 0;
    }
    lhs_base = unwrap_paren_expression(base->as.binary.left);
    rhs_base = unwrap_paren_expression(base->as.binary.right);
    if (!lhs_base || !rhs_base ||
        lhs_base->kind != AST_EXPR_IDENTIFIER || !lhs_base->as.identifier.name) {
        return 0;
    }

    if (!semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
            current_func,
            lhs_base->as.identifier.name,
            &expected_type) ||
        expected_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }
    if (current_func && program &&
        !semantic_extension_resolve_local_function_value_binding_with_expected_type(
            program,
            func_index,
            1,
            current_func,
            lhs_base->as.identifier.name,
            expected_type.parameter_count,
            &expected_type,
            &lhs_closure_backed,
            NULL)) {
        return 0;
    }

    if (rhs_base->kind == AST_EXPR_TERNARY) {
        return semantic_extension_function_value_initializer_expr_matches_expected(
            base->as.binary.right,
            program,
            func_index,
            1,
            current_func,
            &expected_type,
            NULL);
    }

    if (rhs_base->kind != AST_EXPR_IDENTIFIER || !rhs_base->as.identifier.name) {
        return 0;
    }

    rhs_name = rhs_base->as.identifier.name;
    if (current_func &&
        semantic_function_parameter_name_is_function_valued(current_func, rhs_name)) {
        return 0;
    }
    if (current_func && program) {
        int rhs_closure_backed = 0;
        if (semantic_extension_resolve_local_function_value_binding_with_expected_type(
                program,
                func_index,
                1,
                current_func,
                rhs_name,
                expected_type.parameter_count,
                &expected_type,
                &rhs_closure_backed,
                NULL)) {
            return lhs_closure_backed == rhs_closure_backed;
        }
    }
        if (program) {
            const AstExternal *visible_function = semantic_find_visible_function_external(
                program,
                func_index,
                1,
                rhs_name);
        if (visible_function && visible_function->kind == AST_EXTERNAL_FUNCTION) {
            return !lhs_closure_backed &&
                semantic_function_signature_matches_type_descriptor(
                    visible_function,
                    &expected_type);
        }
    }
    {
        const SemanticBuiltinFunctionInfo *builtin = semantic_find_builtin_function(rhs_name);
        if (builtin) {
            return !lhs_closure_backed &&
                semantic_builtin_signature_matches_type_descriptor(
                    builtin,
                    &expected_type);
        }
    }

    return 0;
}

static int semantic_extension_check_function_value_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    int in_call_callee,
    SemanticError *error,
    size_t depth) {
    size_t i;

    if (!expr) {
        return 1;
    }
    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        if (!in_call_callee && expr->as.identifier.name) {
            const AstExternal *visible_function = NULL;

            if (current_func &&
                semantic_function_parameter_name_is_function_valued(current_func, expr->as.identifier.name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-017: uses of function-valued parameters are not yet enabled in the current extension semantic slice");
                return 0;
            }
            if (current_func &&
                semantic_current_function_has_local_function_value_name(
                    current_func,
                    expr->as.identifier.name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-019: function values are not yet enabled in the current extension semantic slice");
                return 0;
            }

            visible_function = semantic_find_visible_function_external(
                program,
                func_index,
                include_current_external,
                expr->as.identifier.name);
            if (visible_function) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-019: function values are not yet enabled in the current extension semantic slice");
                return 0;
            }
        }
        return 1;
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0u; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_extension_check_function_value_expression(
                    expr->as.init_list.items[i],
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    0,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_extension_check_function_value_expression(
            expr->as.inner,
            program,
            func_index,
            include_current_external,
            current_func,
            in_call_callee,
            error,
            depth + 1u);
    case AST_EXPR_UNARY:
        return semantic_extension_check_function_value_expression(
            expr->as.unary.operand,
            program,
            func_index,
            include_current_external,
            current_func,
            0,
            error,
            depth + 1u);
    case AST_EXPR_POSTFIX:
        return semantic_extension_check_function_value_expression(
            expr->as.postfix.operand,
            program,
            func_index,
            include_current_external,
            current_func,
            0,
            error,
            depth + 1u);
    case AST_EXPR_SUBSCRIPT:
        return semantic_extension_check_function_value_expression(
                expr->as.subscript.base,
                program,
                func_index,
                include_current_external,
                current_func,
                0,
                error,
                depth + 1u) &&
            semantic_extension_check_function_value_expression(
                expr->as.subscript.index,
                program,
                func_index,
                include_current_external,
                current_func,
                0,
                error,
                depth + 1u);
    case AST_EXPR_BINARY:
        if (in_call_callee &&
            expr->as.binary.op == TOKEN_COMMA) {
            return semantic_extension_check_function_value_expression(
                    expr->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    0,
                    error,
                    depth + 1u) &&
                semantic_extension_check_function_value_expression(
                    expr->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    1,
                    error,
                    depth + 1u);
        }
        if (in_call_callee && expr->as.binary.op == TOKEN_ASSIGN) {
            return semantic_extension_check_function_value_expression(
                    expr->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    1,
                    error,
                    depth + 1u) &&
                semantic_extension_check_function_value_expression(
                    expr->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    1,
                    error,
                    depth + 1u);
        }
        return semantic_extension_check_function_value_expression(
                expr->as.binary.left,
                program,
                func_index,
                include_current_external,
                current_func,
                0,
                error,
                depth + 1u) &&
            semantic_extension_check_function_value_expression(
                expr->as.binary.right,
                program,
                func_index,
                include_current_external,
                current_func,
                0,
                error,
                depth + 1u);
    case AST_EXPR_TERNARY:
        return semantic_extension_check_function_value_expression(
                expr->as.ternary.condition,
                program,
                func_index,
                include_current_external,
                current_func,
                0,
                error,
                depth + 1u) &&
            semantic_extension_check_function_value_expression(
                expr->as.ternary.then_expr,
                program,
                func_index,
                include_current_external,
                current_func,
                in_call_callee,
                error,
                depth + 1u) &&
            semantic_extension_check_function_value_expression(
                expr->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                current_func,
                in_call_callee,
                error,
                depth + 1u);
    case AST_EXPR_CLOSURE:
        return 1;
    case AST_EXPR_CALL: {
        const char *called_name = NULL;
        const AstExternal *visible_function = NULL;
        int callee_is_function_param = 0;

        if (classify_ast_call_callee(expr->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || called_name[0] == '\0') {
            if (!semantic_extension_check_function_value_expression(
                    expr->as.call.callee,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    1,
                    error,
                    depth + 1u)) {
                return 0;
            }
            for (i = 0u; i < expr->as.call.arg_count; ++i) {
                if (!semantic_extension_check_function_value_expression(
                        expr->as.call.args[i],
                        program,
                        func_index,
                        include_current_external,
                        current_func,
                        0,
                        error,
                        depth + 1u)) {
                    return 0;
                }
            }
            return 1;
        }

        callee_is_function_param = current_func &&
            semantic_function_parameter_name_is_function_valued(current_func, called_name);

        if (callee_is_function_param) {
            for (i = 0u; i < expr->as.call.arg_count; ++i) {
                if (!semantic_extension_check_function_value_expression(
                        expr->as.call.args[i],
                        program,
                        func_index,
                        include_current_external,
                        current_func,
                        0,
                        error,
                        depth + 1u)) {
                    return 0;
                }
            }
            return 1;
        }

        visible_function = semantic_find_visible_function_external(
            program,
            func_index,
            include_current_external,
            called_name);
        if (!semantic_extension_check_function_value_expression(
                expr->as.call.callee,
                program,
                func_index,
                include_current_external,
                current_func,
                1,
                error,
                depth + 1u)) {
            return 0;
        }
        for (i = 0u; i < expr->as.call.arg_count; ++i) {
            SemanticTypeDescriptor param_type;
            int expects_function_value = visible_function &&
                semantic_build_function_parameter_type_descriptor(
                    visible_function,
                    i,
                    &param_type) &&
                param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION;

            if (expects_function_value) {
                const AstExpression *arg_base =
                    semantic_unwrap_function_value_wrapper_expression(expr->as.call.args[i]);
                int arg_is_supported_ternary =
                    arg_base && arg_base->kind == AST_EXPR_TERNARY;
                if (!semantic_extension_function_value_arg_matches_parameter(
                        expr->as.call.args[i],
                        program,
                        func_index,
                        include_current_external,
                        current_func,
                        visible_function,
                        i)) {
                    semantic_set_error(error,
                        expr->line,
                        expr->column,
                        "SEMA-EXT-018: calls to functions with function-valued parameters are not yet enabled in the current extension semantic slice");
                    return 0;
                }
                if (!(arg_base && arg_base->kind == AST_EXPR_CLOSURE) &&
                    !arg_is_supported_ternary &&
                    !semantic_extension_local_function_value_argument_is_allowed(
                        expr->as.call.args[i],
                        program,
                        func_index,
                        include_current_external,
                        current_func)) {
                    semantic_set_error(error,
                        expr->line,
                        expr->column,
                        "SEMA-EXT-019: function values are not yet enabled in the current extension semantic slice");
                    return 0;
                }
                continue;
            }

            if (!semantic_extension_check_function_value_expression(
                    expr->as.call.args[i],
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    0,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        if (visible_function &&
            semantic_external_has_function_valued_parameters(visible_function)) {
            return 1;
        }
        return 1;
    }
    default:
        return 1;
    }
}

static int semantic_extension_local_function_value_initializer_is_allowed(
    const AstStatement *stmt,
    size_t declaration_index,
    const AstProgram *program,
    size_t func_index,
    const AstExternal *current_func) {
    const AstExpression *expr;
    const AstExpression *base;
    const AstExternal *visible_function;
    SemanticTypeDescriptor expected_type;

    if (!stmt ||
        stmt->kind != AST_STMT_DECLARATION ||
        !stmt->declaration_value_kinds ||
        declaration_index >= stmt->declaration_name_count ||
        stmt->declaration_value_kinds[declaration_index] != AST_DECLARATION_VALUE_FUNCTION ||
        !stmt->expressions ||
        declaration_index >= stmt->expression_count) {
        return 0;
    }
    expr = stmt->expressions[declaration_index];
    base = semantic_unwrap_function_value_wrapper_expression(expr);
    if (!base) {
        return 0;
    }
    if (!semantic_make_function_type_descriptor(
            stmt->declaration_function_return_types
                ? stmt->declaration_function_return_types[declaration_index]
                : AST_FUNCTION_RETURN_INT,
            stmt->declaration_function_parameter_counts
                ? stmt->declaration_function_parameter_counts[declaration_index]
                : 0u,
            stmt->declaration_function_parameter_value_kinds
                ? stmt->declaration_function_parameter_value_kinds[declaration_index]
                : NULL,
            &expected_type)) {
        return 0;
    }

    if (base->kind == AST_EXPR_TERNARY) {
        return semantic_extension_function_value_initializer_expr_matches_expected(
            expr,
            program,
            func_index,
            1,
            current_func,
            &expected_type,
            NULL);
    }
    if (base->kind == AST_EXPR_CALL) {
        const char *called_name = NULL;

        if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || called_name[0] == '\0') {
            return 0;
        }
        visible_function = semantic_find_visible_function_external(
            program,
            func_index,
            1,
            called_name);
        return visible_function != NULL &&
            semantic_function_value_return_matches_type_descriptor(
                visible_function,
                &expected_type);
    }
    if (base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }

    if (current_func &&
        semantic_function_parameter_name_is_function_valued(current_func, base->as.identifier.name)) {
        return 1;
    }

    if (stmt &&
        stmt->declaration_names &&
        stmt->declaration_value_kinds) {
        size_t prior_decl_index;

        for (prior_decl_index = 0u; prior_decl_index < declaration_index; ++prior_decl_index) {
            if (stmt->declaration_names[prior_decl_index] &&
                strcmp(stmt->declaration_names[prior_decl_index], base->as.identifier.name) == 0 &&
                stmt->declaration_value_kinds[prior_decl_index] == AST_DECLARATION_VALUE_FUNCTION) {
                return 1;
            }
        }
    }

    if (current_func &&
        current_func->function_body &&
        current_func->function_body->kind == AST_STMT_COMPOUND) {
        const AstStatement *body = current_func->function_body;
        size_t child_index;

        for (child_index = 0u; child_index < body->child_count; ++child_index) {
            const AstStatement *child = body->children ? body->children[child_index] : NULL;
            size_t local_decl_index;

            if (!child || child == stmt ||
                child->kind != AST_STMT_DECLARATION ||
                !child->declaration_names ||
                !child->declaration_value_kinds) {
                continue;
            }
            for (local_decl_index = 0u; local_decl_index < child->declaration_name_count; ++local_decl_index) {
                if (child->declaration_names[local_decl_index] &&
                    strcmp(child->declaration_names[local_decl_index], base->as.identifier.name) == 0 &&
                    child->declaration_value_kinds[local_decl_index] == AST_DECLARATION_VALUE_FUNCTION) {
                    return 1;
                }
            }
        }
    }

    visible_function = semantic_find_visible_function_external(
        program,
        func_index,
        1,
        base->as.identifier.name);
    return visible_function != NULL;
}

static int semantic_extension_local_function_value_argument_is_allowed(
    const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func) {
    const char *arg_name = NULL;

    {
        const AstExpression *base = semantic_unwrap_function_value_wrapper_expression(expr);
        if (base && base->kind == AST_EXPR_TERNARY) {
            const AstExpression *then_base = unwrap_paren_expression(base->as.ternary.then_expr);
            const AstExpression *else_base = unwrap_paren_expression(base->as.ternary.else_expr);

            if (!then_base || !else_base ||
                then_base->kind != AST_EXPR_IDENTIFIER ||
                else_base->kind != AST_EXPR_IDENTIFIER) {
                return 0;
            }
            return semantic_extension_local_function_value_argument_is_allowed(
                    then_base,
                    program,
                    func_index,
                    include_current_external,
                    current_func) &&
                semantic_extension_local_function_value_argument_is_allowed(
                    else_base,
                    program,
                    func_index,
                    include_current_external,
                    current_func);
        }
        if (base && base->kind == AST_EXPR_CLOSURE) {
            return 1;
        }
        if (base && base->kind == AST_EXPR_CALL) {
            const char *called_name = NULL;
            const AstExternal *visible_function = NULL;

            if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
                !called_name || called_name[0] == '\0') {
                return 0;
            }
            visible_function = semantic_find_visible_function_external(
                program,
                func_index,
                include_current_external,
                called_name);
            return visible_function != NULL && visible_function->has_function_return_signature;
        }
    }

    if (!expr ||
        classify_ast_call_callee(expr, &arg_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !arg_name || arg_name[0] == '\0') {
        return 0;
    }

    if (current_func &&
        semantic_function_parameter_name_is_function_valued(current_func, arg_name)) {
        return 1;
    }

    if (current_func &&
        semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
            current_func,
            arg_name,
            &(SemanticTypeDescriptor){0})) {
        return 1;
    }

    if (semantic_find_visible_function_external(
            program,
            func_index,
            include_current_external,
            arg_name)) {
        return 1;
    }

    return semantic_find_builtin_function(arg_name) != NULL;
}

static int semantic_extension_function_value_initializer_expr_matches_expected(
    const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
    const SemanticTypeDescriptor *expected_type,
    int *out_is_closure_backed) {
    const AstExpression *base;
    const AstExternal *visible_function = NULL;
    int closure_backed = 0;

    if (out_is_closure_backed) {
        *out_is_closure_backed = 0;
    }
    if (!expr || !expected_type || expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    base = semantic_unwrap_function_value_wrapper_expression(expr);
    if (!base) {
        return 0;
    }

    if (base->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_base =
            unwrap_paren_expression(base->as.ternary.then_expr);
        const AstExpression *else_base =
            unwrap_paren_expression(base->as.ternary.else_expr);
        int then_closure_backed = 0;
        int else_closure_backed = 0;

        if (!then_base || !else_base ||
            then_base->kind != AST_EXPR_IDENTIFIER ||
            else_base->kind != AST_EXPR_IDENTIFIER) {
            return 0;
        }
        if (!semantic_extension_function_value_initializer_expr_matches_expected(
                then_base,
                program,
                func_index,
                include_current_external,
                current_func,
                expected_type,
                &then_closure_backed) ||
            !semantic_extension_function_value_initializer_expr_matches_expected(
                else_base,
                program,
                func_index,
                include_current_external,
                current_func,
                expected_type,
                &else_closure_backed)) {
            return 0;
        }
        if (then_closure_backed != else_closure_backed) {
            return 0;
        }
        if (out_is_closure_backed) {
            *out_is_closure_backed = then_closure_backed;
        }
        return 1;
    }

    if (base->kind == AST_EXPR_CALL) {
        const char *called_name = NULL;

        if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || called_name[0] == '\0') {
            return 0;
        }
        visible_function = semantic_find_visible_function_external(
            program,
            func_index,
            1,
            called_name);
        if (!visible_function ||
            !semantic_function_value_return_matches_type_descriptor(
                visible_function,
                expected_type)) {
            return 0;
        }
        if (out_is_closure_backed) {
            *out_is_closure_backed = 1;
        }
        return 1;
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
        if (current_func && program) {
            const char *resolved_name = NULL;
            if (semantic_extension_resolve_local_function_value_binding_with_expected_type(
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    base->as.identifier.name,
                    expected_type->parameter_count,
                    expected_type,
                    &closure_backed,
                    &resolved_name)) {
                if (out_is_closure_backed) {
                    *out_is_closure_backed = closure_backed;
                }
                return 1;
            }
        }

        visible_function = semantic_find_visible_function_external(
            program,
            func_index,
            include_current_external,
            base->as.identifier.name);
        if (visible_function) {
            return semantic_function_signature_matches_type_descriptor(
                visible_function,
                expected_type);
        }

        {
            const SemanticBuiltinFunctionInfo *builtin =
                semantic_find_builtin_function(base->as.identifier.name);
            if (builtin) {
                return semantic_builtin_signature_matches_type_descriptor(
                    builtin,
                    expected_type);
            }
        }
    }

    return 0;
}

static int semantic_extension_check_function_value_statement(const AstStatement *stmt,
    const AstProgram *program,
    size_t func_index,
    const AstExternal *current_func,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    for (i = 0u; i < stmt->expression_count; ++i) {
        if (stmt->kind == AST_STMT_RETURN && current_func) {
            SemanticTypeDescriptor return_type;

            if (semantic_scope_build_function_return_type_descriptor(current_func, &return_type) &&
                return_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                continue;
            }
        }
        if (stmt->kind == AST_STMT_DECLARATION &&
            semantic_extension_local_function_value_initializer_is_allowed(
                stmt,
                i,
                program,
                func_index,
                current_func)) {
            continue;
        }
        if (i == 0u &&
            semantic_extension_local_function_value_assignment_statement_is_allowed(
                stmt,
                program,
                func_index,
                current_func)) {
            continue;
        }
        if (!semantic_extension_check_function_value_expression(
                stmt->expressions ? stmt->expressions[i] : NULL,
                program,
                func_index,
                1,
                current_func,
                0,
                error,
                0u)) {
            return 0;
        }
    }

    for (i = 0u; i < stmt->child_count; ++i) {
        if (!semantic_extension_check_function_value_statement(
                stmt->children ? stmt->children[i] : NULL,
                program,
                func_index,
                current_func,
                error)) {
            return 0;
        }
    }

    return 1;
}

int semantic_analyze_program_with_options(
    const AstProgram *program,
    const SemanticOptions *options,
    SemanticError *error) {
    size_t i;
    size_t j;
    SemanticAnalyzeConfig config;

    if (!program) {
        semantic_set_error(error, 0, 0, "SEMA-INT-013: semantic input program is NULL");
        return 0;
    }

    config.skip_all_paths_return_check =
        options && options->skip_all_paths_return_check ? 1 : 0;
    config.allow_extension_features =
        options && options->allow_extension_features ? 1 : 0;

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    for (i = 0u; i < program->struct_type_count; ++i) {
        size_t j;
        const AstStructType *lhs_struct = &program->struct_types[i];

        if (!config.allow_extension_features) {
            semantic_set_error(error, 0, 0, "SEMA-EXT-021: 'struct' is available only under -extension mode");
            return 0;
        }
        if (!lhs_struct->name || lhs_struct->name[0] == '\0') {
            semantic_set_error(error, 0, 0, "SEMA-STRUCT-001: struct type must have a name");
            return 0;
        }
        if (lhs_struct->field_count == 0u || !lhs_struct->field_names || !lhs_struct->field_value_kinds) {
            semantic_set_error(error, 0, 0, "SEMA-STRUCT-002: current extension struct slice requires at least one supported field");
            return 0;
        }
        for (j = 0u; j < lhs_struct->field_count; ++j) {
            size_t k;

            if (!lhs_struct->field_names[j] || lhs_struct->field_names[j][0] == '\0') {
                semantic_set_error(error, 0, 0, "SEMA-STRUCT-007: struct fields must have names");
                return 0;
            }
            switch (lhs_struct->field_value_kinds[j]) {
            case AST_DECLARATION_VALUE_INT:
            case AST_DECLARATION_VALUE_PAIR:
                break;
            case AST_DECLARATION_VALUE_STRUCT:
                if (!lhs_struct->field_type_names ||
                    !lhs_struct->field_type_names[j] ||
                    !semantic_find_struct_type(program, lhs_struct->field_type_names[j])) {
                    semantic_set_error(error, 0, 0, "SEMA-STRUCT-004: unknown struct type in struct field declaration");
                    return 0;
                }
                break;
            default:
                semantic_set_error(error, 0, 0, "SEMA-STRUCT-009: unsupported struct field type in current extension slice");
                return 0;
            }
            for (k = j + 1u; k < lhs_struct->field_count; ++k) {
                if (lhs_struct->field_names[k] &&
                    strcmp(lhs_struct->field_names[j], lhs_struct->field_names[k]) == 0) {
                    semantic_set_error(error, 0, 0, "SEMA-STRUCT-008: duplicate struct field name");
                    return 0;
                }
            }
        }
        for (j = i + 1u; j < program->struct_type_count; ++j) {
            const AstStructType *rhs_struct = &program->struct_types[j];

            if (rhs_struct->name && strcmp(lhs_struct->name, rhs_struct->name) == 0) {
                semantic_set_error(error, 0, 0, "SEMA-STRUCT-003: duplicate struct type definition");
                return 0;
            }
        }
    }

    for (i = 0; i < program->count; ++i) {
        const AstExternal *lhs = &program->externals[i];
        int returns_all_paths = 0;
        size_t param_index = 0u;

        /* AST contract allows unnamed externals (name_token == NULL). */
        if (!lhs->name || lhs->name_length == 0) {
            continue;
        }

        if (lhs->kind == AST_EXTERNAL_DECLARATION &&
            lhs->is_const_qualified &&
            !lhs->has_initializer) {
            if (error) {
                char preview[128];
                char msg[512];

                format_callee_preview(lhs->name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CONST-002: identifier=%s; const declaration requires initializer",
                    preview);
                semantic_set_error(error, lhs->line, lhs->column, msg);
            }
            return 0;
        }

        if (lhs->kind == AST_EXTERNAL_DECLARATION && lhs->has_initializer) {
            SemanticTypeDescriptor declaration_type;

            if (!lhs->declaration_initializer) {
                semantic_set_error(
                    error,
                    lhs->line,
                    lhs->column,
                    "SEMA-INT-011: top-level declaration initializer metadata missing AST expression");
                return 0;
            }

            if (!semantic_check_top_level_initializer_callable_rules(program,
                i,
                lhs->declaration_initializer,
                error)) {
                return 0;
            }

            if (!semantic_scope_check_top_level_initializer_expression(lhs->declaration_initializer,
                program,
                i,
                config.allow_extension_features,
                error)) {
                return 0;
            }
            if (semantic_build_top_level_declaration_type_descriptor(lhs, &declaration_type) &&
                !semantic_validate_scalar_initializer_type(lhs->declaration_initializer,
                    &declaration_type,
                    program,
                    i,
                    config.allow_extension_features,
                    NULL,
                    error)) {
                return 0;
            }
        }

        if (lhs->kind == AST_EXTERNAL_DECLARATION &&
            lhs->declaration_value_kind == AST_DECLARATION_VALUE_FLOAT &&
            lhs->declaration_array_rank > 0u) {
            semantic_set_error(error,
                lhs->line,
                lhs->column,
                "SEMA-EXT-037: arrays of 'float' are not supported in the current extension slice");
            return 0;
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION &&
            lhs->parameter_value_kinds &&
            !config.allow_extension_features) {
            for (param_index = 0u; param_index < lhs->parameter_count; ++param_index) {
                SemanticTypeDescriptor param_type;

                if (semantic_build_function_parameter_type_descriptor(lhs, param_index, &param_type) &&
                    param_type.kind == SEMANTIC_TYPE_KIND_AGGREGATE) {
                    semantic_set_error(error,
                        lhs->line,
                        lhs->column,
                        param_type.aggregate_kind == SEMANTIC_AGGREGATE_KIND_PAIR
                            ? "SEMA-EXT-020: 'pair' is available only under -extension mode"
                            : "SEMA-EXT-021: 'struct' is available only under -extension mode");
                    return 0;
                }
                if (semantic_build_function_parameter_type_descriptor(lhs, param_index, &param_type) &&
                    param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                    semantic_set_error(error,
                        lhs->line,
                        lhs->column,
                        "SEMA-EXT-015: function-valued parameters are available only under -extension mode");
                    return 0;
                }
            }
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION &&
            lhs->has_function_return_signature &&
            !config.allow_extension_features) {
            semantic_set_error(error,
                lhs->line,
                lhs->column,
                "SEMA-EXT-015: function-valued returns are available only under -extension mode");
            return 0;
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION &&
            lhs->has_function_return_signature &&
            config.allow_extension_features &&
            lhs->is_function_definition &&
            lhs->function_body) {
            if (!semantic_check_function_return_shape_rules(lhs, error)) {
                return 0;
            }
            if (!semantic_function_valued_return_definition_has_consistent_closure_family(
                    lhs,
                    program,
                    i,
                    error)) {
                return 0;
            }
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && !config.allow_extension_features) {
            if (lhs->function_return_type == AST_FUNCTION_RETURN_PAIR) {
                semantic_set_error(error,
                    lhs->line,
                    lhs->column,
                    "SEMA-EXT-020: 'pair' is available only under -extension mode");
                return 0;
            }
            if (lhs->function_return_type == AST_FUNCTION_RETURN_STRUCT) {
                semantic_set_error(error,
                    lhs->line,
                    lhs->column,
                    "SEMA-EXT-021: 'struct' is available only under -extension mode");
                return 0;
            }
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION &&
            lhs->function_return_type == AST_FUNCTION_RETURN_STRUCT &&
            (!lhs->function_return_type_name ||
                !semantic_find_struct_type(program, lhs->function_return_type_name))) {
            semantic_set_error(error,
                lhs->line,
                lhs->column,
                "SEMA-STRUCT-004: unknown struct type in local declaration");
            return 0;
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && lhs->is_function_definition) {
            if (!semantic_check_function_callable_scope_shadow_rules(lhs, error)) {
                return 0;
            }

            if (!config.allow_extension_features &&
                !semantic_check_function_callable_rules(program, i, lhs, error)) {
                return 0;
            }

            if (!semantic_check_function_scope_rules(program, i, lhs, &config, error)) {
                return 0;
            }

            if (config.allow_extension_features &&
                !semantic_extension_check_function_value_statement(
                    lhs->function_body,
                    program,
                    i,
                    lhs,
                    error)) {
                return 0;
            }

             if (!lhs->has_function_return_signature &&
                    !semantic_check_function_return_shape_rules(lhs, error)) {
                return 0;
            }

            if (!config.skip_all_paths_return_check &&
                semantic_function_return_type_requires_value(lhs) &&
                !semantic_compute_function_returns_all_paths(lhs, &returns_all_paths, error)) {
                return 0;
            }

            if (!config.skip_all_paths_return_check &&
                semantic_function_return_type_requires_value(lhs) && !returns_all_paths) {
                if (error) {
                    semantic_set_error(error,
                        lhs->line,
                        lhs->column,
                        "SEMA-CF-001: Function definition must return on all control-flow paths");
                }
                return 0;
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
                SemanticFunctionSignatureMatchKind signature_match_kind =
                    semantic_function_declaration_signature_match_kind(lhs, rhs);

                if (signature_match_kind == SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_RETURN_TYPE) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-005: identifier=%s; conflicting function declarations: return type mismatch",
                            preview);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                if (signature_match_kind == SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_COUNT) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-002: identifier=%s; prior_params=%zu; current_params=%zu; conflicting function declarations: parameter count mismatch",
                            preview,
                            lhs->parameter_count,
                            rhs->parameter_count);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                if (signature_match_kind == SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_KIND) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-006: identifier=%s; conflicting function declarations: parameter kind mismatch",
                            preview);
                        semantic_set_error(error, rhs->line, rhs->column, msg);
                    }
                    return 0;
                }

                if (signature_match_kind == SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_NESTED_SIGNATURE) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-007: identifier=%s; conflicting function declarations: nested function-parameter signature mismatch",
                            preview);
                        semantic_set_error(error, rhs->line, rhs->column, msg);
                    }
                    return 0;
                }

                if (signature_match_kind != SEMANTIC_FUNCTION_SIGNATURE_MATCH) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-INT-013: identifier=%s; unexpected function signature comparison failure",
                            preview);
                        semantic_set_error(error, rhs->line, rhs->column, msg);
                    }
                    return 0;
                }

                if (lhs->is_function_definition && rhs->is_function_definition) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-003: identifier=%s; duplicate function definition",
                            preview);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                /* Multiple compatible declarations are currently accepted. */
                continue;
            }

            if (lhs->kind == AST_EXTERNAL_DECLARATION && rhs->kind == AST_EXTERNAL_DECLARATION) {
                if (!semantic_top_level_declarations_have_compatible_types(lhs, rhs)) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-008: identifier=%s; conflicting top-level declarations: type mismatch",
                            preview);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                if (lhs->has_initializer && rhs->has_initializer) {
                    if (error) {
                        char preview[128];
                        char msg[512];

                        format_callee_preview(lhs->name, preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-TOP-001: identifier=%s; duplicate top-level variable definition",
                            preview);
                        semantic_set_error(error,
                            rhs->line,
                            rhs->column,
                            msg);
                    }
                    return 0;
                }

                /* Repeated top-level declarations without duplicate definition are accepted. */
                continue;
            }

            if (lhs->kind != rhs->kind) {
                if (error) {
                    char preview[128];
                    char msg[512];

                    format_callee_preview(lhs->name, preview, sizeof(preview));
                    snprintf(msg,
                        sizeof(msg),
                        "SEMA-TOP-004: identifier=%s; function/variable name conflict at top level",
                        preview);
                    semantic_set_error(error,
                        rhs->line,
                        rhs->column,
                        msg);
                }
                return 0;
            }
        }
    }

    return 1;
}

int semantic_analyze_program(const AstProgram *program, SemanticError *error) {
    return semantic_analyze_program_with_options(program, NULL, error);
}
#endif
