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
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_find_named_function_parameter_type_descriptor(const AstExternal *func,
    const char *name,
    size_t *out_index,
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
    switch (external->function_return_type) {
    case AST_FUNCTION_RETURN_INT:
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    case AST_FUNCTION_RETURN_FLOAT:
        out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
        return 1;
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
        return lhs->function_return_type == rhs->function_return_type &&
            lhs->parameter_count == rhs->parameter_count;
    }

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

        if (lhs_type.kind == SEMANTIC_TYPE_KIND_FUNCTION &&
            !semantic_type_descriptors_are_compatible(&lhs_type, &rhs_type)) {
            return SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_NESTED_SIGNATURE;
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
            return current_param_type.parameter_count == expected_param_type.parameter_count &&
                current_param_type.function_return_type == expected_param_type.function_return_type;
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
        return builtin->parameter_count == expected_param_type.parameter_count &&
            builtin->return_type == expected_param_type.function_return_type;
    }

    return arg_function->parameter_count == expected_param_type.parameter_count &&
        arg_function->function_return_type == expected_param_type.function_return_type;
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
                0,
                error,
                depth + 1u) &&
            semantic_extension_check_function_value_expression(
                expr->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                current_func,
                0,
                error,
                depth + 1u);
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
        if (lhs_struct->field_count == 0u || !lhs_struct->field_names) {
            semantic_set_error(error, 0, 0, "SEMA-STRUCT-002: current extension struct slice requires at least one int field");
            return 0;
        }
        for (j = 0u; j < lhs_struct->field_count; ++j) {
            size_t k;

            if (!lhs_struct->field_names[j] || lhs_struct->field_names[j][0] == '\0') {
                semantic_set_error(error, 0, 0, "SEMA-STRUCT-007: struct fields must have names");
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
                error)) {
                return 0;
            }
            if (semantic_build_top_level_declaration_type_descriptor(lhs, &declaration_type) &&
                !semantic_validate_scalar_initializer_type(lhs->declaration_initializer,
                    &declaration_type,
                    program,
                    i,
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
                    param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                    semantic_set_error(error,
                        lhs->line,
                        lhs->column,
                        "SEMA-EXT-015: function-valued parameters are available only under -extension mode");
                    return 0;
                }
            }
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && lhs->is_function_definition) {
            if (!semantic_check_function_callable_scope_shadow_rules(lhs, error)) {
                return 0;
            }

            if (!semantic_check_function_callable_rules(program, i, lhs, error)) {
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

             if (!semantic_check_function_return_shape_rules(lhs, error)) {
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
