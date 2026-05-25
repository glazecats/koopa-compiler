#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_lower_scope_clear_mutated_scalar_locals_in_expr(IrLowerScopeStack *stack,
    const AstExpression *expr);
static int ir_expression_is_direct_float_condition(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_expression_is_signed_float_literal(const AstExpression *expr);
static int ir_expression_is_direct_float_transport_root(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_lower_signed_float_literal_expression(const AstExpression *expr,
    IrValueRef *out_value);
static int ir_expression_is_float_arithmetic(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_lower_float_arithmetic_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
static int ir_expression_is_float_direct_compare(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_lower_float_compare_normalize_zero(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_normalized);
static int ir_lower_float_compare_order_key(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_key);
static int ir_lower_float_compare_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
static int ir_lower_refresh_current_function(IrLowerContext *ctx,
    const char *current_function_name,
    int line,
    int column);

static int ir_lower_refresh_current_function(IrLowerContext *ctx,
    const char *current_function_name,
    int line,
    int column) {
    if (!ctx || !current_function_name || current_function_name[0] == '\0') {
        return 1;
    }
    ctx->function = ir_program_find_function_mut(ctx->program, current_function_name);
    if (!ctx->function) {
        ir_set_error(ctx->error,
            line,
            column,
            "IR-INT-084: current lowering function '%s' disappeared after builtin insertion",
            current_function_name);
        return 0;
    }
    return 1;
}

static int ir_lower_boolean_branch(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t then_target,
    size_t else_target) {
    IrValueRef condition_value;
    IrValueRef masked_value;
    IrValueRef compare_value;
    IrBasicBlock *block;
    size_t rhs_block_id;

    if (!ctx || !expr || !ctx->function || !ctx->has_block) {
        return 0;
    }

    if (expr->kind == AST_EXPR_UNARY && expr->as.unary.op == TOKEN_BANG) {
        return ir_lower_boolean_branch(ctx, expr->as.unary.operand, else_target, then_target);
    }

    if (expr->kind == AST_EXPR_BINARY && expr->as.binary.op == TOKEN_AND_AND) {
        if (!ir_function_append_block(ctx->function, &rhs_block_id, NULL, ctx->error)) {
            return 0;
        }
        if (!ir_lower_boolean_branch(ctx, expr->as.binary.left, rhs_block_id, else_target)) {
            return 0;
        }
        ctx->block_id = rhs_block_id;
        ctx->has_block = 1;
        return ir_lower_boolean_branch(ctx, expr->as.binary.right, then_target, else_target);
    }

    if (expr->kind == AST_EXPR_BINARY && expr->as.binary.op == TOKEN_OR_OR) {
        if (!ir_function_append_block(ctx->function, &rhs_block_id, NULL, ctx->error)) {
            return 0;
        }
        if (!ir_lower_boolean_branch(ctx, expr->as.binary.left, then_target, rhs_block_id)) {
            return 0;
        }
        ctx->block_id = rhs_block_id;
        ctx->has_block = 1;
        return ir_lower_boolean_branch(ctx, expr->as.binary.right, then_target, else_target);
    }

    if (expr->kind == AST_EXPR_TERNARY) {
        size_t then_expr_block_id;
        size_t else_expr_block_id;

        if (!ir_function_append_block(ctx->function, &then_expr_block_id, NULL, ctx->error) ||
            !ir_function_append_block(ctx->function, &else_expr_block_id, NULL, ctx->error) ||
            !ir_lower_boolean_branch(ctx,
                expr->as.ternary.condition,
                then_expr_block_id,
                else_expr_block_id)) {
            return 0;
        }

        ctx->block_id = then_expr_block_id;
        ctx->has_block = 1;
        if (!ir_lower_boolean_branch(ctx, expr->as.ternary.then_expr, then_target, else_target)) {
            return 0;
        }

        ctx->block_id = else_expr_block_id;
        ctx->has_block = 1;
        return ir_lower_boolean_branch(ctx, expr->as.ternary.else_expr, then_target, else_target);
    }

    if (!ir_lower_expression(ctx, expr, &condition_value)) {
        return 0;
    }

    if (ir_expression_is_direct_float_condition(ctx, expr)) {
        size_t temp_id = ir_allocate_temp(ctx->function);
        masked_value = ir_value_temp(temp_id);
        temp_id = ir_allocate_temp(ctx->function);
        compare_value = ir_value_temp(temp_id);

        if (!ir_emit_binary(ctx,
                masked_value,
                IR_BINARY_BIT_AND,
                condition_value,
                ir_value_immediate(0x7fffffffLL)) ||
            !ir_emit_binary(ctx,
                compare_value,
                IR_BINARY_NE,
                masked_value,
                ir_value_immediate(0))) {
            return 0;
        }
        condition_value = compare_value;
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }

    return ir_block_set_branch(block, condition_value, then_target, else_target, ctx->error);
}

static int ir_lower_logical_value_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    size_t result_temp_id;
    IrValueRef result_value;
    size_t true_block_id;
    size_t false_block_id;
    size_t join_block_id;

    if (!ctx || !expr || !out_value || !ctx->function || !ctx->has_block) {
        return 0;
    }

    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);

    if (!ir_function_append_block(ctx->function, &true_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &false_block_id, NULL, ctx->error) ||
        !ir_lower_boolean_branch(ctx, expr, true_block_id, false_block_id) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = true_block_id;
    ctx->has_block = 1;
    if (!ir_emit_mov(ctx, result_value, ir_value_immediate(1)) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = false_block_id;
    ctx->has_block = 1;
    if (!ir_emit_mov(ctx, result_value, ir_value_immediate(0)) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(&ctx->scopes, expr)) {
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-INT-077: failed to clear logical-expression mutated local facts during IR lowering");
        return 0;
    }
    *out_value = result_value;
    return 1;
}

static int ir_lower_ternary_value_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    size_t result_temp_id;
    IrValueRef result_value;
    IrValueRef then_value;
    IrValueRef else_value;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;

    if (!ctx || !expr || !out_value || expr->kind != AST_EXPR_TERNARY || !ctx->function ||
        !ctx->has_block) {
        return 0;
    }

    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);

    if (!ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_lower_boolean_branch(ctx,
            expr->as.ternary.condition,
            then_block_id,
            else_block_id) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_lower_expression(ctx, expr->as.ternary.then_expr, &then_value) ||
        !ir_emit_mov(ctx, result_value, then_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_lower_expression(ctx, expr->as.ternary.else_expr, &else_value) ||
        !ir_emit_mov(ctx, result_value, else_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(&ctx->scopes, expr)) {
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-INT-078: failed to clear ternary-expression mutated local facts during IR lowering");
        return 0;
    }
    *out_value = result_value;
    return 1;
}

static int ir_lower_return_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *expr = NULL;
    IrValueRef value = ir_value_immediate(0);
    IrBasicBlock *block;
    int has_return_expr = 0;
    int has_runtime_counted_function_defer = 0;
    size_t fndefer_index;

    if (!ctx || !stmt) {
        return 0;
    }

    if (stmt->has_primary_expression && stmt->primary_expression_index < stmt->expression_count) {
        expr = stmt->expressions[stmt->primary_expression_index];
    } else if (stmt->expression_count == 1) {
        expr = stmt->expressions[0];
    }
    has_return_expr = expr != NULL;

    if (!has_return_expr) {
        if (ctx->function_return_type != AST_FUNCTION_RETURN_VOID) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-001: return statement without expression is not supported in IR v1");
            return 0;
        }
    } else {
        if (!ir_lower_expression(ctx, expr, &value)) {
            return 0;
        }
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }

    for (fndefer_index = 0u; fndefer_index < ctx->function_defer_entries.count; ++fndefer_index) {
        if (ctx->function_defer_entries.items[fndefer_index].has_runtime_repeat_count) {
            has_runtime_counted_function_defer = 1;
            break;
        }
    }

    if ((ctx->defer_entries.count > 0 || ctx->function_defer_entries.count > 0) &&
        has_return_expr) {
        if (has_runtime_counted_function_defer) {
            size_t local_id = (size_t)-1;

            if (!ir_function_append_local(ctx->function, "__fndefer_ret", 0, &local_id, ctx->error) ||
                !ir_emit_mov(ctx, ir_value_local(local_id), value)) {
                return 0;
            }
            value = ir_value_local(local_id);
        } else if (value.kind == IR_VALUE_LOCAL || value.kind == IR_VALUE_GLOBAL) {
            size_t temp_id = ir_allocate_temp(ctx->function);
            IrValueRef temp_ref = ir_value_temp(temp_id);

            if (!ir_emit_mov(ctx, temp_ref, value)) {
                return 0;
            }
            value = temp_ref;
        }
    }

    if (!ir_lower_emit_defer_entries(ctx, 0u)) {
        return 0;
    }
    if (!ir_lower_emit_function_defer_entries(ctx)) {
        return 0;
    }
    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }
    if (block->has_terminator) {
        return 1;
    }

    if (!has_return_expr) {
        return ir_block_set_void_return(block, ctx->error);
    }

    return ir_block_set_return(block, value, ctx->error);
}

static int ir_call_argument_resolve_extension_function_value_target(const AstProgram *program,
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t arg_index,
    const AstExpression *arg_expr,
    const char **out_target_name) {
    const AstExternal *arg_function = NULL;
    const char *arg_name = NULL;
    size_t external_index;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!program || !callee_external || !arg_expr) {
        return 0;
    }

    if (!callee_external->parameter_value_kinds ||
        arg_index >= callee_external->parameter_count ||
        callee_external->parameter_value_kinds[arg_index] != AST_PARAMETER_VALUE_FUNCTION ||
        !ir_classify_direct_identifier_expr(arg_expr, &arg_name) ||
        !arg_name || arg_name[0] == '\0') {
        return 0;
    }

    if (ctx) {
        const char *bound_target_name = ir_lower_lookup_function_value_target(ctx, arg_name);
        if (bound_target_name && bound_target_name[0] != '\0') {
            for (external_index = 0u; external_index < program->count; ++external_index) {
                const AstExternal *candidate = &program->externals[external_index];

                if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                    candidate->name &&
                    strcmp(candidate->name, bound_target_name) == 0) {
                    arg_function = candidate;
                    break;
                }
            }
        }
    }

    if (!arg_function) {
        for (external_index = 0u; external_index < program->count; ++external_index) {
            const AstExternal *candidate = &program->externals[external_index];

            if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                candidate->name &&
                strcmp(candidate->name, arg_name) == 0) {
                arg_function = candidate;
                break;
            }
        }
    }

    if (!arg_function) {
        size_t expected_param_count = callee_external->parameter_function_parameter_counts
            ? callee_external->parameter_function_parameter_counts[arg_index]
            : 0u;
        AstFunctionReturnType expected_return_type = callee_external->parameter_function_return_types
            ? callee_external->parameter_function_return_types[arg_index]
            : AST_FUNCTION_RETURN_INT;

        if (!ir_builtin_function_matches_signature(arg_name, expected_return_type, expected_param_count)) {
            return 0;
        }

        if (out_target_name) {
            *out_target_name = arg_name;
        }
        return 1;
    }

    if (callee_external->parameter_function_return_types &&
        callee_external->parameter_function_return_types[arg_index] != arg_function->function_return_type) {
        return 0;
    }

    if (callee_external->parameter_function_parameter_counts &&
        callee_external->parameter_function_parameter_counts[arg_index] != arg_function->parameter_count) {
        return 0;
    }

    if (out_target_name) {
        *out_target_name = arg_function->name;
    }
    return 1;
}

static int ir_classify_direct_identifier_expr(const AstExpression *expr, const char **out_name) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (out_name) {
        *out_name = NULL;
    }
    if (!base || base->kind != AST_EXPR_IDENTIFIER) {
        return 0;
    }
    if (out_name) {
        *out_name = base->as.identifier.name;
    }
    return 1;
}

static const char *ir_lower_lookup_function_value_target(const IrLowerContext *ctx, const char *name) {
    size_t index;

    if (!ctx || !name || name[0] == '\0' ||
        !ctx->function_value_parameter_names ||
        !ctx->function_value_parameter_targets) {
        return NULL;
    }

    for (index = 0u; index < ctx->function_value_parameter_count; ++index) {
        if (ctx->function_value_parameter_names[index] &&
            ctx->function_value_parameter_targets[index] &&
            strcmp(ctx->function_value_parameter_names[index], name) == 0) {
            return ctx->function_value_parameter_targets[index];
        }
    }

    return NULL;
}

static IrBinaryOp ir_binary_op_from_token(TokenType token_type, int *out_supported) {
    if (out_supported) {
        *out_supported = 1;
    }

    switch (token_type) {
    case TOKEN_PLUS:
        return IR_BINARY_ADD;
    case TOKEN_MINUS:
        return IR_BINARY_SUB;
    case TOKEN_STAR:
        return IR_BINARY_MUL;
    case TOKEN_SLASH:
        return IR_BINARY_DIV;
    case TOKEN_PERCENT:
        return IR_BINARY_MOD;
    case TOKEN_AMP:
        return IR_BINARY_BIT_AND;
    case TOKEN_CARET:
        return IR_BINARY_BIT_XOR;
    case TOKEN_PIPE:
        return IR_BINARY_BIT_OR;
    case TOKEN_SHIFT_LEFT:
        return IR_BINARY_SHIFT_LEFT;
    case TOKEN_SHIFT_RIGHT:
        return IR_BINARY_SHIFT_RIGHT;
    case TOKEN_EQ:
        return IR_BINARY_EQ;
    case TOKEN_NE:
        return IR_BINARY_NE;
    case TOKEN_LT:
        return IR_BINARY_LT;
    case TOKEN_LE:
        return IR_BINARY_LE;
    case TOKEN_GT:
        return IR_BINARY_GT;
    case TOKEN_GE:
        return IR_BINARY_GE;
    default:
        if (out_supported) {
            *out_supported = 0;
        }
        return IR_BINARY_ADD;
    }
}

static IrBinaryOp ir_binary_op_from_assignment_token(TokenType token_type, int *out_supported) {
    if (out_supported) {
        *out_supported = 1;
    }

    switch (token_type) {
    case TOKEN_PLUS_ASSIGN:
        return IR_BINARY_ADD;
    case TOKEN_MINUS_ASSIGN:
        return IR_BINARY_SUB;
    case TOKEN_STAR_ASSIGN:
        return IR_BINARY_MUL;
    case TOKEN_SLASH_ASSIGN:
        return IR_BINARY_DIV;
    case TOKEN_PERCENT_ASSIGN:
        return IR_BINARY_MOD;
    case TOKEN_AMP_ASSIGN:
        return IR_BINARY_BIT_AND;
    case TOKEN_CARET_ASSIGN:
        return IR_BINARY_BIT_XOR;
    case TOKEN_PIPE_ASSIGN:
        return IR_BINARY_BIT_OR;
    case TOKEN_SHIFT_LEFT_ASSIGN:
        return IR_BINARY_SHIFT_LEFT;
    case TOKEN_SHIFT_RIGHT_ASSIGN:
        return IR_BINARY_SHIFT_RIGHT;
    default:
        if (out_supported) {
            *out_supported = 0;
        }
        return IR_BINARY_ADD;
    }
}

static const AstExpression *ir_unwrap_paren_expression(const AstExpression *expr) {
    const AstExpression *current = expr;

    while (current && current->kind == AST_EXPR_PAREN) {
        current = current->as.inner;
    }

    return current;
}

static const char *ir_extract_direct_call_callee_name(const AstExpression *callee) {
    const AstExpression *base = ir_unwrap_paren_expression(callee);

    if (!base || base->kind != AST_EXPR_IDENTIFIER) {
        return NULL;
    }

    return base->as.identifier.name;
}

static int ir_expression_is_direct_float_condition(const IrLowerContext *ctx,
    const AstExpression *expr) {
    return ir_expression_is_direct_float_transport_root(ctx, expr);
}

static int ir_expression_is_signed_float_literal(const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const AstExpression *operand = NULL;

    if (!base) {
        return 0;
    }
    if (base->kind == AST_EXPR_FLOAT_LITERAL) {
        return 1;
    }
    if (base->kind != AST_EXPR_UNARY ||
        (base->as.unary.op != TOKEN_PLUS && base->as.unary.op != TOKEN_MINUS)) {
        return 0;
    }
    operand = ir_unwrap_paren_expression(base->as.unary.operand);
    return operand && operand->kind == AST_EXPR_FLOAT_LITERAL;
}

static int ir_expression_is_direct_float_transport_root(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!ctx || !base) {
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_UNARY:
        if (base->as.unary.op == TOKEN_PLUS || base->as.unary.op == TOKEN_MINUS) {
            if (ir_expression_is_signed_float_literal(base)) {
                return 1;
            }
            return ir_expression_is_direct_float_transport_root(ctx, base->as.unary.operand);
        }
        return 0;
    case AST_EXPR_IDENTIFIER: {
        IrLowerBindingInfo binding_info;
        IrGlobal *global = NULL;

        if (base->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info)) {
            return binding_info.array_rank == 0u &&
                binding_info.kind == IR_LOWER_BINDING_SCALAR_LOCAL &&
                ctx->function &&
                binding_info.local_id < ctx->function->local_count &&
                ctx->function->locals[binding_info.local_id].value_type == AST_FUNCTION_RETURN_FLOAT;
        }
        if (base->as.identifier.name && ctx->program) {
            global = ir_program_find_global_mut(ctx->program, base->as.identifier.name);
            return global &&
                global->array_rank == 0u &&
                global->value_type == AST_FUNCTION_RETURN_FLOAT;
        }
        return 0;
    }
    case AST_EXPR_CALL: {
        const char *callee_name = ir_extract_direct_call_callee_name(base->as.call.callee);
        IrFunction *callee_function = NULL;

        if (!callee_name || !ctx->program) {
            return 0;
        }
        callee_function = ir_program_find_function_mut(ctx->program, callee_name);
        return callee_function &&
            callee_function->return_type == AST_FUNCTION_RETURN_FLOAT;
    }
    default:
        return 0;
    }
}

static int ir_lower_signed_float_literal_expression(const AstExpression *expr,
    IrValueRef *out_value) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const AstExpression *literal_expr = NULL;
    union {
        float f;
        uint32_t u;
    } bits;

    if (!out_value || !ir_expression_is_signed_float_literal(base)) {
        return 0;
    }

    literal_expr = base;
    if (base->kind == AST_EXPR_UNARY) {
        literal_expr = ir_unwrap_paren_expression(base->as.unary.operand);
    }
    if (!literal_expr || literal_expr->kind != AST_EXPR_FLOAT_LITERAL) {
        return 0;
    }

    bits.f = (float)literal_expr->as.float_number_value;
    if (base->kind == AST_EXPR_UNARY && base->as.unary.op == TOKEN_MINUS) {
        bits.u ^= 0x80000000u;
    }
    *out_value = ir_value_immediate((long long)(uint64_t)bits.u);
    return 1;
}

static int ir_expression_is_float_direct_compare(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!ctx || !base || base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_EQ &&
            base->as.binary.op != TOKEN_NE &&
            base->as.binary.op != TOKEN_LT &&
            base->as.binary.op != TOKEN_LE &&
            base->as.binary.op != TOKEN_GT &&
            base->as.binary.op != TOKEN_GE)) {
        return 0;
    }

    return ir_expression_is_direct_float_condition(ctx, base->as.binary.left) &&
        ir_expression_is_direct_float_condition(ctx, base->as.binary.right);
}

static int ir_expression_is_float_arithmetic(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;
    IrGlobal *global = NULL;
    IrFunction *callee_function = NULL;

    if (!ctx || !base || base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_PLUS &&
            base->as.binary.op != TOKEN_MINUS &&
            base->as.binary.op != TOKEN_STAR &&
            base->as.binary.op != TOKEN_SLASH)) {
        return 0;
    }

    switch (base->kind) {
    default:
        break;
    }

    if (base->as.binary.left) {
        const AstExpression *lhs = ir_unwrap_paren_expression(base->as.binary.left);
        if (lhs && lhs->kind == AST_EXPR_BINARY &&
            ir_expression_is_float_arithmetic(ctx, lhs)) {
            /* allowed */
        } else if (!ir_expression_is_direct_float_transport_root(ctx, base->as.binary.left)) {
            return 0;
        }
    }
    if (base->as.binary.right) {
        const AstExpression *rhs = ir_unwrap_paren_expression(base->as.binary.right);
        if (rhs && rhs->kind == AST_EXPR_BINARY &&
            ir_expression_is_float_arithmetic(ctx, rhs)) {
            /* allowed */
        } else if (!ir_expression_is_direct_float_transport_root(ctx, base->as.binary.right)) {
            return 0;
        }
    }
    (void)binding_info;
    (void)global;
    (void)callee_function;
    return 1;
}

static int ir_lower_float_arithmetic_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    IrValueRef lhs;
    IrValueRef rhs;
    IrValueRef call_args_storage[2];
    const char *helper_name = NULL;
    const char *current_function_name = NULL;
    IrFunction *callee_function = NULL;

    if (!ctx || !expr || !out_value || !ir_expression_is_float_arithmetic(ctx, expr)) {
        return 0;
    }

    switch (expr->as.binary.op) {
    case TOKEN_PLUS:
        helper_name = "__builtin_fadd32";
        break;
    case TOKEN_MINUS:
        helper_name = "__builtin_fsub32";
        break;
    case TOKEN_STAR:
        helper_name = "__builtin_fmul32";
        break;
    case TOKEN_SLASH:
        helper_name = "__builtin_fdiv32";
        break;
    default:
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;
    if (!ir_ensure_builtin_function_signature(ctx->program, helper_name, &callee_function, ctx->error) ||
        !ir_lower_refresh_current_function(ctx, current_function_name, expr->line, expr->column) ||
        !ir_lower_expression(ctx, expr->as.binary.left, &lhs) ||
        !ir_lower_expression(ctx, expr->as.binary.right, &rhs) ||
        !callee_function) {
        return 0;
    }

    call_args_storage[0] = lhs;
    call_args_storage[1] = rhs;
    *out_value = ir_value_temp(ir_allocate_temp(ctx->function));
    return ir_emit_call(ctx, *out_value, helper_name, call_args_storage, 2u);
}

static int ir_lower_float_compare_normalize_zero(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_normalized) {
    IrValueRef masked;
    IrValueRef nonzero;
    size_t temp_id;

    if (!ctx || !out_normalized) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    masked = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    nonzero = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    *out_normalized = ir_value_temp(temp_id);

    return ir_emit_binary(ctx, masked, IR_BINARY_BIT_AND, input, ir_value_immediate(0x7fffffffLL)) &&
        ir_emit_binary(ctx, nonzero, IR_BINARY_NE, masked, ir_value_immediate(0)) &&
        ir_emit_binary(ctx, *out_normalized, IR_BINARY_MUL, input, nonzero);
}

static int ir_lower_float_compare_order_key(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_key) {
    IrValueRef normalized;
    IrValueRef sign_mask;
    IrValueRef order_mask;
    size_t temp_id;

    if (!ctx || !out_key) {
        return 0;
    }
    if (!ir_lower_float_compare_normalize_zero(ctx, input, &normalized)) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    sign_mask = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    order_mask = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    *out_key = ir_value_temp(temp_id);

    return ir_emit_binary(ctx, sign_mask, IR_BINARY_SHIFT_RIGHT, normalized, ir_value_immediate(31)) &&
        ir_emit_binary(ctx, order_mask, IR_BINARY_BIT_OR, sign_mask, ir_value_immediate(0x80000000LL)) &&
        ir_emit_binary(ctx, *out_key, IR_BINARY_BIT_XOR, normalized, order_mask);
}

static int ir_lower_float_compare_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    IrValueRef lhs;
    IrValueRef rhs;
    IrValueRef lhs_key;
    IrValueRef rhs_key;
    IrValueRef lhs_normalized;
    IrValueRef rhs_normalized;
    IrBinaryOp compare_op;
    size_t temp_id;

    if (!ctx || !expr || !out_value || !ir_expression_is_float_direct_compare(ctx, expr)) {
        return 0;
    }
    if (!ir_lower_expression(ctx, expr->as.binary.left, &lhs) ||
        !ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    *out_value = ir_value_temp(temp_id);

    switch (expr->as.binary.op) {
    case TOKEN_EQ:
        compare_op = IR_BINARY_EQ;
        break;
    case TOKEN_NE:
        compare_op = IR_BINARY_NE;
        break;
    case TOKEN_LT:
        compare_op = IR_BINARY_LT;
        break;
    case TOKEN_LE:
        compare_op = IR_BINARY_LE;
        break;
    case TOKEN_GT:
        compare_op = IR_BINARY_GT;
        break;
    case TOKEN_GE:
        compare_op = IR_BINARY_GE;
        break;
    default:
        return 0;
    }

    if (compare_op == IR_BINARY_EQ || compare_op == IR_BINARY_NE) {
        return ir_lower_float_compare_normalize_zero(ctx, lhs, &lhs_normalized) &&
            ir_lower_float_compare_normalize_zero(ctx, rhs, &rhs_normalized) &&
            ir_emit_binary(ctx, *out_value, compare_op, lhs_normalized, rhs_normalized);
    }

    return ir_lower_float_compare_order_key(ctx, lhs, &lhs_key) &&
        ir_lower_float_compare_order_key(ctx, rhs, &rhs_key) &&
        ir_emit_binary(ctx, *out_value, compare_op, lhs_key, rhs_key);
}

typedef struct {
    IrValueRef address;
    size_t array_rank;
    const size_t *array_extents;
} IrLowerArrayAddress;

typedef enum {
    IR_LOWER_SCALAR_LVALUE_SLOT = 0,
    IR_LOWER_SCALAR_LVALUE_INDIRECT,
} IrLowerScalarLValueKind;

typedef struct {
    IrLowerScalarLValueKind kind;
    IrValueRef slot;
    IrValueRef address;
} IrLowerScalarLValue;

static int ir_emit_scaled_offset_add(IrLowerContext *ctx,
    IrValueRef *io_address,
    IrValueRef index_value,
    size_t byte_stride) {
    IrValueRef scaled_index;
    IrValueRef new_address;
    size_t temp_id;

    if (!ctx || !io_address) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    scaled_index = ir_value_temp(temp_id);
    if (!ir_emit_binary(ctx,
            scaled_index,
            IR_BINARY_MUL,
            index_value,
            ir_value_immediate((long long)byte_stride))) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    new_address = ir_value_temp(temp_id);
    if (!ir_emit_binary(ctx,
            new_address,
            IR_BINARY_ADD,
            *io_address,
            scaled_index)) {
        return 0;
    }

    *io_address = new_address;
    return 1;
}

static int ir_lower_array_base_address(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerArrayAddress *out_array) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;
    IrGlobal *global;
    size_t temp_id;

    if (out_array) {
        memset(out_array, 0, sizeof(*out_array));
    }
    if (!ctx || !base || !out_array || base->kind != AST_EXPR_IDENTIFIER ||
        !base->as.identifier.name) {
        return 0;
    }

    if (ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info)) {
        if (binding_info.array_rank == 0u) {
            return 0;
        }
        if (binding_info.kind == IR_LOWER_BINDING_ARRAY_PARAMETER) {
            out_array->address = ir_value_local(binding_info.local_id);
        } else {
            temp_id = ir_allocate_temp(ctx->function);
            out_array->address = ir_value_temp(temp_id);
            if (!ir_emit_addr_local(ctx, out_array->address, binding_info.local_id)) {
                return 0;
            }
        }
        out_array->array_rank = binding_info.array_rank;
        out_array->array_extents = binding_info.array_extents;
        return 1;
    }

    if (!ctx->program) {
        return 0;
    }
    global = ir_program_find_global_mut(ctx->program, base->as.identifier.name);
    if (!global || global->array_rank == 0u) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    out_array->address = ir_value_temp(temp_id);
    if (!ir_emit_addr_global(ctx, out_array->address, global->id)) {
        return 0;
    }
    out_array->array_rank = global->array_rank;
    out_array->array_extents = global->array_extents;
    return 1;
}

static int ir_lower_subscript_address(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerArrayAddress *out_array) {
    const AstExpression *base_expr;
    IrValueRef index_value;
    IrLowerArrayAddress base_array;
    size_t stride_elements = 1u;
    size_t stride_bytes;
    size_t i;

    if (!ctx || !expr || !out_array || expr->kind != AST_EXPR_SUBSCRIPT) {
        return 0;
    }

    base_expr = ir_unwrap_paren_expression(expr->as.subscript.base);
    if (!base_expr) {
        return 0;
    }

    if (base_expr->kind == AST_EXPR_SUBSCRIPT) {
        if (!ir_lower_subscript_address(ctx, base_expr, &base_array)) {
            return 0;
        }
    } else {
        if (!ir_lower_array_base_address(ctx, base_expr, &base_array)) {
            return 0;
        }
    }

    if (base_array.array_rank == 0u) {
        return 0;
    }

    for (i = 1u; i < base_array.array_rank; ++i) {
        if (!base_array.array_extents || base_array.array_extents[i] == 0u) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-032: multidimensional array subscript needs known trailing extents");
            return 0;
        }
        stride_elements *= base_array.array_extents[i];
    }
    stride_bytes = stride_elements * 4u;

    if (!ir_lower_expression(ctx, expr->as.subscript.index, &index_value) ||
        !ir_emit_scaled_offset_add(ctx, &base_array.address, index_value, stride_bytes)) {
        return 0;
    }

    out_array->address = base_array.address;
    out_array->array_rank = base_array.array_rank - 1u;
    out_array->array_extents = base_array.array_extents
        ? base_array.array_extents + 1u
        : NULL;
    return 1;
}

static int ir_extract_lvalue_ref(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_lvalue) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    size_t local_id;
    IrGlobal *global;
    IrLowerBindingInfo binding_info;
    const AstExpression *pair_base;

    if (!ctx || !base || !out_lvalue) {
        return 0;
    }

    if (base->kind == AST_EXPR_MEMBER && base->as.member.field_name) {
        pair_base = ir_unwrap_paren_expression(base->as.member.base);
        if (pair_base && pair_base->kind == AST_EXPR_IDENTIFIER && pair_base->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, pair_base->as.identifier.name, &binding_info)) {
            if (binding_info.kind == IR_LOWER_BINDING_PAIR_LOCAL) {
                if (strcmp(base->as.member.field_name, "first") == 0) {
                    *out_lvalue = ir_value_local(binding_info.local_id);
                    return 1;
                }
                if (strcmp(base->as.member.field_name, "second") == 0) {
                    *out_lvalue = ir_value_local(binding_info.pair_second_local_id);
                    return 1;
                }
            } else if (binding_info.kind == IR_LOWER_BINDING_STRUCT_LOCAL) {
                const AstStructType *struct_type = ir_find_struct_type(ctx->ast_program, binding_info.type_name);
                size_t field_index;

                if (struct_type && struct_type->field_names) {
                    for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
                        if (struct_type->field_names[field_index] &&
                            strcmp(base->as.member.field_name, struct_type->field_names[field_index]) == 0) {
                            *out_lvalue = ir_value_local(binding_info.local_id + field_index);
                            return 1;
                        }
                    }
                }
            }
        }
        return 0;
    }

    if (base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }

    if (ir_lower_scope_lookup(&ctx->scopes, base->as.identifier.name, &local_id)) {
        *out_lvalue = ir_value_local(local_id);
        return 1;
    }

    if (ctx->program) {
        global = ir_program_find_global_mut(ctx->program, base->as.identifier.name);
        if (global) {
            *out_lvalue = ir_value_global(global->id);
            return 1;
        }
    }

    return 0;
}

static int ir_lookup_pair_local_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t *out_first_local_id,
    size_t *out_second_local_id) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;

    if (out_first_local_id) {
        *out_first_local_id = (size_t)-1;
    }
    if (out_second_local_id) {
        *out_second_local_id = (size_t)-1;
    }
    if (!ctx || !base || base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name ||
        !ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info) ||
        binding_info.kind != IR_LOWER_BINDING_PAIR_LOCAL) {
        return 0;
    }

    if (out_first_local_id) {
        *out_first_local_id = binding_info.local_id;
    }
    if (out_second_local_id) {
        *out_second_local_id = binding_info.pair_second_local_id;
    }
    return 1;
}

static const AstStructType *ir_find_struct_type(const AstProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0' || !program->struct_types) {
        return NULL;
    }
    for (i = 0u; i < program->struct_type_count; ++i) {
        if (program->struct_types[i].name &&
            strcmp(program->struct_types[i].name, name) == 0) {
            return &program->struct_types[i];
        }
    }
    return NULL;
}

static int ir_lookup_struct_local_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t *out_first_local_id,
    size_t *out_second_local_id,
    const char **out_type_name) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;

    if (out_first_local_id) {
        *out_first_local_id = (size_t)-1;
    }
    if (out_second_local_id) {
        *out_second_local_id = (size_t)-1;
    }
    if (out_type_name) {
        *out_type_name = NULL;
    }
    if (!ctx || !base || base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name ||
        !ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info) ||
        binding_info.kind != IR_LOWER_BINDING_STRUCT_LOCAL) {
        return 0;
    }

    if (out_first_local_id) {
        *out_first_local_id = binding_info.local_id;
    }
    if (out_second_local_id) {
        *out_second_local_id = binding_info.pair_second_local_id;
    }
    if (out_type_name) {
        *out_type_name = binding_info.type_name;
    }
    return 1;
}

static int ir_lower_scalar_lvalue(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerScalarLValue *out_lvalue) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerArrayAddress array_address;

    if (out_lvalue) {
        memset(out_lvalue, 0, sizeof(*out_lvalue));
    }
    if (!ctx || !base || !out_lvalue) {
        return 0;
    }

    if (base->kind == AST_EXPR_SUBSCRIPT) {
        if (!ir_lower_subscript_address(ctx, base, &array_address) ||
            array_address.array_rank > 0u) {
            return 0;
        }
        out_lvalue->kind = IR_LOWER_SCALAR_LVALUE_INDIRECT;
        out_lvalue->address = array_address.address;
        return 1;
    }

    if (base->kind == AST_EXPR_MEMBER) {
        if (!ir_extract_lvalue_ref(ctx, base, &out_lvalue->slot)) {
            return 0;
        }
        out_lvalue->kind = IR_LOWER_SCALAR_LVALUE_SLOT;
        return 1;
    }

    if (!ir_extract_lvalue_ref(ctx, base, &out_lvalue->slot)) {
        return 0;
    }

    out_lvalue->kind = IR_LOWER_SCALAR_LVALUE_SLOT;
    return 1;
}

static int ir_emit_scalar_lvalue_load(IrLowerContext *ctx,
    const IrLowerScalarLValue *lvalue,
    IrValueRef *out_value) {
    size_t temp_id;

    if (!ctx || !lvalue || !out_value || !ctx->function) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    *out_value = ir_value_temp(temp_id);

    if (lvalue->kind == IR_LOWER_SCALAR_LVALUE_INDIRECT) {
        return ir_emit_load_indirect(ctx, *out_value, lvalue->address);
    }

    return ir_emit_mov(ctx, *out_value, lvalue->slot);
}

static int ir_emit_scalar_lvalue_store(IrLowerContext *ctx,
    const IrLowerScalarLValue *lvalue,
    IrValueRef value) {
    long long known_value = 0;
    int has_known_value = 0;

    if (!ctx || !lvalue) {
        return 0;
    }

    if (lvalue->kind == IR_LOWER_SCALAR_LVALUE_INDIRECT) {
        return ir_emit_store_indirect(ctx, lvalue->address, value);
    }

    if (value.kind == IR_VALUE_IMMEDIATE) {
        has_known_value = 1;
        known_value = value.immediate;
    } else if (value.kind == IR_VALUE_LOCAL && ctx->scopes.count > 0) {
        IrLowerBindingInfo binding_info;

        if (ir_function_get_local(ctx->function, value.id) &&
            ir_lower_scope_lookup_binding(&ctx->scopes,
                ir_function_get_local(ctx->function, value.id)->source_name,
                &binding_info) &&
            binding_info.has_const_value) {
            has_known_value = 1;
            known_value = binding_info.const_value;
        }
    }

    if (!ir_lower_scope_update_binding_const_value_by_local_id(&ctx->scopes,
            lvalue->slot.id,
            has_known_value,
            known_value)) {
        /* Best-effort metadata update only; lowering still succeeds without it. */
    }

    return ir_emit_mov(ctx, lvalue->slot, value);
}

static int ir_lower_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    IrValueRef operand;
    IrValueRef lhs;
    IrValueRef rhs;
    IrValueRef *call_args = NULL;
    const char *callee_name;
    const AstExternal *callee_external = NULL;
    IrFunction *callee_function;
    size_t arg_count;
    size_t i;
    int op_supported;
    IrBinaryOp op;
    size_t temp_id;
    IrLowerScalarLValue scalar_lvalue;

    if (!ctx || !expr || !out_value) {
        ir_set_error(ctx ? ctx->error : NULL,
            expr ? expr->line : 0,
            expr ? expr->column : 0,
            "IR-INT-015: invalid expression lowering contract");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        *out_value = ir_value_immediate(expr->as.number_value);
        return 1;
    case AST_EXPR_FLOAT_LITERAL:
        return ir_lower_signed_float_literal_expression(expr, out_value);
    case AST_EXPR_CONVERSION:
        if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_INT) {
            IrValueRef call_args_storage[1];
            const char *current_function_name = ctx->function ? ctx->function->name : NULL;

            if (!ir_ensure_builtin_function_signature(
                    ctx->program,
                    "__builtin_f2i32",
                    &callee_function,
                    ctx->error) ||
                !ir_lower_refresh_current_function(
                    ctx,
                    current_function_name,
                    expr->line,
                    expr->column) ||
                !ir_lower_expression(ctx, expr->as.conversion.operand, &operand) ||
                !callee_function) {
                return 0;
            }

            call_args_storage[0] = operand;
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_call(ctx, *out_value, "__builtin_f2i32", call_args_storage, 1u);
        } else if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT) {
            IrValueRef call_args_storage[1];
            const char *current_function_name = ctx->function ? ctx->function->name : NULL;

            if (!ir_ensure_builtin_function_signature(
                    ctx->program,
                    "__builtin_i2f32",
                    &callee_function,
                    ctx->error) ||
                !ir_lower_refresh_current_function(
                    ctx,
                    current_function_name,
                    expr->line,
                    expr->column) ||
                !ir_lower_expression(ctx, expr->as.conversion.operand, &operand) ||
                !callee_function) {
                return 0;
            }

            call_args_storage[0] = operand;
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_call(ctx, *out_value, "__builtin_i2f32", call_args_storage, 1u);
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-054: explicit scalar conversion is not lowered for this target type");
        return 0;
    case AST_EXPR_IDENTIFIER: {
        IrLowerBindingInfo binding_info;
        IrGlobal *global = NULL;

        if (expr->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, expr->as.identifier.name, &binding_info)) {
            if (binding_info.kind == IR_LOWER_BINDING_PAIR_LOCAL) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-046: plain pair values are not supported in IR lowering");
                return 0;
            }
            if (binding_info.kind == IR_LOWER_BINDING_STRUCT_LOCAL) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-048: plain struct values are not supported in IR lowering");
                return 0;
            }
            if (binding_info.array_rank > 0u) {
                if (binding_info.kind == IR_LOWER_BINDING_ARRAY_PARAMETER) {
                    *out_value = ir_value_local(binding_info.local_id);
                    return 1;
                }
                {
                    IrLowerArrayAddress array_value;

                    if (!ir_lower_array_base_address(ctx, expr, &array_value)) {
                        return 0;
                    }
                    *out_value = array_value.address;
                    return 1;
                }
            }
            if (binding_info.is_const && binding_info.has_const_value) {
                *out_value = ir_value_immediate(binding_info.const_value);
                return 1;
            }
            *out_value = ir_value_local(binding_info.local_id);
            return 1;
        }
        if (expr->as.identifier.name && ctx->program) {
            global = ir_program_find_global_mut(ctx->program, expr->as.identifier.name);
            if (global && global->array_rank > 0u) {
                IrLowerArrayAddress array_value;

                if (!ir_lower_array_base_address(ctx, expr, &array_value)) {
                    return 0;
                }
                *out_value = array_value.address;
                return 1;
            }
        }
        if (expr->as.identifier.name &&
            ir_extract_lvalue_ref(ctx, expr, out_value)) {
            return 1;
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-002: unresolved identifier '%s' during IR lowering",
            expr->as.identifier.name ? expr->as.identifier.name : "<unnamed>");
        return 0;
    }
    case AST_EXPR_MEMBER:
        if (!ir_lower_scalar_lvalue(ctx, expr, &scalar_lvalue) ||
            !ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, out_value)) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-044: unsupported pair member access during IR lowering");
            return 0;
        }
        return 1;
    case AST_EXPR_PAREN:
        return ir_lower_expression(ctx, expr->as.inner, out_value);
    case AST_EXPR_CALL:
        callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
        if (ir_classify_direct_identifier_expr(expr->as.call.callee, &callee_name) &&
            callee_name && callee_name[0] != '\0') {
            const char *bound_target = ir_lower_lookup_function_value_target(ctx, callee_name);
            if (bound_target && bound_target[0] != '\0') {
                callee_name = bound_target;
            }
        }
        if (!callee_name || callee_name[0] == '\0') {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-013: call callee is not a supported direct identifier in IR v1");
            return 0;
        }

        if (!ctx->program) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-INT-028: call lowering requires program symbol table context");
            return 0;
        }

        callee_function = ir_program_find_function_mut(ctx->program, callee_name);
        if (!callee_function &&
            !ir_ensure_builtin_function_signature(ctx->program, callee_name, &callee_function, ctx->error)) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-025: call target '%s' is not declared in IR lowering context",
                callee_name);
            return 0;
        }

        if (ctx->ast_program) {
            const char *specialized_name = NULL;
            const char **bound_targets = NULL;
            const char *current_function_name = ctx->function ? ctx->function->name : NULL;

            for (i = 0u; i < ctx->ast_program->count; ++i) {
                const AstExternal *candidate = &ctx->ast_program->externals[i];
                if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                    candidate->name &&
                    strcmp(candidate->name, callee_name) == 0) {
                    callee_external = candidate;
                    break;
                }
            }

            if (callee_external &&
                callee_external->parameter_value_kinds &&
                ir_external_body_needs_function_value_specialization(
                    ctx->ast_program, callee_external, callee_external->function_body)) {
                bound_targets = (const char **)calloc(callee_external->parameter_count, sizeof(const char *));
                if (!bound_targets) {
                    ir_set_error(ctx->error,
                        expr->line,
                        expr->column,
                        "IR-INT-029: out of memory while preparing function-value specialization");
                    return 0;
                }
                for (i = 0u; i < callee_external->parameter_count; ++i) {
                    if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
                        if (!ir_call_argument_resolve_extension_function_value_target(
                                ctx->ast_program,
                                ctx,
                                callee_external,
                                i,
                                expr->as.call.args[i],
                                &bound_targets[i])) {
                            free(bound_targets);
                            ir_set_error(ctx->error,
                                expr->line,
                                expr->column,
                                "IR-LOWER-028: failed to resolve function-value call specialization target");
                            return 0;
                        }
                    }
                }
                if (!ir_lower_specialized_function_for_function_value_call(
                        ctx->ast_program,
                        ctx->program,
                        NULL,
                        callee_external,
                        bound_targets,
                        &specialized_name,
                        ctx->error)) {
                    free(bound_targets);
                    return 0;
                }
                free(bound_targets);
                if (current_function_name && current_function_name[0] != '\0') {
                    ctx->function = ir_program_find_function_mut(ctx->program, current_function_name);
                    if (!ctx->function) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-INT-083: current lowering function '%s' disappeared after specialization",
                            current_function_name);
                        return 0;
                    }
                }
                if (specialized_name && specialized_name[0] != '\0') {
                    callee_name = specialized_name;
                    callee_function = ir_program_find_function_mut(ctx->program, callee_name);
                }
            }
        }

        arg_count = expr->as.call.arg_count;
        if (arg_count != callee_function->parameter_count) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-026: call to '%s' has %zu args but callee signature expects %zu",
                callee_name,
                arg_count,
                callee_function->parameter_count);
            return 0;
        }

        if (arg_count > 0) {
            call_args = (IrValueRef *)malloc(arg_count * sizeof(IrValueRef));
            if (!call_args) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-INT-027: out of memory while lowering call arguments");
                return 0;
            }
        }
        for (i = 0; i < arg_count; ++i) {
            if (ctx->ast_program &&
                ir_call_argument_resolve_extension_function_value_target(
                    ctx->ast_program,
                    ctx,
                    callee_external,
                    i,
                    expr->as.call.args[i],
                    NULL)) {
                call_args[i] = ir_value_immediate(0);
                continue;
            }
            if (!ir_lower_expression(ctx, expr->as.call.args[i], &call_args[i])) {
                free(call_args);
                return 0;
            }
        }

        temp_id = ir_allocate_temp(ctx->function);
        *out_value = ir_value_temp(temp_id);
        if (!ir_emit_call(ctx, *out_value, callee_name, call_args, arg_count)) {
            free(call_args);
            return 0;
        }
        free(call_args);
        return 1;
    case AST_EXPR_TERNARY:
        return ir_lower_ternary_value_expression(ctx, expr, out_value);
    case AST_EXPR_UNARY:
        if ((expr->as.unary.op == TOKEN_PLUS || expr->as.unary.op == TOKEN_MINUS) &&
            ir_expression_is_direct_float_transport_root(ctx, expr)) {
            if (expr->as.unary.op == TOKEN_PLUS) {
                return ir_lower_expression(ctx, expr->as.unary.operand, out_value);
            }
            if (ir_expression_is_signed_float_literal(expr)) {
                return ir_lower_signed_float_literal_expression(expr, out_value);
            }
            if (!ir_lower_expression(ctx, expr->as.unary.operand, &operand)) {
                return 0;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_binary(ctx,
                *out_value,
                IR_BINARY_BIT_XOR,
                operand,
                ir_value_immediate(0x80000000LL));
        }
        if (ir_expression_is_signed_float_literal(expr)) {
            return ir_lower_signed_float_literal_expression(expr, out_value);
        }
        if (expr->as.unary.op == TOKEN_PLUS_PLUS || expr->as.unary.op == TOKEN_MINUS_MINUS) {
            IrBinaryOp update_op = expr->as.unary.op == TOKEN_PLUS_PLUS
                ? IR_BINARY_ADD
                : IR_BINARY_SUB;
            IrValueRef current_value;

            if (!ir_lower_scalar_lvalue(ctx, expr->as.unary.operand, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-014: prefix update target is not a supported scalar lvalue");
                return 0;
            }

            if (!ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, &current_value)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            if (!ir_emit_binary(ctx,
                    *out_value,
                    update_op,
                    current_value,
                    ir_value_immediate(1)) ||
                !ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, *out_value)) {
                return 0;
            }
            return 1;
        }
        if (expr->as.unary.op == TOKEN_PLUS) {
            return ir_lower_expression(ctx, expr->as.unary.operand, out_value);
        }
        if (expr->as.unary.op == TOKEN_MINUS) {
            if (!ir_lower_expression(ctx, expr->as.unary.operand, &operand)) {
                return 0;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_binary(ctx,
                *out_value,
                IR_BINARY_SUB,
                ir_value_immediate(0),
                operand);
        }
        if (expr->as.unary.op == TOKEN_TILDE) {
            if (!ir_lower_expression(ctx, expr->as.unary.operand, &operand)) {
                return 0;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_binary(ctx,
                *out_value,
                IR_BINARY_BIT_XOR,
                operand,
                ir_value_immediate(-1));
        }
        if (expr->as.unary.op == TOKEN_BANG) {
            return ir_lower_logical_value_expression(ctx, expr, out_value);
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-003: unary operator '%s' is not supported in IR v1",
            lexer_token_type_name(expr->as.unary.op));
        return 0;
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_COMMA) {
            if (!ir_lower_expression(ctx, expr->as.binary.left, &lhs)) {
                return 0;
            }
            return ir_lower_expression(ctx, expr->as.binary.right, out_value);
        }

        if (expr->as.binary.op == TOKEN_ASSIGN) {
            const AstExpression *lhs_expr = expr->as.binary.left;
            size_t pair_lhs_first_local_id = (size_t)-1;
            size_t pair_lhs_second_local_id = (size_t)-1;
            size_t struct_lhs_first_local_id = (size_t)-1;
            size_t struct_lhs_second_local_id = (size_t)-1;
            const char *struct_lhs_type_name = NULL;

            if (ir_lookup_pair_local_binding(ctx,
                    lhs_expr,
                    &pair_lhs_first_local_id,
                    &pair_lhs_second_local_id)) {
                size_t pair_rhs_first_local_id = (size_t)-1;
                size_t pair_rhs_second_local_id = (size_t)-1;
                const AstExpression *rhs_expr = ir_unwrap_paren_expression(expr->as.binary.right);
                IrValueRef field_value;

                if (ir_lookup_pair_local_binding(ctx,
                        rhs_expr,
                        &pair_rhs_first_local_id,
                        &pair_rhs_second_local_id)) {
                    if (!ir_emit_mov(ctx,
                            ir_value_local(pair_lhs_first_local_id),
                            ir_value_local(pair_rhs_first_local_id)) ||
                        !ir_emit_mov(ctx,
                            ir_value_local(pair_lhs_second_local_id),
                            ir_value_local(pair_rhs_second_local_id))) {
                        return 0;
                    }
                    *out_value = ir_value_immediate(0);
                    return 1;
                }
                if (rhs_expr && rhs_expr->kind == AST_EXPR_INIT_LIST) {
                    if (rhs_expr->as.init_list.item_count > 0u &&
                        rhs_expr->as.init_list.items &&
                        rhs_expr->as.init_list.items[0]) {
                        if (!ir_lower_expression(ctx, rhs_expr->as.init_list.items[0], &field_value) ||
                            !ir_emit_mov(ctx, ir_value_local(pair_lhs_first_local_id), field_value)) {
                            return 0;
                        }
                    }
                    if (rhs_expr->as.init_list.item_count > 1u &&
                        rhs_expr->as.init_list.items &&
                        rhs_expr->as.init_list.items[1]) {
                        if (!ir_lower_expression(ctx, rhs_expr->as.init_list.items[1], &field_value) ||
                            !ir_emit_mov(ctx, ir_value_local(pair_lhs_second_local_id), field_value)) {
                            return 0;
                        }
                    }
                    *out_value = ir_value_immediate(0);
                    return 1;
                }
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-047: pair assignment requires a local pair or initializer list");
                return 0;
            }
            if (ir_lookup_struct_local_binding(ctx,
                    lhs_expr,
                    &struct_lhs_first_local_id,
                    &struct_lhs_second_local_id,
                    &struct_lhs_type_name)) {
                size_t struct_rhs_first_local_id = (size_t)-1;
                size_t struct_rhs_second_local_id = (size_t)-1;
                const char *struct_rhs_type_name = NULL;
                const AstExpression *rhs_expr = ir_unwrap_paren_expression(expr->as.binary.right);
                IrValueRef field_value;

                if (ir_lookup_struct_local_binding(ctx,
                        rhs_expr,
                        &struct_rhs_first_local_id,
                        &struct_rhs_second_local_id,
                        &struct_rhs_type_name)) {
                    const AstStructType *struct_lhs_type = ir_find_struct_type(ctx->ast_program, struct_lhs_type_name);
                    size_t field_index;

                    if (!struct_rhs_type_name || !struct_lhs_type_name ||
                        strcmp(struct_rhs_type_name, struct_lhs_type_name) != 0) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-LOWER-052: struct assignment requires matching struct types");
                        return 0;
                    }
                    if (!struct_lhs_type || !struct_lhs_type->field_names || struct_lhs_type->field_count == 0u) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-LOWER-049: unsupported struct assignment lowering shape");
                        return 0;
                    }
                    for (field_index = 0u; field_index < struct_lhs_type->field_count; ++field_index) {
                        if (!ir_emit_mov(ctx,
                                ir_value_local(struct_lhs_first_local_id + field_index),
                                ir_value_local(struct_rhs_first_local_id + field_index))) {
                            return 0;
                        }
                    }
                    *out_value = ir_value_immediate(0);
                    return 1;
                }
                if (rhs_expr && rhs_expr->kind == AST_EXPR_INIT_LIST) {
                    const AstStructType *struct_lhs_type = ir_find_struct_type(ctx->ast_program, struct_lhs_type_name);
                    size_t field_index;

                    if (!struct_lhs_type || !struct_lhs_type->field_names) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-LOWER-049: unsupported struct assignment lowering shape");
                        return 0;
                    }
                    if (rhs_expr->as.init_list.item_count > struct_lhs_type->field_count) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-LOWER-051: struct initializers currently require an initializer list");
                        return 0;
                    }
                    for (field_index = 0u;
                         field_index < rhs_expr->as.init_list.item_count &&
                         field_index < struct_lhs_type->field_count;
                         ++field_index) {
                        if (rhs_expr->as.init_list.items &&
                            rhs_expr->as.init_list.items[field_index] &&
                            (!ir_lower_expression(ctx, rhs_expr->as.init_list.items[field_index], &field_value) ||
                                !ir_emit_mov(ctx,
                                    ir_value_local(struct_lhs_first_local_id + field_index),
                                    field_value))) {
                            return 0;
                        }
                    }
                    for (; field_index < struct_lhs_type->field_count; ++field_index) {
                        if (!ir_emit_mov(ctx,
                                ir_value_local(struct_lhs_first_local_id + field_index),
                                ir_value_immediate(0))) {
                            return 0;
                        }
                    }
                    *out_value = ir_value_immediate(0);
                    return 1;
                }
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-053: struct assignment requires a local struct or initializer list");
                return 0;
            }

            if (!ir_lower_scalar_lvalue(ctx, lhs_expr, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-004: assignment target is not a supported scalar lvalue");
                return 0;
            }
            if (!ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
                return 0;
            }
            *out_value = rhs;
            return ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, rhs);
        }

        op = ir_binary_op_from_assignment_token(expr->as.binary.op, &op_supported);
        if (op_supported) {
            IrValueRef current_value;

            if (!ir_lower_scalar_lvalue(ctx, expr->as.binary.left, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-015: compound assignment target is not a supported scalar lvalue");
                return 0;
            }
            if (!ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, &current_value)) {
                return 0;
            }
            if (!ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            lhs = current_value;
            *out_value = ir_value_temp(temp_id);
            if (!ir_emit_binary(ctx, *out_value, op, lhs, rhs) ||
                !ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, *out_value)) {
                return 0;
            }
            return 1;
        }

        if (expr->as.binary.op == TOKEN_AND_AND || expr->as.binary.op == TOKEN_OR_OR) {
            return ir_lower_logical_value_expression(ctx, expr, out_value);
        }
        if (ir_expression_is_float_arithmetic(ctx, expr)) {
            return ir_lower_float_arithmetic_expression(ctx, expr, out_value);
        }
        if (ir_expression_is_float_direct_compare(ctx, expr)) {
            return ir_lower_float_compare_expression(ctx, expr, out_value);
        }

        op = ir_binary_op_from_token(expr->as.binary.op, &op_supported);
        if (!op_supported) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-005: binary operator '%s' is not supported in IR v1",
                lexer_token_type_name(expr->as.binary.op));
            return 0;
        }

        if (!ir_lower_expression(ctx, expr->as.binary.left, &lhs) ||
            !ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
            return 0;
        }

        temp_id = ir_allocate_temp(ctx->function);
        *out_value = ir_value_temp(temp_id);
        return ir_emit_binary(ctx, *out_value, op, lhs, rhs);
    case AST_EXPR_POSTFIX:
        if (expr->as.postfix.op == TOKEN_PLUS_PLUS || expr->as.postfix.op == TOKEN_MINUS_MINUS) {
            IrBinaryOp update_op = expr->as.postfix.op == TOKEN_PLUS_PLUS
                ? IR_BINARY_ADD
                : IR_BINARY_SUB;
            IrValueRef original_value;
            IrValueRef updated_value;
            IrValueRef current_value;

            if (!ir_lower_scalar_lvalue(ctx, expr->as.postfix.operand, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-016: postfix update target is not a supported scalar lvalue");
                return 0;
            }

            if (!ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, &current_value)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            original_value = ir_value_temp(temp_id);
            if (!ir_emit_mov(ctx, original_value, current_value)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            updated_value = ir_value_temp(temp_id);
            if (!ir_emit_binary(ctx,
                    updated_value,
                    update_op,
                    current_value,
                    ir_value_immediate(1)) ||
                !ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, updated_value)) {
                return 0;
            }

            *out_value = original_value;
            return 1;
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-017: postfix operator '%s' is not supported in IR v1",
            lexer_token_type_name(expr->as.postfix.op));
        return 0;
    case AST_EXPR_SUBSCRIPT:
        {
            IrLowerArrayAddress array_address;

            if (!ir_lower_subscript_address(ctx, expr, &array_address)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-018: array subscript expressions are not supported in IR v1 yet");
                return 0;
            }
            if (array_address.array_rank > 0u) {
                *out_value = array_address.address;
                return 1;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_load_indirect(ctx, *out_value, array_address.address);
        }
    default:
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-006: expression kind '%s' is not supported in IR v1",
            ast_expression_kind_name(expr->kind));
        return 0;
    }
}

#endif
