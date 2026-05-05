#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_lower_boolean_branch(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t then_target,
    size_t else_target) {
    IrValueRef condition_value;
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
    *out_value = result_value;
    return 1;
}

static int ir_lower_return_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *expr = NULL;
    IrValueRef value = ir_value_immediate(0);
    IrBasicBlock *block;

    if (!ctx || !stmt) {
        return 0;
    }

    if (stmt->has_primary_expression && stmt->primary_expression_index < stmt->expression_count) {
        expr = stmt->expressions[stmt->primary_expression_index];
    } else if (stmt->expression_count == 1) {
        expr = stmt->expressions[0];
    }

    if (!expr) {
        if (ctx->function_return_type != AST_FUNCTION_RETURN_VOID) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-001: return statement without expression is not supported in IR v1");
            return 0;
        }
        block = ir_lower_current_block(ctx);
        if (!block) {
            return 0;
        }
        return ir_block_set_void_return(block, ctx->error);
    } else {
        if (!ir_lower_expression(ctx, expr, &value)) {
            return 0;
        }
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }

    return ir_block_set_return(block, value, ctx->error);
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

    if (!ctx || !base || !out_lvalue || base->kind != AST_EXPR_IDENTIFIER ||
        !base->as.identifier.name) {
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
    if (!ctx || !lvalue) {
        return 0;
    }

    if (lvalue->kind == IR_LOWER_SCALAR_LVALUE_INDIRECT) {
        return ir_emit_store_indirect(ctx, lvalue->address, value);
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
    case AST_EXPR_IDENTIFIER: {
        IrLowerBindingInfo binding_info;

        if (expr->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, expr->as.identifier.name, &binding_info)) {
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
            *out_value = ir_value_local(binding_info.local_id);
            return 1;
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
    case AST_EXPR_PAREN:
        return ir_lower_expression(ctx, expr->as.inner, out_value);
    case AST_EXPR_CALL:
        callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
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
