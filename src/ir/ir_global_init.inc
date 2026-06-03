#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

#define IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME "__global.init"
#define IR_PROGRAM_INIT_FUNCTION_NAME "__program.init"

#include <limits.h>

typedef struct {
    const char *name;
    AstFunctionReturnType return_type;
    size_t parameter_count;
    const AstFunctionReturnType *parameter_types;
} IrBuiltinFunctionInfo;

static const AstFunctionReturnType k_ir_builtin_float_binary_param_types[2] = {
    AST_FUNCTION_RETURN_FLOAT,
    AST_FUNCTION_RETURN_FLOAT,
};

static const AstFunctionReturnType k_ir_builtin_float_unary_param_types[1] = {
    AST_FUNCTION_RETURN_FLOAT,
};

static const AstFunctionReturnType k_ir_builtin_int_unary_param_types[1] = {
    AST_FUNCTION_RETURN_INT,
};

static const IrBuiltinFunctionInfo k_ir_builtin_functions[] = {
    {"getint", AST_FUNCTION_RETURN_INT, 0u, NULL},
    {"getch", AST_FUNCTION_RETURN_INT, 0u, NULL},
    {"getarray", AST_FUNCTION_RETURN_INT, 1u, NULL},
    {"putint", AST_FUNCTION_RETURN_VOID, 1u, NULL},
    {"putch", AST_FUNCTION_RETURN_VOID, 1u, NULL},
    {"putarray", AST_FUNCTION_RETURN_VOID, 2u, NULL},
    {"starttime", AST_FUNCTION_RETURN_VOID, 0u, NULL},
    {"stoptime", AST_FUNCTION_RETURN_VOID, 0u, NULL},
    {"__builtin_fadd32", AST_FUNCTION_RETURN_FLOAT, 2u, k_ir_builtin_float_binary_param_types},
    {"__builtin_fsub32", AST_FUNCTION_RETURN_FLOAT, 2u, k_ir_builtin_float_binary_param_types},
    {"__builtin_fmul32", AST_FUNCTION_RETURN_FLOAT, 2u, k_ir_builtin_float_binary_param_types},
    {"__builtin_fdiv32", AST_FUNCTION_RETURN_FLOAT, 2u, k_ir_builtin_float_binary_param_types},
    {"__builtin_f2i32", AST_FUNCTION_RETURN_INT, 1u, k_ir_builtin_float_unary_param_types},
    {"__builtin_i2f32", AST_FUNCTION_RETURN_FLOAT, 1u, k_ir_builtin_int_unary_param_types},
};

static const AstExpression *ir_find_local_function_value_initializer_expr(const AstExternal *external,
    const char *name);
static const AstExpression *ir_find_resolved_local_function_value_initializer_expr(const AstProgram *ast_program,
    const AstExternal *external,
    const char *name);
static int ir_try_resolve_passthrough_function_value_argument_expr(const AstProgram *ast_program,
    const AstExpression *call_expr,
    const AstExpression **out_arg_expr);
static int ir_try_resolve_returned_function_value_passthrough_argument_expr(const AstProgram *ast_program,
    const AstExpression *call_expr,
    const AstExpression **out_arg_expr);
static int ir_collect_function_value_wrapper_target_names_in_return_context(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error);
static int ir_collect_function_value_wrapper_target_names_in_declaration_context(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error);
static int ir_resolve_function_value_passthrough_target_name_recursive(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstExpression *expr,
    const char **out_target_name,
    IrError *error);
static int ir_collect_function_value_wrapper_target_names(const AstProgram *ast_program,
    IrProgram *program,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error);

static int ir_function_find_last_unterminated_block(const IrFunction *function,
    size_t *out_block_id);

static int ir_try_resolve_passthrough_function_value_argument_expr(const AstProgram *ast_program,
    const AstExpression *call_expr,
    const AstExpression **out_arg_expr) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(call_expr);
    const AstExpression *callee_base = NULL;
    const AstExternal *callee_external = NULL;
    size_t passthrough_index = (size_t)-1;

    if (out_arg_expr) {
        *out_arg_expr = NULL;
    }
    if (!ast_program || !call_expr || !out_arg_expr || !base || base->kind != AST_EXPR_CALL) {
        return 0;
    }

    callee_base = ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
    if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
        return 0;
    }
    callee_external =
        ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
    if (!callee_external || !callee_external->has_function_return_signature ||
        !ir_function_value_return_exact_parameter_passthrough_index(
            ast_program,
            callee_external,
            &passthrough_index) ||
        passthrough_index == (size_t)-1 ||
        !base->as.call.args ||
        passthrough_index >= base->as.call.arg_count) {
        return 0;
    }

    *out_arg_expr = base->as.call.args[passthrough_index];
    return 1;
}

static int ir_try_resolve_returned_function_value_passthrough_argument_expr(const AstProgram *ast_program,
    const AstExpression *call_expr,
    const AstExpression **out_arg_expr) {
    const AstExpression *current_expr = call_expr;
    const AstExpression *next_arg_expr = NULL;
    size_t hop_count = 0u;

    if (out_arg_expr) {
        *out_arg_expr = NULL;
    }
    if (!ast_program || !call_expr || !out_arg_expr) {
        return 0;
    }

    while (current_expr &&
        ir_try_resolve_passthrough_function_value_argument_expr(
            ast_program,
            current_expr,
            &next_arg_expr) &&
        next_arg_expr &&
        hop_count < 64u) {
        *out_arg_expr = next_arg_expr;
        current_expr = ir_unwrap_function_value_wrapper_expression(next_arg_expr);
        ++hop_count;
    }

    return *out_arg_expr != NULL;
}

static int ir_try_resolve_exact_returned_function_value_wrapper_parameter_index(
    const AstProgram *ast_program,
    const AstExternal *external,
    size_t *out_param_index) {
    const AstExternal *resolved_external = external;
    const AstStatement *body = NULL;
    const AstStatement *decl_stmt = NULL;
    const AstStatement *return_stmt = NULL;
    const AstExpression *return_expr = NULL;
    const AstExpression *return_base = NULL;
    const char *local_name = NULL;
    const AstExpression *init_expr = NULL;
    const AstExpression *init_base = NULL;
    const AstExpression *passthrough_arg_expr = NULL;
    size_t decl_index;
    size_t param_index;

    if (out_param_index) {
        *out_param_index = (size_t)-1;
    }
    if (!external || !external->has_function_return_signature || !out_param_index) {
        return 0;
    }
    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        return 0;
    }

    body = resolved_external->function_body;
    if (body->kind != AST_STMT_COMPOUND ||
        body->child_count != 2u ||
        !body->children ||
        !body->children[0] ||
        !body->children[1] ||
        body->children[0]->kind != AST_STMT_DECLARATION ||
        body->children[1]->kind != AST_STMT_RETURN ||
        body->children[1]->expression_count != 1u) {
        return 0;
    }

    decl_stmt = body->children[0];
    return_stmt = body->children[1];
    if (!decl_stmt->declaration_names || !decl_stmt->declaration_value_kinds ||
        !decl_stmt->expressions || !resolved_external->parameter_names) {
        return 0;
    }

    return_expr =
        (return_stmt->expressions && return_stmt->expression_count > 0u)
            ? return_stmt->expressions[0]
            : NULL;
    return_base = ir_unwrap_function_value_wrapper_expression(return_expr);
    if (!return_base ||
        return_base->kind != AST_EXPR_IDENTIFIER ||
        !return_base->as.identifier.name) {
        return 0;
    }
    local_name = return_base->as.identifier.name;
    if (!local_name || local_name[0] == '\0') {
        return 0;
    }

    for (decl_index = 0u; decl_index < decl_stmt->declaration_name_count &&
            decl_index < decl_stmt->expression_count; ++decl_index) {
        if (!decl_stmt->declaration_names[decl_index] ||
            strcmp(decl_stmt->declaration_names[decl_index], local_name) != 0 ||
            decl_stmt->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_FUNCTION) {
            continue;
        }
        init_expr = decl_stmt->expressions[decl_index];
        break;
    }
    if (!init_expr) {
        return 0;
    }

    init_base = ir_unwrap_function_value_wrapper_expression(init_expr);
    if (!init_base) {
        return 0;
    }
    if (init_base->kind == AST_EXPR_CALL &&
        ir_try_resolve_returned_function_value_passthrough_argument_expr(
            ast_program,
            init_base,
            &passthrough_arg_expr) &&
        passthrough_arg_expr) {
        init_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
    }
    if (!init_base ||
        init_base->kind != AST_EXPR_IDENTIFIER ||
        !init_base->as.identifier.name) {
        return 0;
    }

    for (param_index = 0u; param_index < resolved_external->parameter_count; ++param_index) {
        if (resolved_external->parameter_names[param_index] &&
            strcmp(resolved_external->parameter_names[param_index], init_base->as.identifier.name) == 0 &&
            ir_function_parameter_name_is_compatible_function_value(
                resolved_external,
                init_base->as.identifier.name,
                resolved_external->function_return_function_return_type,
                resolved_external->function_return_function_parameter_count)) {
            *out_param_index = param_index;
            return 1;
        }
    }

    return 0;
}

static const IrBuiltinFunctionInfo *ir_find_builtin_function_info(const char *name) {
    size_t builtin_index;

    if (!name || name[0] == '\0') {
        return NULL;
    }
    for (builtin_index = 0u;
        builtin_index < sizeof(k_ir_builtin_functions) / sizeof(k_ir_builtin_functions[0]);
        ++builtin_index) {
        if (strcmp(k_ir_builtin_functions[builtin_index].name, name) == 0) {
            return &k_ir_builtin_functions[builtin_index];
        }
    }
    return NULL;
}

static int ir_builtin_function_returns_value(const char *name) {
    const IrBuiltinFunctionInfo *builtin = ir_find_builtin_function_info(name);

    if (!name || name[0] == '\0') {
        return 0;
    }
    return builtin && builtin->return_type != AST_FUNCTION_RETURN_VOID;
}

static int ir_builtin_function_matches_signature(const char *name,
    AstFunctionReturnType expected_return_type,
    size_t expected_parameter_count) {
    const IrBuiltinFunctionInfo *builtin = ir_find_builtin_function_info(name);
    if (!builtin) {
        return 0;
    }
    return builtin->parameter_count == expected_parameter_count &&
        builtin->return_type == expected_return_type;
}

static int ir_predeclare_builtin_calls_in_statement(IrProgram *program,
    const AstStatement *stmt,
    IrError *error);

static AstFunctionReturnType ir_scalar_declaration_value_kind_to_return_type(int declaration_value_kind) {
    return declaration_value_kind == AST_DECLARATION_VALUE_FLOAT
        ? AST_FUNCTION_RETURN_FLOAT
        : AST_FUNCTION_RETURN_INT;
}

static AstFunctionReturnType ir_scalar_parameter_value_kind_to_return_type(int parameter_value_kind) {
    return parameter_value_kind == AST_PARAMETER_VALUE_FLOAT
        ? AST_FUNCTION_RETURN_FLOAT
        : AST_FUNCTION_RETURN_INT;
}

static int ir_function_return_type_as_aggregate_kind(const AstExternal *external,
    AstDeclarationValueKind *out_kind) {
    if (out_kind) {
        *out_kind = AST_DECLARATION_VALUE_INT;
    }
    if (!external || !out_kind) {
        return 0;
    }

    switch (external->function_return_type) {
    case AST_FUNCTION_RETURN_PAIR:
        *out_kind = AST_DECLARATION_VALUE_PAIR;
        return 1;
    case AST_FUNCTION_RETURN_STRUCT:
        *out_kind = AST_DECLARATION_VALUE_STRUCT;
        return 1;
    default:
        return 0;
    }
}

static const AstExternal *ir_find_named_function_external_prefer_definition(const AstProgram *ast_program,
    const char *name) {
    const AstExternal *fallback = NULL;
    size_t external_index;

    if (!ast_program || !name || name[0] == '\0') {
        return NULL;
    }

    for (external_index = 0u; external_index < ast_program->count; ++external_index) {
        const AstExternal *candidate = &ast_program->externals[external_index];

        if (candidate->kind != AST_EXTERNAL_FUNCTION ||
            !candidate->name ||
            strcmp(candidate->name, name) != 0) {
            continue;
        }
        if (!fallback) {
            fallback = candidate;
        }
        if (candidate->is_function_definition && candidate->function_body) {
            return candidate;
        }
    }

    return fallback;
}

static const AstExpression *ir_find_function_returned_closure_expr(const AstStatement *stmt) {
    size_t child_index;

    if (!stmt) {
        return NULL;
    }
    if (stmt->kind == AST_STMT_RETURN) {
        const AstExpression *expr =
            (stmt->expressions && stmt->expression_count > 0u) ? stmt->expressions[0] : NULL;
        const AstExpression *base = ir_unwrap_paren_expression(expr);

        return (base && base->kind == AST_EXPR_CLOSURE) ? base : NULL;
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        const AstExpression *found = ir_find_function_returned_closure_expr(
            stmt->children ? stmt->children[child_index] : NULL);
        if (found) {
            return found;
        }
    }

    return NULL;
}

static const AstExpression *ir_find_function_returned_value_expr(const AstStatement *stmt) {
    size_t child_index;

    if (!stmt) {
        return NULL;
    }
    if (stmt->kind == AST_STMT_RETURN) {
        const AstExpression *expr =
            (stmt->expressions && stmt->expression_count > 0u) ? stmt->expressions[0] : NULL;
        return ir_unwrap_paren_expression(expr);
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        const AstExpression *found = ir_find_function_returned_value_expr(
            stmt->children ? stmt->children[child_index] : NULL);
        if (found) {
            return found;
        }
    }

    return NULL;
}

static int ir_function_value_return_is_closure_family(const AstProgram *ast_program,
    const AstExternal *external,
    int *out_is_closure) {
    const AstExternal *resolved_external = external;
    const AstExpression *return_expr = NULL;

    if (out_is_closure) {
        *out_is_closure = 0;
    }
    if (!external || !external->has_function_return_signature || !out_is_closure) {
        return 0;
    }

    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        *out_is_closure = 0;
        return 1;
    }

    return_expr = ir_find_function_returned_value_expr(resolved_external->function_body);
    if (!return_expr) {
        return 0;
    }

    return_expr = ir_unwrap_function_value_wrapper_expression(return_expr);
    if (!return_expr) {
        return 0;
    }
retry_return_expr_shape:
    if (return_expr->kind == AST_EXPR_IDENTIFIER && return_expr->as.identifier.name) {
        const AstExpression *init_expr =
            ir_find_resolved_local_function_value_initializer_expr(
                ast_program,
                resolved_external,
                return_expr->as.identifier.name);
        const AstExpression *init_base = ir_unwrap_function_value_wrapper_expression(init_expr);

        if (ir_function_parameter_name_is_compatible_function_value(
                resolved_external,
                return_expr->as.identifier.name,
                resolved_external->function_return_function_return_type,
                resolved_external->function_return_function_parameter_count)) {
            *out_is_closure = 0;
            return 1;
        }
        if (init_base) {
            if (init_base->kind == AST_EXPR_CLOSURE) {
                *out_is_closure = 1;
                return 1;
            }
            if (init_base->kind == AST_EXPR_TERNARY) {
                return_expr = init_base;
                goto retry_return_expr_shape;
            }
            if (init_base->kind == AST_EXPR_CALL) {
                const AstExpression *passthrough_arg_expr = NULL;
                const AstExpression *callee_base =
                    ir_unwrap_function_value_wrapper_expression(init_base->as.call.callee);
                const AstExternal *callee_external = NULL;

                if (ir_try_resolve_passthrough_function_value_argument_expr(
                        ast_program,
                        init_base,
                        &passthrough_arg_expr) &&
                    passthrough_arg_expr) {
                    init_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
                    if (init_base && init_base->kind == AST_EXPR_CLOSURE) {
                        *out_is_closure = 1;
                        return 1;
                    }
                    if (init_base &&
                        (init_base->kind == AST_EXPR_IDENTIFIER ||
                            init_base->kind == AST_EXPR_CALL ||
                            init_base->kind == AST_EXPR_TERNARY)) {
                        return_expr = init_base;
                        goto retry_return_expr_shape;
                    }
                }

                if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
                    return 0;
                }
                callee_external =
                    ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
                if (!callee_external || !callee_external->has_function_return_signature) {
                    return 0;
                }
                return ir_function_value_return_is_closure_family(
                    ast_program,
                    callee_external,
                    out_is_closure);
            }
        }
    }
    if (return_expr->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.then_expr);
        const AstExpression *else_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.else_expr);
        int then_is_closure = 0;
        int else_is_closure = 0;

        if (!then_expr || !else_expr) {
            return 0;
        }
        {
            const AstExpression *then_init_expr = NULL;
            const AstExpression *else_init_expr = NULL;
            const AstExpression *then_init_base = then_expr;
            const AstExpression *else_init_base = else_expr;

            if (then_expr->kind == AST_EXPR_IDENTIFIER && then_expr->as.identifier.name) {
                then_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        then_expr->as.identifier.name);
                then_init_base = ir_unwrap_function_value_wrapper_expression(then_init_expr);
            }
            if (else_expr->kind == AST_EXPR_IDENTIFIER && else_expr->as.identifier.name) {
                else_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        else_expr->as.identifier.name);
                else_init_base = ir_unwrap_function_value_wrapper_expression(else_init_expr);
            }

            if (then_init_base && then_init_base->kind == AST_EXPR_CLOSURE) {
                then_is_closure = 1;
            } else if (then_init_base && then_init_base->kind == AST_EXPR_CALL) {
                const AstExpression *passthrough_arg_expr = NULL;
                const AstExpression *callee_base =
                    ir_unwrap_function_value_wrapper_expression(then_init_base->as.call.callee);
                const AstExternal *callee_external = NULL;

                if (ir_try_resolve_passthrough_function_value_argument_expr(
                        ast_program,
                        then_init_base,
                        &passthrough_arg_expr) &&
                    passthrough_arg_expr) {
                    then_init_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
                    if (then_init_base && then_init_base->kind == AST_EXPR_CLOSURE) {
                        then_is_closure = 1;
                    } else if (then_init_base && then_init_base->kind == AST_EXPR_CALL) {
                        callee_base =
                            ir_unwrap_function_value_wrapper_expression(then_init_base->as.call.callee);
                    }
                }
                if (then_is_closure) {
                    goto have_then_is_closure;
                }

                if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
                    return 0;
                }
                callee_external =
                    ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
                if (!callee_external ||
                    !ir_function_value_return_is_closure_family(ast_program, callee_external, &then_is_closure)) {
                    return 0;
                }
have_then_is_closure:
                ;
            } else if (then_init_base &&
                then_init_base->kind != AST_EXPR_IDENTIFIER) {
                return 0;
            }
            if (else_init_base && else_init_base->kind == AST_EXPR_CLOSURE) {
                else_is_closure = 1;
            } else if (else_init_base && else_init_base->kind == AST_EXPR_CALL) {
                const AstExpression *passthrough_arg_expr = NULL;
                const AstExpression *callee_base =
                    ir_unwrap_function_value_wrapper_expression(else_init_base->as.call.callee);
                    const AstExternal *callee_external = NULL;

                if (ir_try_resolve_passthrough_function_value_argument_expr(
                        ast_program,
                        else_init_base,
                        &passthrough_arg_expr) &&
                    passthrough_arg_expr) {
                    else_init_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
                    if (else_init_base && else_init_base->kind == AST_EXPR_CLOSURE) {
                        else_is_closure = 1;
                    } else if (else_init_base && else_init_base->kind == AST_EXPR_CALL) {
                        callee_base =
                            ir_unwrap_function_value_wrapper_expression(else_init_base->as.call.callee);
                    }
                }
                if (else_is_closure) {
                    goto have_else_is_closure;
                }

                if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
                    return 0;
                }
                callee_external =
                    ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
                if (!callee_external ||
                    !ir_function_value_return_is_closure_family(ast_program, callee_external, &else_is_closure)) {
                    return 0;
                }
have_else_is_closure:
                ;
            } else if (else_init_base &&
                else_init_base->kind != AST_EXPR_IDENTIFIER) {
                return 0;
            }
        }
        if (then_is_closure != else_is_closure) {
            return 0;
        }
        *out_is_closure = then_is_closure;
        return 1;
    }

    if (return_expr->kind == AST_EXPR_CALL) {
        const AstExpression *passthrough_arg_expr = NULL;
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.call.callee);
        const AstExternal *callee_external = NULL;

        if (ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                return_expr,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            const AstExpression *resolved_passthrough_base =
                ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);

            if (resolved_passthrough_base &&
                resolved_passthrough_base->kind == AST_EXPR_IDENTIFIER &&
                resolved_passthrough_base->as.identifier.name) {
                const AstExpression *resolved_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        resolved_passthrough_base->as.identifier.name);
                const AstExpression *resolved_init_base =
                    ir_unwrap_function_value_wrapper_expression(resolved_init_expr);

                if (resolved_init_base && resolved_init_base != resolved_passthrough_base) {
                    if (resolved_init_base->kind == AST_EXPR_TERNARY) {
                        return_expr = resolved_init_base;
                        goto retry_return_expr_shape;
                    }
                    if (resolved_init_base->kind == AST_EXPR_CLOSURE) {
                        *out_is_closure = 1;
                        return 1;
                    }
                    if (resolved_init_base->kind == AST_EXPR_IDENTIFIER ||
                        resolved_init_base->kind == AST_EXPR_CALL ||
                        resolved_init_base->kind == AST_EXPR_TERNARY) {
                        return_expr = resolved_init_base;
                        goto retry_return_expr_shape;
                    }
                }
            } else if (resolved_passthrough_base &&
                resolved_passthrough_base->kind == AST_EXPR_CLOSURE) {
                *out_is_closure = 1;
                return 1;
            } else if (resolved_passthrough_base &&
                (resolved_passthrough_base->kind == AST_EXPR_IDENTIFIER ||
                    resolved_passthrough_base->kind == AST_EXPR_CALL ||
                    resolved_passthrough_base->kind == AST_EXPR_TERNARY)) {
                return_expr = resolved_passthrough_base;
                goto retry_return_expr_shape;
            }
        }

        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            return 0;
        }
        callee_external =
            ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        if (!callee_external || !callee_external->has_function_return_signature) {
            return 0;
        }
        return ir_function_value_return_is_closure_family(
            ast_program,
            callee_external,
            out_is_closure);
    }

    *out_is_closure = return_expr->kind == AST_EXPR_CLOSURE ? 1 : 0;
    return 1;
}

static int ir_function_value_return_parameter_index(const AstProgram *ast_program,
    const AstExternal *external,
    size_t *out_param_index) {
    const AstExternal *resolved_external = external;
    const AstExpression *return_expr = NULL;
    size_t param_index;

    if (out_param_index) {
        *out_param_index = (size_t)-1;
    }
    if (!external || !external->has_function_return_signature || !out_param_index) {
        return 0;
    }
    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        return 1;
    }

    if (ir_try_resolve_exact_returned_function_value_wrapper_parameter_index(
            ast_program,
            resolved_external,
            out_param_index) &&
        *out_param_index != (size_t)-1) {
        return 1;
    }

    return_expr = ir_find_function_returned_value_expr(resolved_external->function_body);
    return_expr = ir_unwrap_function_value_wrapper_expression(return_expr);
    if (return_expr &&
        return_expr->kind == AST_EXPR_CALL) {
        const AstExpression *passthrough_arg_expr = NULL;

        if (ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                return_expr,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            return_expr = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
        }
    }
    if (!return_expr ||
        return_expr->kind != AST_EXPR_IDENTIFIER ||
        !return_expr->as.identifier.name ||
        !resolved_external->parameter_names) {
        return 1;
    }

    for (param_index = 0u; param_index < resolved_external->parameter_count; ++param_index) {
        if (resolved_external->parameter_names[param_index] &&
            strcmp(resolved_external->parameter_names[param_index], return_expr->as.identifier.name) == 0 &&
            ir_function_parameter_name_is_compatible_function_value(
                resolved_external,
                return_expr->as.identifier.name,
                resolved_external->function_return_function_return_type,
                resolved_external->function_return_function_parameter_count)) {
            *out_param_index = param_index;
            return 1;
        }
    }

    return 1;
}

static int ir_function_value_return_exact_parameter_passthrough_index(const AstProgram *ast_program,
    const AstExternal *external,
    size_t *out_param_index) {
    const AstExternal *resolved_external = external;
    const AstStatement *body = NULL;
    const AstExpression *resolved_return_expr = NULL;
    const AstExpression *resolved_return_base = NULL;
    size_t param_index;

    if (out_param_index) {
        *out_param_index = (size_t)-1;
    }
    if (!external || !external->has_function_return_signature || !out_param_index) {
        return 0;
    }
    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        return 0;
    }

    if (ir_try_resolve_exact_returned_function_value_wrapper_parameter_index(
            ast_program,
            resolved_external,
            out_param_index) &&
        *out_param_index != (size_t)-1) {
        return 1;
    }

    body = resolved_external->function_body;
    if (body->kind != AST_STMT_COMPOUND) {
        return 0;
    }

    for (param_index = 0u; param_index < resolved_external->parameter_count; ++param_index) {
        if (!resolved_external->parameter_names ||
            !resolved_external->parameter_names[param_index]) {
            continue;
        }
        if (!ir_function_parameter_name_is_compatible_function_value(
                resolved_external,
                resolved_external->parameter_names[param_index],
                resolved_external->function_return_function_return_type,
                resolved_external->function_return_function_parameter_count)) {
            continue;
        }
        if (*out_param_index != (size_t)-1) {
            *out_param_index = (size_t)-1;
            return 0;
        }
        *out_param_index = param_index;
    }

    if (*out_param_index == (size_t)-1) {
        return 0;
    }

    resolved_return_expr = ir_find_function_returned_value_expr(body);
    resolved_return_base = ir_unwrap_function_value_wrapper_expression(resolved_return_expr);
    if (resolved_return_base &&
        resolved_return_base->kind == AST_EXPR_CALL) {
        const AstExpression *passthrough_arg_expr = NULL;

        if (ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                resolved_return_base,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            resolved_return_base =
                ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
        }
    }
    if (resolved_return_base &&
        resolved_return_base->kind == AST_EXPR_IDENTIFIER &&
        resolved_return_base->as.identifier.name) {
        const AstExpression *resolved_init_expr =
            ir_find_resolved_local_function_value_initializer_expr(
                ast_program,
                resolved_external,
                resolved_return_base->as.identifier.name);
        const AstExpression *resolved_init_base =
            ir_unwrap_function_value_wrapper_expression(resolved_init_expr);
        const AstExpression *passthrough_arg_expr = NULL;

        if (resolved_init_base &&
            resolved_init_base->kind == AST_EXPR_CALL &&
            ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                resolved_init_base,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            resolved_return_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
        }
    }
    if (!resolved_return_base ||
        resolved_return_base->kind != AST_EXPR_IDENTIFIER ||
        !resolved_return_base->as.identifier.name ||
        !resolved_external->parameter_names) {
        *out_param_index = (size_t)-1;
        return 0;
    }

    if (resolved_external->parameter_names[*out_param_index] &&
        strcmp(resolved_external->parameter_names[*out_param_index],
            resolved_return_base->as.identifier.name) == 0) {
        return 1;
    }

    *out_param_index = (size_t)-1;
    return 0;
}

static int ir_function_returned_closure_payload_slot_count(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    size_t *out_slot_count) {
    const AstExternal *resolved_external = external;
    const AstExpression *return_expr = NULL;

    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!external || !out_slot_count || !external->has_function_return_signature) {
        return 0;
    }

    resolved_external = external;
    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        *out_slot_count = 0u;
        return 1;
    }

    return_expr = ir_find_function_returned_value_expr(resolved_external->function_body);
    if (!return_expr) {
        return 0;
    }
    return_expr = ir_unwrap_function_value_wrapper_expression(return_expr);
    if (!return_expr) {
        return 0;
    }
retry_return_expr_shape:
    if (return_expr->kind == AST_EXPR_IDENTIFIER && return_expr->as.identifier.name) {
        const AstExpression *init_expr =
            ir_find_resolved_local_function_value_initializer_expr(
                ast_program,
                resolved_external,
                return_expr->as.identifier.name);
        const AstExpression *init_base = ir_unwrap_function_value_wrapper_expression(init_expr);
        size_t return_param_index = (size_t)-1;
        const AstExpression *resolved_init_base = init_base;

        if (ir_function_value_return_parameter_index(ast_program, resolved_external, &return_param_index) &&
            return_param_index != (size_t)-1) {
            *out_slot_count = 1u;
            return 1;
        }
        if (resolved_init_base) {
            if (resolved_init_base->kind == AST_EXPR_CLOSURE) {
                size_t slot_count = resolved_init_base->as.closure.capture_name_count;

                if (program) {
                    const char **returned_target_names = NULL;
                    size_t returned_target_count = 0u;

                    if (!ir_collect_function_value_return_target_names(
                            ast_program,
                            program,
                            resolved_external,
                            &returned_target_names,
                            &returned_target_count,
                            NULL)) {
                        free(returned_target_names);
                        return 0;
                    }
                    if (returned_target_count > 1u) {
                        slot_count += 1u;
                    }
                    free(returned_target_names);
                }
                *out_slot_count = slot_count;
                return 1;
            }
            if (resolved_init_base->kind == AST_EXPR_CALL) {
                const AstExpression *passthrough_arg_expr = NULL;
                const AstExpression *callee_base =
                    ir_unwrap_function_value_wrapper_expression(resolved_init_base->as.call.callee);
                const AstExternal *callee_external = NULL;

                if (ir_try_resolve_passthrough_function_value_argument_expr(
                        ast_program,
                        resolved_init_base,
                        &passthrough_arg_expr) &&
                    passthrough_arg_expr) {
                    resolved_init_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
                    if (resolved_init_base && resolved_init_base->kind == AST_EXPR_CLOSURE) {
                        size_t slot_count = resolved_init_base->as.closure.capture_name_count;

                        if (program) {
                            const char **returned_target_names = NULL;
                            size_t returned_target_count = 0u;

                            if (!ir_collect_function_value_return_target_names(
                                    ast_program,
                                    program,
                                    resolved_external,
                                    &returned_target_names,
                                    &returned_target_count,
                                    NULL)) {
                                free(returned_target_names);
                                return 0;
                            }
                            if (returned_target_count > 1u) {
                                slot_count += 1u;
                            }
                            free(returned_target_names);
                        }
                        *out_slot_count = slot_count;
                        return 1;
                    }
                    if (resolved_init_base &&
                        (resolved_init_base->kind == AST_EXPR_IDENTIFIER ||
                            resolved_init_base->kind == AST_EXPR_CALL ||
                            resolved_init_base->kind == AST_EXPR_TERNARY)) {
                        return_expr = resolved_init_base;
                        goto retry_return_expr_shape;
                    }
                }

                if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
                    return 0;
                }
                callee_external =
                    ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
                if (!callee_external) {
                    return 0;
                }
                return ir_function_returned_closure_payload_slot_count(
                    ast_program,
                    program,
                    callee_external,
                    out_slot_count);
            }
            if (resolved_init_base->kind == AST_EXPR_IDENTIFIER &&
                resolved_init_base->as.identifier.name &&
                strcmp(resolved_init_base->as.identifier.name, return_expr->as.identifier.name) != 0) {
                const AstExpression *recursive_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        resolved_init_base->as.identifier.name);
                const AstExpression *recursive_init_base =
                    ir_unwrap_function_value_wrapper_expression(recursive_init_expr);

                if (recursive_init_base &&
                    recursive_init_base != resolved_init_base) {
                    if (recursive_init_base->kind == AST_EXPR_TERNARY ||
                        recursive_init_base->kind == AST_EXPR_IDENTIFIER) {
                        return_expr = recursive_init_base;
                        goto retry_return_expr_shape;
                    } else if (recursive_init_base->kind == AST_EXPR_CLOSURE) {
                        size_t slot_count = recursive_init_base->as.closure.capture_name_count;

                        if (program) {
                            const char **returned_target_names = NULL;
                            size_t returned_target_count = 0u;

                            if (!ir_collect_function_value_return_target_names(
                                    ast_program,
                                    program,
                                    resolved_external,
                                    &returned_target_names,
                                    &returned_target_count,
                                    NULL)) {
                                free(returned_target_names);
                                return 0;
                            }
                            if (returned_target_count > 1u) {
                                slot_count += 1u;
                            }
                            free(returned_target_names);
                        }
                        *out_slot_count = slot_count;
                        return 1;
                    } else if (recursive_init_base->kind == AST_EXPR_CALL) {
                        return_expr = recursive_init_base;
                        goto retry_return_expr_shape;
                    }
                }
            }
            if (resolved_init_base->kind == AST_EXPR_TERNARY) {
                return_expr = resolved_init_base;
                goto retry_return_expr_shape;
            }
        }
    }
    if (return_expr->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.then_expr);
        const AstExpression *else_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.else_expr);
        const AstExpression *then_init_expr = NULL;
        const AstExpression *else_init_expr = NULL;
        const AstExpression *then_init_base = NULL;
        const AstExpression *else_init_base = NULL;

        if (!then_expr || !else_expr) {
            return 0;
        }
        then_init_base = then_expr;
        else_init_base = else_expr;
        if (then_expr->kind == AST_EXPR_IDENTIFIER && then_expr->as.identifier.name) {
            then_init_expr =
                ir_find_resolved_local_function_value_initializer_expr(
                    ast_program,
                    resolved_external,
                    then_expr->as.identifier.name);
            then_init_base = ir_unwrap_function_value_wrapper_expression(then_init_expr);
        }
        if (else_expr->kind == AST_EXPR_IDENTIFIER && else_expr->as.identifier.name) {
            else_init_expr =
                ir_find_resolved_local_function_value_initializer_expr(
                    ast_program,
                    resolved_external,
                    else_expr->as.identifier.name);
            else_init_base = ir_unwrap_function_value_wrapper_expression(else_init_expr);
        }
        if (then_init_base &&
            else_init_base &&
            then_init_base->kind == AST_EXPR_CLOSURE &&
            else_init_base->kind == AST_EXPR_CLOSURE) {
            size_t slot_count = then_init_base->as.closure.capture_name_count;

            if (else_init_base->as.closure.capture_name_count != slot_count) {
                return 0;
            }
            if (program) {
                const char **returned_target_names = NULL;
                size_t returned_target_count = 0u;

                if (!ir_collect_function_value_return_target_names(
                        ast_program,
                        program,
                        resolved_external,
                        &returned_target_names,
                        &returned_target_count,
                        NULL)) {
                    free(returned_target_names);
                    return 0;
                }
                if (returned_target_count > 1u) {
                    slot_count += 1u;
                }
                free(returned_target_names);
            }
            *out_slot_count = slot_count;
            return 1;
        }
        if (then_init_base &&
            else_init_base &&
            then_init_base->kind == AST_EXPR_CALL &&
            else_init_base->kind == AST_EXPR_CALL) {
            const AstExpression *then_passthrough_arg_expr = NULL;
            const AstExpression *else_passthrough_arg_expr = NULL;
            const AstExpression *then_resolved_init_expr = NULL;
            const AstExpression *else_resolved_init_expr = NULL;
            const AstExpression *then_resolved_base = then_init_base;
            const AstExpression *else_resolved_base = else_init_base;
            const AstExpression *callee_base = NULL;
            const AstExternal *callee_external = NULL;

            if (ir_try_resolve_passthrough_function_value_argument_expr(
                    ast_program,
                    then_resolved_base,
                    &then_passthrough_arg_expr) &&
                then_passthrough_arg_expr) {
                then_resolved_base =
                    ir_unwrap_function_value_wrapper_expression(then_passthrough_arg_expr);
            }
            if (ir_try_resolve_passthrough_function_value_argument_expr(
                    ast_program,
                    else_resolved_base,
                    &else_passthrough_arg_expr) &&
                else_passthrough_arg_expr) {
                else_resolved_base =
                    ir_unwrap_function_value_wrapper_expression(else_passthrough_arg_expr);
            }
            if (then_resolved_base &&
                then_resolved_base->kind == AST_EXPR_IDENTIFIER &&
                then_resolved_base->as.identifier.name) {
                then_resolved_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        then_resolved_base->as.identifier.name);
                if (then_resolved_init_expr) {
                    then_resolved_base =
                        ir_unwrap_function_value_wrapper_expression(then_resolved_init_expr);
                }
            }
            if (else_resolved_base &&
                else_resolved_base->kind == AST_EXPR_IDENTIFIER &&
                else_resolved_base->as.identifier.name) {
                else_resolved_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        else_resolved_base->as.identifier.name);
                if (else_resolved_init_expr) {
                    else_resolved_base =
                        ir_unwrap_function_value_wrapper_expression(else_resolved_init_expr);
                }
            }
            if (!then_resolved_base || !else_resolved_base ||
                then_resolved_base->kind != else_resolved_base->kind) {
                return 0;
            }

            if (then_resolved_base->kind == AST_EXPR_CLOSURE &&
                else_resolved_base->kind == AST_EXPR_CLOSURE) {
                size_t slot_count = then_resolved_base->as.closure.capture_name_count;

                if (else_resolved_base->as.closure.capture_name_count != slot_count) {
                    return 0;
                }
                if (program) {
                    const char **returned_target_names = NULL;
                    size_t returned_target_count = 0u;

                    if (!ir_collect_function_value_return_target_names(
                            ast_program,
                            program,
                            resolved_external,
                            &returned_target_names,
                            &returned_target_count,
                            NULL)) {
                        free(returned_target_names);
                        return 0;
                    }
                    if (returned_target_count > 1u) {
                        slot_count += 1u;
                    }
                    free(returned_target_names);
                }
                *out_slot_count = slot_count;
                return 1;
            }

            if (then_resolved_base->kind == AST_EXPR_IDENTIFIER &&
                else_resolved_base->kind == AST_EXPR_IDENTIFIER) {
                *out_slot_count = 1u;
                return 1;
            }

            if (then_resolved_base->kind != AST_EXPR_CALL ||
                else_resolved_base->kind != AST_EXPR_CALL) {
                return 0;
            }

            callee_base =
                ir_unwrap_function_value_wrapper_expression(then_resolved_base->as.call.callee);
            if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
                return 0;
            }
            callee_external =
                ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
            if (!callee_external) {
                return 0;
            }
            return ir_function_returned_closure_payload_slot_count(
                ast_program,
                program,
                callee_external,
                out_slot_count);
        }
        *out_slot_count = 1u;
        return 1;
    }
    if (return_expr->kind == AST_EXPR_CALL) {
        const AstExpression *passthrough_arg_expr = NULL;
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.call.callee);
        const AstExternal *callee_external = NULL;

        if (ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                return_expr,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            const AstExpression *resolved_passthrough_base =
                ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);

            if (resolved_passthrough_base &&
                resolved_passthrough_base->kind == AST_EXPR_IDENTIFIER &&
                resolved_passthrough_base->as.identifier.name) {
                const AstExpression *resolved_init_expr =
                    ir_find_resolved_local_function_value_initializer_expr(
                        ast_program,
                        resolved_external,
                        resolved_passthrough_base->as.identifier.name);
                const AstExpression *resolved_init_base =
                    ir_unwrap_function_value_wrapper_expression(resolved_init_expr);

                if (resolved_init_base && resolved_init_base != resolved_passthrough_base) {
                    if (resolved_init_base->kind == AST_EXPR_TERNARY) {
                        return_expr = resolved_init_base;
                        goto retry_return_expr_shape;
                    }
                    if (resolved_init_base->kind == AST_EXPR_CLOSURE) {
                        size_t slot_count = resolved_init_base->as.closure.capture_name_count;

                        if (program) {
                            const char **returned_target_names = NULL;
                            size_t returned_target_count = 0u;

                            if (!ir_collect_function_value_return_target_names(
                                    ast_program,
                                    program,
                                    resolved_external,
                                    &returned_target_names,
                                    &returned_target_count,
                                    NULL)) {
                                free(returned_target_names);
                                return 0;
                            }
                            if (returned_target_count > 1u) {
                                slot_count += 1u;
                            }
                            free(returned_target_names);
                        }
                        *out_slot_count = slot_count;
                        return 1;
                    }
                    if (resolved_init_base->kind == AST_EXPR_IDENTIFIER ||
                        resolved_init_base->kind == AST_EXPR_CALL) {
                        return_expr = resolved_init_base;
                        goto retry_return_expr_shape;
                    }
                }
            } else if (resolved_passthrough_base &&
                resolved_passthrough_base->kind == AST_EXPR_CLOSURE) {
                size_t slot_count = resolved_passthrough_base->as.closure.capture_name_count;

                if (program) {
                    const char **returned_target_names = NULL;
                    size_t returned_target_count = 0u;

                    if (!ir_collect_function_value_return_target_names(
                            ast_program,
                            program,
                            resolved_external,
                            &returned_target_names,
                            &returned_target_count,
                            NULL)) {
                        free(returned_target_names);
                        return 0;
                    }
                    if (returned_target_count > 1u) {
                        slot_count += 1u;
                    }
                    free(returned_target_names);
                }
                *out_slot_count = slot_count;
                return 1;
            } else if (resolved_passthrough_base &&
                (resolved_passthrough_base->kind == AST_EXPR_IDENTIFIER ||
                    resolved_passthrough_base->kind == AST_EXPR_CALL)) {
                return_expr = resolved_passthrough_base;
                goto retry_return_expr_shape;
            }
        }

        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            return 0;
        }
        callee_external =
            ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        if (!callee_external) {
            return 0;
        }
        return ir_function_returned_closure_payload_slot_count(
            ast_program,
            program,
            callee_external,
            out_slot_count);
    }
    if (return_expr->kind != AST_EXPR_CLOSURE) {
        *out_slot_count = 1u;
        return 1;
    }

    *out_slot_count = return_expr->as.closure.capture_name_count;
    return 1;
    if (return_expr->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.then_expr);
        const AstExpression *else_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.else_expr);
        const AstExpression *then_init_expr = NULL;
        const AstExpression *else_init_expr = NULL;
        const AstExpression *then_init_base = NULL;
        const AstExpression *else_init_base = NULL;

        if (!then_expr || !else_expr) {
            return 0;
        }
        then_init_base = then_expr;
        else_init_base = else_expr;
        if (then_expr->kind == AST_EXPR_IDENTIFIER && then_expr->as.identifier.name) {
            then_init_expr =
                ir_find_resolved_local_function_value_initializer_expr(
                    ast_program,
                    resolved_external,
                    then_expr->as.identifier.name);
            then_init_base = ir_unwrap_function_value_wrapper_expression(then_init_expr);
        }
        if (else_expr->kind == AST_EXPR_IDENTIFIER && else_expr->as.identifier.name) {
            else_init_expr =
                ir_find_resolved_local_function_value_initializer_expr(
                    ast_program,
                    resolved_external,
                    else_expr->as.identifier.name);
            else_init_base = ir_unwrap_function_value_wrapper_expression(else_init_expr);
        }
        if (then_init_base &&
            else_init_base &&
            then_init_base->kind == AST_EXPR_CLOSURE &&
            else_init_base->kind == AST_EXPR_CLOSURE) {
            size_t slot_count = then_init_base->as.closure.capture_name_count;

            if (else_init_base->as.closure.capture_name_count != slot_count) {
                return 0;
            }
            if (program) {
                const char **returned_target_names = NULL;
                size_t returned_target_count = 0u;

                if (!ir_collect_function_value_return_target_names(
                        ast_program,
                        program,
                        resolved_external,
                        &returned_target_names,
                        &returned_target_count,
                        NULL)) {
                    free(returned_target_names);
                    return 0;
                }
                if (returned_target_count > 1u) {
                    slot_count += 1u;
                }
                free(returned_target_names);
            }
            *out_slot_count = slot_count;
            return 1;
        }
    }
    return 0;
}

static int ir_build_returned_closure_helper_name(const AstExternal *external,
    char **out_name,
    IrError *error) {
    const AstExpression *closure_expr = NULL;
    size_t needed = 0u;
    char *name = NULL;

    if (out_name) {
        *out_name = NULL;
    }
    if (!external || !external->name || external->name[0] == '\0' || !external->function_body || !out_name) {
        ir_set_error(error, 0, 0, "IR-INT-091: invalid returned-closure helper naming contract");
        return 0;
    }

    closure_expr = ir_find_function_returned_closure_expr(external->function_body);
    if (!closure_expr) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-092: returned-closure helper naming requires a closure return expression");
        return 0;
    }

    needed = strlen(external->name) + 48u;
    name = (char *)malloc(needed);
    if (!name) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-093: out of memory while naming returned-closure helper");
        return 0;
    }

    snprintf(name,
        needed,
        "%s__retclosure_%d_%d",
        external->name,
        closure_expr->line,
        closure_expr->column);
    *out_name = name;
    return 1;
}

static int ir_lower_closure_helper_function_from_expr(IrLowerContext *ctx,
    const AstExpression *closure_expr,
    const char *helper_name,
    const char *const *capture_names,
    const size_t *capture_local_ids,
    size_t capture_count,
    IrError *error) {
    AstExternal synthetic_external;
    AstExternal *registered_external = NULL;
    IrFunction *function = NULL;
    char **parameter_names = NULL;
    int *parameter_value_kinds = NULL;
    AstFunctionReturnType *parameter_function_return_types = NULL;
    size_t *parameter_function_parameter_counts = NULL;
    int **parameter_function_parameter_value_kinds = NULL;
    AstFunctionReturnType **parameter_function_parameter_return_types = NULL;
    size_t **parameter_function_parameter_parameter_counts = NULL;
    int *parameter_name_lines = NULL;
    int *parameter_name_columns = NULL;
    size_t parameter_count = 0u;
    size_t parameter_index = 0u;
    int ok = 0;

    if (!ctx || !ctx->ast_program || !ctx->program || !closure_expr ||
        closure_expr->kind != AST_EXPR_CLOSURE || !helper_name || helper_name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-094: invalid direct closure helper lowering contract");
        return 0;
    }

    parameter_count = capture_count + closure_expr->as.closure.parameter_count;
    if (parameter_count > 0u) {
        parameter_names = (char **)calloc(parameter_count, sizeof(char *));
        parameter_value_kinds = (int *)calloc(parameter_count, sizeof(int));
        parameter_function_return_types =
            (AstFunctionReturnType *)calloc(parameter_count, sizeof(AstFunctionReturnType));
        parameter_function_parameter_counts =
            (size_t *)calloc(parameter_count, sizeof(size_t));
        parameter_function_parameter_value_kinds =
            (int **)calloc(parameter_count, sizeof(int *));
        parameter_function_parameter_return_types =
            (AstFunctionReturnType **)calloc(parameter_count, sizeof(AstFunctionReturnType *));
        parameter_function_parameter_parameter_counts =
            (size_t **)calloc(parameter_count, sizeof(size_t *));
        parameter_name_lines = (int *)calloc(parameter_count, sizeof(int));
        parameter_name_columns = (int *)calloc(parameter_count, sizeof(int));
        if (!parameter_names || !parameter_value_kinds ||
            !parameter_function_return_types ||
            !parameter_function_parameter_counts ||
            !parameter_function_parameter_value_kinds ||
            !parameter_function_parameter_return_types ||
            !parameter_function_parameter_parameter_counts ||
            !parameter_name_lines || !parameter_name_columns) {
            ir_set_error(error, closure_expr->line, closure_expr->column,
                "IR-INT-095: out of memory while building direct closure helper signature");
            goto cleanup;
        }
    }

    for (parameter_index = 0u; parameter_index < capture_count; ++parameter_index) {
        parameter_names[parameter_index] = (char *)(capture_names ? capture_names[parameter_index] : NULL);
        parameter_value_kinds[parameter_index] = AST_PARAMETER_VALUE_INT;
        parameter_name_lines[parameter_index] = closure_expr->line;
        parameter_name_columns[parameter_index] = closure_expr->column;
    }
    for (parameter_index = 0u; parameter_index < closure_expr->as.closure.parameter_count; ++parameter_index) {
        size_t dest_index = capture_count + parameter_index;

        parameter_names[dest_index] =
            closure_expr->as.closure.parameter_names
                ? closure_expr->as.closure.parameter_names[parameter_index]
                : NULL;
        parameter_value_kinds[dest_index] =
            closure_expr->as.closure.parameter_value_kinds
                ? closure_expr->as.closure.parameter_value_kinds[parameter_index]
                : AST_PARAMETER_VALUE_INT;
        parameter_function_return_types[dest_index] =
            closure_expr->as.closure.parameter_function_return_types
                ? closure_expr->as.closure.parameter_function_return_types[parameter_index]
                : AST_FUNCTION_RETURN_INT;
        parameter_function_parameter_counts[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_counts
                ? closure_expr->as.closure.parameter_function_parameter_counts[parameter_index]
                : 0u;
        parameter_function_parameter_value_kinds[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_value_kinds
                ? closure_expr->as.closure.parameter_function_parameter_value_kinds[parameter_index]
                : NULL;
        parameter_function_parameter_return_types[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_return_types
                ? closure_expr->as.closure.parameter_function_parameter_return_types[parameter_index]
                : NULL;
        parameter_function_parameter_parameter_counts[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_parameter_counts
                ? closure_expr->as.closure.parameter_function_parameter_parameter_counts[parameter_index]
                : NULL;
        parameter_name_lines[dest_index] = closure_expr->line;
        parameter_name_columns[dest_index] = closure_expr->column;
    }

    memset(&synthetic_external, 0, sizeof(synthetic_external));
    synthetic_external.kind = AST_EXTERNAL_FUNCTION;
    synthetic_external.function_return_type = closure_expr->as.closure.return_type;
    synthetic_external.name = (char *)helper_name;
    synthetic_external.name_length = strlen(helper_name);
    synthetic_external.parameter_count = parameter_count;
    synthetic_external.parameter_names = parameter_names;
    synthetic_external.parameter_value_kinds = parameter_value_kinds;
    synthetic_external.parameter_type_names = NULL;
    synthetic_external.parameter_array_ranks = NULL;
    synthetic_external.parameter_array_extent_exprs = NULL;
    synthetic_external.parameter_function_return_types =
        parameter_function_return_types;
    synthetic_external.parameter_function_parameter_counts =
        parameter_function_parameter_counts;
    synthetic_external.parameter_function_parameter_value_kinds =
        parameter_function_parameter_value_kinds;
    synthetic_external.parameter_function_parameter_return_types =
        parameter_function_parameter_return_types;
    synthetic_external.parameter_function_parameter_parameter_counts =
        parameter_function_parameter_parameter_counts;
    synthetic_external.parameter_is_const = NULL;
    synthetic_external.parameter_name_lines = parameter_name_lines;
    synthetic_external.parameter_name_columns = parameter_name_columns;
    synthetic_external.is_function_definition = 1;
    synthetic_external.function_body = closure_expr->as.closure.body;
    synthetic_external.line = closure_expr->line;
    synthetic_external.column = closure_expr->column;

    function = ir_program_find_function_mut(ctx->program, helper_name);
    if (function && function->has_body) {
        ok = 1;
        goto cleanup;
    }
    if (!ir_ensure_function_signature(
            ctx->ast_program,
            ctx->program,
            &synthetic_external,
            &function,
            error)) {
        goto cleanup;
    }
    if (function) {
        registered_external = (AstExternal *)malloc(sizeof(AstExternal));
        if (!registered_external) {
            ir_set_error(error, closure_expr->line, closure_expr->column,
                "IR-INT-144: out of memory while registering closure helper external metadata");
            goto cleanup;
        }
        *registered_external = synthetic_external;
        registered_external->name = function->name;
        registered_external->name_length = function->name ? strlen(function->name) : 0u;
        if (!ir_program_bind_function_external_ref(
                ctx->program,
                helper_name,
                registered_external,
                1,
                error)) {
            free(registered_external);
            registered_external = NULL;
            goto cleanup;
        }
        registered_external = NULL;
        parameter_names = NULL;
        parameter_value_kinds = NULL;
        parameter_function_return_types = NULL;
        parameter_function_parameter_counts = NULL;
        parameter_function_parameter_value_kinds = NULL;
        parameter_function_parameter_return_types = NULL;
        parameter_function_parameter_parameter_counts = NULL;
        parameter_name_lines = NULL;
        parameter_name_columns = NULL;
    }
    if (ir_external_body_needs_function_value_specialization(
            ctx->ast_program,
            &synthetic_external,
            synthetic_external.function_body)) {
        ok = 1;
        goto cleanup;
    }
    ok = ir_lower_function_core(
        ctx->ast_program,
        ctx->program,
        function,
        NULL,
        &synthetic_external,
        NULL,
        NULL,
        NULL,
        error);
    (void)capture_local_ids;

cleanup:
    free(parameter_names);
    free(parameter_value_kinds);
    free(parameter_function_return_types);
    free(parameter_function_parameter_counts);
    free(parameter_function_parameter_value_kinds);
    free(parameter_function_parameter_return_types);
    free(parameter_function_parameter_parameter_counts);
    free(parameter_name_lines);
    free(parameter_name_columns);
    return ok;
}

static int ir_ensure_returned_closure_helper_function(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const char **out_target_name,
    IrError *error) {
    const AstExpression *closure_expr = NULL;
    IrLowerContext helper_ctx;
    char *helper_name = NULL;
    int ok = 0;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ast_program || !program || !external || !external->function_body || !out_target_name) {
        ir_set_error(error, 0, 0, "IR-INT-096: invalid returned-closure helper ensure contract");
        return 0;
    }

    closure_expr = ir_find_function_returned_closure_expr(external->function_body);
    if (!closure_expr) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-097: returned-closure helper ensure requires a closure return expression");
        return 0;
    }
    if (!ir_build_returned_closure_helper_name(external, &helper_name, error)) {
        return 0;
    }

    memset(&helper_ctx, 0, sizeof(helper_ctx));
    helper_ctx.ast_program = ast_program;
    helper_ctx.program = program;
    ok = ir_lower_closure_helper_function_from_expr(
        &helper_ctx,
        closure_expr,
        helper_name,
        (const char *const *)closure_expr->as.closure.capture_names,
        NULL,
        closure_expr->as.closure.capture_name_count,
        error);
    if (ok) {
        IrFunction *helper_function = ir_program_find_function_mut(program, helper_name);
        if (!helper_function || !helper_function->name) {
            ir_set_error(error, external->line, external->column,
                "IR-INT-098: returned-closure helper disappeared after lowering");
            ok = 0;
        } else {
            *out_target_name = helper_function->name;
        }
    }

    free(helper_name);
    return ok;
}

static int ir_resolve_local_function_value_declaration_target_name(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstStatement *decl_stmt,
    size_t decl_index,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char **out_target_name,
    IrError *error) {
    const AstExpression *init_expr = NULL;
    const AstExpression *base = NULL;
    const char **target_names = NULL;
    size_t target_count = 0u;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ast_program || !program || !external || !external->name || !decl_stmt ||
        !decl_stmt->declaration_names || !decl_stmt->expressions ||
        decl_index >= decl_stmt->declaration_name_count ||
        decl_index >= decl_stmt->expression_count || !out_target_name) {
        return 0;
    }

    init_expr = decl_stmt->expressions[decl_index];
    base = ir_unwrap_function_value_wrapper_expression(init_expr);
    if (!base) {
        return 0;
    }

    if (base->kind == AST_EXPR_CLOSURE) {
        IrLowerContext helper_ctx;
        char *helper_name = NULL;
        const char *resolved_name = NULL;
        int ok = 0;
        size_t closure_ordinal =
            ((size_t)(decl_stmt->line > 0 ? decl_stmt->line : 0) << 16) |
            (size_t)((decl_stmt->column > 0 ? decl_stmt->column : 0) + decl_index);

        if (!decl_stmt->declaration_names[decl_index] ||
            decl_stmt->declaration_names[decl_index][0] == '\0' ||
            !ir_build_closure_helper_name(
                external->name,
                decl_stmt->declaration_names[decl_index],
                closure_ordinal,
                &helper_name,
                error)) {
            free(helper_name);
            return 0;
        }

        memset(&helper_ctx, 0, sizeof(helper_ctx));
        helper_ctx.ast_program = ast_program;
        helper_ctx.program = program;
        ok = ir_lower_closure_helper_function_from_expr(
            &helper_ctx,
            base,
            helper_name,
            (const char *const *)base->as.closure.capture_names,
            NULL,
            base->as.closure.capture_name_count,
            error);
        if (ok) {
            IrFunction *helper_function = ir_program_find_function_mut(program, helper_name);
            if (!helper_function || !helper_function->name) {
                ir_set_error(error,
                    decl_stmt->line,
                    decl_stmt->column,
                    "IR-INT-098: local returned-closure helper disappeared after lowering");
                ok = 0;
            } else {
                resolved_name = helper_function->name;
            }
        }

        free(helper_name);
        if (!ok) {
            return 0;
        }
        *out_target_name = resolved_name;
        return resolved_name != NULL;
    }

    if (base->kind == AST_EXPR_CALL) {
        if (ir_resolve_function_value_passthrough_target_name_recursive(
                ast_program,
                program,
                external,
                init_expr,
                out_target_name,
                error) &&
            out_target_name &&
            *out_target_name &&
            (*out_target_name)[0] != '\0') {
            return 1;
        }
    }

    if (ir_resolve_function_value_wrapper_target_name(
            ast_program,
            program,
            init_expr,
            binding_names,
            binding_targets,
            binding_count,
            out_target_name,
            error) &&
        out_target_name &&
        *out_target_name &&
        (*out_target_name)[0] != '\0') {
        return 1;
    }

    if (ir_collect_function_value_wrapper_target_names_in_declaration_context(
            ast_program,
            program,
            external,
            init_expr,
            binding_names,
            binding_targets,
            binding_count,
            &target_names,
            &target_count,
            error) &&
        target_names &&
        target_count > 0u &&
        target_names[0] &&
        target_names[0][0] != '\0') {
        *out_target_name = target_names[0];
        free(target_names);
        return 1;
    }

    free(target_names);
    return 0;
}

static int ir_resolve_function_value_return_target_name(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const char **out_target_name,
    IrError *error) {
    const AstExternal *resolved_external = external;
    const AstExpression *return_expr = NULL;
    const char *name = NULL;
    IrFunction *builtin_function = NULL;
    IrFunction *existing_function = NULL;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ast_program || !program || !external || !out_target_name) {
        ir_set_error(error, 0, 0, "IR-INT-099: invalid function-value return target resolution contract");
        return 0;
    }
    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        return 1;
    }

    return_expr = ir_find_function_returned_value_expr(resolved_external->function_body);
    if (!return_expr) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-100: function-value return target resolution requires a return expression");
        return 0;
    }
    return_expr = ir_unwrap_function_value_wrapper_expression(return_expr);
    if (!return_expr) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
        return 0;
    }
    if (return_expr->kind == AST_EXPR_CLOSURE) {
        return ir_ensure_returned_closure_helper_function(
            ast_program,
            program,
            resolved_external,
            out_target_name,
            error);
    }
    if (return_expr->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.call.callee);
        const AstExternal *callee_external = NULL;
        const AstExpression *passthrough_arg_expr = NULL;

        if (ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                return_expr,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            const AstExpression *passthrough_base =
                ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);

            if (passthrough_base &&
                passthrough_base->kind == AST_EXPR_IDENTIFIER &&
                passthrough_base->as.identifier.name &&
                ir_resolve_function_value_return_local_binding_target_name(
                    ast_program,
                    program,
                    resolved_external,
                    passthrough_base->as.identifier.name,
                    out_target_name,
                    error)) {
                return 1;
            }
            return ir_resolve_function_value_wrapper_target_name(
                ast_program,
                program,
                passthrough_arg_expr,
                NULL,
                NULL,
                0u,
                out_target_name,
                error);
        }

        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            ir_set_error(error, external->line, external->column,
                "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
            return 0;
        }
        callee_external =
            ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        if (!callee_external || !callee_external->has_function_return_signature) {
            ir_set_error(error, external->line, external->column,
                "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
            return 0;
        }
        return ir_resolve_function_value_return_target_name(
            ast_program,
            program,
            callee_external,
            out_target_name,
            error);
    }
    if (return_expr->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.then_expr);
        const AstExpression *else_expr =
            ir_unwrap_function_value_wrapper_expression(return_expr->as.ternary.else_expr);
        const AstExpression *then_passthrough_arg_expr = NULL;
        const AstExpression *else_passthrough_arg_expr = NULL;
        const char *then_target_name = NULL;
        const char *else_target_name = NULL;

        if (!then_expr || !else_expr) {
            ir_set_error(error, external->line, external->column,
                "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
            return 0;
        }
        if (then_expr->kind == AST_EXPR_CALL &&
            ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                then_expr,
                &then_passthrough_arg_expr) &&
            then_passthrough_arg_expr) {
            then_expr =
                ir_unwrap_function_value_wrapper_expression(then_passthrough_arg_expr);
        }
        if (else_expr->kind == AST_EXPR_CALL &&
            ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                else_expr,
                &else_passthrough_arg_expr) &&
            else_passthrough_arg_expr) {
            else_expr =
                ir_unwrap_function_value_wrapper_expression(else_passthrough_arg_expr);
        }
        if (!then_expr || !else_expr) {
            ir_set_error(error, external->line, external->column,
                "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
            return 0;
        }
        if (((then_expr->kind == AST_EXPR_IDENTIFIER && then_expr->as.identifier.name)
                ? ((!ir_resolve_function_value_return_local_binding_target_name(
                        ast_program,
                        program,
                        resolved_external,
                        then_expr->as.identifier.name,
                        &then_target_name,
                        error) &&
                    !ir_resolve_noncapturing_function_target_name(
                        program,
                        then_expr->as.identifier.name,
                        NULL,
                        NULL,
                        0u,
                        &then_target_name,
                        error)))
                : (!ir_resolve_function_value_wrapper_target_name(
                        ast_program,
                        program,
                        then_expr,
                        NULL,
                        NULL,
                        0u,
                        &then_target_name,
                        error))) ||
            ((else_expr->kind == AST_EXPR_IDENTIFIER && else_expr->as.identifier.name)
                ? ((!ir_resolve_function_value_return_local_binding_target_name(
                        ast_program,
                        program,
                        resolved_external,
                        else_expr->as.identifier.name,
                        &else_target_name,
                        error) &&
                    !ir_resolve_noncapturing_function_target_name(
                        program,
                        else_expr->as.identifier.name,
                        NULL,
                        NULL,
                        0u,
                        &else_target_name,
                        error)))
                : (!ir_resolve_function_value_wrapper_target_name(
                        ast_program,
                        program,
                        else_expr,
                        NULL,
                        NULL,
                        0u,
                        &else_target_name,
                        error))) ||
            !then_target_name || !else_target_name || then_target_name[0] == '\0') {
            ir_set_error(error, external->line, external->column,
                "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
            return 0;
        }
        *out_target_name = then_target_name;
        return 1;
    }
    if (return_expr->kind != AST_EXPR_IDENTIFIER || !return_expr->as.identifier.name) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-101: function-value return target resolution currently requires a closure literal or direct function identifier");
        return 0;
    }

    name = return_expr->as.identifier.name;
    if (ir_function_parameter_name_is_compatible_function_value(
            resolved_external,
            name,
            resolved_external->function_return_function_return_type,
            resolved_external->function_return_function_parameter_count)) {
        return 1;
    }
    if (ir_resolve_function_value_return_local_binding_target_name(
            ast_program,
            program,
            resolved_external,
            name,
            out_target_name,
            error)) {
        return 1;
    }
    existing_function = ir_program_find_function_mut(program, name);
    if (existing_function && existing_function->name) {
        *out_target_name = existing_function->name;
        return 1;
    }
    if (ir_ensure_builtin_function_signature(program, name, &builtin_function, error) &&
        builtin_function && builtin_function->name) {
        *out_target_name = builtin_function->name;
        return 1;
    }

    ir_set_error(error, external->line, external->column,
        "IR-INT-102: function-value return target '%s' is not available during IR lowering",
        name);
    return 0;
}

static int ir_resolve_noncapturing_function_target_name(IrProgram *program,
    const char *name,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char **out_target_name,
    IrError *error) {
    size_t binding_index;
    IrFunction *builtin_function = NULL;
    IrFunction *existing_function = NULL;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!program || !name || name[0] == '\0' || !out_target_name) {
        return 0;
    }

    for (binding_index = binding_count; binding_index > 0u; --binding_index) {
        if (binding_names &&
            binding_names[binding_index - 1u] &&
            strcmp(binding_names[binding_index - 1u], name) == 0) {
            if (!binding_targets || !binding_targets[binding_index - 1u] ||
                binding_targets[binding_index - 1u][0] == '\0') {
                return 0;
            }
            *out_target_name = binding_targets[binding_index - 1u];
            return 1;
        }
    }

    existing_function = ir_program_find_function_mut(program, name);
    if (existing_function && existing_function->name) {
        *out_target_name = existing_function->name;
        return 1;
    }
    if (ir_ensure_builtin_function_signature(program, name, &builtin_function, error) &&
        builtin_function && builtin_function->name) {
        *out_target_name = builtin_function->name;
        return 1;
    }

    return 0;
}

static const AstExpression *ir_find_local_function_value_initializer_expr(const AstExternal *external,
    const char *name) {
    const AstStatement *body = NULL;
    size_t child_index;

    if (!external || !name || name[0] == '\0' ||
        !external->function_body ||
        external->function_body->kind != AST_STMT_COMPOUND) {
        return NULL;
    }

    body = external->function_body;
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
            if (!child->declaration_names[decl_index] ||
                strcmp(child->declaration_names[decl_index], name) != 0 ||
                child->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_FUNCTION) {
                continue;
            }
            return child->expressions[decl_index];
        }
    }

    return NULL;
}

static const AstExpression *ir_find_resolved_local_function_value_initializer_expr(const AstProgram *ast_program,
    const AstExternal *external,
    const char *name) {
    const AstExpression *current_expr = NULL;
    const AstExpression *current_base = NULL;
    const char *current_name = name;
    size_t hop_count = 0u;

    if (!ast_program || !external || !name || name[0] == '\0') {
        return NULL;
    }

    while (current_name && current_name[0] != '\0' && hop_count < 64u) {
        const AstExpression *passthrough_arg_expr = NULL;

        current_expr = ir_find_local_function_value_initializer_expr(external, current_name);
        current_base = ir_unwrap_function_value_wrapper_expression(current_expr);
        if (current_base &&
            current_base->kind == AST_EXPR_CALL &&
            ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                current_base,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            current_base = ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);
        }
        if (!current_expr || !current_base ||
            ((current_base->kind != AST_EXPR_IDENTIFIER ||
                !current_base->as.identifier.name ||
                strcmp(current_base->as.identifier.name, current_name) == 0) &&
                current_base->kind != AST_EXPR_TERNARY &&
                current_base->kind != AST_EXPR_CLOSURE &&
                current_base->kind != AST_EXPR_CALL)) {
            return current_expr;
        }
        if (current_base->kind != AST_EXPR_IDENTIFIER) {
            return current_base;
        }
        current_name = current_base->as.identifier.name;
        ++hop_count;
    }

    return current_expr;
}

static int ir_collect_function_value_wrapper_target_names_in_return_context(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!ast_program || !program || !external || !base ||
        !io_target_names || !io_target_count) {
        return 0;
    }
    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const AstExternal *callee_external = NULL;
        size_t passthrough_index = (size_t)-1;

        if (callee_base &&
            callee_base->kind == AST_EXPR_IDENTIFIER &&
            callee_base->as.identifier.name) {
            callee_external =
                ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        }
        if (callee_external &&
            callee_external->has_function_return_signature &&
            ir_function_value_return_exact_parameter_passthrough_index(
                ast_program,
                callee_external,
                &passthrough_index) &&
            passthrough_index != (size_t)-1 &&
            base->as.call.args &&
            passthrough_index < base->as.call.arg_count) {
            return ir_collect_function_value_wrapper_target_names_in_return_context(
                ast_program,
                program,
                external,
                base->as.call.args[passthrough_index],
                binding_names,
                binding_targets,
                binding_count,
                io_target_names,
                io_target_count,
                error);
        }
    }
    if (base->kind == AST_EXPR_IDENTIFIER &&
        base->as.identifier.name) {
        const AstExpression *resolved_init_expr =
            ir_find_resolved_local_function_value_initializer_expr(
                ast_program,
                external,
                base->as.identifier.name);
        const AstExpression *resolved_init_base =
            ir_unwrap_function_value_wrapper_expression(resolved_init_expr);

        if (resolved_init_expr && resolved_init_base &&
            resolved_init_base != base) {
            if (ir_collect_function_value_wrapper_target_names_in_return_context(
                    ast_program,
                    program,
                    external,
                    resolved_init_expr,
                    binding_names,
                    binding_targets,
                    binding_count,
                    io_target_names,
                    io_target_count,
                    error)) {
                return 1;
            }
        }
    }

    if (base->kind == AST_EXPR_TERNARY) {
        if (!ir_collect_function_value_wrapper_target_names_in_return_context(
                ast_program,
                program,
                external,
                base->as.ternary.then_expr,
                binding_names,
                binding_targets,
                binding_count,
                io_target_names,
                io_target_count,
                error)) {
            return 0;
        }
        return ir_collect_function_value_wrapper_target_names_in_return_context(
            ast_program,
            program,
            external,
            base->as.ternary.else_expr,
            binding_names,
            binding_targets,
            binding_count,
            io_target_names,
            io_target_count,
            error);
    }

    return ir_collect_function_value_wrapper_target_names(
        ast_program,
        program,
        expr,
        binding_names,
        binding_targets,
        binding_count,
        io_target_names,
        io_target_count,
        error);
}

static int ir_resolve_function_value_passthrough_target_name_recursive(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstExpression *expr,
    const char **out_target_name,
    IrError *error) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);
    const AstExpression *resolved_init_expr = NULL;
    const AstExpression *resolved_init_base = NULL;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ast_program || !program || !expr || !base || !out_target_name) {
        return 0;
    }
    if (base->kind == AST_EXPR_IDENTIFIER &&
        base->as.identifier.name) {
        if (external) {
            resolved_init_expr =
                ir_find_resolved_local_function_value_initializer_expr(
                    ast_program,
                    external,
                    base->as.identifier.name);
            resolved_init_base = ir_unwrap_function_value_wrapper_expression(resolved_init_expr);
            if (resolved_init_expr && resolved_init_base && resolved_init_base != base) {
                if (ir_resolve_function_value_passthrough_target_name_recursive(
                        ast_program,
                        program,
                        external,
                        resolved_init_expr,
                        out_target_name,
                        error) &&
                    *out_target_name &&
                    (*out_target_name)[0] != '\0') {
                    return 1;
                }
            }
        }
        return ir_resolve_noncapturing_function_target_name(
            program,
            base->as.identifier.name,
            NULL,
            NULL,
            0u,
            out_target_name,
            error);
    }
    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *passthrough_arg_expr = NULL;
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const AstExternal *callee_external = NULL;

        if (ir_try_resolve_returned_function_value_passthrough_argument_expr(
                ast_program,
                base,
                &passthrough_arg_expr) &&
            passthrough_arg_expr &&
            ir_resolve_function_value_passthrough_target_name_recursive(
                ast_program,
                program,
                external,
                passthrough_arg_expr,
                out_target_name,
                error) &&
            *out_target_name &&
            (*out_target_name)[0] != '\0') {
            return 1;
        }

        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            return 0;
        }
        callee_external =
            ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        if (!callee_external || !callee_external->has_function_return_signature) {
            return 0;
        }
        return ir_resolve_function_value_return_target_name(
            ast_program,
            program,
            callee_external,
            out_target_name,
            error);
    }

    return 0;
}

static int ir_collect_function_value_wrapper_target_names_in_declaration_context(
    const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!ast_program || !program || !external || !base ||
        !io_target_names || !io_target_count) {
        return 0;
    }
    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const AstExternal *callee_external = NULL;
        size_t passthrough_index = (size_t)-1;

        if (callee_base &&
            callee_base->kind == AST_EXPR_IDENTIFIER &&
            callee_base->as.identifier.name) {
            callee_external =
                ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        }
        if (callee_external &&
            callee_external->has_function_return_signature &&
            ir_function_value_return_exact_parameter_passthrough_index(
                ast_program,
                callee_external,
                &passthrough_index) &&
            passthrough_index != (size_t)-1 &&
            base->as.call.args &&
            passthrough_index < base->as.call.arg_count) {
            return ir_collect_function_value_wrapper_target_names_in_declaration_context(
                ast_program,
                program,
                external,
                base->as.call.args[passthrough_index],
                binding_names,
                binding_targets,
                binding_count,
                io_target_names,
                io_target_count,
                error);
        }
    }
    if (base->kind == AST_EXPR_IDENTIFIER &&
        base->as.identifier.name) {
        const AstExpression *resolved_init_expr =
            ir_find_resolved_local_function_value_initializer_expr(
                ast_program,
                external,
                base->as.identifier.name);
        const AstExpression *resolved_init_base =
            ir_unwrap_function_value_wrapper_expression(resolved_init_expr);

        if (resolved_init_expr && resolved_init_base &&
            resolved_init_base != base) {
            return ir_collect_function_value_wrapper_target_names_in_declaration_context(
                ast_program,
                program,
                external,
                resolved_init_expr,
                binding_names,
                binding_targets,
                binding_count,
                io_target_names,
                io_target_count,
                error);
        }
    }

    return ir_collect_function_value_wrapper_target_names(
        ast_program,
        program,
        expr,
        binding_names,
        binding_targets,
        binding_count,
        io_target_names,
        io_target_count,
        error);
}

static int ir_resolve_function_value_wrapper_target_name(const AstProgram *ast_program,
    IrProgram *program,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char **out_target_name,
    IrError *error) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ast_program || !program || !base || !out_target_name) {
        return 0;
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_COMMA) {
        return ir_resolve_function_value_wrapper_target_name(
            ast_program,
            program,
            base->as.binary.right,
            binding_names,
            binding_targets,
            binding_count,
            out_target_name,
            error);
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_ASSIGN) {
        return ir_resolve_function_value_wrapper_target_name(
            ast_program,
            program,
            base->as.binary.right,
            binding_names,
            binding_targets,
            binding_count,
            out_target_name,
            error);
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
        if (ir_resolve_function_value_return_local_binding_target_name(
                ast_program,
                program,
                NULL,
                base->as.identifier.name,
                out_target_name,
                error)) {
            return 1;
        }
        return ir_resolve_noncapturing_function_target_name(
            program,
            base->as.identifier.name,
            binding_names,
            binding_targets,
            binding_count,
            out_target_name,
            error);
    }

    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const AstExternal *callee_external = NULL;
        size_t passthrough_index = (size_t)-1;

        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            return 0;
        }

        callee_external =
            ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        if (!callee_external || !callee_external->has_function_return_signature) {
            return 0;
        }

        if (ir_function_value_return_exact_parameter_passthrough_index(
                ast_program,
                callee_external,
                &passthrough_index) &&
            passthrough_index != (size_t)-1 &&
            base->as.call.args &&
            passthrough_index < base->as.call.arg_count) {
            return ir_resolve_function_value_wrapper_target_name(
                ast_program,
                program,
                base->as.call.args[passthrough_index],
                binding_names,
                binding_targets,
                binding_count,
                out_target_name,
                error);
        }

        return ir_resolve_function_value_return_target_name(
            ast_program,
            program,
            callee_external,
            out_target_name,
            error);
    }

    return 0;
}

static int ir_resolve_function_value_return_local_binding_target_name(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const char *return_name,
    const char **out_target_name,
    IrError *error) {
    const AstStatement *body = NULL;
    const char **binding_names = NULL;
    const char **binding_targets = NULL;
    size_t binding_count = 0u;
    size_t child_index;
    int ok = 0;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ast_program || !program || !return_name || return_name[0] == '\0' ||
        !out_target_name) {
        return 0;
    }

    if (!external || !external->function_body || external->function_body->kind != AST_STMT_COMPOUND) {
        return 0;
    }

    body = external->function_body;
    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children ? body->children[child_index] : NULL;
        if (!child) {
            continue;
        }

        if (child->kind == AST_STMT_DECLARATION) {
            size_t decl_index;
            for (decl_index = 0u; decl_index < child->declaration_name_count &&
                    decl_index < child->expression_count; ++decl_index) {
                const char *decl_name =
                    child->declaration_names ? child->declaration_names[decl_index] : NULL;
                int decl_value_kind =
                    child->declaration_value_kinds ? child->declaration_value_kinds[decl_index] : AST_DECLARATION_VALUE_INT;
                const char *resolved_target = NULL;

                if (!decl_name || decl_name[0] == '\0' ||
                    decl_value_kind != AST_DECLARATION_VALUE_FUNCTION) {
                    continue;
                }
                if (!ir_resolve_local_function_value_declaration_target_name(
                        ast_program,
                        program,
                        external,
                        child,
                        decl_index,
                        binding_names,
                        binding_targets,
                        binding_count,
                        &resolved_target,
                        error)) {
                    continue;
                }
                binding_names = (const char **)realloc(binding_names, (binding_count + 1u) * sizeof(const char *));
                binding_targets = (const char **)realloc(binding_targets, (binding_count + 1u) * sizeof(const char *));
                if (!binding_names || !binding_targets) {
                    free(binding_names);
                    free(binding_targets);
                    return 0;
                }
                binding_names[binding_count] = decl_name;
                binding_targets[binding_count] = resolved_target;
                binding_count += 1u;
            }
            continue;
        }

        if (child->kind == AST_STMT_EXPRESSION &&
            child->expression_count == 1u &&
            child->expressions) {
            const AstExpression *expr = child->expressions[0];
            const AstExpression *expr_base = ir_unwrap_paren_expression(expr);
            if (expr_base && expr_base->kind == AST_EXPR_BINARY && expr_base->as.binary.op == TOKEN_ASSIGN) {
                const AstExpression *lhs_base = ir_unwrap_paren_expression(expr_base->as.binary.left);
                const AstExpression *rhs_base = ir_unwrap_paren_expression(expr_base->as.binary.right);
                const char *resolved_target = NULL;
                size_t binding_index;

                if (lhs_base && rhs_base &&
                    lhs_base->kind == AST_EXPR_IDENTIFIER && lhs_base->as.identifier.name &&
                    ((rhs_base->kind == AST_EXPR_IDENTIFIER &&
                        rhs_base->as.identifier.name &&
                        ir_resolve_noncapturing_function_target_name(
                            program,
                            rhs_base->as.identifier.name,
                            binding_names,
                            binding_targets,
                            binding_count,
                            &resolved_target,
                            error)) ||
                        ir_resolve_function_value_wrapper_target_name(
                            ast_program,
                            program,
                            expr_base->as.binary.right,
                            binding_names,
                            binding_targets,
                            binding_count,
                            &resolved_target,
                            error))) {
                    for (binding_index = binding_count; binding_index > 0u; --binding_index) {
                        if (binding_names[binding_index - 1u] &&
                            strcmp(binding_names[binding_index - 1u], lhs_base->as.identifier.name) == 0) {
                            binding_targets[binding_index - 1u] = resolved_target;
                            break;
                        }
                    }
                }
            }
            continue;
        }

        if (child->kind == AST_STMT_RETURN) {
            const AstExpression *return_expr =
                (child->expressions && child->expression_count > 0u) ? child->expressions[0] : NULL;
            const AstExpression *return_base =
                ir_unwrap_function_value_wrapper_expression(return_expr);

            if (return_base &&
                return_base->kind == AST_EXPR_IDENTIFIER &&
                return_base->as.identifier.name &&
                strcmp(return_base->as.identifier.name, return_name) == 0) {
                ok = ir_resolve_function_value_wrapper_target_name(
                    ast_program,
                    program,
                    return_expr,
                    binding_names,
                    binding_targets,
                    binding_count,
                    out_target_name,
                    error);
                free(binding_names);
                free(binding_targets);
                return ok;
            }
            continue;
        }
    }

    if (out_target_name) {
        size_t binding_index;
        for (binding_index = binding_count; binding_index > 0u; --binding_index) {
            if (binding_names[binding_index - 1u] &&
                strcmp(binding_names[binding_index - 1u], return_name) == 0 &&
                binding_targets[binding_index - 1u] &&
                binding_targets[binding_index - 1u][0] != '\0') {
                *out_target_name = binding_targets[binding_index - 1u];
                free(binding_names);
                free(binding_targets);
                return 1;
            }
        }
    }
    free(binding_names);
    free(binding_targets);
    return 0;
}

static int ir_collect_function_value_return_target_names_append_unique(
    const char ***io_target_names,
    size_t *io_target_count,
    const char *target_name) {
    const char **new_target_names = NULL;
    size_t index;

    if (!io_target_names || !io_target_count || !target_name || target_name[0] == '\0') {
        return 0;
    }

    for (index = 0u; index < *io_target_count; ++index) {
        if ((*io_target_names)[index] && strcmp((*io_target_names)[index], target_name) == 0) {
            return 1;
        }
    }

    new_target_names =
        (const char **)realloc((void *)*io_target_names, (*io_target_count + 1u) * sizeof(const char *));
    if (!new_target_names) {
        return 0;
    }
    *io_target_names = new_target_names;
    (*io_target_names)[(*io_target_count)++] = target_name;
    return 1;
}

static int ir_collect_function_value_return_target_names_bind_name(
    const char ***io_binding_names,
    const char ***io_binding_targets,
    size_t *io_binding_count,
    const char *binding_name,
    const char *target_name) {
    const char **new_binding_names = NULL;
    const char **new_binding_targets = NULL;
    size_t binding_index;

    if (!io_binding_names || !io_binding_targets || !io_binding_count ||
        !binding_name || binding_name[0] == '\0' || !target_name || target_name[0] == '\0') {
        return 0;
    }

    for (binding_index = *io_binding_count; binding_index > 0u; --binding_index) {
        if ((*io_binding_names)[binding_index - 1u] &&
            strcmp((*io_binding_names)[binding_index - 1u], binding_name) == 0) {
            (*io_binding_targets)[binding_index - 1u] = target_name;
            return 1;
        }
    }

    new_binding_names =
        (const char **)realloc((void *)*io_binding_names, (*io_binding_count + 1u) * sizeof(const char *));
    if (!new_binding_names) {
        return 0;
    }
    *io_binding_names = new_binding_names;
    new_binding_targets =
        (const char **)realloc((void *)*io_binding_targets, (*io_binding_count + 1u) * sizeof(const char *));
    if (!new_binding_targets) {
        return 0;
    }
    *io_binding_targets = new_binding_targets;
    (*io_binding_names)[*io_binding_count] = binding_name;
    (*io_binding_targets)[*io_binding_count] = target_name;
    *io_binding_count += 1u;
    return 1;
}

static int ir_collect_function_value_wrapper_target_names(const AstProgram *ast_program,
    IrProgram *program,
    const AstExpression *expr,
    const char *const *binding_names,
    const char *const *binding_targets,
    size_t binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const char *target_name = NULL;

    if (!ast_program || !program || !base || !io_target_names || !io_target_count) {
        return 0;
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_COMMA) {
        return ir_collect_function_value_wrapper_target_names(
            ast_program,
            program,
            base->as.binary.right,
            binding_names,
            binding_targets,
            binding_count,
            io_target_names,
            io_target_count,
            error);
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_ASSIGN) {
        return ir_collect_function_value_wrapper_target_names(
            ast_program,
            program,
            base->as.binary.right,
            binding_names,
            binding_targets,
            binding_count,
            io_target_names,
            io_target_count,
            error);
    }

    if (base->kind == AST_EXPR_TERNARY) {
        if (!ir_collect_function_value_wrapper_target_names(
                ast_program,
                program,
                base->as.ternary.then_expr,
                binding_names,
                binding_targets,
                binding_count,
                io_target_names,
                io_target_count,
                error)) {
            return 0;
        }
        return ir_collect_function_value_wrapper_target_names(
            ast_program,
            program,
            base->as.ternary.else_expr,
            binding_names,
            binding_targets,
            binding_count,
            io_target_names,
            io_target_count,
            error);
    }

    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const AstExternal *callee_external = NULL;
        const char **returned_target_names = NULL;
        size_t returned_target_count = 0u;
        size_t target_index;
        size_t passthrough_index = (size_t)-1;
        int ok = 1;

        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            return 0;
        }

        callee_external =
            ir_find_named_function_external_prefer_definition(ast_program, callee_base->as.identifier.name);
        if (!callee_external || !callee_external->has_function_return_signature) {
            return 0;
        }

        if (ir_function_value_return_exact_parameter_passthrough_index(
                ast_program,
                callee_external,
                &passthrough_index) &&
            passthrough_index != (size_t)-1 &&
            base->as.call.args &&
            passthrough_index < base->as.call.arg_count) {
            return ir_collect_function_value_wrapper_target_names(
                ast_program,
                program,
                base->as.call.args[passthrough_index],
                binding_names,
                binding_targets,
                binding_count,
                io_target_names,
                io_target_count,
                error);
        }

        if (!ir_collect_function_value_return_target_names(
                ast_program,
                program,
                callee_external,
                &returned_target_names,
                &returned_target_count,
                error)) {
            return 0;
        }

        for (target_index = 0u; target_index < returned_target_count; ++target_index) {
            if (!returned_target_names[target_index] ||
                !ir_collect_function_value_return_target_names_append_unique(
                    io_target_names,
                    io_target_count,
                    returned_target_names[target_index])) {
                ok = 0;
                break;
            }
        }

        free(returned_target_names);
        return ok;
    }

    if (!ir_resolve_function_value_wrapper_target_name(
            ast_program,
            program,
            expr,
            binding_names,
            binding_targets,
            binding_count,
            &target_name,
            error) ||
        !target_name || target_name[0] == '\0') {
        return 0;
    }

    return ir_collect_function_value_return_target_names_append_unique(
        io_target_names,
        io_target_count,
        target_name);
}

static int ir_collect_function_value_return_targets_in_statement(
    const AstProgram *ast_program,
    const AstExternal *external,
    const AstStatement *stmt,
    IrProgram *program,
    const char *tracked_return_name,
    const char ***io_binding_names,
    const char ***io_binding_targets,
    size_t *io_binding_count,
    const char ***io_target_names,
    size_t *io_target_count,
    IrError *error) {
    size_t child_index;

    if (!ast_program || !stmt) {
        return 1;
    }

    if (stmt->kind == AST_STMT_DECLARATION) {
        size_t decl_index;
        for (decl_index = 0u; decl_index < stmt->declaration_name_count &&
                decl_index < stmt->expression_count; ++decl_index) {
            const char *decl_name =
                stmt->declaration_names ? stmt->declaration_names[decl_index] : NULL;
            int decl_value_kind =
                stmt->declaration_value_kinds ? stmt->declaration_value_kinds[decl_index] : AST_DECLARATION_VALUE_INT;
            const char *resolved_target = NULL;

            if (!decl_name || decl_name[0] == '\0' ||
                decl_value_kind != AST_DECLARATION_VALUE_FUNCTION ||
                !program ||
                !ir_resolve_local_function_value_declaration_target_name(
                    ast_program,
                    program,
                    external,
                    stmt,
                    decl_index,
                    *io_binding_names,
                    *io_binding_targets,
                    *io_binding_count,
                    &resolved_target,
                    error) ||
                !ir_collect_function_value_return_target_names_bind_name(
                    io_binding_names,
                    io_binding_targets,
                    io_binding_count,
                    decl_name,
                    resolved_target)) {
                continue;
            }
            if (tracked_return_name &&
                strcmp(tracked_return_name, decl_name) == 0) {
                const AstExpression *initializer_expr =
                    (stmt->expressions && decl_index < stmt->expression_count)
                        ? stmt->expressions[decl_index]
                        : NULL;
                const AstExpression *resolved_initializer_expr = initializer_expr;

                if (decl_name && decl_name[0] != '\0') {
                    const AstExpression *recursive_initializer_expr =
                        ir_find_resolved_local_function_value_initializer_expr(
                            ast_program,
                            external,
                            decl_name);
                    if (recursive_initializer_expr) {
                        resolved_initializer_expr = recursive_initializer_expr;
                    }
                }
                if (!initializer_expr ||
                    !ir_collect_function_value_wrapper_target_names_in_return_context(
                        ast_program,
                        program,
                        external,
                        resolved_initializer_expr,
                        *io_binding_names,
                        *io_binding_targets,
                        *io_binding_count,
                        io_target_names,
                        io_target_count,
                        error)) {
                    if (!ir_collect_function_value_return_target_names_append_unique(
                            io_target_names,
                            io_target_count,
                            resolved_target)) {
                        return 0;
                    }
                }
            }
        }
    } else if (stmt->kind == AST_STMT_EXPRESSION &&
        stmt->expression_count == 1u &&
        stmt->expressions) {
        const AstExpression *expr = stmt->expressions[0];
        const AstExpression *expr_base = ir_unwrap_paren_expression(expr);
        if (expr_base && expr_base->kind == AST_EXPR_BINARY && expr_base->as.binary.op == TOKEN_ASSIGN) {
            const AstExpression *lhs_base = ir_unwrap_paren_expression(expr_base->as.binary.left);
            const AstExpression *rhs_base = ir_unwrap_paren_expression(expr_base->as.binary.right);
            const char *resolved_target = NULL;
            const char **resolved_target_names = NULL;
            size_t resolved_target_count = 0u;

            if (lhs_base && rhs_base &&
                lhs_base->kind == AST_EXPR_IDENTIFIER && lhs_base->as.identifier.name &&
                program) {
                if (rhs_base->kind == AST_EXPR_IDENTIFIER &&
                    rhs_base->as.identifier.name &&
                    ir_resolve_noncapturing_function_target_name(
                        program,
                        rhs_base->as.identifier.name,
                        *io_binding_names,
                        *io_binding_targets,
                        *io_binding_count,
                        &resolved_target,
                        error)) {
                    if (!ir_collect_function_value_return_target_names_bind_name(
                            io_binding_names,
                            io_binding_targets,
                            io_binding_count,
                            lhs_base->as.identifier.name,
                            resolved_target)) {
                        return 0;
                    }
                } else if (ir_collect_function_value_wrapper_target_names(
                        ast_program,
                        program,
                        expr_base->as.binary.right,
                        *io_binding_names,
                        *io_binding_targets,
                        *io_binding_count,
                        &resolved_target_names,
                        &resolved_target_count,
                        error) &&
                    resolved_target_names &&
                    resolved_target_count > 0u &&
                    resolved_target_names[0] &&
                    resolved_target_names[0][0] != '\0') {
                    if (!ir_collect_function_value_return_target_names_bind_name(
                            io_binding_names,
                            io_binding_targets,
                            io_binding_count,
                            lhs_base->as.identifier.name,
                            resolved_target_names[0])) {
                        free(resolved_target_names);
                        return 0;
                    }
                }
            }
            if (tracked_return_name &&
                strcmp(tracked_return_name, lhs_base->as.identifier.name) == 0) {
                if (!ir_collect_function_value_wrapper_target_names_in_return_context(
                        ast_program,
                        program,
                        external,
                        expr_base->as.binary.right,
                        *io_binding_names,
                        *io_binding_targets,
                        *io_binding_count,
                        io_target_names,
                        io_target_count,
                        error)) {
                    if (!resolved_target ||
                        !ir_collect_function_value_return_target_names_append_unique(
                            io_target_names,
                            io_target_count,
                            resolved_target)) {
                        return 0;
                    }
                }
            }
            free(resolved_target_names);
        }
    } else if (stmt->kind == AST_STMT_RETURN) {
        const AstExpression *return_expr =
            (stmt->expressions && stmt->expression_count > 0u) ? stmt->expressions[0] : NULL;
        if (program &&
            !ir_collect_function_value_wrapper_target_names_in_return_context(
                ast_program,
                program,
                external,
                return_expr,
                *io_binding_names,
                *io_binding_targets,
                *io_binding_count,
                io_target_names,
                io_target_count,
                error)) {
            return 0;
        }
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        if (!ir_collect_function_value_return_targets_in_statement(
                ast_program,
                external,
                stmt->children ? stmt->children[child_index] : NULL,
                program,
                tracked_return_name,
                io_binding_names,
                io_binding_targets,
                io_binding_count,
                io_target_names,
                io_target_count,
                error)) {
            return 0;
        }
    }

    return 1;
}

static int ir_collect_function_value_return_target_names(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    const char ***out_target_names,
    size_t *out_target_count,
    IrError *error) {
    const AstExternal *resolved_external = external;
    const AstExpression *return_expr = NULL;
    const AstExpression *return_base = NULL;
    const char *tracked_return_name = NULL;
    const char **binding_names = NULL;
    const char **binding_targets = NULL;
    size_t binding_count = 0u;
    const char **target_names = NULL;
    size_t target_count = 0u;
    int ok = 0;

    if (out_target_names) {
        *out_target_names = NULL;
    }
    if (out_target_count) {
        *out_target_count = 0u;
    }
    if (!ast_program || !external || !external->has_function_return_signature ||
        !out_target_names || !out_target_count) {
        return 0;
    }

    if (external->function_body == NULL && external->name) {
        resolved_external = ir_find_named_function_external_prefer_definition(ast_program, external->name);
    }
    if (!resolved_external || !resolved_external->function_body) {
        *out_target_names = NULL;
        *out_target_count = 0u;
        return 1;
    }
    return_expr = ir_find_function_returned_value_expr(resolved_external->function_body);
    return_base = ir_unwrap_function_value_wrapper_expression(return_expr);
    if (return_base && return_base->kind == AST_EXPR_IDENTIFIER && return_base->as.identifier.name) {
        tracked_return_name = return_base->as.identifier.name;
        if (ir_function_parameter_name_is_compatible_function_value(
                resolved_external,
                tracked_return_name,
                resolved_external->function_return_function_return_type,
                resolved_external->function_return_function_parameter_count)) {
            *out_target_names = NULL;
            *out_target_count = 0u;
            return 1;
        }
    }
    if (!ir_collect_function_value_return_targets_in_statement(
            ast_program,
            resolved_external,
            resolved_external->function_body,
            program,
            tracked_return_name,
            &binding_names,
            &binding_targets,
            &binding_count,
            &target_names,
            &target_count,
            error)) {
        goto cleanup;
    }

    *out_target_names = target_names;
    *out_target_count = target_count;
    target_names = NULL;
    target_count = 0u;
    ok = 1;

cleanup:
    free(binding_names);
    free(binding_targets);
    free(target_names);
    if (!ok) {
        if (out_target_names) {
            *out_target_names = NULL;
        }
        if (out_target_count) {
            *out_target_count = 0u;
        }
    }
    return ok;
}

static int ir_aggregate_slot_count_for_kind_and_type_name(const AstProgram *ast_program,
    AstDeclarationValueKind aggregate_kind,
    const char *type_name,
    size_t *out_slot_count) {
    const AstStructType *struct_type;
    size_t field_index;
    size_t slot_count = 0u;

    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!out_slot_count) {
        return 0;
    }

    if (aggregate_kind == AST_DECLARATION_VALUE_PAIR) {
        *out_slot_count = 2u;
        return 1;
    }
    if (aggregate_kind != AST_DECLARATION_VALUE_STRUCT) {
        return 0;
    }

    struct_type = ir_find_struct_type(ast_program, type_name);
    if (!struct_type || !struct_type->field_value_kinds) {
        return 0;
    }

    for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
        size_t field_slots = 0u;
        int field_value_kind = struct_type->field_value_kinds[field_index];

        switch (field_value_kind) {
        case AST_DECLARATION_VALUE_INT:
        case AST_DECLARATION_VALUE_FLOAT:
            field_slots = 1u;
            break;
        case AST_DECLARATION_VALUE_PAIR:
        case AST_DECLARATION_VALUE_STRUCT:
            if (!ir_aggregate_slot_count_for_kind_and_type_name(
                    ast_program,
                    (AstDeclarationValueKind)field_value_kind,
                    struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL,
                    &field_slots)) {
                return 0;
            }
            break;
        default:
            return 0;
        }
        slot_count += field_slots;
    }

    *out_slot_count = slot_count;
    return slot_count > 0u;
}

static int ir_parameter_value_kind_as_aggregate_kind(int parameter_value_kind,
    AstDeclarationValueKind *out_kind) {
    if (out_kind) {
        *out_kind = AST_DECLARATION_VALUE_INT;
    }
    if (!out_kind) {
        return 0;
    }

    switch (parameter_value_kind) {
    case AST_PARAMETER_VALUE_PAIR:
        *out_kind = AST_DECLARATION_VALUE_PAIR;
        return 1;
    case AST_PARAMETER_VALUE_STRUCT:
        *out_kind = AST_DECLARATION_VALUE_STRUCT;
        return 1;
    default:
        return 0;
    }
}

static int ir_aggregate_slot_count_for_parameter(const AstProgram *ast_program,
    const AstExternal *external,
    size_t param_index,
    size_t *out_slot_count) {
    AstDeclarationValueKind aggregate_kind;

    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!external || !out_slot_count || param_index >= external->parameter_count) {
        return 0;
    }

    if (!ir_parameter_value_kind_as_aggregate_kind(
            external->parameter_value_kinds
                ? external->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT,
            &aggregate_kind)) {
        return 0;
    }

    return ir_aggregate_slot_count_for_kind_and_type_name(
        ast_program,
        aggregate_kind,
        external->parameter_type_names ? external->parameter_type_names[param_index] : NULL,
        out_slot_count);
}

static int ir_function_source_parameter_slot_count(const AstProgram *ast_program,
    const AstExternal *external,
    size_t param_index,
    size_t *out_slot_count) {
    AstDeclarationValueKind aggregate_kind;

    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!external || !out_slot_count || param_index >= external->parameter_count) {
        return 0;
    }

    if (ir_parameter_value_kind_as_aggregate_kind(
            external->parameter_value_kinds
                ? external->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT,
            &aggregate_kind)) {
        return ir_aggregate_slot_count_for_parameter(ast_program, external, param_index, out_slot_count);
    }

    *out_slot_count = 1u;
    return 1;
}

static int ir_function_lowered_parameter_count(const AstProgram *ast_program,
    const AstExternal *external,
    size_t *out_parameter_count) {
    size_t param_index;
    size_t total = 0u;

    if (out_parameter_count) {
        *out_parameter_count = 0u;
    }
    if (!external || !out_parameter_count) {
        return 0;
    }

    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
        size_t slot_count = 0u;

        if (!ir_function_source_parameter_slot_count(ast_program, external, param_index, &slot_count)) {
            return 0;
        }
        total += slot_count;
    }

    *out_parameter_count = total;
    return 1;
}

static int ir_function_lowered_return_slot_count(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    size_t *out_slot_count) {
    AstDeclarationValueKind aggregate_kind;
    int return_is_closure_family = 0;

    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!external || !out_slot_count) {
        return 0;
    }

    if (external->has_function_return_signature) {
        if (!ir_function_value_return_is_closure_family(
                ast_program,
                external,
                &return_is_closure_family)) {
            return 0;
        }
        if (!return_is_closure_family) {
            *out_slot_count = 1u;
            return 1;
        }
        return ir_function_returned_closure_payload_slot_count(ast_program, program, external, out_slot_count);
    }

    if (!ir_function_return_type_as_aggregate_kind(external, &aggregate_kind)) {
        return 1;
    }

    return ir_aggregate_slot_count_for_kind_and_type_name(
        ast_program,
        aggregate_kind,
        external->function_return_type_name,
        out_slot_count);
}

static int ir_function_total_lowered_parameter_count(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    size_t *out_parameter_count) {
    size_t source_parameter_count = 0u;
    size_t return_slot_count = 0u;

    if (out_parameter_count) {
        *out_parameter_count = 0u;
    }
    if (!external || !out_parameter_count) {
        return 0;
    }

    if (!ir_function_lowered_parameter_count(ast_program, external, &source_parameter_count) ||
        !ir_function_lowered_return_slot_count(ast_program, program, external, &return_slot_count)) {
        return 0;
    }

    *out_parameter_count = source_parameter_count + return_slot_count;
    return 1;
}

static size_t ir_function_specialized_hidden_capture_count_for_parameter(
    const AstExternal *external,
    size_t param_index,
    const size_t *bound_function_capture_counts) {
    if (!external ||
        param_index >= external->parameter_count ||
        !external->parameter_value_kinds ||
        external->parameter_value_kinds[param_index] != AST_PARAMETER_VALUE_FUNCTION ||
        !bound_function_capture_counts) {
        return 0u;
    }

    return bound_function_capture_counts[param_index];
}

static size_t ir_function_specialized_lowered_parameter_count(
    const AstProgram *ast_program,
    const AstExternal *external,
    const char *const *bound_function_targets,
    const size_t *bound_function_capture_counts) {
    size_t param_index;
    size_t total = 0u;
    size_t return_slot_count = 0u;

    if (!external) {
        return 0u;
    }
    if (!ir_function_lowered_return_slot_count(ast_program, NULL, external, &return_slot_count)) {
        return 0u;
    }
    total += return_slot_count;

    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
        size_t slot_count = 0u;

        if (external->parameter_value_kinds &&
            external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
            bound_function_targets &&
            bound_function_targets[param_index] &&
            bound_function_targets[param_index][0] != '\0') {
            total += ir_function_specialized_hidden_capture_count_for_parameter(
                external,
                param_index,
                bound_function_capture_counts);
            continue;
        }

        if (!ir_function_source_parameter_slot_count(ast_program, external, param_index, &slot_count)) {
            return 0u;
        }
        total += slot_count;
    }

    return total;
}

static AstFunctionReturnType ir_lowered_function_return_type(const AstExternal *external) {
    return external &&
            external->has_function_return_signature
        ? AST_FUNCTION_RETURN_VOID
        : external &&
            (external->function_return_type == AST_FUNCTION_RETURN_PAIR ||
                external->function_return_type == AST_FUNCTION_RETURN_STRUCT)
        ? AST_FUNCTION_RETURN_VOID
        : (external ? external->function_return_type : AST_FUNCTION_RETURN_INT);
}

static int ir_function_parameter_count_matches_external(const AstProgram *ast_program,
    IrProgram *program,
    const IrFunction *function,
    const AstExternal *external) {
    size_t lowered_parameter_count = 0u;

    return function &&
        external &&
        ir_function_total_lowered_parameter_count(ast_program, program, external, &lowered_parameter_count) &&
        function->parameter_count == lowered_parameter_count;
}

static int ir_apply_array_rank_to_local(IrFunction *function,
    size_t local_id,
    size_t array_rank,
    IrError *error) {
    IrLocal *local;

    if (!function || local_id >= function->local_count || !function->locals) {
        ir_set_error(error, 0, 0, "IR-INT-068: invalid local array-rank assignment");
        return 0;
    }

    local = &function->locals[local_id];
    if (local->array_rank > 0u && array_rank > 0u && local->array_rank != array_rank) {
        ir_set_error(error,
            0,
            0,
            "IR-LOWER-029: function parameter '%s' is declared with conflicting array ranks",
            local->source_name ? local->source_name : "<unnamed>");
        return 0;
    }

    if (array_rank > 0u) {
        local->array_rank = array_rank;
    }
    return 1;
}

static int ir_apply_array_rank_to_global(IrGlobal *global,
    size_t array_rank,
    IrError *error) {
    if (!global) {
        ir_set_error(error, 0, 0, "IR-INT-069: invalid global array-rank assignment");
        return 0;
    }

    if (global->array_rank > 0u && array_rank > 0u && global->array_rank != array_rank) {
        ir_set_error(error,
            0,
            0,
            "IR-LOWER-030: global '%s' is declared with conflicting array ranks",
            global->name ? global->name : "<unnamed>");
        return 0;
    }

    if (array_rank > 0u) {
        global->array_rank = array_rank;
    }
    return 1;
}

static int ir_ensure_builtin_function_signature(IrProgram *program,
    const char *name,
    IrFunction **out_function,
    IrError *error) {
    size_t param_index;
    const IrBuiltinFunctionInfo *builtin = NULL;
    IrFunction *function = NULL;

    if (out_function) {
        *out_function = NULL;
    }
    if (!program || !name || name[0] == '\0') {
        return 0;
    }

    builtin = ir_find_builtin_function_info(name);
    if (!builtin) {
        return 0;
    }

    function = ir_program_find_function_mut(program, builtin->name);
    if (!function) {
        if (!ir_program_append_function(program, builtin->name, &function, error)) {
            return 0;
        }
    }
    function->return_type = builtin->return_type;

    if (function->local_count == 0u && function->parameter_count == 0u &&
        builtin->parameter_count > 0u) {
        for (param_index = 0u; param_index < builtin->parameter_count; ++param_index) {
            char fallback_name[32];

            snprintf(fallback_name, sizeof(fallback_name), "param%zu", param_index);
            if (!ir_function_append_local(function, fallback_name, 1, NULL, error)) {
                return 0;
            }
            function->locals[param_index].value_type =
                (builtin->parameter_types && param_index < builtin->parameter_count)
                    ? builtin->parameter_types[param_index]
                    : AST_FUNCTION_RETURN_INT;
        }
    }

    if (function->parameter_count != builtin->parameter_count) {
        ir_set_error(error,
            0,
            0,
            "IR-LOWER-028: builtin function '%s' declaration arity mismatch during IR lowering",
            builtin->name);
        return 0;
    }

    if (out_function) {
        *out_function = function;
    }
    return 1;
}

static int ir_predeclare_builtin_calls_in_expression(IrProgram *program,
    const AstExpression *expr,
    IrError *error) {
    size_t arg_index;
    const char *called_name = NULL;

    if (!program || !expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_INIT_LIST:
        for (arg_index = 0u; arg_index < expr->as.init_list.item_count; ++arg_index) {
            if (!ir_predeclare_builtin_calls_in_expression(program, expr->as.init_list.items[arg_index], error)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return ir_predeclare_builtin_calls_in_expression(program, expr->as.inner, error);
    case AST_EXPR_CONVERSION:
        if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_INT) {
            if (!ir_ensure_builtin_function_signature(program, "__builtin_f2i32", NULL, error)) {
                return 0;
            }
        } else if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT) {
            if (!ir_ensure_builtin_function_signature(program, "__builtin_i2f32", NULL, error)) {
                return 0;
            }
        }
        return ir_predeclare_builtin_calls_in_expression(program, expr->as.conversion.operand, error);
    case AST_EXPR_UNARY:
        return ir_predeclare_builtin_calls_in_expression(program, expr->as.unary.operand, error);
    case AST_EXPR_POSTFIX:
        return ir_predeclare_builtin_calls_in_expression(program, expr->as.postfix.operand, error);
    case AST_EXPR_CLOSURE:
        return ir_predeclare_builtin_calls_in_statement(program, expr->as.closure.body, error);
    case AST_EXPR_BINARY:
        return ir_predeclare_builtin_calls_in_expression(program, expr->as.binary.left, error) &&
            ir_predeclare_builtin_calls_in_expression(program, expr->as.binary.right, error);
    case AST_EXPR_TERNARY:
        return ir_predeclare_builtin_calls_in_expression(program, expr->as.ternary.condition, error) &&
            ir_predeclare_builtin_calls_in_expression(program, expr->as.ternary.then_expr, error) &&
            ir_predeclare_builtin_calls_in_expression(program, expr->as.ternary.else_expr, error);
    case AST_EXPR_CALL:
        called_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
        if (called_name && ir_find_builtin_function_info(called_name) &&
            !ir_ensure_builtin_function_signature(program, called_name, NULL, error)) {
            return 0;
        }
        if (!ir_predeclare_builtin_calls_in_expression(program, expr->as.call.callee, error)) {
            return 0;
        }
        for (arg_index = 0u; arg_index < expr->as.call.arg_count; ++arg_index) {
            if (!ir_predeclare_builtin_calls_in_expression(program, expr->as.call.args[arg_index], error)) {
                return 0;
            }
        }
        return 1;
    default:
        return 1;
    }
}

static int ir_predeclare_builtin_calls_in_statement(IrProgram *program,
    const AstStatement *stmt,
    IrError *error) {
    size_t i;

    if (!program || !stmt) {
        return 1;
    }

    for (i = 0u; i < stmt->expression_count; ++i) {
        if (!ir_predeclare_builtin_calls_in_expression(program, stmt->expressions[i], error)) {
            return 0;
        }
    }
    for (i = 0u; i < stmt->child_count; ++i) {
        if (!ir_predeclare_builtin_calls_in_statement(program, stmt->children[i], error)) {
            return 0;
        }
    }
    return 1;
}

static int ir_predeclare_used_builtin_functions(IrProgram *program,
    const AstProgram *ast_program,
    IrError *error) {
    size_t external_index;

    if (!program || !ast_program) {
        return 1;
    }

    for (external_index = 0u; external_index < ast_program->count; ++external_index) {
        const AstExternal *external = &ast_program->externals[external_index];

        if (external->declaration_initializer &&
            !ir_predeclare_builtin_calls_in_expression(program, external->declaration_initializer, error)) {
            return 0;
        }
        if (external->function_body &&
            !ir_predeclare_builtin_calls_in_statement(program, external->function_body, error)) {
            return 0;
        }
    }

    return 1;
}

static int ir_function_set_parameter_name(IrFunction *function,
    size_t parameter_index,
    const char *parameter_name,
    IrError *error) {
    IrLocal *local;
    char *name_copy;
    size_t name_length;

    if (!function || !parameter_name || parameter_name[0] == '\0') {
        return 1;
    }
    if (parameter_index >= function->parameter_count || parameter_index >= function->local_count) {
        ir_set_error(error, 0, 0, "IR-INT-030: invalid parameter rename target");
        return 0;
    }

    local = &function->locals[parameter_index];
    if (local->source_name && strcmp(local->source_name, parameter_name) == 0) {
        return 1;
    }

    name_length = strlen(parameter_name);
    name_copy = (char *)malloc(name_length + 1);
    if (!name_copy) {
        ir_set_error(error,
            0,
            0,
            "IR-INT-031: out of memory while renaming function parameter");
        return 0;
    }

    memcpy(name_copy, parameter_name, name_length + 1);
    free(local->source_name);
    local->source_name = name_copy;
    return 1;
}

static int ir_predeclare_program_globals(IrProgram *program,
    const AstProgram *ast_program,
    IrError *error) {
    size_t i;

    if (!program || !ast_program) {
        return 1;
    }

    for (i = 0; i < ast_program->count; ++i) {
        const AstExternal *external = &ast_program->externals[i];
        IrGlobal *global;

        if (external->kind != AST_EXTERNAL_DECLARATION || !external->name ||
            external->name[0] == '\0') {
            continue;
        }

        global = ir_program_find_global_mut(program, external->name);
        if (!global && !ir_program_append_global(program, external->name, &global, error)) {
            return 0;
        }
        if (global) {
            global->value_type =
                ir_scalar_declaration_value_kind_to_return_type(external->declaration_value_kind);
        }
        if (global &&
            !ir_apply_array_rank_to_global(global, external->declaration_array_rank, error)) {
            return 0;
        }
    }

    return 1;
}

static int ir_eval_global_binary(TokenType op,
    long long lhs,
    long long rhs,
    long long *out_value,
    int line,
    int column,
    IrError *error,
    int *out_invalid_constant) {
    int32_t lhs32;
    int32_t rhs32;

    if (!out_value) {
        return 0;
    }

    lhs32 = (int32_t)ir_normalize_sysy_int_value(lhs);
    rhs32 = (int32_t)ir_normalize_sysy_int_value(rhs);

    switch (op) {
    case TOKEN_PLUS:
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 + (long long)rhs32);
        return 1;
    case TOKEN_MINUS:
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 - (long long)rhs32);
        return 1;
    case TOKEN_STAR:
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 * (long long)rhs32);
        return 1;
    case TOKEN_SLASH:
        if (rhs32 == 0) {
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-018: top-level initializer division by zero is not allowed");
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 / (long long)rhs32);
        return 1;
    case TOKEN_PERCENT:
        if (rhs32 == 0) {
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-018: top-level initializer modulo by zero is not allowed");
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 % (long long)rhs32);
        return 1;
    case TOKEN_SHIFT_LEFT:
        if (rhs32 < 0 || rhs32 >= 32) {
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-019: top-level initializer shift count %d is out of range",
                rhs32);
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)((uint32_t)lhs32 << (uint32_t)rhs32));
        return 1;
    case TOKEN_SHIFT_RIGHT:
        if (rhs32 < 0 || rhs32 >= 32) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            ir_set_error(error,
                line,
                column,
                "IR-LOWER-019: top-level initializer shift count %d is out of range",
                rhs32);
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 >> rhs32));
        return 1;
    case TOKEN_AMP:
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 & rhs32));
        return 1;
    case TOKEN_CARET:
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 ^ rhs32));
        return 1;
    case TOKEN_PIPE:
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 | rhs32));
        return 1;
    case TOKEN_EQ:
        *out_value = (lhs32 == rhs32) ? 1 : 0;
        return 1;
    case TOKEN_NE:
        *out_value = (lhs32 != rhs32) ? 1 : 0;
        return 1;
    case TOKEN_LT:
        *out_value = (lhs32 < rhs32) ? 1 : 0;
        return 1;
    case TOKEN_LE:
        *out_value = (lhs32 <= rhs32) ? 1 : 0;
        return 1;
    case TOKEN_GT:
        *out_value = (lhs32 > rhs32) ? 1 : 0;
        return 1;
    case TOKEN_GE:
        *out_value = (lhs32 >= rhs32) ? 1 : 0;
        return 1;
    case TOKEN_AND_AND:
        *out_value = (lhs32 != 0 && rhs32 != 0) ? 1 : 0;
        return 1;
    case TOKEN_OR_OR:
        *out_value = (lhs32 != 0 || rhs32 != 0) ? 1 : 0;
        return 1;
    default:
        ir_set_error(error,
            line,
            column,
            "IR-LOWER-018: top-level initializer operator '%s' is not supported",
            lexer_token_type_name(op));
        return 0;
    }
}

static int ir_global_initializer_number_fits_fold_range(long long value) {
    return value >= INT32_MIN && value <= (long long)UINT32_MAX;
}

static int ir_eval_global_initializer_expr(const IrProgram *program,
    const AstExpression *expr,
    long long *out_value,
    IrError *error,
    int *out_invalid_constant) {
    long long lhs;
    long long rhs;

    if (!expr || !out_value) {
        return 0;
    }
    if (out_invalid_constant) {
        *out_invalid_constant = 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        if (!ir_global_initializer_number_fits_fold_range(expr->as.number_value)) {
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value(expr->as.number_value);
        return 1;
    case AST_EXPR_FLOAT_LITERAL:
        if (out_invalid_constant) {
            *out_invalid_constant = 1;
        }
        return 0;
    case AST_EXPR_INIT_LIST:
        if (out_invalid_constant) {
            *out_invalid_constant = 1;
        }
        return 0;
    case AST_EXPR_IDENTIFIER:
        {
            const IrGlobal *global;

            if (!program || !expr->as.identifier.name || expr->as.identifier.name[0] == '\0') {
                if (out_invalid_constant) {
                    *out_invalid_constant = 1;
                }
                return 0;
            }

            global = ir_program_find_global(program, expr->as.identifier.name);
            if (!global || !global->has_initializer) {
                if (out_invalid_constant) {
                    *out_invalid_constant = 1;
                }
                return 0;
            }

            *out_value = global->initializer_value;
            return 1;
        }
    case AST_EXPR_PAREN:
        return ir_eval_global_initializer_expr(program,
            expr->as.inner,
            out_value,
            error,
            out_invalid_constant);
    case AST_EXPR_UNARY:
        if (!ir_eval_global_initializer_expr(program,
                expr->as.unary.operand,
                &rhs,
                error,
                out_invalid_constant)) {
            return 0;
        }
        switch (expr->as.unary.op) {
        case TOKEN_PLUS:
            *out_value = ir_normalize_sysy_int_value(rhs);
            return 1;
        case TOKEN_MINUS:
            *out_value = ir_normalize_sysy_int_value(-rhs);
            return 1;
        case TOKEN_BANG:
            *out_value = rhs == 0 ? 1 : 0;
            return 1;
        case TOKEN_TILDE:
            *out_value = ir_normalize_sysy_int_value(~rhs);
            return 1;
        default:
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        }
    case AST_EXPR_TERNARY:
        if (!ir_eval_global_initializer_expr(program,
                expr->as.ternary.condition,
                &lhs,
                error,
                out_invalid_constant)) {
            return 0;
        }
        return ir_eval_global_initializer_expr(program,
            lhs != 0 ? expr->as.ternary.then_expr : expr->as.ternary.else_expr,
            out_value,
            error,
            out_invalid_constant);
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_COMMA) {
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.left,
                    &lhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            return ir_eval_global_initializer_expr(program,
                expr->as.binary.right,
                out_value,
                error,
                out_invalid_constant);
        }

        if (expr->as.binary.op == TOKEN_AND_AND) {
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.left,
                    &lhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            if (lhs == 0) {
                *out_value = 0;
                return 1;
            }
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.right,
                    &rhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            *out_value = rhs != 0 ? 1 : 0;
            return 1;
        }

        if (expr->as.binary.op == TOKEN_OR_OR) {
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.left,
                    &lhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            if (lhs != 0) {
                *out_value = 1;
                return 1;
            }
            if (!ir_eval_global_initializer_expr(program,
                    expr->as.binary.right,
                    &rhs,
                    error,
                    out_invalid_constant)) {
                return 0;
            }
            *out_value = rhs != 0 ? 1 : 0;
            return 1;
        }

        switch (expr->as.binary.op) {
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_STAR_ASSIGN:
        case TOKEN_SLASH_ASSIGN:
        case TOKEN_PERCENT_ASSIGN:
        case TOKEN_AMP_ASSIGN:
        case TOKEN_CARET_ASSIGN:
        case TOKEN_PIPE_ASSIGN:
        case TOKEN_SHIFT_LEFT_ASSIGN:
        case TOKEN_SHIFT_RIGHT_ASSIGN:
            if (out_invalid_constant) {
                *out_invalid_constant = 1;
            }
            return 0;
        default:
            break;
        }

        if (!ir_eval_global_initializer_expr(program,
                expr->as.binary.left,
                &lhs,
                error,
                out_invalid_constant) ||
            !ir_eval_global_initializer_expr(program,
                expr->as.binary.right,
                &rhs,
                error,
                out_invalid_constant)) {
            return 0;
        }
        return ir_eval_global_binary(expr->as.binary.op,
            lhs,
            rhs,
            out_value,
            expr->line,
            expr->column,
            error,
            out_invalid_constant);
    case AST_EXPR_POSTFIX:
    case AST_EXPR_CALL:
    default:
        if (out_invalid_constant) {
            *out_invalid_constant = 1;
        }
        return 0;
    }
}

static int ir_ensure_function_signature(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    IrFunction **out_function,
    IrError *error) {
    IrFunction *function;
    size_t param_index;
    size_t physical_param_index = 0u;

    if (!program || !external || !external->name || external->name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-058: invalid function signature lowering contract");
        return 0;
    }
    function = ir_program_find_function_mut(program, external->name);
    if (!function) {
        if (!ir_program_append_function(program, external->name, &function, error)) {
            return 0;
        }
    }
    function->return_type = ir_lowered_function_return_type(external);

    if (function->local_count == 0 && function->parameter_count == 0 &&
        (external->parameter_count > 0 ||
            external->has_function_return_signature ||
            external->function_return_type == AST_FUNCTION_RETURN_PAIR ||
            external->function_return_type == AST_FUNCTION_RETURN_STRUCT)) {
        size_t return_slot_count = 0u;
        size_t return_slot_index;

        if (!ir_function_lowered_return_slot_count(ast_program, program, external, &return_slot_count)) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-LOWER-020: function '%s' return lowering shape is unsupported",
                external->name);
            return 0;
        }
        function = ir_program_find_function_mut(program, external->name);
        if (!function) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-058: function '%s' disappeared during signature lowering",
                external->name);
            return 0;
        }
        for (return_slot_index = 0u; return_slot_index < return_slot_count; ++return_slot_index) {
            char hidden_name[32];

            snprintf(hidden_name, sizeof(hidden_name), "__ret%zu", return_slot_index);
            if (!ir_function_append_local(function, hidden_name, 1, NULL, error)) {
                return 0;
            }
            function->locals[return_slot_index].value_type =
                external->function_return_type == AST_FUNCTION_RETURN_FLOAT
                    ? AST_FUNCTION_RETURN_FLOAT
                    : AST_FUNCTION_RETURN_INT;
        }

        for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
            const char *parameter_name =
                (external->parameter_names && external->parameter_names[param_index] &&
                    external->parameter_names[param_index][0] != '\0')
                ? external->parameter_names[param_index]
                : NULL;
            size_t slot_count = 1u;
            size_t slot_index;
            char fallback_name[32];

            if (!parameter_name) {
                snprintf(fallback_name, sizeof(fallback_name), "param%zu", param_index);
                parameter_name = fallback_name;
            }

            if (!ir_function_source_parameter_slot_count(ast_program, external, param_index, &slot_count)) {
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-LOWER-020: function '%s' parameter lowering shape is unsupported",
                    external->name);
                return 0;
            }

            for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
                if (!ir_function_append_local(function, parameter_name, 1, NULL, error)) {
                    return 0;
                }
                function->locals[return_slot_count + physical_param_index].value_type =
                    ir_scalar_parameter_value_kind_to_return_type(
                external->parameter_value_kinds
                    ? external->parameter_value_kinds[param_index]
                    : AST_PARAMETER_VALUE_INT);
                physical_param_index++;
            }
        }
    }

    if (!ir_function_parameter_count_matches_external(ast_program, program, function, external)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-LOWER-020: function '%s' declaration arity mismatch during IR lowering",
            external->name);
        return 0;
    }

    {
        size_t return_slot_count = 0u;

        if (!ir_function_lowered_return_slot_count(ast_program, program, external, &return_slot_count)) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-LOWER-020: function '%s' return lowering shape is unsupported",
                external->name);
            return 0;
        }
        physical_param_index = return_slot_count;
    }
    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
        const char *parameter_name = external->parameter_names ? external->parameter_names[param_index] : NULL;
        size_t slot_count = 1u;
        size_t slot_index;

        if (!ir_function_source_parameter_slot_count(ast_program, external, param_index, &slot_count)) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-LOWER-020: function '%s' parameter lowering shape is unsupported",
                external->name);
            return 0;
        }

        for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
            if (!ir_function_set_parameter_name(function, physical_param_index, parameter_name, error)) {
                return 0;
            }
            if (function->locals && physical_param_index < function->local_count) {
                function->locals[physical_param_index].value_type = ir_scalar_parameter_value_kind_to_return_type(
                    external->parameter_value_kinds
                        ? external->parameter_value_kinds[param_index]
                        : AST_PARAMETER_VALUE_INT);
            }
            if (slot_index == 0u && external->parameter_array_ranks &&
                !ir_apply_array_rank_to_local(function,
                    physical_param_index,
                    external->parameter_array_ranks[param_index],
                    error)) {
                return 0;
            }
            physical_param_index++;
        }
    }

    if (out_function) {
        *out_function = function;
    }
    return 1;
}

static int ir_ensure_runtime_global_init_function(IrProgram *program,
    IrFunction **out_function,
    IrError *error) {
    IrFunction *function;

    if (!program) {
        return 0;
    }

    function = ir_program_find_function_mut(program, IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
    if (!function) {
        if (!ir_program_append_function(program,
                IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME,
                &function,
                error)) {
            return 0;
        }
    }

    if (function->parameter_count != 0 || function->local_count != 0) {
        ir_set_error(error,
            0,
            0,
            "IR-LOWER-024: reserved internal helper '%s' cannot be user-defined with parameters/locals",
            IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
        return 0;
    }

    if (!function->has_body) {
        if (!ir_function_append_block(function, NULL, NULL, error)) {
            return 0;
        }
        function->has_body = 1;
    } else if (!function->blocks || function->block_count == 0u) {
        ir_set_error(error, 0, 0, "IR-INT-059: runtime initializer helper has no entry block");
        return 0;
    }

    if (out_function) {
        *out_function = function;
    }
    return 1;
}

static int ir_emit_runtime_initializer_assignment(IrProgram *program,
    IrFunction *runtime_init_function,
    IrGlobal *global,
    const AstExpression *initializer_expr,
    IrError *error) {
    size_t append_block_id = 0u;

    if (global && global->array_rank > 0u) {
        IrLowerContext ctx;
        IrValueRef base_address;
        const AstExpression **initializer_slots = NULL;
        size_t slot_count = 0u;
        size_t slot_index;

        if (!program || !runtime_init_function || !global || !initializer_expr || !global->array_extents) {
            return 0;
        }
        if (!ir_function_find_last_unterminated_block(runtime_init_function, &append_block_id)) {
            ir_set_error(error,
                initializer_expr->line,
                initializer_expr->column,
                "IR-INT-060: runtime initializer helper has no appendable block");
            return 0;
        }

        memset(&ctx, 0, sizeof(ctx));
        ctx.program = program;
        ctx.function = runtime_init_function;
        ctx.block_id = append_block_id;
        ctx.has_block = 1;
        ctx.error = error;

        if (!ir_expand_array_initializer_slots(initializer_expr,
                global->array_rank,
                global->array_extents,
                &initializer_slots,
                &slot_count,
                error)) {
            return 0;
        }

        base_address = ir_value_temp(ir_allocate_temp(runtime_init_function));
        if (!ir_emit_addr_global(&ctx, base_address, global->id)) {
            free(initializer_slots);
            return 0;
        }

        for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
            IrValueRef slot_value;
            IrValueRef slot_address = base_address;

            if (!initializer_slots[slot_index]) {
                continue;
            }

            if (slot_index > 0u) {
                slot_address = ir_value_temp(ir_allocate_temp(runtime_init_function));
                if (!ir_emit_binary(&ctx,
                        slot_address,
                        IR_BINARY_ADD,
                        base_address,
                        ir_value_immediate((long long)(slot_index * 4u)))) {
                    free(initializer_slots);
                    return 0;
                }
            }

            if (!ir_lower_expression(&ctx, initializer_slots[slot_index], &slot_value) ||
                !ir_emit_store_indirect(&ctx, slot_address, slot_value)) {
                free(initializer_slots);
                return 0;
            }
        }

        free(initializer_slots);
        return 1;
    }

    IrLowerContext ctx;
    IrValueRef initializer_value;

    if (!program || !runtime_init_function || !global || !initializer_expr) {
        return 0;
    }
    if (!ir_function_find_last_unterminated_block(runtime_init_function, &append_block_id)) {
        ir_set_error(error,
            initializer_expr->line,
            initializer_expr->column,
            "IR-INT-060: runtime initializer helper has no appendable block");
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.program = program;
    ctx.function = runtime_init_function;
    ctx.block_id = append_block_id;
    ctx.has_block = 1;
    ctx.error = error;

    if (!ir_lower_expression(&ctx, initializer_expr, &initializer_value)) {
        return 0;
    }

    return ir_emit_mov(&ctx, ir_value_global(global->id), initializer_value);
}

static int ir_prepend_startup_runtime_init_call(IrFunction *function,
    IrError *error) {
    IrInstruction call_instruction;
    size_t name_length;

    if (!function || function->block_count == 0 || !function->blocks) {
        return 0;
    }

    memset(&call_instruction, 0, sizeof(call_instruction));
    call_instruction.kind = IR_INSTR_CALL;
    call_instruction.result = ir_value_temp(ir_allocate_temp(function));
    call_instruction.as.call.arg_count = 0;
    call_instruction.as.call.args = NULL;

    name_length = strlen(IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);
    call_instruction.as.call.callee_name = (char *)malloc(name_length + 1);
    if (!call_instruction.as.call.callee_name) {
        ir_set_error(error,
            0,
            0,
            "IR-INT-061: out of memory while preparing runtime startup call");
        return 0;
    }
    memcpy(call_instruction.as.call.callee_name,
        IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME,
        name_length + 1);

    if (!ir_block_prepend_instruction(&function->blocks[0], &call_instruction, error)) {
        free(call_instruction.as.call.callee_name);
        return 0;
    }

    return 1;
}

static int ir_function_find_last_unterminated_block(const IrFunction *function,
    size_t *out_block_id) {
    size_t block_index;

    if (out_block_id) {
        *out_block_id = 0u;
    }
    if (!function || !function->blocks || function->block_count == 0u || !out_block_id) {
        return 0;
    }

    for (block_index = function->block_count; block_index > 0u; --block_index) {
        if (!function->blocks[block_index - 1u].has_terminator) {
            *out_block_id = block_index - 1u;
            return 1;
        }
    }

    return 0;
}

static int ir_finalize_runtime_init_function_returns_zero(IrFunction *function,
    IrError *error) {
    size_t block_index;

    if (!function || !function->has_body || function->block_count == 0u || !function->blocks) {
        return 0;
    }

    for (block_index = 0u; block_index < function->block_count; ++block_index) {
        IrBasicBlock *block = &function->blocks[block_index];

        if (block->has_terminator) {
            continue;
        }
        if (!ir_block_set_return(block, ir_value_immediate(0), error)) {
            return 0;
        }
    }

    return 1;
}

static int ir_lower_global_declaration(IrProgram *program,
    const AstProgram *ast_program,
    const AstExternal *external,
    IrFunction **runtime_init_function,
    IrError *error) {
    IrGlobal *global;
    int invalid_constant = 0;
    long long initializer_value = 0;
    IrLowerContext extent_ctx;
    size_t *array_extents = NULL;
    size_t array_element_count = 0u;

    if (!program || !external || external->kind != AST_EXTERNAL_DECLARATION) {
        return 1;
    }
    if (!external->name || external->name[0] == '\0') {
        return 1;
    }

    global = ir_program_find_global_mut(program, external->name);
    if (!global && !ir_program_append_global(program, external->name, &global, error)) {
        return 0;
    }
    if (!global) {
        return 0;
    }
    if (!ir_apply_array_rank_to_global(global, external->declaration_array_rank, error)) {
        return 0;
    }

    if (external->declaration_array_rank > 0u) {
        memset(&extent_ctx, 0, sizeof(extent_ctx));
        extent_ctx.program = program;
        extent_ctx.error = error;
        if (!ir_compute_array_extents(&extent_ctx,
                external->declaration_array_extent_exprs,
                external->declaration_array_rank,
                0,
                &array_extents,
                &array_element_count)) {
            return 0;
        }
        free(global->array_extents);
        global->array_extents = array_extents;
        global->byte_size = array_element_count * 4u;
        global->has_initializer = 0;
        global->initializer_value = 0;
        global->has_runtime_initializer = 0;
        if (!external->has_initializer) {
            return 1;
        }
        if (!external->declaration_initializer) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-062: top-level declaration '%s' is marked initialized but has no initializer AST",
                external->name);
            return 0;
        }

        if (error) {
            error->line = 0;
            error->column = 0;
            error->message[0] = '\0';
        }

        {
            size_t *deps = NULL;
            size_t dep_count = 0;
            size_t dep_capacity = 0;
            size_t i;

            if (!ir_collect_global_initializer_dependencies(program,
                    ast_program,
                    external->declaration_initializer,
                    &deps,
                    &dep_count,
                    &dep_capacity,
                    error)) {
                free(deps);
                return 0;
            }

            for (i = 0; i < dep_count; ++i) {
                size_t dep_id = deps[i];
                const IrGlobal *dep_global = ir_program_get_global(program, dep_id);

                if (!dep_global) {
                    continue;
                }
                if (!dep_global->has_initializer && !dep_global->has_runtime_initializer) {
                    char dep_path[256];
                    char cycle_chain[256];
                    int via_callee = 0;
                    const char *source_name = (global->name && global->name[0] != '\0')
                        ? global->name
                        : "<global>";
                    const char *blocked_name = (dep_global->name && dep_global->name[0] != '\0')
                        ? dep_global->name
                        : "<global>";

                    dep_path[0] = '\0';
                    cycle_chain[0] = '\0';

                    if (!ir_try_format_runtime_global_dependency_path(program,
                            ast_program,
                            global->id,
                            external->declaration_initializer,
                            dep_id,
                            dep_path,
                            sizeof(dep_path),
                            &via_callee)) {
                        snprintf(dep_path, sizeof(dep_path), "%s -> %s", source_name, blocked_name);
                    }

                    if (ir_try_format_runtime_global_cycle(program,
                            ast_program,
                            global->id,
                            dep_id,
                            cycle_chain,
                            sizeof(cycle_chain))) {
                        ir_set_error(error,
                            external->line,
                            external->column,
                            "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency cycle: %s)",
                            source_name,
                            blocked_name,
                            cycle_chain);
                    } else if (via_callee) {
                        ir_set_error(error,
                            external->line,
                            external->column,
                            "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency path via callee body: %s)",
                            source_name,
                            blocked_name,
                            dep_path);
                    } else {
                        ir_set_error(error,
                            external->line,
                            external->column,
                            "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency path: %s)",
                            source_name,
                            blocked_name,
                            dep_path);
                    }

                    free(deps);
                    return 0;
                }
            }

            free(deps);
        }

        if (!ir_ensure_runtime_global_init_function(program, runtime_init_function, error)) {
            return 0;
        }
        if (!*runtime_init_function ||
            !ir_emit_runtime_initializer_assignment(program,
                *runtime_init_function,
                global,
                external->declaration_initializer,
                error)) {
            return 0;
        }

        global->has_initializer = 0;
        global->has_runtime_initializer = 1;
        return 1;
    }

    if (!external->has_initializer) {
        return 1;
    }
    if (!external->declaration_initializer) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-062: top-level declaration '%s' is marked initialized but has no initializer AST",
            external->name);
        return 0;
    }

    if (ir_eval_global_initializer_expr(program,
            external->declaration_initializer,
            &initializer_value,
            error,
            &invalid_constant)) {
        global->has_initializer = 1;
        global->initializer_value = initializer_value;
        global->has_runtime_initializer = 0;
        return 1;
    }

    if (!invalid_constant) {
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    {
        size_t *deps = NULL;
        size_t dep_count = 0;
        size_t dep_capacity = 0;
        size_t i;

        if (!ir_collect_global_initializer_dependencies(program,
                ast_program,
                external->declaration_initializer,
                &deps,
                &dep_count,
                &dep_capacity,
                error)) {
            free(deps);
            return 0;
        }

        for (i = 0; i < dep_count; ++i) {
            size_t dep_id = deps[i];
            const IrGlobal *dep_global = ir_program_get_global(program, dep_id);

            if (!dep_global) {
                continue;
            }
            if (!dep_global->has_initializer && !dep_global->has_runtime_initializer) {
                char dep_path[256];
                char cycle_chain[256];
                int via_callee = 0;
                const char *source_name = (global->name && global->name[0] != '\0')
                    ? global->name
                    : "<global>";
                const char *blocked_name = (dep_global->name && dep_global->name[0] != '\0')
                    ? dep_global->name
                    : "<global>";

                dep_path[0] = '\0';
                cycle_chain[0] = '\0';

                if (!ir_try_format_runtime_global_dependency_path(program,
                        ast_program,
                        global->id,
                        external->declaration_initializer,
                        dep_id,
                        dep_path,
                        sizeof(dep_path),
                        &via_callee)) {
                    snprintf(dep_path, sizeof(dep_path), "%s -> %s", source_name, blocked_name);
                }

                if (ir_try_format_runtime_global_cycle(program,
                        ast_program,
                        global->id,
                        dep_id,
                        cycle_chain,
                        sizeof(cycle_chain))) {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency cycle: %s)",
                        source_name,
                        blocked_name,
                        cycle_chain);
                } else if (via_callee) {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency path via callee body: %s)",
                        source_name,
                        blocked_name,
                        dep_path);
                } else {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-LOWER-022: top-level initializer for global '%s' depends on not-yet-initialized global '%s' (dependency path: %s)",
                        source_name,
                        blocked_name,
                        dep_path);
                }

                free(deps);
                return 0;
            }
        }

        free(deps);
    }

    if (!ir_ensure_runtime_global_init_function(program, runtime_init_function, error)) {
        return 0;
    }
    if (!*runtime_init_function ||
        !ir_emit_runtime_initializer_assignment(program,
            *runtime_init_function,
            global,
            external->declaration_initializer,
            error)) {
        return 0;
    }

    global->has_initializer = 0;
    global->has_runtime_initializer = 1;
    return 1;
}

static int ir_lower_function(const AstProgram *ast_program,
    IrProgram *program,
    const IrLowerOptions *options,
    const AstExternal *external,
    IrError *error) {
    if (ir_external_body_needs_function_value_specialization(ast_program, external, external->function_body)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-LOWER-027: function '%s' needs function-value callee specialization before direct IR lowering",
            external->name);
        return 0;
    }
    return ir_lower_function_core(ast_program, program, NULL, options, external, NULL, NULL, NULL, error);
}

static int ir_external_body_has_function_parameter_callee_calls(const AstExternal *external,
    const AstStatement *stmt) {
    size_t child_index;
    size_t expr_index;
    size_t param_index;
    const char *called_name = NULL;

    if (!external || !stmt) {
        return 0;
    }

    for (expr_index = 0u; expr_index < stmt->expression_count; ++expr_index) {
        const AstExpression *expr = stmt->expressions ? stmt->expressions[expr_index] : NULL;

        if (!expr || expr->kind != AST_EXPR_CALL) {
            continue;
        }
        called_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
        if ((!called_name || called_name[0] == '\0') &&
            !ir_classify_direct_identifier_expr(expr->as.call.callee, &called_name)) {
            continue;
        }
        if (called_name &&
            external->parameter_value_kinds &&
            external->parameter_names) {
            size_t param_index;
            for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
                if (external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
                    external->parameter_names[param_index] &&
                    strcmp(external->parameter_names[param_index], called_name) == 0) {
                    return 1;
                }
            }
        }
        if (called_name &&
            external->function_body &&
            external->function_body->kind == AST_STMT_COMPOUND &&
            external->function_body->children) {
            size_t body_child_index;

            for (body_child_index = 0u;
                    body_child_index < external->function_body->child_count;
                    ++body_child_index) {
                const AstStatement *child =
                    external->function_body->children[body_child_index];
                size_t decl_index;

                if (!child || child->kind != AST_STMT_DECLARATION ||
                    !child->declaration_names || !child->declaration_value_kinds ||
                    !child->expressions) {
                    continue;
                }
                for (decl_index = 0u;
                        decl_index < child->declaration_name_count &&
                        decl_index < child->expression_count;
                        ++decl_index) {
                    const AstExpression *init_expr = NULL;
                    const AstExpression *init_base = NULL;

                    if (!child->declaration_names[decl_index] ||
                        strcmp(child->declaration_names[decl_index], called_name) != 0 ||
                        child->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_FUNCTION) {
                        continue;
                    }
                    init_expr = child->expressions[decl_index];
                    init_base = ir_unwrap_function_value_wrapper_expression(init_expr);
                    if (!init_base ||
                        init_base->kind != AST_EXPR_IDENTIFIER ||
                        !init_base->as.identifier.name) {
                        continue;
                    }
                    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
                        if (external->parameter_value_kinds &&
                            external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
                            external->parameter_names[param_index] &&
                            strcmp(external->parameter_names[param_index], init_base->as.identifier.name) == 0) {
                            return 1;
                        }
                    }
                }
            }
        }
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        if (ir_external_body_has_function_parameter_callee_calls(
                external,
                stmt->children ? stmt->children[child_index] : NULL)) {
            return 1;
        }
    }

    return 0;
}

static int ir_function_parameter_name_is_compatible_function_value(const AstExternal *current_external,
    const char *name,
    AstFunctionReturnType expected_return_type,
    size_t expected_parameter_count) {
    size_t param_index;

    if (!current_external || !name || name[0] == '\0' ||
        !current_external->parameter_names ||
        !current_external->parameter_value_kinds) {
        return 0;
    }

    for (param_index = 0u; param_index < current_external->parameter_count; ++param_index) {
        if (current_external->parameter_value_kinds[param_index] != AST_PARAMETER_VALUE_FUNCTION ||
            !current_external->parameter_names[param_index] ||
            strcmp(current_external->parameter_names[param_index], name) != 0) {
            continue;
        }

        return (!current_external->parameter_function_return_types ||
                   current_external->parameter_function_return_types[param_index] == expected_return_type) &&
            (!current_external->parameter_function_parameter_counts ||
                current_external->parameter_function_parameter_counts[param_index] == expected_parameter_count);
    }

    return 0;
}

static int ir_expression_contains_function_parameter_callee_call(
    const AstExternal *external,
    const AstExpression *expr) {
    const AstExpression *base = NULL;
    size_t i;

    if (!external || !expr) {
        return 0;
    }

    base = ir_unwrap_paren_expression(expr);
    if (!base) {
        return 0;
    }

    if (base->kind == AST_EXPR_CALL) {
        const char *called_name = ir_extract_direct_call_callee_name(base->as.call.callee);
        size_t param_index;

        if ((!called_name || called_name[0] == '\0') &&
            !ir_classify_direct_identifier_expr(base->as.call.callee, &called_name)) {
            called_name = NULL;
        }
        if (called_name &&
            external->parameter_value_kinds &&
            external->parameter_names) {
            for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
                if (external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
                    external->parameter_names[param_index] &&
                    strcmp(external->parameter_names[param_index], called_name) == 0) {
                    return 1;
                }
            }
        }
        if (ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.call.callee)) {
            return 1;
        }
        for (i = 0u; i < base->as.call.arg_count; ++i) {
            if (base->as.call.args &&
                ir_expression_contains_function_parameter_callee_call(
                    external,
                    base->as.call.args[i])) {
                return 1;
            }
        }
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_BINARY:
        return ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.binary.left) ||
            ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.binary.right);
    case AST_EXPR_TERNARY:
        return ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.ternary.condition) ||
            ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.ternary.then_expr) ||
            ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.ternary.else_expr);
    case AST_EXPR_UNARY:
        return ir_expression_contains_function_parameter_callee_call(
            external,
            base->as.unary.operand);
    case AST_EXPR_POSTFIX:
        return ir_expression_contains_function_parameter_callee_call(
            external,
            base->as.postfix.operand);
    case AST_EXPR_SUBSCRIPT:
        return ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.subscript.base) ||
            ir_expression_contains_function_parameter_callee_call(
                external,
                base->as.subscript.index);
    case AST_EXPR_MEMBER:
        return ir_expression_contains_function_parameter_callee_call(
            external,
            base->as.member.base);
    case AST_EXPR_PAREN:
        return ir_expression_contains_function_parameter_callee_call(
            external,
            base->as.inner);
    case AST_EXPR_INIT_LIST:
        for (i = 0u; i < base->as.init_list.item_count; ++i) {
            if (base->as.init_list.items &&
                ir_expression_contains_function_parameter_callee_call(
                    external,
                    base->as.init_list.items[i])) {
                return 1;
            }
        }
        return 0;
    default:
        return 0;
    }
}

static int ir_external_body_needs_function_value_specialization(const AstProgram *program,
    const AstExternal *current_external,
    const AstStatement *stmt) {
    size_t expr_index;
    size_t child_index;
    size_t param_index;

    if (!current_external || !stmt) {
        return 0;
    }

    if (ir_external_body_has_function_parameter_callee_calls(current_external, stmt)) {
        return 1;
    }
    if (stmt->kind == AST_STMT_RETURN &&
        current_external &&
        current_external->has_function_return_signature &&
        stmt->expression_count > 0u &&
        stmt->expressions) {
        const AstExpression *return_expr =
            ir_unwrap_function_value_wrapper_expression(stmt->expressions[0]);
        const AstExpression *passthrough_arg_expr = NULL;
        if (return_expr &&
            return_expr->kind == AST_EXPR_IDENTIFIER &&
            return_expr->as.identifier.name &&
            ir_function_parameter_name_is_compatible_function_value(
                current_external,
                return_expr->as.identifier.name,
                current_external->function_return_function_return_type,
                current_external->function_return_function_parameter_count)) {
            return 1;
        }
        if (return_expr &&
            return_expr->kind == AST_EXPR_CALL &&
            ir_try_resolve_passthrough_function_value_argument_expr(
                program,
                return_expr,
                &passthrough_arg_expr) &&
            passthrough_arg_expr) {
            const AstExpression *passthrough_base =
                ir_unwrap_function_value_wrapper_expression(passthrough_arg_expr);

            if (passthrough_base &&
                passthrough_base->kind == AST_EXPR_IDENTIFIER &&
                passthrough_base->as.identifier.name &&
                ir_function_parameter_name_is_compatible_function_value(
                    current_external,
                    passthrough_base->as.identifier.name,
                    current_external->function_return_function_return_type,
                    current_external->function_return_function_parameter_count)) {
                return 1;
            }
        }
    }

    for (expr_index = 0u; expr_index < stmt->expression_count; ++expr_index) {
        const AstExpression *expr = stmt->expressions ? stmt->expressions[expr_index] : NULL;
        const char *callee_name = NULL;
        const AstExternal *callee_external = NULL;
        size_t external_index;
        size_t arg_index;

        if (!expr) {
            continue;
        }

        if (ir_expression_contains_function_parameter_callee_call(current_external, expr)) {
            return 1;
        }

        if (expr->kind != AST_EXPR_CALL) {
            continue;
        }

        callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
        if ((!callee_name || callee_name[0] == '\0') &&
            !ir_classify_direct_identifier_expr(expr->as.call.callee, &callee_name)) {
            continue;
        }

        if (callee_name && current_external->parameter_value_kinds && current_external->parameter_names) {
            size_t param_index;

            for (param_index = 0u; param_index < current_external->parameter_count; ++param_index) {
                if (current_external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
                    current_external->parameter_names[param_index] &&
                    strcmp(current_external->parameter_names[param_index], callee_name) == 0) {
                    return 1;
                }
            }
        }

        if (!program || !callee_name || callee_name[0] == '\0') {
            continue;
        }

        for (external_index = 0u; external_index < program->count; ++external_index) {
            const AstExternal *candidate = &program->externals[external_index];

            if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                candidate->name &&
                strcmp(candidate->name, callee_name) == 0) {
                callee_external = candidate;
                break;
            }
        }
        if (!callee_external || !callee_external->parameter_value_kinds) {
            continue;
        }

        for (arg_index = 0u; arg_index < expr->as.call.arg_count && arg_index < callee_external->parameter_count;
             ++arg_index) {
            const AstExpression *arg_expr = expr->as.call.args ? expr->as.call.args[arg_index] : NULL;
            const char *arg_name = NULL;
            size_t expected_nested_count = 0u;
            AstFunctionReturnType expected_return_type = AST_FUNCTION_RETURN_INT;

            if (!arg_expr ||
                callee_external->parameter_value_kinds[arg_index] != AST_PARAMETER_VALUE_FUNCTION ||
                !ir_classify_direct_identifier_expr(arg_expr, &arg_name) ||
                !arg_name || arg_name[0] == '\0') {
                continue;
            }

            expected_nested_count = callee_external->parameter_function_parameter_counts
                ? callee_external->parameter_function_parameter_counts[arg_index]
                : 0u;
            expected_return_type = callee_external->parameter_function_return_types
                ? callee_external->parameter_function_return_types[arg_index]
                : AST_FUNCTION_RETURN_INT;

            if (ir_function_parameter_name_is_compatible_function_value(
                    current_external,
                    arg_name,
                    expected_return_type,
                    expected_nested_count)) {
                return 1;
            }

            if (current_external->function_body &&
                current_external->function_body->kind == AST_STMT_COMPOUND &&
                current_external->function_body->children) {
                size_t body_child_index;

                for (body_child_index = 0u;
                        body_child_index < current_external->function_body->child_count;
                        ++body_child_index) {
                    const AstStatement *child =
                        current_external->function_body->children[body_child_index];
                    size_t decl_index;

                    if (!child || child->kind != AST_STMT_DECLARATION ||
                        !child->declaration_names || !child->declaration_value_kinds ||
                        !child->expressions) {
                        continue;
                    }
                    for (decl_index = 0u;
                            decl_index < child->declaration_name_count &&
                            decl_index < child->expression_count;
                            ++decl_index) {
                        const AstExpression *init_expr = NULL;
                        const AstExpression *init_base = NULL;

                        if (!child->declaration_names[decl_index] ||
                            strcmp(child->declaration_names[decl_index], arg_name) != 0 ||
                            child->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_FUNCTION) {
                            continue;
                        }
                        init_expr = child->expressions[decl_index];
                        init_base = ir_unwrap_function_value_wrapper_expression(init_expr);
                        if (!init_base ||
                            init_base->kind != AST_EXPR_IDENTIFIER ||
                            !init_base->as.identifier.name) {
                            continue;
                        }
                        for (param_index = 0u; param_index < current_external->parameter_count; ++param_index) {
                            if (current_external->parameter_value_kinds &&
                                current_external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
                                current_external->parameter_names[param_index] &&
                                strcmp(current_external->parameter_names[param_index], init_base->as.identifier.name) == 0) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        if (ir_external_body_needs_function_value_specialization(
                program,
                current_external,
                stmt->children ? stmt->children[child_index] : NULL)) {
            return 1;
        }
    }

    return 0;
}

#if 0
static int ir_external_body_has_function_parameter_forwarding_calls_unused(const AstExternal *external,
    const AstStatement *stmt) {
    size_t child_index;
    size_t expr_index;

    if (!external || !stmt || !external->parameter_value_kinds || !external->parameter_names) {
        return 0;
    }

    for (expr_index = 0u; expr_index < stmt->expression_count; ++expr_index) {
        const AstExpression *expr = stmt->expressions ? stmt->expressions[expr_index] : NULL;
        const char *callee_name = NULL;
        const AstExternal *callee_external = NULL;
        size_t external_index;
        size_t arg_index;

        if (!expr || expr->kind != AST_EXPR_CALL ||
            !ir_classify_direct_identifier_expr(expr->as.call.callee, &callee_name) ||
            !callee_name || callee_name[0] == '\0') {
            continue;
        }

        for (external_index = 0u; external_index < stmt->line; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        callee_external = NULL;
        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < external->parameter_count; ++external_index) {
            (void)external_index;
        }

        for (external_index = 0u; external_index < 0u; ++external_index) {
            (void)external_index;
        }

        (void)callee_external;
        (void)arg_index;
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        if (ir_external_body_has_function_parameter_forwarding_calls(
                external,
                stmt->children ? stmt->children[child_index] : NULL)) {
            return 1;
        }
    }

    return 0;
}
#endif

static int ir_lower_function_core(const AstProgram *ast_program,
    IrProgram *program,
    IrFunction *existing_function,
    const IrLowerOptions *options,
    const AstExternal *external,
    const char *const *bound_function_targets,
    const size_t *const *bound_function_capture_local_ids,
    const size_t *bound_function_capture_counts,
    IrError *error) {
    IrFunction *function;
    IrLowerContext ctx;
    size_t i;
    int scope_pushed = 0;
    int ok = 0;
    const char *current_function_name = NULL;
    const char *failure_stage = "entry";

    if (!program || !external || external->kind != AST_EXTERNAL_FUNCTION ||
        !external->name || external->name[0] == '\0') {
        ir_set_error(error, 0, 0, "IR-INT-063: invalid function lowering contract");
        return 0;
    }

    function = existing_function;
    if (!function) {
        if (!ir_ensure_function_signature(ast_program, program, external, &function, error)) {
            return 0;
        }
    }

    if (!external->is_function_definition) {
        return 1;
    }

    if (!external->function_body) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-064: function '%s' definition is missing body AST",
            external->name);
        return 0;
    }

    if (function->has_body) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-LOWER-021: duplicate function body lowering attempt for '%s'",
            external->name);
        return 0;
    }

    current_function_name = function->name;

    if (!ir_function_append_block(function, &ctx.block_id, NULL, error)) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.program = program;
    ctx.ast_program = ast_program;
    ctx.function = function;
    ctx.function_return_type = external->function_return_type;
    ctx.hidden_return_aggregate_kind = AST_DECLARATION_VALUE_INT;
    ctx.hidden_return_type_name = external->function_return_type_name;
    ctx.hidden_return_first_local_id = 0u;
    ctx.hidden_return_slot_count = 0u;
    ctx.has_function_value_return_payload = external->has_function_return_signature ? 1 : 0;
    ctx.function_value_return_is_closure_family = 0;
    ctx.function_value_return_target_name = NULL;
    ctx.allow_implicit_fallthrough_return =
        options && options->allow_implicit_fallthrough_return ? 1 : 0;
    ctx.block_id = 0;
    ctx.has_block = 1;
    ctx.error = error;
    ctx.function_value_parameter_names = external->parameter_names
        ? (const char **)external->parameter_names
        : NULL;
    ctx.function_value_parameter_targets = bound_function_targets;
    ctx.function_value_parameter_capture_local_ids = bound_function_capture_local_ids;
    ctx.function_value_parameter_capture_counts = bound_function_capture_counts;
    ctx.function_value_parameter_count = external->parameter_count;

    if (!ir_lower_scope_stack_push(&ctx.scopes)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-065: failed to initialize function lowering scope stack");
        return 0;
    }
    scope_pushed = 1;

    {
        size_t physical_cursor = 0u;

        failure_stage = "parameter-binding";

        if (!ir_function_lowered_return_slot_count(ast_program, program, external, &ctx.hidden_return_slot_count)) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-LOWER-020: function '%s' return lowering shape is unsupported",
                external->name);
            goto cleanup;
        }
        if (ctx.has_function_value_return_payload &&
            !ir_function_value_return_is_closure_family(
                ast_program,
                external,
                &ctx.function_value_return_is_closure_family)) {
            goto cleanup;
        }
        if (ctx.has_function_value_return_payload &&
            !ir_resolve_function_value_return_target_name(
                ast_program,
                program,
                external,
                &ctx.function_value_return_target_name,
                error)) {
            goto cleanup;
        }
        if (ctx.hidden_return_slot_count > 0u) {
            ctx.hidden_return_first_local_id = 0u;
            (void)ir_function_return_type_as_aggregate_kind(external, &ctx.hidden_return_aggregate_kind);
            physical_cursor = ctx.hidden_return_slot_count;
        }

        for (i = 0u; i < external->parameter_count; ++i) {
            const char *parameter_name =
                external->parameter_names && external->parameter_names[i] &&
                    external->parameter_names[i][0] != '\0'
                    ? external->parameter_names[i]
                    : ((function->locals && physical_cursor < function->local_count)
                        ? function->locals[physical_cursor].source_name
                        : NULL);
            size_t parameter_array_rank =
                external->parameter_array_ranks ? external->parameter_array_ranks[i] : 0u;
            int parameter_is_const =
                external->parameter_is_const ? external->parameter_is_const[i] : 0;
            size_t *parameter_array_extents = NULL;
            size_t ignored_element_count = 0u;
            size_t parameter_slot_count = 1u;
            AstDeclarationValueKind aggregate_kind = AST_DECLARATION_VALUE_INT;

            if (!ir_function_source_parameter_slot_count(ast_program, external, i, &parameter_slot_count)) {
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-LOWER-020: function '%s' parameter lowering shape is unsupported",
                    external->name);
                goto cleanup;
            }
            if (!parameter_name || parameter_name[0] == '\0') {
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-INT-066: function '%s' parameter slot %zu has no source name",
                    external->name,
                    i);
                goto cleanup;
            }

            if (parameter_array_rank > 0u &&
                !ir_compute_array_extents(&ctx,
                    external->parameter_array_extent_exprs
                        ? external->parameter_array_extent_exprs[i]
                        : NULL,
                    parameter_array_rank,
                    1,
                    &parameter_array_extents,
                    &ignored_element_count)) {
                goto cleanup;
            }

            if (ir_parameter_value_kind_as_aggregate_kind(
                    external->parameter_value_kinds
                        ? external->parameter_value_kinds[i]
                        : AST_PARAMETER_VALUE_INT,
                    &aggregate_kind)) {
                if (!ir_lower_scope_add_binding_with_metadata(&ctx.scopes,
                        parameter_name,
                        physical_cursor,
                        aggregate_kind == AST_DECLARATION_VALUE_PAIR
                            ? IR_LOWER_BINDING_PAIR_LOCAL
                            : IR_LOWER_BINDING_STRUCT_LOCAL,
                        0u,
                        NULL,
                        parameter_slot_count > 1u ? physical_cursor + 1u : physical_cursor,
                        parameter_slot_count,
                        aggregate_kind == AST_DECLARATION_VALUE_STRUCT &&
                                external->parameter_type_names
                            ? external->parameter_type_names[i]
                            : NULL,
                        0u,
                        NULL,
                        0u,
                        parameter_is_const,
                        0,
                        0,
                        0)) {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-INT-067: failed to bind parameter '%s' during function lowering",
                        parameter_name);
                    goto cleanup;
                }
            } else if (external->parameter_value_kinds &&
                    external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION &&
                    bound_function_targets &&
                    bound_function_targets[i] &&
                    bound_function_targets[i][0] != '\0') {
                size_t capture_count =
                    bound_function_capture_counts ? bound_function_capture_counts[i] : 0u;
                size_t *specialized_capture_local_ids = NULL;
                size_t capture_index;

                if (capture_count > 0u) {
                    specialized_capture_local_ids =
                        (size_t *)malloc(capture_count * sizeof(size_t));
                    if (!specialized_capture_local_ids) {
                        free(parameter_array_extents);
                        ir_set_error(error,
                            external->line,
                            external->column,
                            "IR-INT-067: out of memory while binding closure captures for specialized parameter '%s'",
                            parameter_name ? parameter_name : "<unnamed>");
                        goto cleanup;
                    }
                    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
                        specialized_capture_local_ids[capture_index] = physical_cursor + capture_index;
                    }
                }
                if (!ir_lower_scope_add_binding_with_metadata(&ctx.scopes,
                        parameter_name,
                        (size_t)-1,
                        capture_count > 0u
                            ? IR_LOWER_BINDING_CLOSURE_LOCAL
                            : IR_LOWER_BINDING_FUNCTION_LOCAL,
                        0u,
                        NULL,
                        0u,
                        0u,
                        bound_function_targets[i],
                        capture_count,
                        specialized_capture_local_ids,
                        (size_t)-1,
                        parameter_is_const,
                        0,
                        0,
                        0)) {
                    free(specialized_capture_local_ids);
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-INT-067: failed to bind specialized function-value parameter '%s' during function lowering",
                        parameter_name);
                    goto cleanup;
                }
                free(specialized_capture_local_ids);
                free(parameter_array_extents);
                physical_cursor += capture_count;
                continue;
            } else if (external->parameter_value_kinds &&
                    external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
                if (!ir_lower_scope_add_binding_with_metadata(&ctx.scopes,
                        parameter_name,
                        (size_t)-1,
                        IR_LOWER_BINDING_FUNCTION_LOCAL,
                        0u,
                        NULL,
                        0u,
                        0u,
                        parameter_name,
                        0u,
                        NULL,
                        physical_cursor,
                        parameter_is_const,
                        0,
                        0,
                        0)) {
                    free(parameter_array_extents);
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-INT-067: failed to bind function-valued parameter '%s' during function lowering",
                        parameter_name);
                    goto cleanup;
                }

                free(parameter_array_extents);
                physical_cursor += parameter_slot_count;
                continue;
            } else if (!ir_lower_scope_add_binding_with_metadata(&ctx.scopes,
                    parameter_name,
                    physical_cursor,
                    parameter_array_rank > 0u
                        ? IR_LOWER_BINDING_ARRAY_PARAMETER
                        : IR_LOWER_BINDING_SCALAR_LOCAL,
                    parameter_array_rank,
                    parameter_array_extents,
                    0u,
                    0u,
                    NULL,
                    0u,
                    NULL,
                    0u,
                    parameter_is_const,
                    0,
                    0,
                    0)) {
                free(parameter_array_extents);
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-INT-067: failed to bind parameter '%s' during function lowering",
                    parameter_name);
                goto cleanup;
            }

            free(parameter_array_extents);
            physical_cursor += parameter_slot_count;
        }
    }

    if (external->function_body) {
        size_t fndefer_site_capacity = 0u;

        failure_stage = "fndefer-collection";

        if (!ir_collect_fndefer_sites(external->function_body,
                &ctx.fndefer_sites,
                &ctx.fndefer_site_count,
                &fndefer_site_capacity,
                error)) {
            if (error && error->message[0] == '\0') {
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-INT-115: function '%s' failed while collecting function-exit defer sites",
                    external->name);
            }
            goto cleanup;
        }
        if (ctx.fndefer_site_count > 0u) {
            ctx.fndefer_counter_local_ids = (size_t *)calloc(ctx.fndefer_site_count, sizeof(size_t));
            if (!ctx.fndefer_counter_local_ids) {
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-INT-086: out of memory while preparing function-exit defer counters");
                goto cleanup;
            }
            for (i = 0u; i < ctx.fndefer_site_count; ++i) {
                char hidden_name[64];
                size_t counter_local_id = (size_t)-1;

                snprintf(hidden_name, sizeof(hidden_name), "__fndefer_count_%zu", i);
                if (!ir_function_append_local(function, hidden_name, 0, &counter_local_id, error) ||
                    !ir_emit_mov(&ctx, ir_value_local(counter_local_id), ir_value_immediate(0)) ||
                    !ir_lower_function_defer_entry_stack_push_with_runtime_repeat_count(
                        &ctx.function_defer_entries,
                        ctx.fndefer_sites[i]->children[0],
                        counter_local_id)) {
                    if (error && error->message[0] == '\0') {
                        ir_set_error(error,
                            external->line,
                            external->column,
                            "IR-INT-116: function '%s' failed while preparing function-exit defer counter %zu",
                            external->name,
                            i);
                    }
                    goto cleanup;
                }
                ctx.fndefer_counter_local_ids[i] = counter_local_id;
            }
        }
    }

    failure_stage = "body-lowering";
    if (!ir_lower_statement(&ctx, external->function_body)) {
        if (error && error->message[0] == '\0') {
            ir_set_error(error,
                external->function_body ? external->function_body->line : external->line,
                external->function_body ? external->function_body->column : external->column,
                "IR-INT-117: function '%s' failed while lowering function body statement tree",
                external->name);
        }
        goto cleanup;
    }

    if (current_function_name && current_function_name[0] != '\0') {
        failure_stage = "post-body-refresh";
        function = ir_program_find_function_mut(program, current_function_name);
        if (!function) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-084: current function '%s' disappeared during lowering",
                current_function_name);
            goto cleanup;
        }
        ctx.function = function;
    }

    failure_stage = "post-body-block-check";
    if (ctx.has_block && !ir_lower_current_block(&ctx)) {
        if (error && error->message[0] == '\0') {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-123: function '%s' lost its current basic block after body lowering",
                external->name);
        }
        goto cleanup;
    }

    failure_stage = "function-defer-emit";
    if (ctx.has_block && !ir_lower_current_block(&ctx)->has_terminator) {
        if (!ir_lower_emit_function_defer_entries(&ctx)) {
            if (error && error->message[0] == '\0') {
                ir_set_error(error,
                    external->line,
                    external->column,
                    "IR-INT-118: function '%s' failed while emitting function-exit defer entries",
                    external->name);
            }
            goto cleanup;
        }
    }

    failure_stage = "implicit-return";
    if (ctx.has_block && !ir_lower_current_block(&ctx)->has_terminator) {
        if (ctx.hidden_return_slot_count > 0u && ctx.allow_implicit_fallthrough_return) {
            size_t slot_index;

            for (slot_index = 0u; slot_index < ctx.hidden_return_slot_count; ++slot_index) {
                if (!ir_emit_store_indirect(
                        &ctx,
                        ir_value_local(ctx.hidden_return_first_local_id + slot_index),
                        ir_value_immediate(0))) {
                    if (error && error->message[0] == '\0') {
                        ir_set_error(error,
                            external->line,
                            external->column,
                            "IR-INT-119: function '%s' failed while materializing implicit aggregate zero return slot %zu",
                            external->name,
                            slot_index);
                    }
                    goto cleanup;
                }
            }
            if (!ir_block_set_void_return(ir_lower_current_block(&ctx), error)) {
                if (error && error->message[0] == '\0') {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-INT-120: function '%s' failed while emitting implicit aggregate void return terminator",
                        external->name);
                }
                goto cleanup;
            }
        } else if (external->function_return_type == AST_FUNCTION_RETURN_VOID) {
            if (!ir_block_set_void_return(ir_lower_current_block(&ctx), error)) {
                if (error && error->message[0] == '\0') {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-INT-121: function '%s' failed while emitting implicit void return terminator",
                        external->name);
                }
                goto cleanup;
            }
        } else if (ctx.allow_implicit_fallthrough_return) {
            if (!ir_block_set_return(ir_lower_current_block(&ctx), ir_value_immediate(0), error)) {
                if (error && error->message[0] == '\0') {
                    ir_set_error(error,
                        external->line,
                        external->column,
                        "IR-INT-122: function '%s' failed while emitting implicit scalar zero return terminator",
                        external->name);
                }
                goto cleanup;
            }
        } else {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-LOWER-009: function '%s' body does not terminate on all lowered paths",
                external->name);
            goto cleanup;
        }
    }

    function->has_body = 1;
    ok = 1;

cleanup:
    if (!ok && error && error->message[0] == '\0') {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-090: function '%s' lowering failed without a specific diagnostic (stage=%s)",
            external->name ? external->name : "<anon>",
            failure_stage ? failure_stage : "unknown");
    }
    if (scope_pushed) {
        ir_lower_scope_stack_pop(&ctx.scopes);
    }
    ir_lower_scope_stack_free(&ctx.scopes);
    ir_loop_target_stack_free(&ctx.loop_targets);
    ir_lower_defer_scope_stack_free(&ctx.defer_scopes);
    ir_lower_defer_entry_stack_free(&ctx.defer_entries);
    ir_lower_function_defer_entry_stack_free(&ctx.function_defer_entries);
    free((void *)ctx.fndefer_sites);
    free(ctx.fndefer_counter_local_ids);
    ir_lower_function_value_target_sets_free(
        ctx.function_value_target_sets,
        ctx.function_value_target_set_count);
    return ok;
}

static int ir_build_function_value_specialized_name(const AstExternal *external,
    const char *const *bound_function_targets,
    char **out_name,
    IrError *error) {
    size_t param_index;
    size_t needed = 0u;
    char *name = NULL;
    size_t offset = 0u;

    if (out_name) {
        *out_name = NULL;
    }
    if (!external || !external->name || !out_name) {
        ir_set_error(error, 0, 0, "IR-INT-080: invalid specialized-name build contract");
        return 0;
    }

    needed = strlen(external->name) + strlen("__fv") + 1u;
    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
        if (external->parameter_value_kinds &&
            external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
            bound_function_targets &&
            bound_function_targets[param_index]) {
            needed += strlen(bound_function_targets[param_index]) + 3u;
        }
    }

    name = (char *)malloc(needed);
    if (!name) {
        ir_set_error(error, external->line, external->column,
            "IR-INT-081: out of memory while naming function-value specialization");
        return 0;
    }

    offset += (size_t)snprintf(name + offset, needed - offset, "%s__fv", external->name);
    for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
        if (external->parameter_value_kinds &&
            external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION &&
            bound_function_targets &&
            bound_function_targets[param_index]) {
            offset += (size_t)snprintf(name + offset,
                needed - offset,
                "_%zu_%s",
                param_index,
                bound_function_targets[param_index]);
        }
    }

    *out_name = name;
    return 1;
}

static int ir_build_closure_helper_name(const char *enclosing_function_name,
    const char *local_name,
    size_t closure_ordinal,
    char **out_name,
    IrError *error) {
    size_t needed = 0u;
    char *name = NULL;

    if (out_name) {
        *out_name = NULL;
    }
    if (!enclosing_function_name || enclosing_function_name[0] == '\0' ||
        !local_name || local_name[0] == '\0' || !out_name) {
        ir_set_error(error, 0, 0, "IR-INT-084: invalid closure helper naming contract");
        return 0;
    }

    needed = strlen(enclosing_function_name) + strlen(local_name) + 32u;
    name = (char *)malloc(needed);
    if (!name) {
        ir_set_error(error, 0, 0, "IR-INT-085: out of memory while naming closure helper");
        return 0;
    }

    snprintf(name,
        needed,
        "%s__closure_%s_%zu",
        enclosing_function_name,
        local_name,
        closure_ordinal);
    *out_name = name;
    return 1;
}

static int ir_lower_closure_helper_function(IrLowerContext *ctx,
    const AstStatement *decl_stmt,
    size_t declaration_index,
    const char *helper_name,
    const char *const *capture_names,
    const size_t *capture_local_ids,
    size_t capture_count,
    IrError *error) {
    const AstExpression *initializer_expr = NULL;
    const AstExpression *closure_expr = NULL;
    AstExternal synthetic_external;
    AstExternal *registered_external = NULL;
    IrFunction *function = NULL;
    char **parameter_names = NULL;
    int *parameter_value_kinds = NULL;
    AstFunctionReturnType *parameter_function_return_types = NULL;
    size_t *parameter_function_parameter_counts = NULL;
    int **parameter_function_parameter_value_kinds = NULL;
    AstFunctionReturnType **parameter_function_parameter_return_types = NULL;
    size_t **parameter_function_parameter_parameter_counts = NULL;
    int *parameter_name_lines = NULL;
    int *parameter_name_columns = NULL;
    size_t parameter_count = 0u;
    size_t parameter_index = 0u;
    int ok = 0;

    if (!ctx || !ctx->ast_program || !ctx->program || !ctx->function ||
        !decl_stmt || !helper_name || helper_name[0] == '\0' ||
        declaration_index >= decl_stmt->declaration_name_count ||
        !decl_stmt->expressions || declaration_index >= decl_stmt->expression_count) {
        ir_set_error(error, 0, 0, "IR-INT-086: invalid closure helper lowering contract");
        return 0;
    }

    initializer_expr = decl_stmt->expressions[declaration_index];
    closure_expr = ir_unwrap_paren_expression(initializer_expr);
    if (!closure_expr || closure_expr->kind != AST_EXPR_CLOSURE) {
        ir_set_error(error, decl_stmt->line, decl_stmt->column,
            "IR-INT-087: closure helper lowering requires a closure initializer expression");
        return 0;
    }

    parameter_count = capture_count + closure_expr->as.closure.parameter_count;
    if (parameter_count > 0u) {
        parameter_names = (char **)calloc(parameter_count, sizeof(char *));
        parameter_value_kinds = (int *)calloc(parameter_count, sizeof(int));
        parameter_function_return_types =
            (AstFunctionReturnType *)calloc(parameter_count, sizeof(AstFunctionReturnType));
        parameter_function_parameter_counts =
            (size_t *)calloc(parameter_count, sizeof(size_t));
        parameter_function_parameter_value_kinds =
            (int **)calloc(parameter_count, sizeof(int *));
        parameter_function_parameter_return_types =
            (AstFunctionReturnType **)calloc(parameter_count, sizeof(AstFunctionReturnType *));
        parameter_function_parameter_parameter_counts =
            (size_t **)calloc(parameter_count, sizeof(size_t *));
        parameter_name_lines = (int *)calloc(parameter_count, sizeof(int));
        parameter_name_columns = (int *)calloc(parameter_count, sizeof(int));
        if (!parameter_names || !parameter_value_kinds ||
            !parameter_function_return_types ||
            !parameter_function_parameter_counts ||
            !parameter_function_parameter_value_kinds ||
            !parameter_function_parameter_return_types ||
            !parameter_function_parameter_parameter_counts ||
            !parameter_name_lines || !parameter_name_columns) {
            ir_set_error(error, decl_stmt->line, decl_stmt->column,
                "IR-INT-088: out of memory while building closure helper signature");
            goto cleanup;
        }
    }

    for (parameter_index = 0u; parameter_index < capture_count; ++parameter_index) {
        parameter_names[parameter_index] = (char *)(capture_names ? capture_names[parameter_index] : NULL);
        parameter_value_kinds[parameter_index] = AST_PARAMETER_VALUE_INT;
        parameter_name_lines[parameter_index] = closure_expr->line;
        parameter_name_columns[parameter_index] = closure_expr->column;
    }
    for (parameter_index = 0u; parameter_index < closure_expr->as.closure.parameter_count; ++parameter_index) {
        size_t dest_index = capture_count + parameter_index;

        parameter_names[dest_index] =
            closure_expr->as.closure.parameter_names
                ? closure_expr->as.closure.parameter_names[parameter_index]
                : NULL;
        parameter_value_kinds[dest_index] =
            closure_expr->as.closure.parameter_value_kinds
                ? closure_expr->as.closure.parameter_value_kinds[parameter_index]
                : AST_PARAMETER_VALUE_INT;
        parameter_function_return_types[dest_index] =
            closure_expr->as.closure.parameter_function_return_types
                ? closure_expr->as.closure.parameter_function_return_types[parameter_index]
                : AST_FUNCTION_RETURN_INT;
        parameter_function_parameter_counts[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_counts
                ? closure_expr->as.closure.parameter_function_parameter_counts[parameter_index]
                : 0u;
        parameter_function_parameter_value_kinds[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_value_kinds
                ? closure_expr->as.closure.parameter_function_parameter_value_kinds[parameter_index]
                : NULL;
        parameter_function_parameter_return_types[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_return_types
                ? closure_expr->as.closure.parameter_function_parameter_return_types[parameter_index]
                : NULL;
        parameter_function_parameter_parameter_counts[dest_index] =
            closure_expr->as.closure.parameter_function_parameter_parameter_counts
                ? closure_expr->as.closure.parameter_function_parameter_parameter_counts[parameter_index]
                : NULL;
        parameter_name_lines[dest_index] = closure_expr->line;
        parameter_name_columns[dest_index] = closure_expr->column;
    }

    memset(&synthetic_external, 0, sizeof(synthetic_external));
    synthetic_external.kind = AST_EXTERNAL_FUNCTION;
    synthetic_external.function_return_type = closure_expr->as.closure.return_type;
    synthetic_external.name = (char *)helper_name;
    synthetic_external.name_length = strlen(helper_name);
    synthetic_external.parameter_count = parameter_count;
    synthetic_external.parameter_names = parameter_names;
    synthetic_external.parameter_value_kinds = parameter_value_kinds;
    synthetic_external.parameter_type_names = NULL;
    synthetic_external.parameter_array_ranks = NULL;
    synthetic_external.parameter_array_extent_exprs = NULL;
    synthetic_external.parameter_function_return_types =
        parameter_function_return_types;
    synthetic_external.parameter_function_parameter_counts =
        parameter_function_parameter_counts;
    synthetic_external.parameter_function_parameter_value_kinds =
        parameter_function_parameter_value_kinds;
    synthetic_external.parameter_function_parameter_return_types =
        parameter_function_parameter_return_types;
    synthetic_external.parameter_function_parameter_parameter_counts =
        parameter_function_parameter_parameter_counts;
    synthetic_external.parameter_is_const = NULL;
    synthetic_external.parameter_name_lines = parameter_name_lines;
    synthetic_external.parameter_name_columns = parameter_name_columns;
    synthetic_external.is_function_definition = 1;
    synthetic_external.function_body = closure_expr->as.closure.body;
    synthetic_external.line = closure_expr->line;
    synthetic_external.column = closure_expr->column;

    function = ir_program_find_function_mut(ctx->program, helper_name);
    if (function && function->has_body) {
        ok = 1;
        goto cleanup;
    }
    if (!ir_ensure_function_signature(
            ctx->ast_program,
            ctx->program,
            &synthetic_external,
            &function,
            error)) {
        goto cleanup;
    }
    if (function) {
        registered_external = (AstExternal *)malloc(sizeof(AstExternal));
        if (!registered_external) {
            ir_set_error(error, decl_stmt->line, decl_stmt->column,
                "IR-INT-144: out of memory while registering closure helper external metadata");
            goto cleanup;
        }
        *registered_external = synthetic_external;
        registered_external->name = function->name;
        registered_external->name_length = function->name ? strlen(function->name) : 0u;
        if (!ir_program_bind_function_external_ref(
                ctx->program,
                helper_name,
                registered_external,
                1,
                error)) {
            free(registered_external);
            registered_external = NULL;
            goto cleanup;
        }
        registered_external = NULL;
        parameter_names = NULL;
        parameter_value_kinds = NULL;
        parameter_function_return_types = NULL;
        parameter_function_parameter_counts = NULL;
        parameter_function_parameter_value_kinds = NULL;
        parameter_function_parameter_return_types = NULL;
        parameter_function_parameter_parameter_counts = NULL;
        parameter_name_lines = NULL;
        parameter_name_columns = NULL;
    }
    if (ir_external_body_needs_function_value_specialization(
            ctx->ast_program,
            &synthetic_external,
            synthetic_external.function_body)) {
        ok = 1;
        goto cleanup;
    }
    ok = ir_lower_function_core(
        ctx->ast_program,
        ctx->program,
        function,
        NULL,
        &synthetic_external,
        NULL,
        NULL,
        NULL,
        error);
    (void)capture_local_ids;

cleanup:
    free(parameter_names);
    free(parameter_value_kinds);
    free(parameter_function_return_types);
    free(parameter_function_parameter_counts);
    free(parameter_function_parameter_value_kinds);
    free(parameter_function_parameter_return_types);
    free(parameter_function_parameter_parameter_counts);
    free(parameter_name_lines);
    free(parameter_name_columns);
    return ok;
}

static int ir_lower_specialized_function_for_function_value_call(
    const AstProgram *ast_program,
    IrProgram *program,
    const IrLowerOptions *options,
    const AstExternal *external,
    const char *const *bound_function_targets,
    const size_t *const *bound_function_capture_local_ids,
    const size_t *bound_function_capture_counts,
    const char **out_specialized_name,
    IrError *error) {
    char *specialized_name = NULL;
    IrFunction *function = NULL;
    int ok = 0;

    if (out_specialized_name) {
        *out_specialized_name = NULL;
    }
    if (!ast_program || !program || !external || !bound_function_targets) {
        ir_set_error(error, 0, 0, "IR-INT-082: invalid specialized function lowering contract");
        return 0;
    }

    if (!ir_build_function_value_specialized_name(external, bound_function_targets, &specialized_name, error)) {
        return 0;
    }

    function = ir_program_find_function_mut(program, specialized_name);
    if (function) {
        if (out_specialized_name) {
            *out_specialized_name = function->name;
        }
        free(specialized_name);
        return 1;
    }

    if (!ir_program_append_function(program, specialized_name, &function, error)) {
        free(specialized_name);
        return 0;
    }
    function->return_type = ir_lowered_function_return_type(external);

    {
        size_t return_slot_count = 0u;
        size_t return_slot_index;
        size_t param_index;
        size_t capture_param_index = 0u;

        if (!ir_function_lowered_return_slot_count(ast_program, program, external, &return_slot_count)) {
            free(specialized_name);
            return 0;
        }
        function = ir_program_find_function_mut(program, specialized_name);
        if (!function) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-082: specialized function '%s' disappeared during signature lowering",
                specialized_name);
            free(specialized_name);
            return 0;
        }
        for (return_slot_index = 0u; return_slot_index < return_slot_count; ++return_slot_index) {
            char hidden_name[32];

            snprintf(hidden_name, sizeof(hidden_name), "__ret%zu", return_slot_index);
            if (!ir_function_append_local(function, hidden_name, 1, NULL, error)) {
                free(specialized_name);
                return 0;
            }
            function->locals[return_slot_index].value_type =
                external->function_return_type == AST_FUNCTION_RETURN_FLOAT
                    ? AST_FUNCTION_RETURN_FLOAT
                    : AST_FUNCTION_RETURN_INT;
        }
        for (param_index = 0u; param_index < external->parameter_count; ++param_index) {
            size_t hidden_capture_count =
                ir_function_specialized_hidden_capture_count_for_parameter(
                    external,
                    param_index,
                    bound_function_capture_counts);
            size_t capture_index;

            for (capture_index = 0u; capture_index < hidden_capture_count; ++capture_index) {
                char hidden_name[64];

                snprintf(hidden_name,
                    sizeof(hidden_name),
                    "__cap_%zu_%zu",
                    param_index,
                    capture_param_index + capture_index);
                if (!ir_function_append_local(function, hidden_name, 1, NULL, error)) {
                    free(specialized_name);
                    return 0;
                }
            }
            capture_param_index += hidden_capture_count;

            if (!(external->parameter_value_kinds &&
                    external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION)) {
                if (!ir_function_append_local(function,
                        external->parameter_names && external->parameter_names[param_index]
                            ? external->parameter_names[param_index]
                            : "param",
                        1,
                        NULL,
                        error)) {
                    free(specialized_name);
                    return 0;
                }
            }
        }
    }

    function = ir_program_find_function_mut(program, specialized_name);
    if (!function) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-082: specialized function '%s' disappeared before body lowering",
            specialized_name);
        free(specialized_name);
        return 0;
    }

    ok = ir_lower_function_core(
        ast_program,
        program,
        function,
        options,
        external,
        bound_function_targets,
        bound_function_capture_local_ids,
        bound_function_capture_counts,
        error);
    if (!ok) {
        free(specialized_name);
        return 0;
    }
    function = ir_program_find_function_mut(program, specialized_name);
    if (!function) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-082: specialized function '%s' disappeared after lowering",
            specialized_name);
        free(specialized_name);
        return 0;
    }

    if (out_specialized_name) {
        *out_specialized_name = function->name;
    }
    free(specialized_name);
    return 1;
}

int ir_lower_program(const AstProgram *ast_program,
    const IrLowerOptions *options,
    IrProgram *out_program,
    IrError *error) {
    IrFunction *runtime_init_function = NULL;
    size_t i;

    if (!ast_program || !out_program) {
        ir_set_error(error, 0, 0, "IR-INT-068: invalid program lowering contract");
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    ir_program_free(out_program);
    ir_program_init(out_program);

    if (!ir_predeclare_program_globals(out_program, ast_program, error)) {
        return 0;
    }
    if (!ir_predeclare_used_builtin_functions(out_program, ast_program, error)) {
        return 0;
    }

    for (i = 0; i < ast_program->count; ++i) {
        const AstExternal *external = &ast_program->externals[i];

        if (external->kind == AST_EXTERNAL_DECLARATION) {
            if (!ir_lower_global_declaration(out_program,
                    ast_program,
                    external,
                    &runtime_init_function,
                    error)) {
                return 0;
            }
            continue;
        }

        if (external->kind != AST_EXTERNAL_FUNCTION) {
            continue;
        }
        if (!external->name || external->name[0] == '\0') {
            continue;
        }
        if (strcmp(external->name, IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME) == 0 ||
            strcmp(external->name, IR_PROGRAM_INIT_FUNCTION_NAME) == 0) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-LOWER-024: reserved internal helper '%s' cannot be user-defined",
                external->name);
            return 0;
        }

        if (!ir_ensure_function_signature(ast_program, out_program, external, NULL, error)) {
            return 0;
        }
        if (external->is_function_definition &&
            ir_external_body_needs_function_value_specialization(ast_program, external, external->function_body)) {
            continue;
        }
        if (external->is_function_definition && !ir_lower_function(ast_program, out_program, options, external, error)) {
            return 0;
        }
    }

    if (runtime_init_function) {
        runtime_init_function = ir_program_find_function_mut(out_program,
            IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME);

        if (!runtime_init_function) {
            ir_set_error(error, 0, 0, "IR-INT-069: runtime initializer helper disappeared");
            return 0;
        }

        IrFunction *main_function = ir_program_find_function_mut(out_program, "main");

        if (!runtime_init_function->has_body ||
            runtime_init_function->block_count == 0u ||
            !runtime_init_function->blocks) {
            ir_set_error(error, 0, 0, "IR-INT-069: runtime initializer helper entry block missing");
            return 0;
        }
        if (!ir_finalize_runtime_init_function_returns_zero(runtime_init_function, error)) {
            return 0;
        }

        if (main_function && main_function->has_body) {
            if (!ir_prepend_startup_runtime_init_call(main_function, error)) {
                return 0;
            }
        } else {
            IrFunction *program_init = ir_program_find_function_mut(out_program,
                IR_PROGRAM_INIT_FUNCTION_NAME);
            IrLowerContext ctx;
            IrValueRef call_result;

            if (!program_init) {
                if (!ir_program_append_function(out_program,
                        IR_PROGRAM_INIT_FUNCTION_NAME,
                        &program_init,
                        error)) {
                    return 0;
                }
            }

            if (program_init->parameter_count != 0 || program_init->local_count != 0 ||
                program_init->has_body) {
                ir_set_error(error,
                    0,
                    0,
                    "IR-LOWER-024: reserved internal helper '%s' cannot be user-defined",
                    IR_PROGRAM_INIT_FUNCTION_NAME);
                return 0;
            }

            if (!ir_function_append_block(program_init, NULL, NULL, error)) {
                return 0;
            }
            program_init->has_body = 1;

            memset(&ctx, 0, sizeof(ctx));
            ctx.program = out_program;
            ctx.function = program_init;
            ctx.block_id = 0;
            ctx.has_block = 1;
            ctx.error = error;

            call_result = ir_value_temp(ir_allocate_temp(program_init));
            if (!ir_emit_call(&ctx,
                    call_result,
                    IR_RUNTIME_GLOBAL_INIT_FUNCTION_NAME,
                    NULL,
                    0) ||
                !ir_block_set_return(ir_lower_current_block(&ctx),
                    ir_value_immediate(0),
                    error)) {
                return 0;
            }
        }
    }

    if (!ir_verify_program(out_program, error)) {
        return 0;
    }
    return 1;
}

#endif
