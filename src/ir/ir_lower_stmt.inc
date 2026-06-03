#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_dependency_try_eval_condition_truthiness(const AstExpression *expr,
    int *out_known,
    int *out_truthy);
static int ir_try_eval_condition_truthiness_with_scope(const IrLowerContext *ctx,
    const AstExpression *expr,
    int *out_known,
    int *out_truthy);
static const AstExpression *statement_get_condition_expression(const AstStatement *stmt);
static int ir_statement_contains_nested_loop(const AstStatement *stmt);
static int ir_expression_has_side_effects(const AstExpression *expr);
static int ir_prepare_condition_scope_snapshot(const IrLowerScopeStack *src_scopes,
    const AstExpression *condition_expr,
    int condition_has_side_effects,
    IrLowerScopeStack *dest_scopes);
static int ir_lower_scope_clear_mutated_scalar_locals_in_expr(IrLowerScopeStack *stack,
    const AstExpression *expr);
static int ir_lower_scope_clear_mutated_scalar_locals_in_statement(IrLowerScopeStack *stack,
    const AstStatement *stmt);
static int ir_try_build_identifier_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor);
typedef struct IrLowerReturnedFunctionValueFamilyInfo IrLowerReturnedFunctionValueFamilyInfo;
static int ir_try_analyze_returned_function_value_family(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrLowerReturnedFunctionValueFamilyInfo *out_info);

static const AstExpression *statement_get_condition_expression(const AstStatement *stmt) {
    if (!stmt || !stmt->has_condition_expression) {
        return NULL;
    }

    if (stmt->condition_expression_index >= stmt->expression_count) {
        return NULL;
    }

    return stmt->expressions[stmt->condition_expression_index];
}

typedef struct {
    const char *names[256];
    size_t lengths[256];
    unsigned char known[256];
    long long values[256];
    size_t count;
    size_t scope_offsets[256];
    size_t scope_count;
    int overflowed;
} IrLowerFlowState;

typedef struct {
    int may_fallthrough;
    int may_break;
    int may_continue;
    int may_return;
} IrLowerLoopTransferInfo;

static void ir_lower_flow_state_init(IrLowerFlowState *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
}

static int ir_lower_flow_state_push_scope(IrLowerFlowState *state) {
    if (!state || state->overflowed) {
        return 0;
    }

    if (state->scope_count >= sizeof(state->scope_offsets) / sizeof(state->scope_offsets[0])) {
        state->overflowed = 1;
        return 0;
    }

    state->scope_offsets[state->scope_count++] = state->count;
    return 1;
}

static void ir_lower_flow_state_pop_scope(IrLowerFlowState *state) {
    if (!state || state->scope_count == 0) {
        return;
    }

    state->count = state->scope_offsets[--state->scope_count];
}

static size_t ir_lower_flow_state_resolve_binding(const IrLowerFlowState *state,
    const char *name,
    size_t name_length) {
    size_t i;

    if (!state || !name || name_length == 0) {
        return 0;
    }

    for (i = state->count; i > 0; --i) {
        size_t index = i - 1;

        if (state->lengths[index] == name_length &&
            strncmp(state->names[index], name, name_length) == 0) {
            return index + 1u;
        }
    }

    return 0;
}

static int ir_lower_flow_state_add_name(IrLowerFlowState *state,
    const char *name,
    size_t name_length) {
    if (!state || !name || name_length == 0 || state->overflowed) {
        return 0;
    }

    if (state->count >= sizeof(state->names) / sizeof(state->names[0])) {
        state->overflowed = 1;
        return 0;
    }

    state->names[state->count] = name;
    state->lengths[state->count] = name_length;
    state->known[state->count] = 0u;
    state->values[state->count] = 0;
    state->count++;
    return 1;
}

static int ir_lower_flow_state_eval_expr(const IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerFlowState *state,
    long long *out_value);

static int ir_lower_flow_state_eval_mutating_identifier_update(const IrLowerContext *ctx,
    IrLowerFlowState *state,
    const char *name,
    size_t name_length,
    TokenType op,
    const AstExpression *rhs_expr,
    int return_original_value,
    long long *out_value) {
    size_t index;
    long long lhs = 0;
    long long rhs = 0;
    long long result = 0;
    int32_t lhs32;
    int32_t rhs32;

    (void)ctx;

    if (!state || !name || name_length == 0 || !out_value) {
        return 0;
    }

    index = ir_lower_flow_state_resolve_binding(state, name, name_length);
    if (index == 0u) {
        return 0;
    }

    if (!state->known[index - 1u]) {
        state->known[index - 1u] = 0u;
        return 0;
    }
    lhs = state->values[index - 1u];
    lhs32 = (int32_t)ir_normalize_sysy_int_value(lhs);

    switch (op) {
    case TOKEN_PLUS_PLUS:
        result = ir_normalize_sysy_int_value((long long)lhs32 + 1ll);
        break;
    case TOKEN_MINUS_MINUS:
        result = ir_normalize_sysy_int_value((long long)lhs32 - 1ll);
        break;
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
        if (!rhs_expr || !ir_lower_flow_state_eval_expr(ctx, rhs_expr, state, &rhs)) {
            state->known[index - 1u] = 0u;
            return 0;
        }
        rhs32 = (int32_t)ir_normalize_sysy_int_value(rhs);
        switch (op) {
        case TOKEN_PLUS_ASSIGN:
            result = ir_normalize_sysy_int_value((long long)lhs32 + (long long)rhs32);
            break;
        case TOKEN_MINUS_ASSIGN:
            result = ir_normalize_sysy_int_value((long long)lhs32 - (long long)rhs32);
            break;
        case TOKEN_STAR_ASSIGN:
            result = ir_normalize_sysy_int_value((long long)lhs32 * (long long)rhs32);
            break;
        case TOKEN_SLASH_ASSIGN:
            if (rhs32 == 0) {
                state->known[index - 1u] = 0u;
                return 0;
            }
            result = ir_normalize_sysy_int_value((long long)lhs32 / (long long)rhs32);
            break;
        case TOKEN_PERCENT_ASSIGN:
            if (rhs32 == 0) {
                state->known[index - 1u] = 0u;
                return 0;
            }
            result = ir_normalize_sysy_int_value((long long)lhs32 % (long long)rhs32);
            break;
        case TOKEN_AMP_ASSIGN:
            result = ir_normalize_sysy_int_value((long long)(lhs32 & rhs32));
            break;
        case TOKEN_CARET_ASSIGN:
            result = ir_normalize_sysy_int_value((long long)(lhs32 ^ rhs32));
            break;
        case TOKEN_PIPE_ASSIGN:
            result = ir_normalize_sysy_int_value((long long)(lhs32 | rhs32));
            break;
        case TOKEN_SHIFT_LEFT_ASSIGN:
            if (rhs32 < 0 || rhs32 >= 32) {
                state->known[index - 1u] = 0u;
                return 0;
            }
            result = ir_normalize_sysy_int_value((long long)((uint32_t)lhs32 << (uint32_t)rhs32));
            break;
        case TOKEN_SHIFT_RIGHT_ASSIGN:
            if (rhs32 < 0 || rhs32 >= 32) {
                state->known[index - 1u] = 0u;
                return 0;
            }
            result = ir_normalize_sysy_int_value((long long)(lhs32 >> rhs32));
            break;
        default:
            state->known[index - 1u] = 0u;
            return 0;
        }
        break;
    default:
        return 0;
    }

    state->known[index - 1u] = 1u;
    state->values[index - 1u] = result;
    *out_value = return_original_value ? lhs : result;
    return 1;
}

static int ir_lower_flow_state_eval_binary(const IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerFlowState *state,
    long long *out_value) {
    long long lhs = 0;
    long long rhs = 0;
    int32_t lhs32;
    int32_t rhs32;

    if (!ir_lower_flow_state_eval_expr(ctx, expr->as.binary.left, state, &lhs) ||
        !ir_lower_flow_state_eval_expr(ctx, expr->as.binary.right, state, &rhs)) {
        return 0;
    }

    lhs32 = (int32_t)ir_normalize_sysy_int_value(lhs);
    rhs32 = (int32_t)ir_normalize_sysy_int_value(rhs);

    switch (expr->as.binary.op) {
    case TOKEN_COMMA:
        *out_value = ir_normalize_sysy_int_value(rhs);
        return 1;
    case TOKEN_EQ:
        *out_value = lhs32 == rhs32 ? 1 : 0;
        return 1;
    case TOKEN_NE:
        *out_value = lhs32 != rhs32 ? 1 : 0;
        return 1;
    case TOKEN_LT:
        *out_value = lhs32 < rhs32 ? 1 : 0;
        return 1;
    case TOKEN_LE:
        *out_value = lhs32 <= rhs32 ? 1 : 0;
        return 1;
    case TOKEN_GT:
        *out_value = lhs32 > rhs32 ? 1 : 0;
        return 1;
    case TOKEN_GE:
        *out_value = lhs32 >= rhs32 ? 1 : 0;
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
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 / (long long)rhs32);
        return 1;
    case TOKEN_PERCENT:
        if (rhs32 == 0) {
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 % (long long)rhs32);
        return 1;
    case TOKEN_SHIFT_LEFT:
        if (rhs32 < 0 || rhs32 >= 32) {
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)((uint32_t)lhs32 << (uint32_t)rhs32));
        return 1;
    case TOKEN_SHIFT_RIGHT:
        if (rhs32 < 0 || rhs32 >= 32) {
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 >> rhs32));
        return 1;
    case TOKEN_AND_AND:
        *out_value = (lhs32 != 0 && rhs32 != 0) ? 1 : 0;
        return 1;
    case TOKEN_OR_OR:
        *out_value = (lhs32 != 0 || rhs32 != 0) ? 1 : 0;
        return 1;
    default:
        return 0;
    }
}

static int ir_lower_flow_state_eval_expr(const IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerFlowState *state,
    long long *out_value) {
    const AstExpression *base;
    long long operand = 0;
    long long cond = 0;
    size_t index;

    if (!expr || !out_value) {
        return 0;
    }

    base = expr;
    while (base && base->kind == AST_EXPR_PAREN) {
        base = base->as.inner;
    }
    if (!base) {
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_NUMBER:
        *out_value = ir_normalize_sysy_int_value(base->as.number_value);
        return 1;
    case AST_EXPR_FLOAT_LITERAL:
        return 0;
    case AST_EXPR_IDENTIFIER:
        index = ir_lower_flow_state_resolve_binding(state,
            base->as.identifier.name,
            base->as.identifier.name_length);
        if (index > 0u && state->known[index - 1u]) {
            *out_value = state->values[index - 1u];
            return 1;
        }
        return 0;
    case AST_EXPR_UNARY:
        if ((base->as.unary.op == TOKEN_PLUS_PLUS || base->as.unary.op == TOKEN_MINUS_MINUS) &&
            base->as.unary.operand &&
            base->as.unary.operand->kind == AST_EXPR_IDENTIFIER) {
            return ir_lower_flow_state_eval_mutating_identifier_update(ctx,
                state,
                base->as.unary.operand->as.identifier.name,
                base->as.unary.operand->as.identifier.name_length,
                base->as.unary.op,
                NULL,
                0,
                out_value);
        }
        if (!ir_lower_flow_state_eval_expr(ctx, base->as.unary.operand, state, &operand)) {
            return 0;
        }
        switch (base->as.unary.op) {
        case TOKEN_PLUS:
            *out_value = ir_normalize_sysy_int_value(operand);
            return 1;
        case TOKEN_MINUS:
            *out_value = ir_normalize_sysy_int_value(-(long long)(int32_t)ir_normalize_sysy_int_value(operand));
            return 1;
        case TOKEN_BANG:
            *out_value = operand == 0 ? 1 : 0;
            return 1;
        case TOKEN_TILDE:
            *out_value = ir_normalize_sysy_int_value((long long)(~(int32_t)ir_normalize_sysy_int_value(operand)));
            return 1;
        default:
            return 0;
        }
    case AST_EXPR_BINARY:
        if (base->as.binary.op == TOKEN_ASSIGN) {
            index = ir_lower_flow_state_resolve_binding(state,
                base->as.binary.left && base->as.binary.left->kind == AST_EXPR_IDENTIFIER
                    ? base->as.binary.left->as.identifier.name
                    : NULL,
                base->as.binary.left && base->as.binary.left->kind == AST_EXPR_IDENTIFIER
                    ? base->as.binary.left->as.identifier.name_length
                    : 0u);
            if (index == 0u) {
                return 0;
            }
            if (!ir_lower_flow_state_eval_expr(ctx, base->as.binary.right, state, &operand)) {
                state->known[index - 1u] = 0u;
                return 0;
            }
            state->known[index - 1u] = 1u;
            state->values[index - 1u] = ir_normalize_sysy_int_value(operand);
            *out_value = state->values[index - 1u];
            return 1;
        }
        if ((base->as.binary.op == TOKEN_PLUS_ASSIGN ||
                base->as.binary.op == TOKEN_MINUS_ASSIGN ||
                base->as.binary.op == TOKEN_STAR_ASSIGN ||
                base->as.binary.op == TOKEN_SLASH_ASSIGN ||
                base->as.binary.op == TOKEN_PERCENT_ASSIGN ||
                base->as.binary.op == TOKEN_AMP_ASSIGN ||
                base->as.binary.op == TOKEN_CARET_ASSIGN ||
                base->as.binary.op == TOKEN_PIPE_ASSIGN ||
                base->as.binary.op == TOKEN_SHIFT_LEFT_ASSIGN ||
                base->as.binary.op == TOKEN_SHIFT_RIGHT_ASSIGN) &&
            base->as.binary.left &&
            base->as.binary.left->kind == AST_EXPR_IDENTIFIER) {
            return ir_lower_flow_state_eval_mutating_identifier_update(ctx,
                state,
                base->as.binary.left->as.identifier.name,
                base->as.binary.left->as.identifier.name_length,
                base->as.binary.op,
                base->as.binary.right,
                0,
                out_value);
        }
        if (base->as.binary.op == TOKEN_AND_AND) {
            if (!ir_lower_flow_state_eval_expr(ctx, base->as.binary.left, state, &cond)) {
                return 0;
            }
            if (cond == 0) {
                *out_value = 0;
                return 1;
            }
            if (!ir_lower_flow_state_eval_expr(ctx, base->as.binary.right, state, &operand)) {
                return 0;
            }
            *out_value = operand != 0 ? 1 : 0;
            return 1;
        }
        if (base->as.binary.op == TOKEN_OR_OR) {
            if (!ir_lower_flow_state_eval_expr(ctx, base->as.binary.left, state, &cond)) {
                return 0;
            }
            if (cond != 0) {
                *out_value = 1;
                return 1;
            }
            if (!ir_lower_flow_state_eval_expr(ctx, base->as.binary.right, state, &operand)) {
                return 0;
            }
            *out_value = operand != 0 ? 1 : 0;
            return 1;
        }
        return ir_lower_flow_state_eval_binary(ctx, base, state, out_value);
    case AST_EXPR_POSTFIX:
        if ((base->as.postfix.op == TOKEN_PLUS_PLUS || base->as.postfix.op == TOKEN_MINUS_MINUS) &&
            base->as.postfix.operand &&
            base->as.postfix.operand->kind == AST_EXPR_IDENTIFIER) {
            return ir_lower_flow_state_eval_mutating_identifier_update(ctx,
                state,
                base->as.postfix.operand->as.identifier.name,
                base->as.postfix.operand->as.identifier.name_length,
                base->as.postfix.op,
                NULL,
                1,
                out_value);
        }
        return 0;
    case AST_EXPR_TERNARY:
        if (!ir_lower_flow_state_eval_expr(ctx, base->as.ternary.condition, state, &cond)) {
            return 0;
        }
        return ir_lower_flow_state_eval_expr(ctx,
            cond != 0 ? base->as.ternary.then_expr : base->as.ternary.else_expr,
            state,
            out_value);
    default:
        return 0;
    }
}

static int ir_lower_flow_state_process_statement(const IrLowerContext *ctx,
    const AstStatement *stmt,
    IrLowerFlowState *state,
    int *out_may_fallthrough);
static int ir_lower_flow_state_contains_unknown_control(const IrLowerContext *ctx,
    const AstStatement *stmt,
    IrLowerFlowState *state,
    int *out_has_unknown_control);
static int ir_lower_flow_state_analyze_loop_transfer(const IrLowerContext *ctx,
    const AstStatement *stmt,
    IrLowerFlowState *state,
    IrLowerLoopTransferInfo *out_info);
static int ir_lower_flow_state_import_scope(const IrLowerContext *ctx,
    IrLowerFlowState *state);
static int ir_lower_flow_state_proves_loop_condition_stays_true(const IrLowerContext *ctx,
    const AstStatement *loop_stmt);

static int ir_lower_flow_state_equals(const IrLowerFlowState *lhs,
    const IrLowerFlowState *rhs) {
    size_t i;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->count != rhs->count || lhs->scope_count != rhs->scope_count) {
        return 0;
    }
    for (i = 0; i < lhs->count; ++i) {
        if (lhs->names[i] != rhs->names[i] ||
            lhs->lengths[i] != rhs->lengths[i] ||
            lhs->known[i] != rhs->known[i] ||
            lhs->values[i] != rhs->values[i]) {
            return 0;
        }
    }
    for (i = 0; i < lhs->scope_count; ++i) {
        if (lhs->scope_offsets[i] != rhs->scope_offsets[i]) {
            return 0;
        }
    }
    return 1;
}

static void ir_lower_flow_state_clear_known(IrLowerFlowState *state) {
    size_t i;

    if (!state) {
        return;
    }

    for (i = 0; i < state->count; ++i) {
        state->known[i] = 0u;
    }
}

static int ir_lower_flow_state_merge_known(IrLowerFlowState *dest,
    const IrLowerFlowState *lhs,
    const IrLowerFlowState *rhs) {
    size_t i;

    if (!dest || !lhs || !rhs) {
        return 0;
    }

    *dest = *lhs;
    if (lhs->count != rhs->count || lhs->scope_count != rhs->scope_count) {
        ir_lower_flow_state_clear_known(dest);
        return 1;
    }
    for (i = 0; i < lhs->scope_count; ++i) {
        if (lhs->scope_offsets[i] != rhs->scope_offsets[i]) {
            ir_lower_flow_state_clear_known(dest);
            return 1;
        }
    }
    for (i = 0; i < lhs->count; ++i) {
        if (lhs->names[i] != rhs->names[i] || lhs->lengths[i] != rhs->lengths[i]) {
            ir_lower_flow_state_clear_known(dest);
            return 1;
        }
        if (!(lhs->known[i] && rhs->known[i] && lhs->values[i] == rhs->values[i])) {
            dest->known[i] = 0u;
        }
    }
    return 1;
}

static int ir_lower_flow_state_import_scope(const IrLowerContext *ctx,
    IrLowerFlowState *state) {
    size_t frame_index;

    if (!ctx || !state) {
        return 0;
    }

    for (frame_index = 0; frame_index < ctx->scopes.count; ++frame_index) {
        const IrLowerScope *frame = &ctx->scopes.frames[frame_index];
        size_t binding_index;

        if (!ir_lower_flow_state_push_scope(state)) {
            return 0;
        }
        for (binding_index = 0; binding_index < frame->count; ++binding_index) {
            const IrLowerBinding *binding = &frame->bindings[binding_index];

            if (!binding->name || binding->kind != IR_LOWER_BINDING_SCALAR_LOCAL) {
                continue;
            }
            if (!ir_lower_flow_state_add_name(state, binding->name, strlen(binding->name))) {
                return 0;
            }
            if (binding->has_const_value) {
                state->known[state->count - 1u] = 1u;
                state->values[state->count - 1u] = ir_normalize_sysy_int_value(binding->const_value);
            }
        }
    }

    return 1;
}

static int ir_lower_flow_state_proves_loop_condition_stays_true_from_state(const IrLowerContext *ctx,
    const AstStatement *loop_stmt,
    const IrLowerFlowState *base_state) {
    const AstExpression *cond_expr;
    IrLowerFlowState state;
    size_t iteration;

    if (!ctx || !loop_stmt) {
        return 0;
    }

    cond_expr = statement_get_condition_expression(loop_stmt);
    if (!cond_expr && !(loop_stmt->kind == AST_STMT_FOR && !loop_stmt->has_condition_expression)) {
        return 0;
    }
    if (cond_expr && ir_expression_has_side_effects(cond_expr)) {
        return 0;
    }

    if (base_state) {
        state = *base_state;
    } else {
        ir_lower_flow_state_init(&state);
        if (!ir_lower_flow_state_import_scope(ctx, &state)) {
            return 0;
        }
    }
    for (iteration = 0; iteration < 16u; ++iteration) {
        IrLowerFlowState before_state = state;
        IrLowerLoopTransferInfo body_info = {1, 0, 0, 0};
        int body_may_fallthrough = 1;
        int has_unknown_control = 0;
        long long cond_truthy = 1;

        if (loop_stmt->kind == AST_STMT_FOR && iteration == 0u &&
            loop_stmt->declaration_name_count == 0u &&
            loop_stmt->has_for_init_expression &&
            loop_stmt->for_init_expression_index < loop_stmt->expression_count &&
            loop_stmt->expressions && loop_stmt->expressions[loop_stmt->for_init_expression_index]) {
            long long ignored = 0;

            (void)ir_lower_flow_state_eval_expr(ctx,
                loop_stmt->expressions[loop_stmt->for_init_expression_index],
                &state,
                &ignored);
        }

        if (cond_expr) {
            if (!ir_lower_flow_state_eval_expr(ctx, cond_expr, &state, &cond_truthy)) {
                return 0;
            }
            if (cond_truthy == 0) {
                return 0;
            }
        }

        if (loop_stmt->child_count >= 1) {
            IrLowerFlowState body_state = state;

            if (!ir_lower_flow_state_contains_unknown_control(
                    ctx, loop_stmt->children[0], &body_state, &has_unknown_control)) {
                return 0;
            }
            if (has_unknown_control) {
                return 0;
            }
            if (!ir_lower_flow_state_analyze_loop_transfer(ctx, loop_stmt->children[0], &body_state, &body_info) ||
                !ir_lower_flow_state_process_statement(ctx, loop_stmt->children[0], &state, &body_may_fallthrough)) {
                return 0;
            }
        }

        if (body_info.may_break) {
            return 0;
        }

        if (body_may_fallthrough && loop_stmt->kind == AST_STMT_FOR &&
            loop_stmt->has_for_step_expression &&
            loop_stmt->for_step_expression_index < loop_stmt->expression_count &&
            loop_stmt->expressions && loop_stmt->expressions[loop_stmt->for_step_expression_index]) {
            long long ignored = 0;

            (void)ir_lower_flow_state_eval_expr(ctx,
                loop_stmt->expressions[loop_stmt->for_step_expression_index],
                &state,
                &ignored);
        }

        if (ir_lower_flow_state_equals(&before_state, &state)) {
            if (!cond_expr) {
                return body_info.may_break ? 0 : 1;
            }
            if (!ir_lower_flow_state_eval_expr(ctx, cond_expr, &state, &cond_truthy)) {
                return 0;
            }
            return (cond_truthy != 0) && !body_info.may_break;
        }

    }

    return 0;
}

static int ir_lower_flow_state_proves_loop_condition_stays_true(const IrLowerContext *ctx,
    const AstStatement *loop_stmt) {
    return ir_lower_flow_state_proves_loop_condition_stays_true_from_state(ctx, loop_stmt, NULL);
}

static int ir_lower_flow_state_analyze_loop_transfer(const IrLowerContext *ctx,
    const AstStatement *stmt,
    IrLowerFlowState *state,
    IrLowerLoopTransferInfo *out_info) {
    size_t i;

    if (!out_info) {
        return 0;
    }

    out_info->may_fallthrough = 1;
    out_info->may_break = 0;
    out_info->may_continue = 0;
    out_info->may_return = 0;

    if (!stmt || !state || state->overflowed) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND: {
        IrLowerFlowState local = *state;
        int may_fallthrough = 1;

        if (!ir_lower_flow_state_push_scope(&local)) {
            return 0;
        }
        for (i = 0; i < stmt->child_count && may_fallthrough; ++i) {
            IrLowerLoopTransferInfo child_info;

            if (!ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[i], &local, &child_info)) {
                return 0;
            }
            out_info->may_break = out_info->may_break || child_info.may_break;
            out_info->may_continue = out_info->may_continue || child_info.may_continue;
            out_info->may_return = out_info->may_return || child_info.may_return;
            may_fallthrough = child_info.may_fallthrough;
        }
        out_info->may_fallthrough = may_fallthrough;
        ir_lower_flow_state_pop_scope(&local);
        *state = local;
        return 1;
    }
    case AST_STMT_DECLARATION:
        for (i = 0; i < stmt->declaration_name_count; ++i) {
            const char *name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
            const AstExpression *init_expr =
                (stmt->expressions && i < stmt->expression_count) ? stmt->expressions[i] : NULL;
            long long value = 0;

            if (!name || name[0] == '\0') {
                continue;
            }
            if (!ir_lower_flow_state_add_name(state, name, strlen(name))) {
                return 0;
            }
            if (init_expr &&
                ir_lower_flow_state_eval_expr(ctx, init_expr, state, &value)) {
                state->known[state->count - 1u] = 1u;
                state->values[state->count - 1u] = ir_normalize_sysy_int_value(value);
            }
        }
        return 1;
    case AST_STMT_EXPRESSION:
        for (i = 0; i < stmt->expression_count; ++i) {
            long long value = 0;

            (void)ir_lower_flow_state_eval_expr(ctx, stmt->expressions[i], state, &value);
        }
        return 1;
    case AST_STMT_RETURN:
        out_info->may_fallthrough = 0;
        out_info->may_return = 1;
        return 1;
    case AST_STMT_BREAK:
        out_info->may_fallthrough = 0;
        out_info->may_break = 1;
        return 1;
    case AST_STMT_CONTINUE:
        out_info->may_fallthrough = 0;
        out_info->may_continue = 1;
        return 1;
    case AST_STMT_IF: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_known = 0;
        long long cond_truthy = 0;
        IrLowerFlowState then_state = *state;
        IrLowerFlowState else_state = *state;
        IrLowerLoopTransferInfo then_info = {1, 0, 0, 0};
        IrLowerLoopTransferInfo else_info = {1, 0, 0, 0};
        int condition_has_side_effects = cond_expr ? ir_expression_has_side_effects(cond_expr) : 0;

        if (cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
        }
        if (condition_has_side_effects) {
            cond_known = 0;
            cond_truthy = 0;
        }

        if (cond_known) {
            if (cond_truthy) {
                if (stmt->child_count >= 1 &&
                    !ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[0], &then_state, &then_info)) {
                    return 0;
                }
                *out_info = then_info;
                *state = then_state;
                return 1;
            }
            if (stmt->child_count >= 2 &&
                !ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[1], &else_state, &else_info)) {
                return 0;
            }
            *out_info = else_info;
            *state = else_state;
            return 1;
        }

        if (stmt->child_count >= 1 &&
            !ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[0], &then_state, &then_info)) {
            return 0;
        }
        if (stmt->child_count >= 2 &&
            !ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[1], &else_state, &else_info)) {
            return 0;
        }

        out_info->may_fallthrough = stmt->child_count >= 2
            ? (then_info.may_fallthrough || else_info.may_fallthrough)
            : 1;
        out_info->may_break = then_info.may_break || else_info.may_break;
        out_info->may_continue = then_info.may_continue || else_info.may_continue;
        out_info->may_return = then_info.may_return || else_info.may_return;
        if (!ir_lower_flow_state_merge_known(state, &then_state, &else_state)) {
            return 0;
        }
        return 1;
    }
    case AST_STMT_WHILE:
    case AST_STMT_FOR: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_known = (stmt->kind == AST_STMT_FOR && !stmt->has_condition_expression) ? 1 : 0;
        long long cond_truthy = cond_known ? 1 : 0;
        IrLowerFlowState body_state = *state;
        IrLowerLoopTransferInfo body_info = {1, 0, 0, 0};
        int condition_has_side_effects = cond_expr ? ir_expression_has_side_effects(cond_expr) : 0;

        if (!cond_known && cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
        }
        if (condition_has_side_effects) {
            cond_known = 0;
            cond_truthy = 0;
        }
        if (cond_known && !cond_truthy) {
            return 1;
        }

        if (stmt->child_count >= 1 &&
            !ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[0], &body_state, &body_info)) {
            return 0;
        }

        out_info->may_break = 0;
        out_info->may_continue = 0;
        out_info->may_return = body_info.may_return;
        if (cond_known && !cond_truthy) {
            out_info->may_fallthrough = 1;
        } else if (body_info.may_break) {
            out_info->may_fallthrough = 1;
        } else if (!body_info.may_fallthrough && !body_info.may_return) {
            out_info->may_fallthrough = 0;
        } else {
            out_info->may_fallthrough =
                ir_lower_flow_state_proves_loop_condition_stays_true_from_state(ctx, stmt, state) ? 0 : 1;
        }
        return 1;
    }
    default:
        return 1;
    }
}

static int ir_lower_flow_state_process_statement(const IrLowerContext *ctx,
    const AstStatement *stmt,
    IrLowerFlowState *state,
    int *out_may_fallthrough) {
    size_t i;
    int may_fallthrough = 1;

    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (!stmt || !state || state->overflowed) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND: {
        IrLowerFlowState local = *state;

        if (!ir_lower_flow_state_push_scope(&local)) {
            return 0;
        }
        for (i = 0; i < stmt->child_count && may_fallthrough; ++i) {
            int child_may_fallthrough = 1;
            const AstStatement *child = stmt->children[i];

            if (!ir_lower_flow_state_process_statement(ctx, child, &local, &child_may_fallthrough)) {
                return 0;
            }
            may_fallthrough = child_may_fallthrough;
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = may_fallthrough;
        }
        ir_lower_flow_state_pop_scope(&local);
        *state = local;
        return 1;
    }
    case AST_STMT_DECLARATION:
        for (i = 0; i < stmt->declaration_name_count; ++i) {
            const char *name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
            const AstExpression *init_expr =
                (stmt->expressions && i < stmt->expression_count) ? stmt->expressions[i] : NULL;
            long long value = 0;

            if (!name || name[0] == '\0') {
                continue;
            }
            if (!ir_lower_flow_state_add_name(state, name, strlen(name))) {
                return 0;
            }
            if (init_expr &&
                ir_lower_flow_state_eval_expr(ctx, init_expr, state, &value)) {
                state->known[state->count - 1u] = 1u;
                state->values[state->count - 1u] = ir_normalize_sysy_int_value(value);
            }
        }
        return 1;
    case AST_STMT_EXPRESSION:
        for (i = 0; i < stmt->expression_count; ++i) {
            long long value = 0;

            if (!ir_lower_flow_state_eval_expr(ctx, stmt->expressions[i], state, &value)) {
                /* Non-constant expressions do not break the current proof. */
            }
        }
        return 1;
    case AST_STMT_RETURN:
        if (out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        return 1;
    case AST_STMT_IF: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_known = 0;
        long long cond_truthy = 0;
        IrLowerFlowState then_state = *state;
        IrLowerFlowState else_state = *state;
        int then_may_fallthrough = 1;
        int else_may_fallthrough = 1;
        int condition_has_side_effects = cond_expr ? ir_expression_has_side_effects(cond_expr) : 0;

        if (cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
        }
        if (condition_has_side_effects) {
            cond_known = 0;
            cond_truthy = 0;
        }
        if (cond_known) {
            if (cond_truthy) {
                if (stmt->child_count >= 1 &&
                    !ir_lower_flow_state_process_statement(ctx, stmt->children[0], &then_state, &then_may_fallthrough)) {
                    return 0;
                }
                if (out_may_fallthrough) {
                    *out_may_fallthrough = stmt->child_count >= 1 ? then_may_fallthrough : 1;
                }
                *state = then_state;
                return 1;
            }
            if (stmt->child_count >= 2 &&
                !ir_lower_flow_state_process_statement(ctx, stmt->children[1], &else_state, &else_may_fallthrough)) {
                return 0;
            }
            if (out_may_fallthrough) {
                *out_may_fallthrough = stmt->child_count >= 2 ? else_may_fallthrough : 1;
            }
            *state = else_state;
            return 1;
        }

        if (stmt->child_count >= 1 &&
            !ir_lower_flow_state_process_statement(ctx, stmt->children[0], &then_state, &then_may_fallthrough)) {
            return 0;
        }
        if (stmt->child_count >= 2 &&
            !ir_lower_flow_state_process_statement(ctx, stmt->children[1], &else_state, &else_may_fallthrough)) {
            return 0;
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = stmt->child_count >= 2
                ? (then_may_fallthrough || else_may_fallthrough)
                : 1;
        }
        if (!ir_lower_flow_state_merge_known(state, &then_state, &else_state)) {
            return 0;
        }
        return 1;
    }
    case AST_STMT_WHILE:
    case AST_STMT_FOR: {
        IrLowerFlowState body_state = *state;
        int body_may_fallthrough = 1;
        int proven_stays_true = 0;

        if (stmt->child_count >= 1 &&
            !ir_lower_flow_state_process_statement(ctx, stmt->children[0], &body_state, &body_may_fallthrough)) {
            return 0;
        }

        proven_stays_true = ir_lower_flow_state_proves_loop_condition_stays_true_from_state(ctx, stmt, state);
        if (out_may_fallthrough) {
            *out_may_fallthrough = proven_stays_true ? 0 : 1;
        }
        return 1;
    }
    default:
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }
}

static int ir_lower_flow_state_contains_unknown_control(const IrLowerContext *ctx,
    const AstStatement *stmt,
    IrLowerFlowState *state,
    int *out_has_unknown_control) {
    size_t i;
    int may_fallthrough = 1;

    if (out_has_unknown_control) {
        *out_has_unknown_control = 0;
    }
    if (!stmt || !state || state->overflowed) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND: {
        IrLowerFlowState local = *state;

        if (!ir_lower_flow_state_push_scope(&local)) {
            return 0;
        }
        for (i = 0; i < stmt->child_count && may_fallthrough; ++i) {
            int child_has_unknown = 0;
            int child_may_fallthrough = 1;

            if (!ir_lower_flow_state_contains_unknown_control(
                    ctx, stmt->children[i], &local, &child_has_unknown)) {
                return 0;
            }
            if (child_has_unknown) {
                if (out_has_unknown_control) {
                    *out_has_unknown_control = 1;
                }
                return 1;
            }
            if (!ir_lower_flow_state_process_statement(
                    ctx, stmt->children[i], &local, &child_may_fallthrough)) {
                return 0;
            }
            may_fallthrough = child_may_fallthrough;
        }
        ir_lower_flow_state_pop_scope(&local);
        *state = local;
        return 1;
    }
    case AST_STMT_DECLARATION:
    case AST_STMT_EXPRESSION:
    case AST_STMT_RETURN:
    case AST_STMT_BREAK:
    case AST_STMT_CONTINUE: {
        int ignored_may_fallthrough = 1;

        return ir_lower_flow_state_process_statement(ctx, stmt, state, &ignored_may_fallthrough);
    }
    case AST_STMT_IF: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_known = 0;
        long long cond_truthy = 0;
        int condition_has_side_effects = cond_expr ? ir_expression_has_side_effects(cond_expr) : 0;

        if (cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
        }
        if (condition_has_side_effects) {
            cond_known = 0;
            cond_truthy = 0;
        }
        if (!cond_known) {
            if (out_has_unknown_control) {
                *out_has_unknown_control = 1;
            }
            return 1;
        }
        if (cond_truthy) {
            if (stmt->child_count >= 1) {
                return ir_lower_flow_state_contains_unknown_control(
                    ctx, stmt->children[0], state, out_has_unknown_control);
            }
            return 1;
        }
        if (stmt->child_count >= 2) {
            return ir_lower_flow_state_contains_unknown_control(
                ctx, stmt->children[1], state, out_has_unknown_control);
        }
        return 1;
    }
    case AST_STMT_WHILE:
    case AST_STMT_FOR:
        if (out_has_unknown_control) {
            *out_has_unknown_control = 1;
        }
        return 1;
    default:
        return 1;
    }
}

static int ir_try_eval_condition_truthiness_with_scope(const IrLowerContext *ctx,
    const AstExpression *expr,
    int *out_known,
    int *out_truthy) {
    IrLowerFlowState state;
    long long truthy = 0;

    if (!out_known || !out_truthy) {
        return 0;
    }
    *out_known = 0;
    *out_truthy = 0;

    /*
     * Outside loops we can safely reuse current scope facts for straight-line
     * truthiness pruning. Inside a loop body the same local may have a
     * different value on later iterations, so folding `if (i == 5)` from the
     * entry-state `i == 0` would be unsound.
     */
    if (ctx && expr && ctx->loop_targets.count == 0u) {
        ir_lower_flow_state_init(&state);
        if (!ir_lower_flow_state_import_scope(ctx, &state)) {
            if (ctx && ctx->error) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-INT-079: failed to import scoped flow facts for condition truthiness evaluation");
            }
            return 0;
        }
        if (ir_lower_flow_state_eval_expr(ctx, expr, &state, &truthy)) {
            *out_known = 1;
            *out_truthy = truthy != 0;
            return 1;
        }
    }

    return ir_dependency_try_eval_condition_truthiness(expr, out_known, out_truthy);
}

static void ir_loop_target_stack_free(IrLoopTargetStack *stack) {
    if (!stack) {
        return;
    }

    free(stack->items);
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static int ir_loop_target_stack_push(IrLoopTargetStack *stack,
    size_t break_target,
    size_t continue_target,
    size_t defer_scope_depth) {
    IrLoopTarget *new_items;
    size_t next_capacity;

    if (!stack) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity,
                4,
                sizeof(IrLoopTarget),
                &next_capacity)) {
            return 0;
        }
        new_items = (IrLoopTarget *)realloc(stack->items,
            next_capacity * sizeof(IrLoopTarget));
        if (!new_items) {
            return 0;
        }
        stack->items = new_items;
        stack->capacity = next_capacity;
    }

    stack->items[stack->count].break_target = break_target;
    stack->items[stack->count].continue_target = continue_target;
    stack->items[stack->count].defer_scope_depth = defer_scope_depth;
    stack->count += 1;
    return 1;
}

static void ir_loop_target_stack_pop(IrLoopTargetStack *stack) {
    if (!stack || stack->count == 0) {
        return;
    }

    stack->count -= 1;
}

static const IrLoopTarget *ir_loop_target_stack_current(const IrLoopTargetStack *stack) {
    if (!stack || stack->count == 0) {
        return NULL;
    }

    return &stack->items[stack->count - 1];
}

static int ir_lower_loop_control_statement(IrLowerContext *ctx,
    const AstStatement *stmt,
    int is_continue) {
    const IrLoopTarget *loop_target;
    IrBasicBlock *block;
    size_t target_block_id;
    size_t scope_index;

    if (!ctx || !stmt || !ctx->has_block) {
        return 0;
    }

    loop_target = ir_loop_target_stack_current(&ctx->loop_targets);
    if (!loop_target) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            is_continue
                ? "IR-LOWER-012: continue statement without active loop target"
                : "IR-LOWER-011: break statement without active loop target");
        return 0;
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }

    for (scope_index = ctx->defer_scopes.count; scope_index > loop_target->defer_scope_depth; --scope_index) {
        size_t defer_base = ctx->defer_scopes.items[scope_index - 1u].defer_base;
        size_t defer_end = (scope_index < ctx->defer_scopes.count)
            ? ctx->defer_scopes.items[scope_index].defer_base
            : ctx->defer_entries.count;

        if (!ir_lower_emit_defer_entries_range(ctx, defer_base, defer_end)) {
            return 0;
        }
        if (!ctx->has_block || ir_lower_current_block(ctx)->has_terminator) {
            return 1;
        }
    }

    target_block_id = is_continue ? loop_target->continue_target : loop_target->break_target;
    return ir_block_set_jump(block, target_block_id, ctx->error);
}

static int ir_clone_function_value_alias_tag_binding(IrLowerContext *ctx,
    const char *dest_binding_name,
    const char *source_binding_name,
    size_t source_tag_local_id,
    int source_has_const_value,
    long long source_const_value,
    size_t *out_tag_local_id,
    long long *out_tag_value,
    int *out_has_const_value,
    long long *out_const_value) {
    char tag_local_name[256];
    long long tag_value = 0;

    if (out_tag_local_id) {
        *out_tag_local_id = (size_t)-1;
    }
    if (out_tag_value) {
        *out_tag_value = 0;
    }
    if (out_has_const_value) {
        *out_has_const_value = 0;
    }
    if (out_const_value) {
        *out_const_value = 0;
    }
    if (!ctx || !dest_binding_name || dest_binding_name[0] == '\0' ||
        !source_binding_name || source_binding_name[0] == '\0' ||
        source_tag_local_id == (size_t)-1 || !out_tag_local_id) {
        return 0;
    }

    snprintf(tag_local_name, sizeof(tag_local_name), "%s$ftag", dest_binding_name);
    if (!ir_function_append_local(
            ctx->function,
            tag_local_name,
            0,
            out_tag_local_id,
            ctx->error) ||
        !ir_lower_clone_function_value_target_set(ctx, dest_binding_name, source_binding_name) ||
        !ir_emit_mov(ctx,
            ir_value_local(*out_tag_local_id),
            ir_value_local(source_tag_local_id))) {
        return 0;
    }

    tag_value = source_has_const_value ? source_const_value : 0;
    if (out_tag_value) {
        *out_tag_value = tag_value;
    }
    if (out_has_const_value) {
        *out_has_const_value = source_has_const_value ? 1 : 0;
    }
    if (out_const_value) {
        *out_const_value = source_const_value;
    }
    return 1;
}

static int ir_clone_function_object_descriptor_locals_for_binding(
    IrLowerContext *ctx,
    const char *binding_name,
    const char *source_binding_name,
    const IrLowerBindingInfo *source_binding_info,
    IrLowerFunctionObjectDescriptor *descriptor) {
    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !source_binding_name || source_binding_name[0] == '\0' ||
        !source_binding_info || !descriptor) {
        return 0;
    }

    if (source_binding_info->function_target_tag_local_id != (size_t)-1) {
        size_t cloned_tag_local_id = (size_t)-1;

        if (!ir_clone_function_value_alias_tag_binding(
                ctx,
                binding_name,
                source_binding_name,
                source_binding_info->function_target_tag_local_id,
                source_binding_info->has_const_value,
                source_binding_info->const_value,
                &cloned_tag_local_id,
                NULL,
                &descriptor->has_const_tag_value,
                &descriptor->const_tag_value)) {
            return 0;
        }
        descriptor->dynamic_tag_local_id = cloned_tag_local_id;
    }

    if (descriptor->capture_count > 0u &&
        source_binding_info->kind == IR_LOWER_BINDING_CLOSURE_LOCAL &&
        source_binding_info->closure_capture_count > 0u) {
        size_t *capture_local_ids = NULL;

        if (!ir_append_named_capture_local_slots(
                ctx,
                binding_name,
                descriptor->capture_count,
                &capture_local_ids) ||
            !ir_copy_capture_local_slot_range(
                ctx,
                capture_local_ids,
                descriptor->capture_local_ids,
                descriptor->capture_count)) {
            free(capture_local_ids);
            return 0;
        }
        descriptor->capture_local_ids = capture_local_ids;
        descriptor->owned_payload_local_ids = capture_local_ids;
    }

    return 1;
}

static int ir_copy_materialized_returned_payload_into_binding(
    IrLowerContext *ctx,
    size_t dest_tag_local_id,
    const size_t *dest_capture_local_ids,
    size_t capture_count,
    const size_t *payload_local_ids,
    size_t payload_slot_count) {
    if (!ctx || !payload_local_ids || payload_slot_count == 0u ||
        payload_local_ids[0] == (size_t)-1 ||
        dest_tag_local_id == (size_t)-1) {
        return 0;
    }

    if (!ir_emit_mov(
            ctx,
            ir_value_local(dest_tag_local_id),
            ir_value_local(payload_local_ids[0]))) {
        return 0;
    }

    if (capture_count > 0u) {
        if (!dest_capture_local_ids ||
            payload_slot_count != capture_count + 1u ||
            !ir_copy_capture_local_slot_range(
                ctx,
                (size_t *)dest_capture_local_ids,
                &payload_local_ids[1],
                capture_count)) {
            return 0;
        }
    }

    return 1;
}

static int ir_prepare_returned_function_value_tag_local_and_targets(IrLowerContext *ctx,
    const char *binding_name,
    const char *const *target_names,
    size_t target_count,
    size_t *out_tag_local_id,
    long long *out_first_target_tag_value) {
    char tag_local_name[256];
    size_t target_index;

    if (out_tag_local_id) {
        *out_tag_local_id = (size_t)-1;
    }
    if (out_first_target_tag_value) {
        *out_first_target_tag_value = 0;
    }
    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !target_names || target_count == 0u || !out_tag_local_id) {
        return 0;
    }

    snprintf(tag_local_name, sizeof(tag_local_name), "%s$ftag", binding_name);
    if (!ir_function_append_local(
            ctx->function,
            tag_local_name,
            0,
            out_tag_local_id,
            ctx->error)) {
        return 0;
    }

    for (target_index = 0u; target_index < target_count; ++target_index) {
        long long tag_value = 0;

        if (!target_names[target_index] ||
            !ir_lower_register_function_value_target(
                ctx,
                binding_name,
                target_names[target_index],
                &tag_value)) {
            return 0;
        }
        if (target_index == 0u && out_first_target_tag_value) {
            *out_first_target_tag_value = tag_value;
        }
    }

    return 1;
}

static void ir_finalize_returned_function_object_descriptor_metadata(
    IrLowerFunctionObjectDescriptor *descriptor,
    const AstExternal *producer_external,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count) {
    if (!descriptor) {
        return;
    }

    descriptor->producer_external = producer_external;
    descriptor->dynamic_target_names = dynamic_target_names;
    descriptor->dynamic_target_count = dynamic_target_count;
    descriptor->owned_dynamic_target_names = (const char **)dynamic_target_names;
    descriptor->has_dynamic_family = dynamic_target_count > 1u;
}

static int ir_finalize_materialized_returned_closure_binding_descriptor(
    IrLowerContext *ctx,
    const AstExternal *producer_external,
    const char *returned_primary_target_name,
    const char *const *returned_target_names,
    size_t returned_target_count,
    size_t function_target_tag_local_id,
    const size_t *capture_local_ids,
    size_t capture_count,
    size_t expected_payload_slot_count,
    IrLowerFunctionObjectDescriptor *materialized_descriptor,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    if (!ctx || !producer_external ||
        !returned_primary_target_name || returned_primary_target_name[0] == '\0' ||
        !returned_target_names || returned_target_count == 0u ||
        !materialized_descriptor || !out_descriptor) {
        return 0;
    }

    if (!materialized_descriptor->owned_payload_local_ids ||
        materialized_descriptor->payload_slot_count != expected_payload_slot_count ||
        !ir_copy_materialized_returned_payload_into_binding(
            ctx,
            function_target_tag_local_id,
            capture_local_ids,
            capture_count,
            materialized_descriptor->owned_payload_local_ids,
            materialized_descriptor->payload_slot_count) ||
        !ir_init_function_object_descriptor_from_static_target_with_payload(
            returned_primary_target_name,
            1,
            0,
            0,
            1,
            capture_local_ids,
            capture_count,
            function_target_tag_local_id,
            out_descriptor)) {
        return 0;
    }

    ir_finalize_returned_function_object_descriptor_metadata(
        out_descriptor,
        producer_external,
        returned_target_names,
        returned_target_count);
    return 1;
}

static int ir_try_resolve_exact_returned_function_value_parameter_passthrough_expr(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_expr) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(call_expr);
    const AstExpression *callee_base = NULL;
    const AstExternal *return_function_external = NULL;
    const AstExpression *arg_expr = NULL;
    size_t return_param_index = (size_t)-1;

    if (out_expr) {
        *out_expr = NULL;
    }
    if (!ctx || !ctx->ast_program || !out_expr || !base || base->kind != AST_EXPR_CALL) {
        return 0;
    }

    callee_base = ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
    if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
        return 0;
    }

    return_function_external =
        ir_find_named_function_external_prefer_definition(
            ctx->ast_program,
            callee_base->as.identifier.name);
    if (!return_function_external ||
        !return_function_external->has_function_return_signature ||
        !ir_function_value_return_exact_parameter_passthrough_index(
            ctx->ast_program,
            return_function_external,
            &return_param_index) ||
        return_param_index == (size_t)-1 ||
        !base->as.call.args ||
        return_param_index >= base->as.call.arg_count) {
        return 0;
    }

    arg_expr = base->as.call.args[return_param_index];
    *out_expr = ir_unwrap_function_value_wrapper_expression(arg_expr);
    return *out_expr &&
        ((*out_expr)->kind == AST_EXPR_IDENTIFIER ||
            (*out_expr)->kind == AST_EXPR_CLOSURE ||
            (*out_expr)->kind == AST_EXPR_CALL ||
            (*out_expr)->kind == AST_EXPR_TERNARY);
}

static int ir_write_function_value_binding_tag_from_local(IrLowerContext *ctx,
    const char *dest_binding_name,
    const char *source_binding_name,
    size_t dest_tag_local_id,
    size_t source_tag_local_id) {
    if (!ctx || !dest_binding_name || dest_binding_name[0] == '\0' ||
        !source_binding_name || source_binding_name[0] == '\0' ||
        dest_tag_local_id == (size_t)-1 || source_tag_local_id == (size_t)-1) {
        return 0;
    }

    return ir_lower_clone_function_value_target_set(ctx, dest_binding_name, source_binding_name) &&
        ir_emit_mov(ctx,
            ir_value_local(dest_tag_local_id),
            ir_value_local(source_tag_local_id));
}

static int ir_write_function_value_binding_tag_from_target(IrLowerContext *ctx,
    const char *binding_name,
    const char *target_name,
    size_t dest_tag_local_id,
    long long *out_tag_value) {
    long long tag_value = 0;

    if (out_tag_value) {
        *out_tag_value = 0;
    }
    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !target_name || target_name[0] == '\0' ||
        dest_tag_local_id == (size_t)-1) {
        return 0;
    }

    if (!ir_lower_register_function_value_target(
            ctx,
            binding_name,
            target_name,
            &tag_value) ||
        !ir_emit_mov(ctx,
            ir_value_local(dest_tag_local_id),
            ir_value_immediate(tag_value))) {
        return 0;
    }
    if (out_tag_value) {
        *out_tag_value = tag_value;
    }
    return 1;
}

static int ir_update_function_value_binding_transport(IrLowerContext *ctx,
    const char *binding_name,
    IrLowerBindingKind binding_kind,
    const char *target_name,
    size_t closure_capture_count,
    const size_t *closure_capture_local_ids,
    size_t function_target_tag_local_id,
    int has_const_value,
    long long const_value,
    int from_returned_function_value_call) {
    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !target_name || target_name[0] == '\0') {
        return 0;
    }

    return ir_lower_scope_update_function_binding_by_name(
        &ctx->scopes,
        binding_name,
        binding_kind,
        target_name,
        closure_capture_count,
        closure_capture_local_ids,
        function_target_tag_local_id,
        has_const_value,
        const_value,
        from_returned_function_value_call);
}

static int ir_reset_and_register_function_value_binding_target_family(
    IrLowerContext *ctx,
    const char *binding_name,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count) {
    long long ignored_tag_value = 0;
    size_t target_index;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !direct_target_name || direct_target_name[0] == '\0' ||
        !ir_lower_reset_function_value_target_set(ctx, binding_name)) {
        return 0;
    }

    if (dynamic_target_names && dynamic_target_count > 0u) {
        for (target_index = 0u; target_index < dynamic_target_count; ++target_index) {
            if (!dynamic_target_names[target_index] ||
                dynamic_target_names[target_index][0] == '\0' ||
                !ir_lower_register_function_value_target(
                    ctx,
                    binding_name,
                    dynamic_target_names[target_index],
                    &ignored_tag_value)) {
                return 0;
            }
        }
        return 1;
    }

    return ir_lower_register_function_value_target(
        ctx,
        binding_name,
        direct_target_name,
        &ignored_tag_value);
}

static int ir_update_function_value_binding_transport_from_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    IrLowerBindingKind binding_kind,
    const IrLowerFunctionObjectDescriptor *descriptor,
    size_t closure_capture_count,
    const size_t *closure_capture_local_ids,
    size_t function_target_tag_local_id);

static int ir_emit_function_value_binding_capture_copy(IrLowerContext *ctx,
    const size_t *dest_capture_local_ids,
    const size_t *source_capture_local_ids,
    size_t capture_count) {
    size_t capture_index;

    if (capture_count == 0u) {
        return 1;
    }
    if (!ctx || !dest_capture_local_ids || !source_capture_local_ids) {
        return 0;
    }

    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        if (source_capture_local_ids[capture_index] == (size_t)-1 ||
            dest_capture_local_ids[capture_index] == (size_t)-1 ||
            !ir_emit_mov(ctx,
                ir_value_local(dest_capture_local_ids[capture_index]),
                ir_value_local(source_capture_local_ids[capture_index]))) {
            return 0;
        }
    }
    return 1;
}

static int ir_try_copy_function_value_binding_from_binding_descriptor(IrLowerContext *ctx,
    const char *lhs_name,
    const char *rhs_name,
    const AstStatement *stmt,
    const IrLowerBindingInfo *lhs_binding_info,
    const IrLowerBindingInfo *rhs_binding_info,
    const IrLowerFunctionObjectDescriptor *rhs_descriptor,
    int *out_handled) {
    const char *target_name = NULL;

    if (out_handled) {
        *out_handled = 0;
    }
    if (!ctx || !lhs_name || lhs_name[0] == '\0' ||
        !rhs_name || rhs_name[0] == '\0' ||
        !stmt || !lhs_binding_info || !rhs_binding_info || !rhs_descriptor) {
        return 0;
    }

    if (rhs_binding_info->kind == IR_LOWER_BINDING_CLOSURE_LOCAL) {
        long long lhs_tag_value = 0;

        if (lhs_binding_info->kind != IR_LOWER_BINDING_CLOSURE_LOCAL ||
            lhs_binding_info->closure_capture_count != rhs_binding_info->closure_capture_count ||
            lhs_binding_info->closure_capture_count == 0u ||
            !lhs_binding_info->closure_capture_local_ids ||
            !rhs_binding_info->closure_capture_local_ids) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-063: function-value reassignment currently requires matching closure-local payload shapes");
            return 0;
        }

        target_name = rhs_binding_info->type_name;
        if (!ir_emit_function_value_binding_capture_copy(
                ctx,
                lhs_binding_info->closure_capture_local_ids,
                rhs_binding_info->closure_capture_local_ids,
                rhs_binding_info->closure_capture_count)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-091: failed to copy closure payload during reassignment");
            return 0;
        }

        if (rhs_binding_info->function_target_tag_local_id != (size_t)-1) {
            IrLowerFunctionObjectDescriptor adjusted_descriptor = *rhs_descriptor;

            if (lhs_binding_info->function_target_tag_local_id == (size_t)-1) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-091: dynamic closure-family assignment requires a runtime tag-capable destination binding");
                return 0;
            }
            if (rhs_binding_info->has_const_value) {
                if (!target_name || target_name[0] == '\0' ||
                    !ir_write_function_value_binding_tag_from_target(
                        ctx,
                        lhs_name,
                        target_name,
                        lhs_binding_info->function_target_tag_local_id,
                        &lhs_tag_value)) {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-INT-091: failed to remap static closure-family tag during reassignment");
                    return 0;
                }
                adjusted_descriptor.has_const_tag_value = 1;
                adjusted_descriptor.const_tag_value = lhs_tag_value;
            } else if (!ir_write_function_value_binding_tag_from_local(
                    ctx,
                    lhs_name,
                    rhs_name,
                    lhs_binding_info->function_target_tag_local_id,
                    rhs_binding_info->function_target_tag_local_id)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-091: failed to copy dynamic closure-family binding during reassignment");
                return 0;
            }

            if (!ir_update_function_value_binding_transport_from_descriptor(
                    ctx,
                    lhs_name,
                    IR_LOWER_BINDING_CLOSURE_LOCAL,
                    &adjusted_descriptor,
                    rhs_binding_info->closure_capture_count,
                    lhs_binding_info->closure_capture_local_ids,
                    lhs_binding_info->function_target_tag_local_id)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    rhs_binding_info->has_const_value
                        ? "IR-INT-091: failed to update static closure binding during reassignment"
                        : "IR-INT-091: failed to update closure binding during reassignment");
                if (out_handled) {
                    *out_handled = -1;
                }
                return 0;
            }
            if (out_handled) {
                *out_handled = 1;
            }
            return 1;
        }

        if (!ir_update_function_value_binding_transport_from_descriptor(
                ctx,
                lhs_name,
                IR_LOWER_BINDING_CLOSURE_LOCAL,
                rhs_descriptor,
                rhs_binding_info->closure_capture_count,
                lhs_binding_info->closure_capture_local_ids,
                lhs_binding_info->function_target_tag_local_id)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-091: failed to update closure binding during reassignment");
            if (out_handled) {
                *out_handled = -1;
            }
            return 0;
        }
        if (out_handled) {
            *out_handled = 1;
        }
        return 1;
    }

    if (rhs_binding_info->kind != IR_LOWER_BINDING_FUNCTION_LOCAL) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-063: function-value reassignment currently supports only non-capturing local/top-level targets");
        return 0;
    }

    if (rhs_binding_info->function_target_tag_local_id != (size_t)-1) {
        if (!ir_write_function_value_binding_tag_from_local(
                ctx,
                lhs_name,
                rhs_name,
                lhs_binding_info->function_target_tag_local_id,
                rhs_binding_info->function_target_tag_local_id) ||
            !ir_update_function_value_binding_transport_from_descriptor(
                ctx,
                lhs_name,
                IR_LOWER_BINDING_FUNCTION_LOCAL,
                rhs_descriptor,
                0u,
                NULL,
                lhs_binding_info->function_target_tag_local_id)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-091: failed to copy dynamic function-value binding during reassignment");
            if (out_handled) {
                *out_handled = -1;
            }
            return 0;
        }
        if (out_handled) {
            *out_handled = 1;
        }
        return 1;
    }

    return 0;
}

static int ir_add_function_value_local_binding_transport(IrLowerContext *ctx,
    const char *binding_name,
    const char *target_name,
    size_t closure_capture_count,
    const size_t *closure_capture_local_ids,
    size_t function_target_tag_local_id,
    int declaration_is_const,
    int has_const_value,
    long long const_value,
    int from_returned_function_value_call) {
    IrLowerBindingKind binding_kind;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !target_name || target_name[0] == '\0') {
        return 0;
    }

    binding_kind = closure_capture_count > 0u
        ? IR_LOWER_BINDING_CLOSURE_LOCAL
        : IR_LOWER_BINDING_FUNCTION_LOCAL;

    return ir_lower_scope_add_binding_with_metadata(
        &ctx->scopes,
        binding_name,
        (size_t)-1,
        binding_kind,
        0u,
        NULL,
        0u,
        0u,
        target_name,
        closure_capture_count,
        closure_capture_count > 0u ? closure_capture_local_ids : NULL,
        function_target_tag_local_id,
        declaration_is_const,
        has_const_value,
        const_value,
        from_returned_function_value_call);
}

static int ir_add_function_value_local_binding_transport_from_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    int declaration_is_const,
    const IrLowerFunctionObjectDescriptor *descriptor) {
    size_t target_index;

    if (!ctx || !binding_name || binding_name[0] == '\0' || !descriptor ||
        !descriptor->direct_target_name || descriptor->direct_target_name[0] == '\0') {
        return 0;
    }

    if (descriptor->dynamic_target_names &&
        descriptor->dynamic_target_count > 0u) {
        for (target_index = 0u; target_index < descriptor->dynamic_target_count; ++target_index) {
            long long ignored_tag_value = 0;

            if (!descriptor->dynamic_target_names[target_index] ||
                !ir_lower_register_function_value_target(
                    ctx,
                    binding_name,
                    descriptor->dynamic_target_names[target_index],
                    &ignored_tag_value)) {
                return 0;
            }
        }
    }

    return ir_add_function_value_local_binding_transport(
        ctx,
        binding_name,
        descriptor->direct_target_name,
        descriptor->capture_count,
        descriptor->capture_count > 0u ? descriptor->capture_local_ids : NULL,
        descriptor->dynamic_tag_local_id,
        declaration_is_const,
        descriptor->has_const_tag_value,
        descriptor->const_tag_value,
        descriptor->from_returned_function_value_call);
}

static int ir_update_function_value_binding_transport_from_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    IrLowerBindingKind binding_kind,
    const IrLowerFunctionObjectDescriptor *descriptor,
    size_t closure_capture_count,
    const size_t *closure_capture_local_ids,
    size_t function_target_tag_local_id) {
    if (!ctx || !binding_name || binding_name[0] == '\0' || !descriptor ||
        !descriptor->direct_target_name || descriptor->direct_target_name[0] == '\0') {
        return 0;
    }

    return ir_update_function_value_binding_transport(
        ctx,
        binding_name,
        binding_kind,
        descriptor->direct_target_name,
        closure_capture_count,
        closure_capture_local_ids,
        function_target_tag_local_id,
        descriptor->has_const_tag_value,
        descriptor->const_tag_value,
        descriptor->from_returned_function_value_call);
}

static int ir_prepare_materialized_ternary_function_value_tags(IrLowerContext *ctx,
    const char *binding_name,
    const char *then_source_binding_name,
    const char *else_source_binding_name,
    const char *then_target_name,
    const char *else_target_name,
    long long *out_then_tag_value,
    long long *out_else_tag_value);

static void ir_init_returned_function_value_family_info(
    IrLowerReturnedFunctionValueFamilyInfo *info) {
    if (!info) {
        return;
    }

    memset(info, 0, sizeof(*info));
}

static void ir_free_returned_function_value_family_info(
    IrLowerReturnedFunctionValueFamilyInfo *info) {
    if (!info) {
        return;
    }

    free((void *)info->target_names);
    ir_init_returned_function_value_family_info(info);
}

static int ir_finalize_function_value_local_binding_transport_from_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    int declaration_is_const,
    IrLowerFunctionObjectDescriptor *descriptor) {
    long long function_target_tag_value = 0;

    if (!ctx || !binding_name || binding_name[0] == '\0' || !descriptor ||
        !descriptor->direct_target_name || descriptor->direct_target_name[0] == '\0') {
        return 0;
    }
    if (!ir_program_find_function_mut(ctx->program, descriptor->direct_target_name)) {
        IrFunction *builtin_function = NULL;
        if (!ir_ensure_builtin_function_signature(
                ctx->program,
                descriptor->direct_target_name,
                &builtin_function,
                ctx->error)) {
            return 0;
        }
    }
    if (descriptor->dynamic_tag_local_id == (size_t)-1) {
        char tag_local_name[256];

        snprintf(tag_local_name, sizeof(tag_local_name), "%s$ftag", binding_name);
        if (!ir_function_append_local(
                ctx->function,
                tag_local_name,
                0,
                &descriptor->dynamic_tag_local_id,
                ctx->error) ||
            !ir_lower_register_function_value_target(
                ctx,
                binding_name,
                descriptor->direct_target_name,
                &function_target_tag_value) ||
            !ir_emit_mov(ctx,
                ir_value_local(descriptor->dynamic_tag_local_id),
                ir_value_immediate(function_target_tag_value))) {
            return 0;
        }
        descriptor->has_const_tag_value = 1;
        descriptor->const_tag_value = function_target_tag_value;
        descriptor->payload_has_dynamic_tag =
            !descriptor->is_closure_family || !descriptor->has_const_tag_value;
        descriptor->payload_slot_count = descriptor->capture_count +
            (descriptor->payload_has_dynamic_tag ? 1u : 0u);
    }

    return ir_add_function_value_local_binding_transport_from_descriptor(
        ctx,
        binding_name,
        declaration_is_const,
        descriptor);
}

static int ir_try_build_returned_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    IrLowerReturnedFunctionValueFamilyInfo family_info;
    IrLowerFunctionObjectDescriptor descriptor;
    IrLowerFunctionObjectDescriptor materialized_descriptor;
    size_t *capture_local_ids = NULL;
    size_t function_target_tag_local_id = (size_t)-1;
    long long first_target_tag_value = 0;
    int ok = 0;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !base_expr || base_expr->kind != AST_EXPR_CALL || !out_descriptor) {
        return 0;
    }
    ir_init_function_object_descriptor(out_descriptor);
    ir_init_returned_function_value_family_info(&family_info);
    ir_init_function_object_descriptor(&descriptor);
    ir_init_function_object_descriptor(&materialized_descriptor);

    {
        const AstExpression *returned_arg_base = NULL;
        IrLowerFunctionObjectDescriptor returned_parameter_descriptor;

        ir_init_function_object_descriptor(&returned_parameter_descriptor);
        if (ctx->ast_program &&
            ir_try_resolve_returned_function_value_parameter_argument_info(
                ctx->ast_program,
                ctx,
                base_expr,
                NULL,
                &returned_arg_base,
                NULL,
                NULL,
                NULL,
                NULL) &&
            returned_arg_base &&
            returned_arg_base->kind == AST_EXPR_IDENTIFIER &&
            returned_arg_base->as.identifier.name &&
            ir_try_build_identifier_function_value_descriptor(
                ctx,
                binding_name,
                returned_arg_base,
                &returned_parameter_descriptor)) {
            returned_parameter_descriptor.from_returned_function_value_call = 1;
            *out_descriptor = returned_parameter_descriptor;
            return 1;
        }
    }

    if (!ir_try_analyze_returned_function_value_family(ctx, base_expr, &family_info)) {
        return 0;
    }
    if (family_info.needs_tag_local &&
        !ir_prepare_returned_function_value_tag_local_and_targets(
            ctx,
            binding_name,
            family_info.target_names,
            family_info.target_count,
            &function_target_tag_local_id,
            family_info.is_closure_family ? NULL : &first_target_tag_value)) {
        goto cleanup;
    }

    if (!family_info.is_closure_family) {
        if (!ir_lower_call_into_returned_closure_slots(
                ctx,
                base_expr,
                family_info.producer_external,
                function_target_tag_local_id,
                family_info.payload_slot_count) ||
            !ir_init_function_object_descriptor_from_static_target_with_payload(
                family_info.primary_target_name,
                0,
                family_info.target_count == 1u,
                family_info.target_count == 1u ? first_target_tag_value : 0,
                1,
                NULL,
                0u,
                function_target_tag_local_id,
                &descriptor)) {
            goto cleanup;
        }
        ir_finalize_returned_function_object_descriptor_metadata(
            &descriptor,
            family_info.producer_external,
            family_info.target_names,
            family_info.target_count);
        *out_descriptor = descriptor;
        family_info.target_names = NULL;
        ok = 1;
        goto cleanup;
    }

    if (!ir_append_named_capture_local_slots(
            ctx,
            binding_name,
            family_info.capture_count,
            &capture_local_ids)) {
        ir_set_error(ctx->error,
            base_expr->line,
            base_expr->column,
            "IR-INT-089: out of memory while lowering returned closure payload captures");
        goto cleanup;
    }

    if (family_info.target_count <= 1u) {
        if ((family_info.capture_count > 0u &&
                !ir_lower_call_into_returned_closure_slots(
                    ctx,
                    base_expr,
                    family_info.producer_external,
                    capture_local_ids[0],
                    family_info.capture_count)) ||
            !ir_init_function_object_descriptor_from_static_target_with_payload(
                family_info.primary_target_name,
                1,
                0,
                0,
                1,
                capture_local_ids,
                family_info.capture_count,
                (size_t)-1,
                out_descriptor)) {
            goto cleanup;
        }
        ir_finalize_returned_function_object_descriptor_metadata(
            out_descriptor,
            family_info.producer_external,
            family_info.target_names,
            family_info.target_count);
        family_info.target_names = NULL;
        capture_local_ids = NULL;
        ok = 1;
        goto cleanup;
    }

    if (!ir_materialize_returned_function_object_descriptor(
            ctx,
            base_expr,
            family_info.producer_external,
            1,
            family_info.primary_target_name,
            family_info.target_names,
            family_info.target_count,
            "__retclosure_declslot",
            &materialized_descriptor)) {
        goto cleanup;
    }

    if (family_info.target_count == 2u &&
        !ir_init_function_object_descriptor_from_static_target_with_payload(
            family_info.primary_target_name,
            1,
            0,
            0,
            1,
            capture_local_ids,
            family_info.capture_count,
            function_target_tag_local_id,
            out_descriptor)) {
        goto cleanup;
    }

    if (!ir_finalize_materialized_returned_closure_binding_descriptor(
            ctx,
            family_info.producer_external,
            family_info.primary_target_name,
            family_info.target_names,
            family_info.target_count,
            function_target_tag_local_id,
            capture_local_ids,
            family_info.capture_count,
            family_info.payload_slot_count,
            &materialized_descriptor,
            family_info.target_count == 2u ? out_descriptor : &descriptor)) {
        goto cleanup;
    }

    if (family_info.target_count > 2u) {
        *out_descriptor = descriptor;
    }
    family_info.target_names = NULL;
    capture_local_ids = NULL;
    ok = 1;

cleanup:
    free((void *)materialized_descriptor.owned_payload_local_ids);
    free(capture_local_ids);
    if (!ok) {
        free((void *)out_descriptor->capture_local_ids);
        free((void *)out_descriptor->dynamic_target_names);
        ir_init_function_object_descriptor(out_descriptor);
    }
    ir_free_returned_function_value_family_info(&family_info);
    return ok;
}

static int ir_try_build_identifier_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const char *identifier_name = NULL;
    IrLowerBindingInfo binding_info;
    IrLowerFunctionObjectDescriptor descriptor;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !base_expr || base_expr->kind != AST_EXPR_IDENTIFIER ||
        !base_expr->as.identifier.name || !out_descriptor) {
        return 0;
    }

    identifier_name = base_expr->as.identifier.name;
    ir_init_function_object_descriptor(out_descriptor);
    if (!ir_resolve_identifier_function_value_descriptor(ctx, identifier_name, out_descriptor)) {
        return 0;
    }
    memset(&binding_info, 0, sizeof(binding_info));
    if (out_descriptor->dynamic_tag_local_id != (size_t)-1) {
        if (!ir_lower_scope_lookup_binding(&ctx->scopes, identifier_name, &binding_info) ||
            (binding_info.kind != IR_LOWER_BINDING_FUNCTION_LOCAL &&
                binding_info.kind != IR_LOWER_BINDING_CLOSURE_LOCAL)) {
            return 0;
        }
        descriptor = *out_descriptor;
        if (!ir_init_function_object_descriptor_from_binding_info(
                &binding_info,
                ir_lower_find_function_value_target_set(ctx, identifier_name),
                &descriptor)) {
            return 0;
        }
        if (!ir_clone_function_object_descriptor_locals_for_binding(
                ctx,
                binding_name,
                identifier_name,
                &binding_info,
                &descriptor)) {
            return 0;
        }
        *out_descriptor = descriptor;
        return 1;
    }
    return 1;
}

static int ir_try_build_closure_literal_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstStatement *stmt,
    size_t declarator_index,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const char *current_function_name = NULL;
    char *closure_helper_name = NULL;
    size_t *capture_local_ids = NULL;
    size_t capture_count = 0u;
    IrLowerFunctionObjectDescriptor descriptor;
    size_t closure_capture_index;

    if (!ctx || !binding_name || binding_name[0] == '\0' || !stmt ||
        !base_expr || base_expr->kind != AST_EXPR_CLOSURE || !out_descriptor) {
        return 0;
    }
    ir_init_function_object_descriptor(out_descriptor);

    current_function_name =
        ctx->function && ctx->function->name ? ctx->function->name : NULL;
    capture_count = base_expr->as.closure.capture_name_count;
    if (!ir_append_named_capture_local_slots(
            ctx,
            binding_name,
            capture_count,
            &capture_local_ids)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-089: out of memory while lowering closure captures");
        return 0;
    }

    for (closure_capture_index = 0u;
         closure_capture_index < capture_count;
         ++closure_capture_index) {
        const char *capture_name =
            base_expr->as.closure.capture_names
                ? base_expr->as.closure.capture_names[closure_capture_index]
                : NULL;
        AstExpression capture_expr;
        IrValueRef capture_value;
        int applied_function_capture = 0;

        if (!capture_name || capture_name[0] == '\0') {
            free(capture_local_ids);
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-057: closure capture binding is not available as a lowered scalar local");
            return 0;
        }
        memset(&capture_expr, 0, sizeof(capture_expr));
        capture_expr.kind = AST_EXPR_IDENTIFIER;
        capture_expr.line = base_expr->line;
        capture_expr.column = base_expr->column;
        capture_expr.as.identifier.name = (char *)capture_name;
        capture_expr.as.identifier.name_length = strlen(capture_name);
        if (!ir_lower_expression(ctx, &capture_expr, &capture_value)) {
            free(capture_local_ids);
            return 0;
        }
        if (!ir_try_materialize_identifier_function_value_capture_slot(
                ctx,
                capture_name,
                capture_local_ids[closure_capture_index],
                &applied_function_capture)) {
            free(capture_local_ids);
            return 0;
        }
        if (!applied_function_capture &&
            !ir_emit_mov(ctx,
                ir_value_local(capture_local_ids[closure_capture_index]),
                capture_value)) {
            free(capture_local_ids);
            return 0;
        }
    }

    if (!ir_build_closure_helper_name(
            ctx->function && ctx->function->name ? ctx->function->name : "fn",
            binding_name,
            ((size_t)(stmt->line > 0 ? stmt->line : 0) << 16) |
                (size_t)((stmt->column > 0 ? stmt->column : 0) + declarator_index),
            &closure_helper_name,
            ctx->error) ||
        !ir_lower_closure_helper_function(
            ctx,
            stmt,
            declarator_index,
            closure_helper_name,
            (const char *const *)base_expr->as.closure.capture_names,
            capture_local_ids,
            capture_count,
            ctx->error)) {
        free(capture_local_ids);
        free(closure_helper_name);
        return 0;
    }
    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            stmt->line,
            stmt->column)) {
        free(capture_local_ids);
        free(closure_helper_name);
        return 0;
    }
    if (!ctx->program || !ir_program_find_function_mut(ctx->program, closure_helper_name)) {
        free(capture_local_ids);
        free(closure_helper_name);
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-090: closure helper disappeared after lowering");
        return 0;
    }

    if (!ir_init_function_object_descriptor_from_static_target_with_payload(
            ir_program_find_function_mut(ctx->program, closure_helper_name)->name,
            1,
            0,
            0,
            0,
            capture_local_ids,
            capture_count,
            (size_t)-1,
            &descriptor)) {
        free(capture_local_ids);
        free(closure_helper_name);
        return 0;
    }
    *out_descriptor = descriptor;
    free(closure_helper_name);
    return 1;
}

static int ir_try_build_ternary_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const char *current_function_name = NULL;
    const IrFunctionValueTargetSet *target_set = NULL;
    IrLowerBindingInfo then_binding_info;
    IrLowerBindingInfo else_binding_info;
    size_t *capture_local_ids = NULL;
    size_t capture_count = 0u;
    size_t function_target_tag_local_id = (size_t)-1;
    IrLowerFunctionObjectDescriptor descriptor;
    IrLowerFunctionValueTernaryInfo ternary_info;

    if (!ctx || !binding_name || binding_name[0] == '\0' || !stmt ||
        !base_expr || base_expr->kind != AST_EXPR_TERNARY || !out_descriptor) {
        return 0;
    }
    ir_init_function_object_descriptor(out_descriptor);
    memset(&then_binding_info, 0, sizeof(then_binding_info));
    memset(&else_binding_info, 0, sizeof(else_binding_info));

    if (!ir_resolve_identifier_ternary_function_value_info(ctx, base_expr, &ternary_info)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-055: local function-value declaration requires direct function identifier, closure literal, direct function-valued return call initializer, or matching identifier ternary");
        return 0;
    }
    current_function_name =
        ctx->function && ctx->function->name ? ctx->function->name : NULL;
    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            stmt->line,
            stmt->column)) {
        return 0;
    }
    if (ternary_info.then_base &&
        ternary_info.then_base->kind == AST_EXPR_IDENTIFIER &&
        ternary_info.then_base->as.identifier.name &&
        ir_lower_scope_lookup_binding(
            &ctx->scopes,
            ternary_info.then_base->as.identifier.name,
            &then_binding_info) &&
        (then_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
            then_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL)) {
        ternary_info.then_capture_local_ids = then_binding_info.closure_capture_local_ids;
        ternary_info.then_capture_count = then_binding_info.closure_capture_count;
        ternary_info.then_is_closure_local =
            then_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL;
        if (then_binding_info.function_target_tag_local_id != (size_t)-1) {
            ternary_info.then_tag_local_id = then_binding_info.function_target_tag_local_id;
            ternary_info.then_has_const_value = then_binding_info.has_const_value;
            ternary_info.then_const_value = then_binding_info.const_value;
        }
    }
    if (ternary_info.else_base &&
        ternary_info.else_base->kind == AST_EXPR_IDENTIFIER &&
        ternary_info.else_base->as.identifier.name &&
        ir_lower_scope_lookup_binding(
            &ctx->scopes,
            ternary_info.else_base->as.identifier.name,
            &else_binding_info) &&
        (else_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
            else_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL)) {
        ternary_info.else_capture_local_ids = else_binding_info.closure_capture_local_ids;
        ternary_info.else_capture_count = else_binding_info.closure_capture_count;
        ternary_info.else_is_closure_local =
            else_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL;
        if (else_binding_info.function_target_tag_local_id != (size_t)-1) {
            ternary_info.else_tag_local_id = else_binding_info.function_target_tag_local_id;
            ternary_info.else_has_const_value = else_binding_info.has_const_value;
            ternary_info.else_const_value = else_binding_info.const_value;
        }
    }
    if (ternary_info.then_is_closure_local != ternary_info.else_is_closure_local ||
        ternary_info.then_capture_count != ternary_info.else_capture_count ||
        (!ternary_info.then_target_name || !ternary_info.else_target_name)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-072: ternary function-value local initialization currently requires matching function-value payload shapes");
        return 0;
    }

    capture_count = ternary_info.then_capture_count;
    if (!ir_append_named_capture_local_slots(
            ctx,
            binding_name,
            capture_count,
            &capture_local_ids)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-089: out of memory while lowering ternary function-value captures");
        return 0;
    }

    {
        char tag_local_name[256];

        snprintf(tag_local_name, sizeof(tag_local_name), "%s$ftag", binding_name);
        if (!ir_function_append_local(
                ctx->function,
                tag_local_name,
                0,
                &function_target_tag_local_id,
                ctx->error) ||
            !ir_prepare_materialized_ternary_function_value_tags(
                ctx,
                binding_name,
                ternary_info.then_base->as.identifier.name,
                ternary_info.else_base->as.identifier.name,
                ternary_info.then_target_name,
                ternary_info.else_target_name,
                &ternary_info.then_const_value,
                &ternary_info.else_const_value) ||
            !ir_emit_materialized_ternary_function_value_assignment(
                ctx,
                base_expr->as.ternary.condition,
                &ternary_info,
                function_target_tag_local_id,
                capture_local_ids,
                capture_count)) {
            free(capture_local_ids);
            return 0;
        }
    }

    target_set = ir_lower_find_function_value_target_set(ctx, binding_name);

    if (!ir_init_function_object_descriptor_from_static_target_with_payload(
            ternary_info.then_target_name,
            ternary_info.then_capture_count > 0u,
            0,
            0,
            0,
            capture_local_ids,
            capture_count,
            function_target_tag_local_id,
            &descriptor)) {
        free(capture_local_ids);
        return 0;
    }
    descriptor.dynamic_target_count = target_set ? target_set->target_count : 0u;
    if (!ir_assign_dynamic_target_name_array_copy(
            target_set ? target_set->target_names : NULL,
            descriptor.dynamic_target_count,
            &descriptor.dynamic_target_names,
            &descriptor.owned_dynamic_target_names)) {
        free(capture_local_ids);
        return 0;
    }
    descriptor.has_dynamic_family =
        descriptor.dynamic_target_count > 1u &&
        descriptor.dynamic_tag_local_id != (size_t)-1;
    descriptor.has_const_tag_value = 0;
    descriptor.const_tag_value = 0;
    descriptor.payload_has_dynamic_tag =
        descriptor.dynamic_tag_local_id != (size_t)-1;
    descriptor.payload_slot_count = descriptor.capture_count +
        (descriptor.payload_has_dynamic_tag ? 1u : 0u);
    *out_descriptor = descriptor;
    return 1;
}

static int ir_try_build_returned_parameter_passthrough_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const AstExpression *returned_arg_base = NULL;
    IrLowerFunctionObjectDescriptor returned_parameter_descriptor;
    const size_t *returned_capture_local_ids = NULL;
    size_t returned_capture_count = 0u;
    const char *returned_target_name = NULL;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !base_expr || base_expr->kind != AST_EXPR_CALL || !out_descriptor) {
        return 0;
    }
    ir_init_function_object_descriptor(out_descriptor);
    ir_init_function_object_descriptor(&returned_parameter_descriptor);

    if (!ctx->ast_program ||
        !ir_try_resolve_returned_function_value_parameter_argument_info(
            ctx->ast_program,
            ctx,
            base_expr,
            NULL,
            &returned_arg_base,
            NULL,
            NULL,
            NULL,
            NULL) ||
        !returned_arg_base) {
        return 0;
    }

    if (returned_arg_base->kind == AST_EXPR_IDENTIFIER &&
        returned_arg_base->as.identifier.name &&
        ir_try_build_identifier_function_value_descriptor(
            ctx,
            binding_name,
            returned_arg_base,
            &returned_parameter_descriptor)) {
        returned_parameter_descriptor.from_returned_function_value_call = 1;
        *out_descriptor = returned_parameter_descriptor;
        return 1;
    }

    if (returned_arg_base->kind == AST_EXPR_CALL &&
        ir_try_build_returned_parameter_passthrough_function_value_descriptor(
            ctx,
            binding_name,
            returned_arg_base,
            &returned_parameter_descriptor)) {
        returned_parameter_descriptor.from_returned_function_value_call = 1;
        *out_descriptor = returned_parameter_descriptor;
        return 1;
    }

    if (returned_arg_base->kind == AST_EXPR_CALL &&
        ir_try_resolve_returned_function_value_argument_target(
            ctx->ast_program,
            ctx,
            returned_arg_base,
            &returned_capture_local_ids,
            &returned_capture_count,
            &returned_target_name) &&
        returned_target_name &&
        returned_target_name[0] != '\0') {
        if (!ir_init_function_object_descriptor_from_static_target_with_payload(
                returned_target_name,
                returned_capture_count > 0u,
                0,
                0,
                1,
                returned_capture_local_ids,
                returned_capture_count,
                (size_t)-1,
                &returned_parameter_descriptor)) {
            return 0;
        }
        returned_parameter_descriptor.from_returned_function_value_call = 1;
        *out_descriptor = returned_parameter_descriptor;
        return 1;
    }

    return 0;
}

static int ir_prepare_materialized_ternary_function_value_tags(IrLowerContext *ctx,
    const char *binding_name,
    const char *then_source_binding_name,
    const char *else_source_binding_name,
    const char *then_target_name,
    const char *else_target_name,
    long long *out_then_tag_value,
    long long *out_else_tag_value) {
    if (out_then_tag_value) {
        *out_then_tag_value = 0;
    }
    if (out_else_tag_value) {
        *out_else_tag_value = 0;
    }
    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !then_source_binding_name || then_source_binding_name[0] == '\0' ||
        !else_source_binding_name || else_source_binding_name[0] == '\0' ||
        !then_target_name || then_target_name[0] == '\0' ||
        !else_target_name || else_target_name[0] == '\0') {
        return 0;
    }

    if (!ir_lower_reset_function_value_target_set(ctx, binding_name)) {
        return 0;
    }

    if ((!ir_lower_clone_function_value_target_set(ctx, binding_name, then_source_binding_name) &&
            !ir_lower_register_function_value_target(
                ctx,
                binding_name,
                then_target_name,
                NULL)) ||
        (!ir_lower_clone_function_value_target_set(ctx, binding_name, else_source_binding_name) &&
            !ir_lower_register_function_value_target(
                ctx,
                binding_name,
                else_target_name,
                NULL))) {
        return 0;
    }

    return ir_lower_register_function_value_target(
               ctx,
               binding_name,
               then_target_name,
               out_then_tag_value) &&
        ir_lower_register_function_value_target(
            ctx,
            binding_name,
            else_target_name,
            out_else_tag_value);
}

static int ir_try_analyze_returned_call_function_value_branch(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_materialize_call_expr,
    const AstExternal **out_producer_external,
    int *out_is_closure_family,
    const char ***out_target_names,
    size_t *out_target_count,
    const char **out_primary_target_name,
    size_t *out_capture_count) {
    IrLowerReturnedFunctionValueFamilyInfo family_info;

    if (out_producer_external) {
        *out_producer_external = NULL;
    }
    if (out_materialize_call_expr) {
        *out_materialize_call_expr = NULL;
    }
    if (out_is_closure_family) {
        *out_is_closure_family = 0;
    }
    if (out_target_names) {
        *out_target_names = NULL;
    }
    if (out_target_count) {
        *out_target_count = 0u;
    }
    if (out_primary_target_name) {
        *out_primary_target_name = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    ir_init_returned_function_value_family_info(&family_info);
    if (!ctx || !ctx->ast_program || !call_expr || !out_materialize_call_expr ||
        !out_producer_external ||
        !out_is_closure_family || !out_target_names || !out_target_count ||
        !out_primary_target_name || !out_capture_count) {
        return 0;
    }

    if (!ir_try_analyze_returned_function_value_family(ctx, call_expr, &family_info)) {
        return 0;
    }

    *out_producer_external = family_info.producer_external;
    *out_materialize_call_expr = family_info.materialize_call_expr;
    *out_is_closure_family = family_info.is_closure_family;
    *out_target_names = family_info.target_names;
    *out_target_count = family_info.target_count;
    *out_primary_target_name = family_info.primary_target_name;
    *out_capture_count = family_info.capture_count;
    family_info.target_names = NULL;
    return 1;
}

static int ir_try_analyze_returned_function_value_family(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrLowerReturnedFunctionValueFamilyInfo *out_info) {
    const AstExpression *returned_base_call_expr = NULL;
    const AstExternal *producer_external = NULL;
    int is_closure_family = 0;
    const char **target_names = NULL;
    size_t target_count = 0u;
    const char *primary_target_name = NULL;
    size_t capture_count = 0u;
    size_t payload_slot_count = 0u;

    if (out_info) {
        ir_init_returned_function_value_family_info(out_info);
    }
    if (!ctx || !ctx->ast_program || !call_expr || !out_info) {
        return 0;
    }

    if (!ir_resolve_returned_function_value_call_producer_info(
            ctx,
            call_expr,
            &returned_base_call_expr,
            &producer_external,
            &is_closure_family) ||
        !returned_base_call_expr ||
        !producer_external ||
        !ir_resolve_returned_function_value_target_info(
            ctx,
            returned_base_call_expr,
            producer_external,
            is_closure_family,
            &target_names,
            &target_count,
            &primary_target_name) ||
        !primary_target_name ||
        primary_target_name[0] == '\0') {
        free((void *)target_names);
        if (!is_closure_family || !primary_target_name || primary_target_name[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-065: returned noncapturing function-value family has no resolvable targets");
        }
        return 0;
    }

    if (!is_closure_family &&
        (!target_names || target_count == 0u)) {
        free((void *)target_names);
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-LOWER-065: returned noncapturing function-value family has no resolvable targets");
        return 0;
    }

    if (is_closure_family) {
        if (!ir_function_returned_closure_payload_slot_count(
                ctx->ast_program,
                ctx->program,
                producer_external,
                &payload_slot_count)) {
            free((void *)target_names);
            return 0;
        }
        capture_count = payload_slot_count;
        if (target_count > 1u) {
            if (capture_count == 0u) {
                free((void *)target_names);
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-LOWER-069: dynamic returned closure family requires tag plus copied captures");
                return 0;
            }
            capture_count -= 1u;
        }
    } else {
        payload_slot_count = 1u;
    }

    out_info->materialize_call_expr = returned_base_call_expr;
    out_info->producer_external = producer_external;
    out_info->target_names = target_names;
    out_info->target_count = target_count;
    out_info->primary_target_name = primary_target_name;
    out_info->capture_count = capture_count;
    out_info->payload_slot_count = payload_slot_count;
    out_info->is_closure_family = is_closure_family;
    out_info->needs_tag_local = (!is_closure_family) || target_count > 1u;
    return 1;
}

static int ir_register_function_value_target_name_array(IrLowerContext *ctx,
    const char *binding_name,
    const char *const *target_names,
    size_t target_count) {
    size_t target_index;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !target_names || target_count == 0u) {
        return 0;
    }

    for (target_index = 0u; target_index < target_count; ++target_index) {
        long long ignored_tag_value = 0;

        if (!target_names[target_index] ||
            target_names[target_index][0] == '\0' ||
            !ir_lower_register_function_value_target(
                ctx,
                binding_name,
                target_names[target_index],
                &ignored_tag_value)) {
            return 0;
        }
    }

    return 1;
}

static int ir_register_returned_function_value_target_family(IrLowerContext *ctx,
    const char *binding_name,
    const char *primary_target_name,
    const char *const *target_names,
    size_t target_count) {
    long long ignored_tag_value = 0;

    if (!ctx || !binding_name || binding_name[0] == '\0' ||
        !primary_target_name || primary_target_name[0] == '\0') {
        return 0;
    }

    if (target_names && target_count > 0u) {
        return ir_register_function_value_target_name_array(
            ctx,
            binding_name,
            target_names,
            target_count);
    }

    return ir_lower_register_function_value_target(
        ctx,
        binding_name,
        primary_target_name,
        &ignored_tag_value);
}

static int ir_copy_named_capture_local_slot_range(IrLowerContext *ctx,
    const size_t *dest_capture_local_ids,
    const size_t *source_capture_local_ids,
    size_t capture_count) {
    size_t capture_index;

    if (!ctx || capture_count == 0u) {
        return capture_count == 0u;
    }
    if (!dest_capture_local_ids || !source_capture_local_ids) {
        return 0;
    }

    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        if (!ir_emit_mov(
                ctx,
                ir_value_local(dest_capture_local_ids[capture_index]),
                ir_value_local(source_capture_local_ids[capture_index]))) {
            return 0;
        }
    }

    return 1;
}

static int ir_materialize_returned_call_branch_into_existing_function_binding(IrLowerContext *ctx,
    const char *binding_name,
    const AstExpression *materialize_call_expr,
    const AstExternal *producer_external,
    int is_closure_family,
    const char *const *target_names,
    size_t target_count,
    const char *primary_target_name,
    size_t tag_local_id,
    const size_t *capture_local_ids,
    size_t capture_count) {
    if (!ctx || !binding_name || binding_name[0] == '\0' || !materialize_call_expr ||
        !producer_external || !primary_target_name || primary_target_name[0] == '\0' ||
        tag_local_id == (size_t)-1) {
        return 0;
    }

    if (!ir_register_returned_function_value_target_family(
            ctx,
            binding_name,
            primary_target_name,
            target_names,
            target_count)) {
        return 0;
    }

    if (!is_closure_family) {
        return ir_lower_call_into_returned_closure_slots(
            ctx,
            materialize_call_expr,
            producer_external,
            tag_local_id,
            1u);
    }

    if (target_count > 1u) {
        size_t *payload_local_ids = NULL;

        if (!ir_materialize_returned_function_value_payload_locals(
                ctx,
                materialize_call_expr,
                producer_external,
                "__retclosure_ternaryslot",
                capture_count + 1u,
                &payload_local_ids)) {
            return 0;
        }
        if (!payload_local_ids ||
            !ir_copy_materialized_returned_payload_into_binding(
                ctx,
                tag_local_id,
                capture_local_ids,
                capture_count,
                payload_local_ids,
                capture_count + 1u)) {
            free(payload_local_ids);
            return 0;
        }
        free(payload_local_ids);
        return 1;
    }

    return ir_lower_call_into_returned_closure_slots(
               ctx,
               materialize_call_expr,
               producer_external,
               capture_count > 0u ? capture_local_ids[0] : tag_local_id,
               capture_count > 0u ? capture_count : 1u) &&
        ir_write_function_value_binding_tag_from_target(
            ctx,
            binding_name,
            primary_target_name,
            tag_local_id,
            NULL);
}

static int ir_emit_materialized_returned_call_family_selection(
    IrLowerContext *ctx,
    const AstExpression *condition_expr,
    const char *binding_name,
    const IrLowerReturnedFunctionValueFamilyInfo *then_family_info,
    const IrLowerReturnedFunctionValueFamilyInfo *else_family_info,
    size_t tag_local_id,
    const size_t *capture_local_ids) {
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;

    if (!ctx || !condition_expr || !binding_name || binding_name[0] == '\0' ||
        !then_family_info || !else_family_info ||
        !then_family_info->materialize_call_expr ||
        !else_family_info->materialize_call_expr ||
        !then_family_info->producer_external ||
        !else_family_info->producer_external ||
        !then_family_info->primary_target_name ||
        !else_family_info->primary_target_name ||
        tag_local_id == (size_t)-1) {
        return 0;
    }

    if (!ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_lower_boolean_branch(
            ctx,
            condition_expr,
            then_block_id,
            else_block_id) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_materialize_returned_call_branch_into_existing_function_binding(
            ctx,
            binding_name,
            then_family_info->materialize_call_expr,
            then_family_info->producer_external,
            then_family_info->is_closure_family,
            then_family_info->target_names,
            then_family_info->target_count,
            then_family_info->primary_target_name,
            tag_local_id,
            capture_local_ids,
            then_family_info->capture_count) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_materialize_returned_call_branch_into_existing_function_binding(
            ctx,
            binding_name,
            else_family_info->materialize_call_expr,
            else_family_info->producer_external,
            else_family_info->is_closure_family,
            else_family_info->target_names,
            else_family_info->target_count,
            else_family_info->primary_target_name,
            tag_local_id,
            capture_local_ids,
            else_family_info->capture_count) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    return 1;
}

static int ir_try_build_returned_call_ternary_function_value_descriptor(
    IrLowerContext *ctx,
    const char *binding_name,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const AstExpression *then_base = NULL;
    const AstExpression *else_base = NULL;
    IrLowerReturnedFunctionValueFamilyInfo then_family_info;
    IrLowerReturnedFunctionValueFamilyInfo else_family_info;
    size_t *capture_local_ids = NULL;
    size_t tag_local_id = (size_t)-1;
    const IrFunctionValueTargetSet *target_set = NULL;
    IrLowerFunctionObjectDescriptor descriptor;
    long long const_tag_value = 0;
    int ok = 0;

    if (!ctx || !binding_name || binding_name[0] == '\0' || !stmt ||
        !base_expr || base_expr->kind != AST_EXPR_TERNARY || !out_descriptor) {
        return 0;
    }
    ir_init_function_object_descriptor(out_descriptor);
    ir_init_returned_function_value_family_info(&then_family_info);
    ir_init_returned_function_value_family_info(&else_family_info);

    then_base = ir_unwrap_function_value_wrapper_expression(base_expr->as.ternary.then_expr);
    else_base = ir_unwrap_function_value_wrapper_expression(base_expr->as.ternary.else_expr);
    if (!then_base || !else_base ||
        then_base->kind != AST_EXPR_CALL ||
        else_base->kind != AST_EXPR_CALL ||
        !ir_try_analyze_returned_function_value_family(ctx, then_base, &then_family_info) ||
        !ir_try_analyze_returned_function_value_family(ctx, else_base, &else_family_info)) {
        ir_free_returned_function_value_family_info(&then_family_info);
        ir_free_returned_function_value_family_info(&else_family_info);
        return 0;
    }

    if (then_family_info.is_closure_family != else_family_info.is_closure_family ||
        then_family_info.capture_count != else_family_info.capture_count) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-072: ternary function-value local initialization currently requires matching function-value payload shapes");
        goto cleanup;
    }

    if (then_family_info.capture_count > 0u &&
        !ir_append_named_capture_local_slots(
            ctx,
            binding_name,
            then_family_info.capture_count,
            &capture_local_ids)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-089: out of memory while lowering ternary returned-call captures");
        goto cleanup;
    }

    {
        char tag_local_name[256];

        snprintf(tag_local_name, sizeof(tag_local_name), "%s$ftag", binding_name);
        if (!ir_function_append_local(
                ctx->function,
                tag_local_name,
                0,
                &tag_local_id,
                ctx->error)) {
            goto cleanup;
        }
    }

    if (!ir_emit_materialized_returned_call_family_selection(
            ctx,
            base_expr->as.ternary.condition,
            binding_name,
            &then_family_info,
            &else_family_info,
            tag_local_id,
            capture_local_ids)) {
        goto cleanup;
    }

    target_set = ir_lower_find_function_value_target_set(ctx, binding_name);
    if (!ir_init_function_object_descriptor_from_static_target_with_payload(
            then_family_info.primary_target_name,
            then_family_info.is_closure_family,
            0,
            0,
            1,
            capture_local_ids,
            then_family_info.capture_count,
            tag_local_id,
            &descriptor)) {
        goto cleanup;
    }
    descriptor.dynamic_target_count = target_set ? target_set->target_count : 0u;
    if (!ir_assign_dynamic_target_name_array_copy(
            target_set ? target_set->target_names : NULL,
            descriptor.dynamic_target_count,
            &descriptor.dynamic_target_names,
            &descriptor.owned_dynamic_target_names)) {
        goto cleanup;
    }
    descriptor.has_dynamic_family = descriptor.dynamic_target_count > 1u;
    if (descriptor.dynamic_target_count == 1u &&
        ir_lower_register_function_value_target(
            ctx,
            binding_name,
            then_family_info.primary_target_name,
            &const_tag_value)) {
        descriptor.has_const_tag_value = 1;
        descriptor.const_tag_value = const_tag_value;
    } else {
        descriptor.has_const_tag_value = 0;
        descriptor.const_tag_value = 0;
    }
    descriptor.payload_has_dynamic_tag =
        descriptor.dynamic_tag_local_id != (size_t)-1 &&
        (!descriptor.is_closure_family || !descriptor.has_const_tag_value);
    descriptor.payload_slot_count = descriptor.capture_count +
        (descriptor.payload_has_dynamic_tag ? 1u : 0u);
    *out_descriptor = descriptor;
    ok = 1;

cleanup:
    if (!ok && capture_local_ids) {
        free(capture_local_ids);
    }
    ir_free_returned_function_value_family_info(&then_family_info);
    ir_free_returned_function_value_family_info(&else_family_info);
    return ok;
}

static int ir_lower_declaration_names(IrLowerContext *ctx,
    const AstStatement *stmt,
    size_t init_expr_limit) {
    size_t i;

    if (!ctx || !stmt) {
        return 0;
    }

    for (i = 0; i < stmt->declaration_name_count; ++i) {
        const char *name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
        size_t local_id = (size_t)-1;
        IrValueRef init_value;
        int declaration_value_kind = stmt->declaration_value_kinds
            ? stmt->declaration_value_kinds[i]
            : AST_DECLARATION_VALUE_INT;

        if (!name || name[0] == '\0') {
            continue;
        }
        if (declaration_value_kind == AST_DECLARATION_VALUE_FUNCTION) {
            const AstExpression *initializer_expr =
                (stmt->expressions && i < init_expr_limit) ? stmt->expressions[i] : NULL;
            const AstExpression *base_expr = NULL;
            const AstExpression *passthrough_expr = NULL;
            IrLowerFunctionObjectDescriptor descriptor;
            int descriptor_owns_capture_local_ids = 0;

            ir_init_function_object_descriptor(&descriptor);
            if (!ir_lower_emit_function_value_wrapper_side_effects(ctx, initializer_expr)) {
                return 0;
            }
            base_expr = ir_unwrap_function_value_wrapper_expression(initializer_expr);

            if (base_expr && base_expr->kind == AST_EXPR_CALL &&
                ir_try_resolve_exact_returned_function_value_parameter_passthrough_expr(
                    ctx,
                    base_expr,
                    &passthrough_expr) &&
                passthrough_expr) {
                base_expr = passthrough_expr;
            }

            if (base_expr && base_expr->kind == AST_EXPR_TERNARY) {
                IrLowerFunctionValueTernaryInfo ternary_info;
                const AstExpression *then_branch_base =
                    ir_unwrap_function_value_wrapper_expression(
                        base_expr->as.ternary.then_expr);
                const AstExpression *else_branch_base =
                    ir_unwrap_function_value_wrapper_expression(
                        base_expr->as.ternary.else_expr);
                int ternary_is_returned_call_family = 0;

                memset(&ternary_info, 0, sizeof(ternary_info));
                if (ir_resolve_identifier_ternary_function_value_info(
                        ctx,
                        base_expr,
                        &ternary_info)) {
                    if (!ir_try_build_ternary_function_value_descriptor(
                            ctx,
                            name,
                            stmt,
                            base_expr,
                            &descriptor)) {
                        return 0;
                    }
                } else {
                    if (then_branch_base &&
                        else_branch_base &&
                        then_branch_base->kind == AST_EXPR_CALL &&
                        else_branch_base->kind == AST_EXPR_CALL) {
                        ternary_is_returned_call_family = 1;
                    }
                    if (!ternary_is_returned_call_family ||
                        !ir_try_build_returned_call_ternary_function_value_descriptor(
                            ctx,
                            name,
                            stmt,
                            base_expr,
                            &descriptor)) {
                        return 0;
                    }
                }
                descriptor_owns_capture_local_ids = 1;
            } else if (base_expr && base_expr->kind == AST_EXPR_CALL &&
                ir_try_build_returned_parameter_passthrough_function_value_descriptor(
                    ctx,
                    name,
                    base_expr,
                    &descriptor)) {
                descriptor_owns_capture_local_ids = 1;
            } else if (base_expr && base_expr->kind == AST_EXPR_CLOSURE) {
                if (!ir_try_build_closure_literal_function_value_descriptor(
                        ctx,
                        name,
                        stmt,
                        i,
                        base_expr,
                        &descriptor)) {
                    return 0;
                }
                descriptor_owns_capture_local_ids = 1;
            } else if (base_expr && base_expr->kind == AST_EXPR_CALL) {
                if (!ir_try_build_returned_function_value_descriptor(
                        ctx,
                        name,
                        base_expr,
                        &descriptor)) {
                    if (ctx->error && ctx->error->message[0] == '\0') {
                        ir_set_error(ctx->error,
                            stmt->line,
                            stmt->column,
                            "IR-LOWER-055: local function-value declaration requires direct function identifier, closure literal, or direct function-valued return call initializer");
                    }
                    return 0;
                }
                descriptor_owns_capture_local_ids = 1;
            } else if (!base_expr || base_expr->kind != AST_EXPR_IDENTIFIER || !base_expr->as.identifier.name) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-055: local function-value declaration requires direct function identifier, closure literal, or direct function-valued return call initializer");
                return 0;
            }
            if ((!base_expr || base_expr->kind != AST_EXPR_TERNARY) &&
                base_expr && base_expr->kind == AST_EXPR_IDENTIFIER &&
                !ir_try_build_identifier_function_value_descriptor(
                    ctx,
                    name,
                    base_expr,
                    &descriptor)) {
                return 0;
            }
            if (!descriptor.direct_target_name || descriptor.direct_target_name[0] == '\0') {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-072: ternary function-value local initialization could not resolve a stable target family");
                if (descriptor_owns_capture_local_ids &&
                    descriptor.capture_local_ids) {
                    free((void *)descriptor.capture_local_ids);
                }
                return 0;
            }
            if (!ir_finalize_function_value_local_binding_transport_from_descriptor(
                    ctx,
                    name,
                    stmt->declaration_is_const,
                    &descriptor)) {
                if (descriptor_owns_capture_local_ids &&
                    descriptor.capture_local_ids) {
                    free((void *)descriptor.capture_local_ids);
                }
                if (ctx->error && ctx->error->message[0] == '\0') {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-INT-017: failed to record local function-value '%s' during IR lowering",
                        name);
                }
                return 0;
            }
            if (descriptor_owns_capture_local_ids && descriptor.capture_local_ids) {
                free((void *)descriptor.capture_local_ids);
            }
            continue;
        }
        if (declaration_value_kind == AST_DECLARATION_VALUE_STRUCT) {
            size_t field_index;
            size_t field_count = 0u;
            size_t slot_count = 0u;
            const AstExpression *initializer_expr =
                (stmt->expressions && i < init_expr_limit) ? stmt->expressions[i] : NULL;
            const char *type_name = stmt->declaration_type_names ? stmt->declaration_type_names[i] : NULL;
            const AstStructType *struct_type = ir_find_struct_type(ctx->ast_program, type_name);
            size_t name_len = strlen(name);

            if (!struct_type || !struct_type->field_names || struct_type->field_count == 0u) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-049: unsupported struct declaration lowering shape");
                return 0;
            }
            field_count = struct_type->field_count;
            for (field_index = 0u; field_index < field_count; ++field_index) {
                char *field_local_name;
                size_t current_local_id = (size_t)-1;
                size_t field_name_len;
                size_t field_slots = 0u;
                size_t slot_index;
                const char *field_type_name =
                    struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL;

                if (!struct_type->field_names[field_index] || struct_type->field_names[field_index][0] == '\0') {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-LOWER-049: unsupported struct declaration lowering shape");
                    return 0;
                }

                if (!ir_aggregate_slot_count_for_kind(ctx->ast_program,
                        (AstDeclarationValueKind)struct_type->field_value_kinds[field_index],
                        field_type_name,
                        &field_slots) ||
                    field_slots == 0u) {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-LOWER-049: unsupported struct declaration lowering shape");
                    return 0;
                }

                field_name_len = strlen(struct_type->field_names[field_index]);
                field_local_name = (char *)malloc(name_len + field_name_len + 16u);
                if (!field_local_name) {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-INT-017: failed to allocate struct local metadata for '%s'",
                        name);
                    return 0;
                }
                for (slot_index = 0u; slot_index < field_slots; ++slot_index) {
                    if (field_slots == 1u) {
                        sprintf(field_local_name, "%s$%s", name, struct_type->field_names[field_index]);
                    } else {
                        sprintf(field_local_name, "%s$%s$%zu", name, struct_type->field_names[field_index], slot_index);
                    }
                    if (!ir_function_append_local(ctx->function,
                            field_local_name,
                            0,
                            &current_local_id,
                            ctx->error)) {
                        free(field_local_name);
                        return 0;
                    }
                    if (field_index == 0u && slot_index == 0u) {
                        local_id = current_local_id;
                    }
                    slot_count += 1u;
                }
                free(field_local_name);
            }

            if (!ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
                    name,
                    local_id,
                    IR_LOWER_BINDING_STRUCT_LOCAL,
                    0u,
                    NULL,
                    slot_count > 1u ? (local_id + 1u) : local_id,
                    slot_count,
                    type_name,
                    0u,
                    NULL,
                    0u,
                    stmt->declaration_is_const,
                    0,
                    0,
                    0)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-017: failed to record struct local '%s' during IR lowering",
                    name);
                return 0;
            }

            if (initializer_expr &&
                !ir_lower_initialize_aggregate_slots(ctx,
                    IR_LOWER_BINDING_STRUCT_LOCAL,
                    type_name,
                    local_id,
                    slot_count,
                    initializer_expr,
                    stmt->line,
                    stmt->column)) {
                return 0;
            }
            continue;
        }
        if (declaration_value_kind == AST_DECLARATION_VALUE_PAIR) {
            size_t second_local_id = (size_t)-1;
            const AstExpression *initializer_expr =
                (stmt->expressions && i < init_expr_limit) ? stmt->expressions[i] : NULL;
            char *first_name = NULL;
            char *second_name = NULL;
            size_t name_len = strlen(name);

            first_name = (char *)malloc(name_len + 7u);
            second_name = (char *)malloc(name_len + 8u);
            if (!first_name || !second_name) {
                free(first_name);
                free(second_name);
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-017: failed to allocate pair local metadata for '%s'",
                    name);
                return 0;
            }
            memcpy(first_name, name, name_len);
            memcpy(first_name + name_len, "$first", 7u);
            memcpy(second_name, name, name_len);
            memcpy(second_name + name_len, "$second", 8u);

            if (!ir_function_append_local(ctx->function, first_name, 0, &local_id, ctx->error) ||
                !ir_function_append_local(ctx->function, second_name, 0, &second_local_id, ctx->error) ||
                !ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
                    name,
                    local_id,
                    IR_LOWER_BINDING_PAIR_LOCAL,
                    0u,
                    NULL,
                    second_local_id,
                    2u,
                    NULL,
                    0u,
                    NULL,
                    0u,
                    stmt->declaration_is_const,
                    0,
                    0,
                    0)) {
                free(first_name);
                free(second_name);
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-017: failed to record pair local '%s' during IR lowering",
                    name);
                return 0;
            }
            free(first_name);
            free(second_name);

            if (initializer_expr &&
                !ir_lower_initialize_aggregate_slots(ctx,
                    IR_LOWER_BINDING_PAIR_LOCAL,
                    NULL,
                    local_id,
                    2u,
                    initializer_expr,
                    stmt->line,
                    stmt->column)) {
                return 0;
            }
            continue;
        }
        if (stmt->declaration_array_ranks && stmt->declaration_array_ranks[i] > 0u) {
            size_t *array_extents = NULL;
            size_t element_count = 0u;
            size_t slot_index;
            const AstExpression *initializer_expr =
                (stmt->expressions && i < init_expr_limit) ? stmt->expressions[i] : NULL;

            if (!ir_compute_array_extents(ctx,
                    stmt->declaration_array_extent_exprs
                        ? stmt->declaration_array_extent_exprs[i]
                        : NULL,
                    stmt->declaration_array_ranks[i],
                    0,
                    &array_extents,
                    &element_count) ||
                !ir_append_array_local_slots(ctx, name, element_count, &local_id) ||
                !ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
                    ctx->function->locals[local_id].source_name,
                    local_id,
                    IR_LOWER_BINDING_ARRAY_LOCAL,
                    stmt->declaration_array_ranks[i],
                    array_extents,
                    0u,
                    0u,
                    NULL,
                    0u,
                    NULL,
                    0u,
                    stmt->declaration_is_const,
                    0,
                    0,
                    0)) {
                free(array_extents);
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-017: failed to record array local '%s' during IR lowering",
                    name);
                return 0;
            }
            for (slot_index = 0u; slot_index < element_count; ++slot_index) {
                ctx->function->locals[local_id + slot_index].array_rank = stmt->declaration_array_ranks[i];
            }
            if (initializer_expr) {
                const AstExpression **initializer_slots = NULL;
                size_t slot_count = 0u;
                int compact_applied = 0;

                if (!ir_expand_array_initializer_slots(initializer_expr,
                        stmt->declaration_array_ranks[i],
                        array_extents,
                        &initializer_slots,
                        &slot_count,
                        ctx->error)) {
                    free(array_extents);
                    return 0;
                }
                if (!ir_try_emit_compact_large_local_array_initializer(
                        ctx,
                        local_id,
                        initializer_slots,
                        slot_count,
                        &compact_applied)) {
                    free(initializer_slots);
                    free(array_extents);
                    return 0;
                }
                if (compact_applied) {
                    free(initializer_slots);
                    free(array_extents);
                    continue;
                }
                for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
                    IrValueRef slot_value = ir_value_immediate(0);

                    if (initializer_slots[slot_index] &&
                        !ir_lower_expression(ctx, initializer_slots[slot_index], &slot_value)) {
                        free(initializer_slots);
                        free(array_extents);
                        return 0;
                    }
                    if (!ir_emit_mov(ctx, ir_value_local(local_id + slot_index), slot_value)) {
                        free(initializer_slots);
                        free(array_extents);
                        return 0;
                    }
                }
                free(initializer_slots);
            }
            free(array_extents);
            continue;
        }

        if (!ir_function_append_local(ctx->function, name, 0, &local_id, ctx->error)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-017: failed to record local '%s' during IR lowering",
                name);
            return 0;
        }
        if (ctx->function->locals && local_id < ctx->function->local_count) {
            ctx->function->locals[local_id].value_type =
                declaration_value_kind == AST_DECLARATION_VALUE_FLOAT
                    ? AST_FUNCTION_RETURN_FLOAT
                    : AST_FUNCTION_RETURN_INT;
        }
        if (stmt->expressions && i < init_expr_limit && stmt->expressions[i]) {
            long long const_value = 0;
            int has_const_value = ir_eval_constant_int_expression(ctx, stmt->expressions[i], &const_value);

            if (!ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
                    ctx->function->locals[local_id].source_name,
                    local_id,
                    IR_LOWER_BINDING_SCALAR_LOCAL,
                    0u,
                    NULL,
                    0u,
                    0u,
                    NULL,
                    0u,
                    NULL,
                    0u,
                    stmt->declaration_is_const,
                    has_const_value,
                    const_value,
                    0)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-017: failed to record local '%s' during IR lowering",
                    name);
                return 0;
            }
            if (!ir_lower_expression(ctx, stmt->expressions[i], &init_value) ||
                !ir_emit_mov(ctx, ir_value_local(local_id), init_value)) {
                return 0;
            }
        } else if (!ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
                ctx->function->locals[local_id].source_name,
                local_id,
                IR_LOWER_BINDING_SCALAR_LOCAL,
                0u,
                NULL,
                0u,
                0u,
                NULL,
                0u,
                NULL,
                0u,
                stmt->declaration_is_const,
                0,
                0,
                0)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-017: failed to record local '%s' during IR lowering",
                name);
            return 0;
        }
    }

    return 1;
}

static int ir_statement_contains_nested_loop(const AstStatement *stmt) {
    size_t i;

    if (!stmt) {
        return 0;
    }

    for (i = 0; i < stmt->child_count; ++i) {
        const AstStatement *child = stmt->children[i];

        if (!child) {
            continue;
        }
        if (child->kind == AST_STMT_WHILE || child->kind == AST_STMT_FOR) {
            return 1;
        }
        if (ir_statement_contains_nested_loop(child)) {
            return 1;
        }
    }

    return 0;
}

static int ir_try_lower_unused_returned_function_value_passthrough_call_statement(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    int *out_handled) {
    const AstExpression *expr = NULL;
    const AstExpression *base = NULL;
    const AstExpression *returned_arg_expr = NULL;
    const AstExpression *returned_arg_base = NULL;
    const char *returned_target_name = NULL;
    IrLowerBindingInfo returned_binding_info;

    if (out_handled) {
        *out_handled = 0;
    }
    if (!ctx || !stmt || stmt->kind != AST_STMT_EXPRESSION ||
        stmt->expression_count != 1u || !stmt->expressions || !ctx->ast_program) {
        return 1;
    }

    expr = stmt->expressions[0];
    base = ir_unwrap_function_value_wrapper_expression(expr);
    if (!base || base->kind != AST_EXPR_CALL) {
        return 1;
    }

    memset(&returned_binding_info, 0, sizeof(returned_binding_info));
    if (!ir_try_resolve_returned_function_value_parameter_argument_info(
            ctx->ast_program,
            ctx,
            base,
            &returned_arg_expr,
            &returned_arg_base,
            NULL,
            NULL,
            &returned_target_name,
            NULL) ||
        !returned_arg_expr ||
        !returned_arg_base ||
        returned_arg_base->kind != AST_EXPR_IDENTIFIER ||
        !returned_arg_base->as.identifier.name ||
        !returned_target_name || returned_target_name[0] == '\0' ||
        !ir_lower_scope_lookup_binding(
            &ctx->scopes,
            returned_arg_base->as.identifier.name,
            &returned_binding_info) ||
        (returned_binding_info.kind != IR_LOWER_BINDING_FUNCTION_LOCAL &&
            returned_binding_info.kind != IR_LOWER_BINDING_CLOSURE_LOCAL)) {
        return 1;
    }

    if (!ir_lower_emit_function_value_wrapper_side_effects(ctx, expr)) {
        return 0;
    }

    if (out_handled) {
        *out_handled = 1;
    }
    return 1;
}

static int ir_try_lower_function_value_assignment_statement(IrLowerContext *ctx,
    const AstStatement *stmt,
    int *out_handled) {
    const AstExpression *expr = NULL;
    const AstExpression *base = NULL;
    const AstExpression *lhs_base = NULL;
    const AstExpression *rhs_base = NULL;
    const AstExpression *rhs_transport_expr = NULL;
    const char *lhs_name = NULL;
    const char *rhs_name = NULL;
    const char *target_name = NULL;
    const AstExpression *returned_arg_expr = NULL;
    const AstExpression *returned_arg_base = NULL;
    const size_t *returned_capture_local_ids = NULL;
    size_t returned_capture_count = 0u;
    const char *returned_target_name = NULL;
    IrLowerBindingInfo lhs_binding_info;
    IrLowerBindingInfo rhs_binding_info;
    IrLowerFunctionObjectDescriptor rhs_returned_descriptor;
    int rhs_returned_descriptor_ready = 0;

    if (out_handled) {
        *out_handled = 0;
    }
    if (!ctx || !stmt || stmt->kind != AST_STMT_EXPRESSION ||
        stmt->expression_count != 1u || !stmt->expressions) {
        return 1;
    }

    expr = stmt->expressions[0];
    base = ir_unwrap_paren_expression(expr);
    if (!base || base->kind != AST_EXPR_BINARY || base->as.binary.op != TOKEN_ASSIGN) {
        return 1;
    }
    lhs_base = ir_unwrap_paren_expression(base->as.binary.left);
    rhs_base = ir_unwrap_paren_expression(base->as.binary.right);
    rhs_transport_expr = base->as.binary.right;
    if (!lhs_base || !rhs_base ||
        lhs_base->kind != AST_EXPR_IDENTIFIER || !lhs_base->as.identifier.name) {
        return 1;
    }

    lhs_name = lhs_base->as.identifier.name;
    if (!ir_lower_scope_lookup_binding(&ctx->scopes, lhs_name, &lhs_binding_info) ||
        (lhs_binding_info.kind != IR_LOWER_BINDING_FUNCTION_LOCAL &&
            lhs_binding_info.kind != IR_LOWER_BINDING_CLOSURE_LOCAL)) {
        return 1;
    }

    if (rhs_transport_expr &&
        !ir_lower_emit_function_value_wrapper_side_effects(ctx, rhs_transport_expr)) {
        return 0;
    }

    rhs_base = ir_unwrap_function_value_wrapper_expression(rhs_transport_expr);
    if (!rhs_base) {
        return 1;
    }

    ir_init_function_object_descriptor(&rhs_returned_descriptor);
    if (rhs_base->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_base =
            ir_unwrap_function_value_wrapper_expression(rhs_base->as.ternary.then_expr);
        const AstExpression *else_base =
            ir_unwrap_function_value_wrapper_expression(rhs_base->as.ternary.else_expr);
        IrLowerReturnedFunctionValueFamilyInfo then_family_info;
        IrLowerReturnedFunctionValueFamilyInfo else_family_info;
        IrLowerFunctionValueTernaryInfo ternary_info;

        ir_init_returned_function_value_family_info(&then_family_info);
        ir_init_returned_function_value_family_info(&else_family_info);
        if (then_base && else_base &&
            then_base->kind == AST_EXPR_CALL &&
            else_base->kind == AST_EXPR_CALL &&
            ir_try_analyze_returned_function_value_family(ctx, then_base, &then_family_info) &&
            ir_try_analyze_returned_function_value_family(ctx, else_base, &else_family_info)) {
            int handled_returned_call_ternary = 0;

            if (lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) {
                if (then_family_info.is_closure_family &&
                    else_family_info.is_closure_family &&
                    lhs_binding_info.function_target_tag_local_id != (size_t)-1 &&
                    lhs_binding_info.closure_capture_count == then_family_info.capture_count &&
                    lhs_binding_info.closure_capture_count == else_family_info.capture_count &&
                    lhs_binding_info.closure_capture_local_ids &&
                    ir_reset_and_register_function_value_binding_target_family(
                        ctx,
                        lhs_name,
                        then_family_info.primary_target_name,
                        then_family_info.target_names,
                        then_family_info.target_count) &&
                    ir_register_returned_function_value_target_family(
                        ctx,
                        lhs_name,
                        else_family_info.primary_target_name,
                        else_family_info.target_names,
                        else_family_info.target_count) &&
                    ir_emit_materialized_returned_call_family_selection(
                        ctx,
                        rhs_base->as.ternary.condition,
                        lhs_name,
                        &then_family_info,
                        &else_family_info,
                        lhs_binding_info.function_target_tag_local_id,
                        lhs_binding_info.closure_capture_local_ids) &&
                    ir_update_function_value_binding_transport(
                        ctx,
                        lhs_name,
                        IR_LOWER_BINDING_CLOSURE_LOCAL,
                        then_family_info.primary_target_name,
                        lhs_binding_info.closure_capture_count,
                        lhs_binding_info.closure_capture_local_ids,
                        lhs_binding_info.function_target_tag_local_id,
                        0,
                        0,
                        1)) {
                    handled_returned_call_ternary = 1;
                }
            } else if (lhs_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL) {
                if (!then_family_info.is_closure_family &&
                    !else_family_info.is_closure_family &&
                    lhs_binding_info.function_target_tag_local_id != (size_t)-1 &&
                    ir_reset_and_register_function_value_binding_target_family(
                        ctx,
                        lhs_name,
                        then_family_info.primary_target_name,
                        then_family_info.target_names,
                        then_family_info.target_count) &&
                    ir_register_returned_function_value_target_family(
                        ctx,
                        lhs_name,
                        else_family_info.primary_target_name,
                        else_family_info.target_names,
                        else_family_info.target_count) &&
                    ir_emit_materialized_returned_call_family_selection(
                        ctx,
                        rhs_base->as.ternary.condition,
                        lhs_name,
                        &then_family_info,
                        &else_family_info,
                        lhs_binding_info.function_target_tag_local_id,
                        NULL) &&
                    ir_update_function_value_binding_transport(
                        ctx,
                        lhs_name,
                        IR_LOWER_BINDING_FUNCTION_LOCAL,
                        then_family_info.primary_target_name,
                        0u,
                        NULL,
                        lhs_binding_info.function_target_tag_local_id,
                        0,
                        0,
                        1)) {
                    handled_returned_call_ternary = 1;
                }
            }
            ir_free_returned_function_value_family_info(&then_family_info);
            ir_free_returned_function_value_family_info(&else_family_info);
            if (handled_returned_call_ternary) {
                if (out_handled) {
                    *out_handled = 1;
                }
                return 1;
            }
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-063: function-value reassignment currently requires matching returned-call ternary payload shapes");
            return 0;
        }
        ir_free_returned_function_value_family_info(&then_family_info);
        ir_free_returned_function_value_family_info(&else_family_info);

        if (!ir_resolve_identifier_ternary_function_value_info(ctx, rhs_base, &ternary_info)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-063: function-value reassignment currently requires matching identifier ternary payload shapes");
            return 0;
        }

        if (lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) {
            if (!ternary_info.then_is_closure_local || !ternary_info.else_is_closure_local ||
                lhs_binding_info.closure_capture_count != ternary_info.then_capture_count ||
                lhs_binding_info.closure_capture_count != ternary_info.else_capture_count ||
                !lhs_binding_info.closure_capture_local_ids) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-063: function-value reassignment currently requires matching closure-local payload shapes");
                return 0;
            }
        } else {
            if (ternary_info.then_is_closure_local || ternary_info.else_is_closure_local) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-063: function-value reassignment from ternary static targets currently requires a non-capturing destination binding");
                return 0;
            }
        }

        if (lhs_binding_info.function_target_tag_local_id == (size_t)-1 ||
            !ir_prepare_materialized_ternary_function_value_tags(
                ctx,
                lhs_name,
                ternary_info.then_base->as.identifier.name,
                ternary_info.else_base->as.identifier.name,
                ternary_info.then_target_name,
                ternary_info.else_target_name,
                &ternary_info.then_const_value,
                &ternary_info.else_const_value)) {
            return 0;
        }

        if (!ir_emit_materialized_ternary_function_value_selection(
                ctx,
                rhs_base->as.ternary.condition,
                ternary_info.then_const_value,
                ternary_info.else_const_value,
                ternary_info.then_tag_local_id,
                ternary_info.else_tag_local_id,
                ternary_info.then_has_const_value,
                ternary_info.else_has_const_value,
                ternary_info.then_capture_local_ids,
                ternary_info.else_capture_local_ids,
                lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL
                    ? lhs_binding_info.closure_capture_count
                    : 0u,
                lhs_binding_info.function_target_tag_local_id,
                lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL
                    ? lhs_binding_info.closure_capture_local_ids
                    : NULL)) {
            return 0;
        }
        {
            IrLowerFunctionObjectDescriptor ternary_descriptor;

            if (!ir_init_function_object_descriptor_from_static_target_with_payload(
                    ternary_info.then_target_name,
                    lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL,
                    0,
                    0,
                    0,
                    lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL
                        ? lhs_binding_info.closure_capture_local_ids
                        : NULL,
                    lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL
                        ? lhs_binding_info.closure_capture_count
                        : 0u,
                    lhs_binding_info.function_target_tag_local_id,
                    &ternary_descriptor) ||
                !ir_update_function_value_binding_transport_from_descriptor(
                    ctx,
                    lhs_name,
                    lhs_binding_info.kind,
                    &ternary_descriptor,
                    lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL
                        ? lhs_binding_info.closure_capture_count
                        : 0u,
                    lhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL
                        ? lhs_binding_info.closure_capture_local_ids
                        : NULL,
                    lhs_binding_info.function_target_tag_local_id)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-091: failed to update ternary function-value binding during reassignment");
                return 0;
            }
        }
        if (out_handled) {
            *out_handled = 1;
        }
        return 1;
    }

    if (rhs_base->kind == AST_EXPR_CALL &&
        ctx->ast_program &&
        ir_try_resolve_returned_function_value_parameter_argument_info(
            ctx->ast_program,
            ctx,
            rhs_base,
            &returned_arg_expr,
            &returned_arg_base,
            &returned_capture_local_ids,
            &returned_capture_count,
            &returned_target_name,
            NULL) &&
        returned_target_name &&
        returned_target_name[0] != '\0' &&
        returned_arg_base &&
        returned_arg_base->kind == AST_EXPR_IDENTIFIER &&
        returned_arg_base->as.identifier.name &&
        ir_lower_scope_lookup_binding(
            &ctx->scopes,
            returned_arg_base->as.identifier.name,
            &rhs_binding_info) &&
        rhs_binding_info.type_name &&
        rhs_binding_info.type_name[0] != '\0' &&
        (rhs_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
            rhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL)) {
        rhs_base = returned_arg_base;
    }

    if (rhs_base->kind == AST_EXPR_CALL &&
        ir_try_build_returned_function_value_descriptor(
            ctx,
            lhs_name,
            rhs_base,
            &rhs_returned_descriptor)) {
        size_t capture_index;

        rhs_returned_descriptor_ready = 1;
        if (rhs_returned_descriptor.is_closure_family) {
            if (lhs_binding_info.kind != IR_LOWER_BINDING_CLOSURE_LOCAL ||
                lhs_binding_info.closure_capture_count != rhs_returned_descriptor.capture_count ||
                (rhs_returned_descriptor.capture_count > 0u &&
                    (!lhs_binding_info.closure_capture_local_ids ||
                        !rhs_returned_descriptor.capture_local_ids))) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-063: function-value reassignment currently requires matching closure-local payload shapes");
                free((void *)rhs_returned_descriptor.capture_local_ids);
                free((void *)rhs_returned_descriptor.dynamic_target_names);
                return 0;
            }
            for (capture_index = 0u; capture_index < rhs_returned_descriptor.capture_count; ++capture_index) {
                if (!ir_emit_mov(
                        ctx,
                        ir_value_local(lhs_binding_info.closure_capture_local_ids[capture_index]),
                        ir_value_local(rhs_returned_descriptor.capture_local_ids[capture_index]))) {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-INT-091: failed to copy returned closure payload during reassignment");
                    free((void *)rhs_returned_descriptor.capture_local_ids);
                    free((void *)rhs_returned_descriptor.dynamic_target_names);
                    return 0;
                }
            }
            if (!ir_update_function_value_binding_transport_from_descriptor(
                    ctx,
                    lhs_name,
                    IR_LOWER_BINDING_CLOSURE_LOCAL,
                    &rhs_returned_descriptor,
                    lhs_binding_info.closure_capture_count,
                    lhs_binding_info.closure_capture_local_ids,
                    lhs_binding_info.function_target_tag_local_id)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-091: failed to update returned closure binding during reassignment");
                free((void *)rhs_returned_descriptor.capture_local_ids);
                free((void *)rhs_returned_descriptor.dynamic_target_names);
                return 0;
            }
            free((void *)rhs_returned_descriptor.capture_local_ids);
            free((void *)rhs_returned_descriptor.dynamic_target_names);
            if (out_handled) {
                *out_handled = 1;
            }
            return 1;
        }

        if (lhs_binding_info.kind != IR_LOWER_BINDING_FUNCTION_LOCAL ||
            lhs_binding_info.function_target_tag_local_id == (size_t)-1 ||
            rhs_returned_descriptor.dynamic_tag_local_id == (size_t)-1 ||
            !ir_emit_mov(
                ctx,
                ir_value_local(lhs_binding_info.function_target_tag_local_id),
                ir_value_local(rhs_returned_descriptor.dynamic_tag_local_id)) ||
            !ir_update_function_value_binding_transport_from_descriptor(
                ctx,
                lhs_name,
                IR_LOWER_BINDING_FUNCTION_LOCAL,
                &rhs_returned_descriptor,
                0u,
                NULL,
                lhs_binding_info.function_target_tag_local_id)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-091: failed to update returned function-value binding during reassignment");
            free((void *)rhs_returned_descriptor.dynamic_target_names);
            return 0;
        }
        free((void *)rhs_returned_descriptor.dynamic_target_names);
        if (out_handled) {
            *out_handled = 1;
        }
        return 1;
    }

    if (rhs_base->kind != AST_EXPR_IDENTIFIER || !rhs_base->as.identifier.name) {
        if (rhs_returned_descriptor_ready) {
            free((void *)rhs_returned_descriptor.capture_local_ids);
            free((void *)rhs_returned_descriptor.dynamic_target_names);
        }
        return 1;
    }

    rhs_name = rhs_base->as.identifier.name;
    if (ir_lower_scope_lookup_binding(&ctx->scopes, rhs_name, &rhs_binding_info) &&
        rhs_binding_info.type_name && rhs_binding_info.type_name[0] != '\0') {
        IrLowerFunctionObjectDescriptor rhs_descriptor;
        int copied_from_binding = 0;

        if ((rhs_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
                rhs_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) &&
            !ir_init_function_object_descriptor_from_binding_info(
                &rhs_binding_info,
                ir_lower_find_function_value_target_set(ctx, rhs_name),
                &rhs_descriptor)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-091: failed to recover function-value binding descriptor during reassignment");
            return 0;
        }

        if (ir_try_copy_function_value_binding_from_binding_descriptor(
                ctx,
                lhs_name,
                rhs_name,
                stmt,
                &lhs_binding_info,
                &rhs_binding_info,
                &rhs_descriptor,
                &copied_from_binding)) {
            if (copied_from_binding) {
                if (out_handled) {
                    *out_handled = 1;
                }
                return 1;
            }
        } else if (copied_from_binding < 0) {
            return 0;
        }
        target_name = rhs_binding_info.type_name;
    } else if (ctx->program && ir_program_find_function_mut(ctx->program, rhs_name)) {
        target_name = rhs_name;
    } else {
        IrFunction *builtin_function = NULL;

        if (ctx->program &&
            ir_ensure_builtin_function_signature(ctx->program, rhs_name, &builtin_function, ctx->error)) {
            target_name = rhs_name;
        }
    }

    if (!target_name || target_name[0] == '\0') {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-063: function-value reassignment currently supports only non-capturing local/top-level targets");
        return 0;
    }
    if (lhs_binding_info.kind != IR_LOWER_BINDING_FUNCTION_LOCAL) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-063: function-value reassignment from static targets currently requires a non-capturing destination binding");
        return 0;
    }
    if (lhs_binding_info.function_target_tag_local_id == (size_t)-1) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-063: function-value reassignment currently requires a runtime tag-capable local binding");
        return 0;
    }
    if (!ir_write_function_value_binding_tag_from_target(
            ctx,
            lhs_name,
            target_name,
            lhs_binding_info.function_target_tag_local_id,
            &lhs_binding_info.const_value)) {
        ir_set_error(ctx->error,
        stmt->line,
        stmt->column,
        "IR-INT-091: failed to update function-value runtime tag during reassignment");
        return 0;
    }
    {
        IrLowerFunctionObjectDescriptor target_descriptor;

        if (!ir_init_function_object_descriptor_from_static_target(
                target_name,
                0,
                1,
                lhs_binding_info.const_value,
                0,
                &target_descriptor) ||
            !ir_update_function_value_binding_transport_from_descriptor(
                ctx,
                lhs_name,
                IR_LOWER_BINDING_FUNCTION_LOCAL,
                &target_descriptor,
                0u,
                NULL,
                lhs_binding_info.function_target_tag_local_id)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-091: failed to update function-value binding during reassignment");
            return 0;
        }
    }
    if (out_handled) {
        *out_handled = 1;
    }
    return 1;
}

static int ir_expression_has_side_effects(const AstExpression *expr) {
    size_t i;

    if (!expr) {
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 0;
    case AST_EXPR_INIT_LIST:
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            if (ir_expression_has_side_effects(
                    expr->as.init_list.items ? expr->as.init_list.items[i] : NULL)) {
                return 1;
            }
        }
        return 0;
    case AST_EXPR_PAREN:
        return ir_expression_has_side_effects(expr->as.inner);
    case AST_EXPR_UNARY:
        if (expr->as.unary.op == TOKEN_PLUS_PLUS || expr->as.unary.op == TOKEN_MINUS_MINUS) {
            return 1;
        }
        return ir_expression_has_side_effects(expr->as.unary.operand);
    case AST_EXPR_POSTFIX:
        if (expr->as.postfix.op == TOKEN_PLUS_PLUS || expr->as.postfix.op == TOKEN_MINUS_MINUS) {
            return 1;
        }
        return ir_expression_has_side_effects(expr->as.postfix.operand);
    case AST_EXPR_SUBSCRIPT:
        return ir_expression_has_side_effects(expr->as.subscript.base) ||
            ir_expression_has_side_effects(expr->as.subscript.index);
    case AST_EXPR_MEMBER:
        return ir_expression_has_side_effects(expr->as.member.base);
    case AST_EXPR_CALL:
        return 1;
    case AST_EXPR_BINARY: {
        TokenType op = expr->as.binary.op;

        if (op == TOKEN_ASSIGN ||
            op == TOKEN_PLUS_ASSIGN ||
            op == TOKEN_MINUS_ASSIGN ||
            op == TOKEN_STAR_ASSIGN ||
            op == TOKEN_SLASH_ASSIGN ||
            op == TOKEN_PERCENT_ASSIGN ||
            op == TOKEN_AMP_ASSIGN ||
            op == TOKEN_CARET_ASSIGN ||
            op == TOKEN_PIPE_ASSIGN ||
            op == TOKEN_SHIFT_LEFT_ASSIGN ||
            op == TOKEN_SHIFT_RIGHT_ASSIGN) {
            return 1;
        }
        return ir_expression_has_side_effects(expr->as.binary.left) ||
            ir_expression_has_side_effects(expr->as.binary.right);
    }
    case AST_EXPR_TERNARY:
        return ir_expression_has_side_effects(expr->as.ternary.condition) ||
            ir_expression_has_side_effects(expr->as.ternary.then_expr) ||
            ir_expression_has_side_effects(expr->as.ternary.else_expr);
    default:
        return 1;
    }
}

static int ir_prepare_condition_scope_snapshot(const IrLowerScopeStack *src_scopes,
    const AstExpression *condition_expr,
    int condition_has_side_effects,
    IrLowerScopeStack *dest_scopes) {
    if (!src_scopes || !dest_scopes) {
        return 0;
    }

    if (!ir_lower_scope_stack_clone(src_scopes, dest_scopes)) {
        return 0;
    }

    if (condition_has_side_effects && condition_expr &&
        !ir_lower_scope_clear_mutated_scalar_locals_in_expr(dest_scopes, condition_expr)) {
        ir_lower_scope_stack_free(dest_scopes);
        return 0;
    }

    return 1;
}

static int ir_lower_scope_clear_scalar_local_fact(IrLowerScopeStack *stack,
    const AstExpression *expr) {
    const AstExpression *base = expr;
    IrLowerBindingInfo binding_info;

    if (!stack || !expr) {
        return 1;
    }

    while (base && base->kind == AST_EXPR_PAREN) {
        base = base->as.inner;
    }
    if (!base || base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 1;
    }

    if (!ir_lower_scope_lookup_binding(stack, base->as.identifier.name, &binding_info)) {
        return 1;
    }
    if (binding_info.kind != IR_LOWER_BINDING_SCALAR_LOCAL) {
        return 1;
    }

    return ir_lower_scope_update_binding_const_value_by_local_id(
        stack,
        binding_info.local_id,
        0,
        0);
}

static int ir_lower_scope_clear_mutated_scalar_locals_in_expr(IrLowerScopeStack *stack,
    const AstExpression *expr) {
    size_t i;

    if (!stack || !expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(
                    stack,
                    expr->as.init_list.items ? expr->as.init_list.items[i] : NULL)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.inner);
    case AST_EXPR_UNARY:
        if ((expr->as.unary.op == TOKEN_PLUS_PLUS || expr->as.unary.op == TOKEN_MINUS_MINUS) &&
            !ir_lower_scope_clear_scalar_local_fact(stack, expr->as.unary.operand)) {
            return 0;
        }
        return ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.unary.operand);
    case AST_EXPR_POSTFIX:
        if ((expr->as.postfix.op == TOKEN_PLUS_PLUS || expr->as.postfix.op == TOKEN_MINUS_MINUS) &&
            !ir_lower_scope_clear_scalar_local_fact(stack, expr->as.postfix.operand)) {
            return 0;
        }
        return ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.postfix.operand);
    case AST_EXPR_SUBSCRIPT:
        return ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.subscript.base) &&
            ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.subscript.index);
    case AST_EXPR_MEMBER:
        return ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.member.base);
    case AST_EXPR_CALL:
        if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.call.callee)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(
                    stack,
                    expr->as.call.args ? expr->as.call.args[i] : NULL)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY: {
        TokenType op = expr->as.binary.op;

        if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.binary.left) ||
            !ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.binary.right)) {
            return 0;
        }
        if ((op == TOKEN_ASSIGN ||
                op == TOKEN_PLUS_ASSIGN ||
                op == TOKEN_MINUS_ASSIGN ||
                op == TOKEN_STAR_ASSIGN ||
                op == TOKEN_SLASH_ASSIGN ||
                op == TOKEN_PERCENT_ASSIGN ||
                op == TOKEN_AMP_ASSIGN ||
                op == TOKEN_CARET_ASSIGN ||
                op == TOKEN_PIPE_ASSIGN ||
                op == TOKEN_SHIFT_LEFT_ASSIGN ||
                op == TOKEN_SHIFT_RIGHT_ASSIGN) &&
            !ir_lower_scope_clear_scalar_local_fact(stack, expr->as.binary.left)) {
            return 0;
        }
        return 1;
    }
    case AST_EXPR_TERNARY:
        return ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.ternary.condition) &&
            ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.ternary.then_expr) &&
            ir_lower_scope_clear_mutated_scalar_locals_in_expr(stack, expr->as.ternary.else_expr);
    default:
        return 1;
    }
}

static int ir_lower_scope_clear_mutated_scalar_locals_in_statement(IrLowerScopeStack *stack,
    const AstStatement *stmt) {
    size_t i;

    if (!stack || !stmt) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND:
        for (i = 0; i < stmt->child_count; ++i) {
            if (!ir_lower_scope_clear_mutated_scalar_locals_in_statement(
                    stack,
                    stmt->children ? stmt->children[i] : NULL)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_DECLARATION:
    case AST_STMT_EXPRESSION:
    case AST_STMT_RETURN:
        for (i = 0; i < stmt->expression_count; ++i) {
            if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(
                    stack,
                    stmt->expressions ? stmt->expressions[i] : NULL)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_IF:
    case AST_STMT_WHILE:
    case AST_STMT_FOR:
        for (i = 0; i < stmt->expression_count; ++i) {
            if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(
                    stack,
                    stmt->expressions ? stmt->expressions[i] : NULL)) {
                return 0;
            }
        }
        for (i = 0; i < stmt->child_count; ++i) {
            if (!ir_lower_scope_clear_mutated_scalar_locals_in_statement(
                    stack,
                    stmt->children ? stmt->children[i] : NULL)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_BREAK:
    case AST_STMT_CONTINUE:
        return 1;
    default:
        return 1;
    }
}

static int ir_eval_constant_binary(TokenType op,
    long long lhs,
    long long rhs,
    long long *out_value) {
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
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 / (long long)rhs32);
        return 1;
    case TOKEN_PERCENT:
        if (rhs32 == 0) {
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)lhs32 % (long long)rhs32);
        return 1;
    case TOKEN_SHIFT_LEFT:
        if (rhs32 < 0 || rhs32 >= 32) {
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)((uint32_t)lhs32 << (uint32_t)rhs32));
        return 1;
    case TOKEN_SHIFT_RIGHT:
        if (rhs32 < 0 || rhs32 >= 32) {
            return 0;
        }
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 >> rhs32));
        return 1;
    case TOKEN_AMP:
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 & rhs32));
        return 1;
    case TOKEN_PIPE:
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 | rhs32));
        return 1;
    case TOKEN_CARET:
        *out_value = ir_normalize_sysy_int_value((long long)(lhs32 ^ rhs32));
        return 1;
    case TOKEN_EQ:
        *out_value = lhs32 == rhs32;
        return 1;
    case TOKEN_NE:
        *out_value = lhs32 != rhs32;
        return 1;
    case TOKEN_LT:
        *out_value = lhs32 < rhs32;
        return 1;
    case TOKEN_LE:
        *out_value = lhs32 <= rhs32;
        return 1;
    case TOKEN_GT:
        *out_value = lhs32 > rhs32;
        return 1;
    case TOKEN_GE:
        *out_value = lhs32 >= rhs32;
        return 1;
    case TOKEN_AND_AND:
        *out_value = (lhs32 != 0 && rhs32 != 0) ? 1 : 0;
        return 1;
    case TOKEN_OR_OR:
        *out_value = (lhs32 != 0 || rhs32 != 0) ? 1 : 0;
        return 1;
    default:
        return 0;
    }
}

static int ir_eval_constant_int_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    long long *out_value) {
    IrLowerBindingInfo binding_info;
    long long lhs;
    long long rhs;

    if (!ctx || !expr || !out_value) {
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        *out_value = ir_normalize_sysy_int_value(expr->as.number_value);
        return 1;
    case AST_EXPR_FLOAT_LITERAL:
        return 0;
    case AST_EXPR_IDENTIFIER:
        if (expr->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, expr->as.identifier.name, &binding_info) &&
            binding_info.array_rank == 0u &&
            binding_info.has_const_value) {
            *out_value = binding_info.const_value;
            return 1;
        }
        if (expr->as.identifier.name && ctx->program) {
            IrGlobal *global = ir_program_find_global_mut(ctx->program, expr->as.identifier.name);

            if (global && global->array_rank == 0u && global->has_initializer && !global->has_runtime_initializer) {
                *out_value = global->initializer_value;
                return 1;
            }
        }
        return 0;
    case AST_EXPR_PAREN:
        return ir_eval_constant_int_expression(ctx, expr->as.inner, out_value);
    case AST_EXPR_UNARY:
        if (!ir_eval_constant_int_expression(ctx, expr->as.unary.operand, &lhs)) {
            return 0;
        }
        switch (expr->as.unary.op) {
        case TOKEN_PLUS:
            *out_value = ir_normalize_sysy_int_value(lhs);
            return 1;
        case TOKEN_MINUS:
            *out_value = ir_normalize_sysy_int_value(-lhs);
            return 1;
        case TOKEN_BANG:
            *out_value = lhs == 0 ? 1 : 0;
            return 1;
        case TOKEN_TILDE:
            *out_value = ir_normalize_sysy_int_value(~lhs);
            return 1;
        default:
            return 0;
        }
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_COMMA) {
            return ir_eval_constant_int_expression(ctx, expr->as.binary.right, out_value);
        }
        if (!ir_eval_constant_int_expression(ctx, expr->as.binary.left, &lhs) ||
            !ir_eval_constant_int_expression(ctx, expr->as.binary.right, &rhs)) {
            return 0;
        }
        return ir_eval_constant_binary(expr->as.binary.op, lhs, rhs, out_value);
    case AST_EXPR_TERNARY:
        if (!ir_eval_constant_int_expression(ctx, expr->as.ternary.condition, &lhs)) {
            return 0;
        }
        if (lhs != 0) {
            return ir_eval_constant_int_expression(ctx, expr->as.ternary.then_expr, out_value);
        }
        return ir_eval_constant_int_expression(ctx, expr->as.ternary.else_expr, out_value);
    default:
        return 0;
    }
}

static int ir_compute_array_extents(IrLowerContext *ctx,
    AstExpression **extent_exprs,
    size_t rank,
    int allow_unspecified_first_extent,
    size_t **out_extents,
    size_t *out_total_element_count) {
    size_t *extents;
    size_t i;
    size_t total = 1u;

    if (out_extents) {
        *out_extents = NULL;
    }
    if (out_total_element_count) {
        *out_total_element_count = 0u;
    }
    if (!ctx || !out_extents || !out_total_element_count || rank == 0u) {
        return 0;
    }

    extents = (size_t *)calloc(rank, sizeof(size_t));
    if (!extents) {
        ir_set_error(ctx->error, 0, 0, "IR-INT-070: out of memory while computing array extents");
        return 0;
    }

    for (i = 0; i < rank; ++i) {
        long long extent_value = 0;

        if (!extent_exprs || !extent_exprs[i]) {
            if (allow_unspecified_first_extent && i == 0u) {
                extents[i] = 0u;
                continue;
            }
            ir_set_error(ctx->error,
                0,
                0,
                "IR-LOWER-031: array extent must be a positive compile-time integer constant");
            free(extents);
            return 0;
        }
        if (!ir_eval_constant_int_expression(ctx, extent_exprs[i], &extent_value) ||
            extent_value <= 0) {
            ir_set_error(ctx->error,
                extent_exprs && extent_exprs[i] ? extent_exprs[i]->line : 0,
                extent_exprs && extent_exprs[i] ? extent_exprs[i]->column : 0,
                "IR-LOWER-031: array extent must be a positive compile-time integer constant");
            free(extents);
            return 0;
        }
        extents[i] = (size_t)extent_value;
        if (extents[i] > 0u) {
            total *= extents[i];
        }
    }

    *out_extents = extents;
    *out_total_element_count = total;
    return 1;
}

static int ir_append_array_local_slots(IrLowerContext *ctx,
    const char *name,
    size_t element_count,
    size_t *out_base_local_id) {
    size_t element_index;
    size_t base_local_id = (size_t)-1;

    if (!ctx || !ctx->function || !name || !out_base_local_id || element_count == 0u) {
        return 0;
    }

    for (element_index = 0; element_index < element_count; ++element_index) {
        size_t local_id = (size_t)-1;
        if (!ir_function_append_local(ctx->function, name, 0, &local_id, ctx->error)) {
            return 0;
        }
        if (element_index == 0u) {
            base_local_id = local_id;
        }
    }

    *out_base_local_id = base_local_id;
    return 1;
}

static size_t ir_array_initializer_element_count(const size_t *extents, size_t rank) {
    size_t total = 1u;
    size_t i;

    if (!extents || rank == 0u) {
        return 0u;
    }
    for (i = 0u; i < rank; ++i) {
        total *= extents[i];
    }
    return total;
}

static const AstExpression *ir_array_initializer_first_scalar_item(const AstExpression *expr) {
    const AstExpression *current = expr;

    while (current && current->kind == AST_EXPR_INIT_LIST) {
        if (current->as.init_list.item_count == 0u) {
            return NULL;
        }
        current = current->as.init_list.items[0];
    }

    return current;
}

static int ir_expand_array_initializer_from_items(const AstExpression *const *items,
    size_t item_count,
    size_t *io_item_index,
    size_t rank,
    const size_t *extents,
    size_t start_index,
    const AstExpression **slots,
    IrError *error,
    int line,
    int column) {
    size_t i;

    if (!io_item_index || !extents || !slots || rank == 0u) {
        ir_set_error(error, line, column, "IR-INT-071: invalid array-initializer expansion contract");
        return 0;
    }

    if (rank == 1u) {
        for (i = 0u; i < extents[0] && *io_item_index < item_count; ++i) {
            const AstExpression *item = items[*io_item_index];

            if (item && item->kind == AST_EXPR_INIT_LIST) {
                slots[start_index + i] = ir_array_initializer_first_scalar_item(item);
                *io_item_index += 1u;
            } else {
                slots[start_index + i] = item;
                *io_item_index += 1u;
            }
        }
        return 1;
    }

    for (i = 0u; i < extents[0] && *io_item_index < item_count; ++i) {
        const AstExpression *item = items[*io_item_index];
        size_t sub_start = start_index + (i * ir_array_initializer_element_count(extents + 1u, rank - 1u));

        if (item && item->kind == AST_EXPR_INIT_LIST) {
            size_t nested_index = 0u;

            if (!ir_expand_array_initializer_from_items(
                    (const AstExpression *const *)item->as.init_list.items,
                    item->as.init_list.item_count,
                    &nested_index,
                    rank - 1u,
                    extents + 1u,
                    sub_start,
                    slots,
                    error,
                    item->line,
                    item->column)) {
                return 0;
            }
            if (nested_index < item->as.init_list.item_count) {
                ir_set_error(error,
                    item->line,
                    item->column,
                    "IR-LOWER-034: too many nested initializers for array subobject");
                return 0;
            }
            *io_item_index += 1u;
        } else {
            if (!ir_expand_array_initializer_from_items(
                    items,
                    item_count,
                    io_item_index,
                    rank - 1u,
                    extents + 1u,
                    sub_start,
                    slots,
                    error,
                    line,
                    column)) {
                return 0;
            }
        }
    }

    return 1;
}

static int ir_expand_array_initializer_slots(const AstExpression *initializer_expr,
    size_t rank,
    const size_t *extents,
    const AstExpression ***out_slots,
    size_t *out_slot_count,
    IrError *error) {
    const AstExpression **slots = NULL;
    size_t slot_count;

    if (out_slots) {
        *out_slots = NULL;
    }
    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!initializer_expr || !out_slots || !out_slot_count || !extents || rank == 0u) {
        ir_set_error(error,
            initializer_expr ? initializer_expr->line : 0,
            initializer_expr ? initializer_expr->column : 0,
            "IR-INT-072: invalid array-initializer slot expansion contract");
        return 0;
    }

    slot_count = ir_array_initializer_element_count(extents, rank);
    slots = (const AstExpression **)calloc(slot_count, sizeof(*slots));
    if (!slots) {
        ir_set_error(error,
            initializer_expr->line,
            initializer_expr->column,
            "IR-INT-073: out of memory while expanding array initializer slots");
        return 0;
    }

    if (initializer_expr->kind == AST_EXPR_INIT_LIST) {
        size_t item_index = 0u;

        if (!ir_expand_array_initializer_from_items(
                (const AstExpression *const *)initializer_expr->as.init_list.items,
                initializer_expr->as.init_list.item_count,
                &item_index,
                rank,
                extents,
                0u,
                slots,
                error,
                initializer_expr->line,
                initializer_expr->column)) {
            free(slots);
            return 0;
        }
        if (item_index < initializer_expr->as.init_list.item_count) {
            ir_set_error(error,
                initializer_expr->line,
                initializer_expr->column,
                "IR-LOWER-035: too many initializers for declared array object");
            free(slots);
            return 0;
        }
    } else {
        size_t item_index = 0u;
        const AstExpression *single_item = initializer_expr;

        if (!ir_expand_array_initializer_from_items(
                &single_item,
                1u,
                &item_index,
                rank,
                extents,
                0u,
                slots,
                error,
                initializer_expr->line,
                initializer_expr->column)) {
            free(slots);
            return 0;
        }
    }

    *out_slots = slots;
    *out_slot_count = slot_count;
    return 1;
}

static int ir_try_emit_compact_large_local_array_initializer(IrLowerContext *ctx,
    size_t base_local_id,
    const AstExpression **initializer_slots,
    size_t slot_count,
    int *out_applied) {
    size_t non_zero_count = 0u;
    size_t first_non_zero_index = 0u;
    size_t slot_index;
    size_t loop_index_local_id = (size_t)-1;
    size_t entry_block_id;
    size_t header_block_id;
    size_t body_block_id;
    size_t exit_block_id;
    IrValueRef index_local;
    IrValueRef index_value;
    IrValueRef compare_value;
    IrValueRef one_value = ir_value_immediate(1);
    IrValueRef zero_value = ir_value_immediate(0);
    IrValueRef slot_count_value;
    IrValueRef array_base_addr;
    IrValueRef byte_offset;
    IrValueRef slot_addr;
    IrValueRef next_index;
    size_t temp_id;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !initializer_slots || slot_count == 0u) {
        return 0;
    }

    if (slot_count < 256u) {
        return 1;
    }

    for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
        const AstExpression *slot_expr = initializer_slots[slot_index];
        long long value = 0;

        if (!slot_expr) {
            continue;
        }
        if (!ir_eval_constant_int_expression(ctx, slot_expr, &value)) {
            return 1;
        }
        value = ir_normalize_sysy_int_value(value);
        if (value != 0) {
            if (non_zero_count == 0u) {
                first_non_zero_index = slot_index;
            }
            ++non_zero_count;
            if (non_zero_count > 8u) {
                return 1;
            }
        }
    }

    if (non_zero_count == slot_count) {
        return 1;
    }

    if (!ir_function_append_local(ctx->function, "__arr_init_i", 0, &loop_index_local_id, ctx->error)) {
        return 0;
    }
    if (!ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
            ctx->function->locals[loop_index_local_id].source_name,
            loop_index_local_id,
            IR_LOWER_BINDING_SCALAR_LOCAL,
            0u,
            NULL,
            0u,
            0u,
            NULL,
            0u,
            NULL,
            0u,
            0,
            0,
            0,
            0)) {
        ir_set_error(ctx->error, 0, 0, "IR-INT-077: failed to record compact array-init loop local");
        return 0;
    }

    index_local = ir_value_local(loop_index_local_id);
    entry_block_id = ctx->block_id;
    if (!ir_emit_mov(ctx, index_local, zero_value) ||
        !ir_function_append_block(ctx->function, &header_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &body_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &exit_block_id, NULL, ctx->error) ||
        !ir_block_set_jump(ir_function_get_block_mut(ctx->function, entry_block_id), header_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = header_block_id;
    ctx->has_block = 1;
    temp_id = ir_allocate_temp(ctx->function);
    index_value = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    compare_value = ir_value_temp(temp_id);
    slot_count_value = ir_value_immediate((long long)slot_count);
    if (!ir_emit_mov(ctx, index_value, index_local) ||
        !ir_emit_binary(ctx, compare_value, IR_BINARY_LT, index_value, slot_count_value) ||
        !ir_block_set_branch(ir_lower_current_block(ctx), compare_value, body_block_id, exit_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = body_block_id;
    ctx->has_block = 1;
    temp_id = ir_allocate_temp(ctx->function);
    array_base_addr = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    byte_offset = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    slot_addr = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    next_index = ir_value_temp(temp_id);
    if (!ir_emit_addr_local(ctx, array_base_addr, base_local_id) ||
        !ir_emit_binary(ctx, byte_offset, IR_BINARY_MUL, index_value, ir_value_immediate(4)) ||
        !ir_emit_binary(ctx, slot_addr, IR_BINARY_ADD, array_base_addr, byte_offset) ||
        !ir_emit_store_indirect(ctx, slot_addr, zero_value) ||
        !ir_emit_binary(ctx, next_index, IR_BINARY_ADD, index_value, one_value) ||
        !ir_emit_mov(ctx, index_local, next_index) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), header_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = exit_block_id;
    ctx->has_block = 1;
    for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
        const AstExpression *slot_expr = initializer_slots[slot_index];
        long long value = 0;

        if (!slot_expr || !ir_eval_constant_int_expression(ctx, slot_expr, &value)) {
            continue;
        }
        value = ir_normalize_sysy_int_value(value);
        if (value == 0) {
            continue;
        }
        if (!ir_emit_mov(ctx, ir_value_local(base_local_id + slot_index), ir_value_immediate(value))) {
            return 0;
        }
    }

    (void)first_non_zero_index;
    if (out_applied) {
        *out_applied = 1;
    }
    return 1;
}

static int ir_lower_while_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *condition_expr;
    size_t entry_block_id;
    size_t header_block_id;
    size_t body_block_id;
    size_t exit_block_id = 0;
    IrLowerScopeStack entry_scopes;
    int condition_known = 0;
    int condition_truthy = 0;
    int condition_constant_known = 0;
    int condition_constant_truthy = 0;
    IrLowerLoopTransferInfo body_info = {1, 0, 0, 0};
    int body_has_nested_loop = 0;
    int condition_has_side_effects = 0;
    int needs_exit_block = 1;
    int loop_target_pushed = 0;

    if (!ctx || !stmt || !ctx->function || !ctx->has_block) {
        return 0;
    }

    memset(&entry_scopes, 0, sizeof(entry_scopes));

    condition_expr = (stmt->has_condition_expression &&
        stmt->condition_expression_index < stmt->expression_count)
        ? stmt->expressions[stmt->condition_expression_index]
        : NULL;
    if (!condition_expr) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-010: while statement missing condition expression");
        return 0;
    }

    if (!ir_try_eval_condition_truthiness_with_scope(ctx,
            condition_expr,
            &condition_known,
            &condition_truthy)) {
        return 0;
    }
    if (!ir_dependency_try_eval_condition_truthiness(condition_expr,
            &condition_constant_known,
            &condition_constant_truthy)) {
        return 0;
    }
    condition_has_side_effects = ir_expression_has_side_effects(condition_expr);
    if (condition_has_side_effects) {
        condition_known = 0;
        condition_truthy = 0;
        condition_constant_known = 0;
        condition_constant_truthy = 0;
    }
    if (condition_known && !condition_truthy) {
        return 1;
    }

    if (!ir_lower_scope_stack_clone(&ctx->scopes, &entry_scopes)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-074: failed to snapshot while-loop scope state during IR lowering");
        return 0;
    }

    if (stmt->child_count >= 1) {
        IrLowerFlowState body_state;

        ir_lower_flow_state_init(&body_state);
        if (!ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[0], &body_state, &body_info)) {
            return 0;
        }
        body_has_nested_loop = ir_statement_contains_nested_loop(stmt->children[0]);
    }
    {
        int body_forces_non_exit =
            (((!body_has_nested_loop) && condition_known && condition_truthy) ||
                (condition_constant_known && condition_constant_truthy)) &&
            !body_info.may_break &&
            !body_info.may_continue &&
            !body_info.may_fallthrough;
        int proven_stays_true =
            !condition_has_side_effects &&
            !body_has_nested_loop &&
            ir_lower_flow_state_proves_loop_condition_stays_true(ctx, stmt);
        needs_exit_block = !(body_forces_non_exit || proven_stays_true);
    }

    entry_block_id = ctx->block_id;
    if (!ir_function_append_block(ctx->function, &header_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &body_block_id, NULL, ctx->error) ||
        (needs_exit_block &&
            !ir_function_append_block(ctx->function, &exit_block_id, NULL, ctx->error)) ||
        !ir_block_set_jump(ir_function_get_block_mut(ctx->function, entry_block_id),
            header_block_id,
            ctx->error)) {
        return 0;
    }

    ctx->block_id = header_block_id;
    ctx->has_block = 1;
    if (condition_known && condition_truthy && !needs_exit_block) {
        if (!ir_block_set_jump(ir_lower_current_block(ctx), body_block_id, ctx->error)) {
            return 0;
        }
    } else if (needs_exit_block) {
        if (!ir_lower_boolean_branch(ctx, condition_expr, body_block_id, exit_block_id)) {
            return 0;
        }
    } else if (!ir_block_set_jump(ir_lower_current_block(ctx), body_block_id, ctx->error)) {
        return 0;
    }

    if (!ir_loop_target_stack_push(&ctx->loop_targets,
            needs_exit_block ? exit_block_id : header_block_id,
            header_block_id,
            ctx->defer_scopes.count)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-023: failed to record loop targets for while-statement lowering");
        return 0;
    }
    loop_target_pushed = 1;

    ctx->block_id = body_block_id;
    ctx->has_block = 1;
    if (stmt->child_count >= 1 && !ir_lower_statement(ctx, stmt->children[0])) {
        if (loop_target_pushed) {
            ir_loop_target_stack_pop(&ctx->loop_targets);
        }
        return 0;
    }
    if (loop_target_pushed) {
        ir_loop_target_stack_pop(&ctx->loop_targets);
    }
    if (ctx->has_block) {
        IrBasicBlock *body_exit_block = ir_lower_current_block(ctx);

        if (body_exit_block && !body_exit_block->has_terminator &&
            !ir_block_set_jump(body_exit_block, header_block_id, ctx->error)) {
            return 0;
        }
    }

    if (needs_exit_block) {
        ctx->block_id = exit_block_id;
        ctx->has_block = 1;
        if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(&entry_scopes, condition_expr) ||
            (stmt->child_count >= 1 &&
                !ir_lower_scope_clear_mutated_scalar_locals_in_statement(
                    &entry_scopes,
                    stmt->children[0]))) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-076: failed to clear while-loop mutated local facts during IR lowering");
            ir_lower_scope_stack_free(&entry_scopes);
            return 0;
        }
        ir_lower_scope_stack_free(&ctx->scopes);
        if (!ir_lower_scope_stack_clone(&entry_scopes, &ctx->scopes)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-074: failed to restore while-loop scope state during IR lowering");
            ir_lower_scope_stack_free(&entry_scopes);
            return 0;
        }
    } else {
        ctx->has_block = 0;
    }
    ir_lower_scope_stack_free(&entry_scopes);
    return 1;
}

static int ir_lower_for_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *condition_expr = NULL;
    const AstExpression *step_expr = NULL;
    const AstExpression *init_expr = NULL;
    IrValueRef unused_value;
    size_t entry_block_id;
    size_t header_block_id;
    size_t body_block_id;
    size_t step_block_id = 0;
    size_t exit_block_id = 0;
    size_t continue_target_block_id;
    size_t init_expr_limit = stmt->expression_count;
    int condition_known = 1;
    int condition_truthy = 1;
    int condition_constant_known = 1;
    int condition_constant_truthy = 1;
    int body_has_continue = 0;
    int body_may_fallthrough = 1;
    IrLowerLoopTransferInfo body_info = {1, 0, 0, 0};
    int body_has_nested_loop = 0;
    int condition_has_side_effects = 0;
    IrLowerScopeStack entry_scopes;
    int needs_step_block = 0;
    int needs_exit_block = 1;
    int scope_pushed = 0;
    int loop_target_pushed = 0;

    if (!ctx || !stmt || !ctx->function || !ctx->has_block) {
        return 0;
    }

    memset(&entry_scopes, 0, sizeof(entry_scopes));

    if (!ir_lower_scope_stack_push(&ctx->scopes)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-022: failed to extend IR lowering scope stack for for-loop");
        return 0;
    }
    scope_pushed = 1;

    if (stmt->has_condition_expression && stmt->condition_expression_index < init_expr_limit) {
        init_expr_limit = stmt->condition_expression_index;
        condition_expr = stmt->expressions[stmt->condition_expression_index];
    }
    if (stmt->has_for_step_expression && stmt->for_step_expression_index < init_expr_limit) {
        init_expr_limit = stmt->for_step_expression_index;
    }
    if (stmt->has_for_step_expression && stmt->for_step_expression_index < stmt->expression_count) {
        step_expr = stmt->expressions[stmt->for_step_expression_index];
    }
    if (stmt->has_for_init_expression && stmt->for_init_expression_index < stmt->expression_count &&
        stmt->declaration_name_count == 0) {
        init_expr = stmt->expressions[stmt->for_init_expression_index];
    }

    if (stmt->declaration_name_count > 0) {
        if (!ir_lower_declaration_names(ctx, stmt, init_expr_limit)) {
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    } else if (init_expr && !ir_lower_expression(ctx, init_expr, &unused_value)) {
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }

    if (condition_expr) {
        condition_known = 0;
        condition_truthy = 0;
        condition_constant_known = 0;
        condition_constant_truthy = 0;
        if (!ir_try_eval_condition_truthiness_with_scope(ctx,
                condition_expr,
                &condition_known,
                &condition_truthy)) {
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        if (!ir_dependency_try_eval_condition_truthiness(condition_expr,
                &condition_constant_known,
                &condition_constant_truthy)) {
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        condition_has_side_effects = ir_expression_has_side_effects(condition_expr);
        if (condition_has_side_effects) {
            condition_known = 0;
            condition_truthy = 0;
            condition_constant_known = 0;
            condition_constant_truthy = 0;
        }
    }

    if (condition_known && !condition_truthy) {
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 1;
    }

    if (!ir_lower_scope_stack_clone(&ctx->scopes, &entry_scopes)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-075: failed to snapshot for-loop scope state during IR lowering");
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }

    if (stmt->child_count >= 1) {
        IrLowerFlowState body_state;

        ir_lower_flow_state_init(&body_state);
        if (!ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[0], &body_state, &body_info)) {
            ir_lower_scope_stack_free(&entry_scopes);
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        body_has_continue = body_info.may_continue;
        body_may_fallthrough = body_info.may_fallthrough;
        body_has_nested_loop = ir_statement_contains_nested_loop(stmt->children[0]);
    }

    {
        int body_forces_non_exit =
            (((!body_has_nested_loop) && condition_known && condition_truthy) ||
                (condition_constant_known && condition_constant_truthy)) &&
            !body_info.may_break &&
            !body_info.may_continue &&
            !body_info.may_fallthrough;
        int proven_stays_true =
            !condition_has_side_effects &&
            !body_has_nested_loop &&
            ir_lower_flow_state_proves_loop_condition_stays_true(ctx, stmt);
        needs_exit_block = !(body_forces_non_exit || proven_stays_true);
    }
    needs_step_block = (step_expr != NULL) || body_has_continue;
    if (needs_step_block && !body_may_fallthrough && !body_has_continue) {
        needs_step_block = 0;
    }

    entry_block_id = ctx->block_id;
    if (!ir_function_append_block(ctx->function, &header_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &body_block_id, NULL, ctx->error) ||
        (needs_step_block &&
            !ir_function_append_block(ctx->function, &step_block_id, NULL, ctx->error)) ||
        (needs_exit_block &&
            !ir_function_append_block(ctx->function, &exit_block_id, NULL, ctx->error)) ||
        !ir_block_set_jump(ir_function_get_block_mut(ctx->function, entry_block_id),
            header_block_id,
            ctx->error)) {
        ir_lower_scope_stack_free(&entry_scopes);
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }

    continue_target_block_id = needs_step_block ? step_block_id : header_block_id;

    ctx->block_id = header_block_id;
    ctx->has_block = 1;
    if (condition_known && condition_truthy && !needs_exit_block) {
        if (!ir_block_set_jump(ir_lower_current_block(ctx), body_block_id, ctx->error)) {
            ir_lower_scope_stack_free(&entry_scopes);
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    } else if (needs_exit_block && condition_expr) {
        if (!ir_lower_boolean_branch(ctx, condition_expr, body_block_id, exit_block_id)) {
            ir_lower_scope_stack_free(&entry_scopes);
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    } else if (!ir_block_set_jump(ir_lower_current_block(ctx), body_block_id, ctx->error)) {
        ir_lower_scope_stack_free(&entry_scopes);
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }

    ctx->block_id = body_block_id;
    ctx->has_block = 1;
    if (!ir_loop_target_stack_push(&ctx->loop_targets,
            needs_exit_block ? exit_block_id : header_block_id,
            continue_target_block_id,
            ctx->defer_scopes.count)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-024: failed to record loop targets for for-statement lowering");
        ir_lower_scope_stack_free(&entry_scopes);
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }
    loop_target_pushed = 1;
    if (stmt->child_count >= 1 && !ir_lower_statement(ctx, stmt->children[0])) {
        if (loop_target_pushed) {
            ir_loop_target_stack_pop(&ctx->loop_targets);
        }
        ir_lower_scope_stack_free(&entry_scopes);
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }
    if (loop_target_pushed) {
        ir_loop_target_stack_pop(&ctx->loop_targets);
    }
    if (ctx->has_block) {
        IrBasicBlock *body_exit_block = ir_lower_current_block(ctx);

        if (body_exit_block && !body_exit_block->has_terminator &&
            !ir_block_set_jump(body_exit_block, continue_target_block_id, ctx->error)) {
            ir_lower_scope_stack_free(&entry_scopes);
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    }

    if (needs_step_block) {
        ctx->block_id = step_block_id;
        ctx->has_block = 1;
        if (step_expr && !ir_lower_expression(ctx, step_expr, &unused_value)) {
            ir_lower_scope_stack_free(&entry_scopes);
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        if (ctx->has_block && !ir_lower_current_block(ctx)->has_terminator &&
            !ir_block_set_jump(ir_lower_current_block(ctx), header_block_id, ctx->error)) {
            ir_lower_scope_stack_free(&entry_scopes);
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    }

    if (needs_exit_block) {
        ctx->block_id = exit_block_id;
        ctx->has_block = 1;
        if ((condition_expr &&
                !ir_lower_scope_clear_mutated_scalar_locals_in_expr(&entry_scopes, condition_expr)) ||
            (step_expr &&
                !ir_lower_scope_clear_mutated_scalar_locals_in_expr(&entry_scopes, step_expr)) ||
            (stmt->child_count >= 1 &&
                !ir_lower_scope_clear_mutated_scalar_locals_in_statement(
                    &entry_scopes,
                    stmt->children[0]))) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-076: failed to clear for-loop mutated local facts during IR lowering");
            ir_lower_scope_stack_free(&entry_scopes);
            return 0;
        }
        ir_lower_scope_stack_free(&ctx->scopes);
        if (!ir_lower_scope_stack_clone(&entry_scopes, &ctx->scopes)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-075: failed to restore for-loop scope state during IR lowering");
            ir_lower_scope_stack_free(&entry_scopes);
            return 0;
        }
    } else {
        ctx->has_block = 0;
    }
    ir_lower_scope_stack_free(&entry_scopes);
    if (scope_pushed) {
        ir_lower_scope_stack_pop(&ctx->scopes);
    }
    return 1;
}

static int ir_lower_if_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *condition_expr;
    size_t then_block_id;
    size_t else_block_id;
    size_t continue_block_id;
    size_t then_exit_block_id = 0;
    size_t else_exit_block_id = 0;
    IrLowerScopeStack entry_scopes;
    IrLowerScopeStack then_scopes;
    IrLowerScopeStack else_scopes;
    int has_else;
    int condition_known = 0;
    int condition_truthy = 0;
    int condition_has_side_effects = 0;
    int then_fallthrough;
    int else_fallthrough;
    int ok = 0;

    if (!ctx || !stmt || !ctx->function || !ctx->has_block) {
        return 0;
    }

    memset(&entry_scopes, 0, sizeof(entry_scopes));
    memset(&then_scopes, 0, sizeof(then_scopes));
    memset(&else_scopes, 0, sizeof(else_scopes));

    condition_expr = (stmt->has_condition_expression &&
        stmt->condition_expression_index < stmt->expression_count)
        ? stmt->expressions[stmt->condition_expression_index]
        : NULL;
    if (!condition_expr) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-007: if statement missing condition expression");
        goto cleanup;
    }

    has_else = stmt->child_count >= 2;
    if (!ir_try_eval_condition_truthiness_with_scope(ctx,
            condition_expr,
            &condition_known,
            &condition_truthy)) {
        goto cleanup;
    }
    condition_has_side_effects = ir_expression_has_side_effects(condition_expr);
    if (condition_has_side_effects) {
        condition_known = 0;
        condition_truthy = 0;
    }

    if (condition_known) {
        if (condition_truthy) {
            ok = stmt->child_count >= 1 ? ir_lower_statement(ctx, stmt->children[0]) : 1;
            goto cleanup;
        }
        ok = has_else ? ir_lower_statement(ctx, stmt->children[1]) : 1;
        goto cleanup;
    }

    if (!ir_prepare_condition_scope_snapshot(&ctx->scopes,
            condition_expr,
            condition_has_side_effects,
            &entry_scopes) ||
        !ir_prepare_condition_scope_snapshot(&ctx->scopes,
            condition_expr,
            condition_has_side_effects,
            &then_scopes) ||
        !ir_prepare_condition_scope_snapshot(&ctx->scopes,
            condition_expr,
            condition_has_side_effects,
            &else_scopes)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-019: failed to snapshot if-branch scope state during IR lowering");
        goto cleanup;
    }

    if (!ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error)) {
        goto cleanup;
    }

    if (has_else) {
        if (!ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
            !ir_lower_boolean_branch(ctx, condition_expr, then_block_id, else_block_id)) {
            goto cleanup;
        }
    } else {
        if (!ir_function_append_block(ctx->function, &continue_block_id, NULL, ctx->error) ||
            !ir_lower_boolean_branch(ctx, condition_expr, then_block_id, continue_block_id)) {
            goto cleanup;
        }
        else_block_id = continue_block_id;
    }

    ir_lower_scope_stack_free(&ctx->scopes);
    ctx->scopes = then_scopes;
    memset(&then_scopes, 0, sizeof(then_scopes));
    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (stmt->child_count >= 1 && !ir_lower_statement(ctx, stmt->children[0])) {
        goto cleanup;
    }
    then_fallthrough = ctx->has_block && !ir_lower_current_block(ctx)->has_terminator;
    if (ctx->has_block) {
        then_exit_block_id = ctx->block_id;
    }
    then_scopes = ctx->scopes;
    memset(&ctx->scopes, 0, sizeof(ctx->scopes));

    if (has_else) {
        ir_lower_scope_stack_free(&ctx->scopes);
        ctx->scopes = else_scopes;
        memset(&else_scopes, 0, sizeof(else_scopes));
        ctx->block_id = else_block_id;
        ctx->has_block = 1;
        if (!ir_lower_statement(ctx, stmt->children[1])) {
            goto cleanup;
        }
        else_fallthrough = ctx->has_block && !ir_lower_current_block(ctx)->has_terminator;
        if (ctx->has_block) {
            else_exit_block_id = ctx->block_id;
        }
        else_scopes = ctx->scopes;
        memset(&ctx->scopes, 0, sizeof(ctx->scopes));

        if (then_fallthrough || else_fallthrough) {
            IrBasicBlock *then_exit_block;
            IrBasicBlock *else_exit_block;

            if (!ir_function_append_block(ctx->function, &continue_block_id, NULL, ctx->error)) {
                goto cleanup;
            }
            then_exit_block = ir_function_get_block_mut(ctx->function, then_exit_block_id);
            else_exit_block = ir_function_get_block_mut(ctx->function, else_exit_block_id);
            if (then_fallthrough &&
                !ir_block_set_jump(then_exit_block, continue_block_id, ctx->error)) {
                goto cleanup;
            }
            if (else_fallthrough &&
                !ir_block_set_jump(else_exit_block, continue_block_id, ctx->error)) {
                goto cleanup;
            }
            ctx->block_id = continue_block_id;
            ctx->has_block = 1;
        } else {
            ctx->has_block = 0;
        }

        if (then_fallthrough && else_fallthrough) {
            if (!ir_lower_scope_stack_clone(&entry_scopes, &ctx->scopes) ||
                !ir_lower_scope_stack_merge_const_facts(&ctx->scopes, &then_scopes, &else_scopes)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-019: failed to merge if-branch scope state during IR lowering");
                goto cleanup;
            }
        } else if (then_fallthrough) {
            ctx->scopes = then_scopes;
            memset(&then_scopes, 0, sizeof(then_scopes));
        } else if (else_fallthrough) {
            ctx->scopes = else_scopes;
            memset(&else_scopes, 0, sizeof(else_scopes));
        } else {
            if (!ir_lower_scope_stack_clone(&entry_scopes, &ctx->scopes)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-019: failed to restore if-branch scope state during IR lowering");
                goto cleanup;
            }
        }
    } else {
        IrBasicBlock *then_exit_block = ir_function_get_block_mut(ctx->function, then_exit_block_id);

        if (then_fallthrough &&
            !ir_block_set_jump(then_exit_block, continue_block_id, ctx->error)) {
            goto cleanup;
        }
        ctx->block_id = continue_block_id;
        ctx->has_block = 1;

        if (then_fallthrough) {
            if (!ir_lower_scope_stack_clone(&entry_scopes, &ctx->scopes) ||
                !ir_lower_scope_stack_merge_const_facts(&ctx->scopes, &then_scopes, &entry_scopes)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-019: failed to merge if/no-else scope state during IR lowering");
                goto cleanup;
            }
        } else {
            if (!ir_lower_scope_stack_clone(&entry_scopes, &ctx->scopes)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-019: failed to restore if scope state during IR lowering");
                goto cleanup;
            }
        }
    }

    ok = 1;

cleanup:
    ir_lower_scope_stack_free(&entry_scopes);
    ir_lower_scope_stack_free(&then_scopes);
    ir_lower_scope_stack_free(&else_scopes);
    return ok;
}

static int ir_lower_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    size_t i;
    IrBasicBlock *block;

    if (!ctx || !stmt) {
        return 0;
    }
    if (!ctx->has_block) {
        return 1;
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }
    if (block->has_terminator) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND: {
        size_t defer_base = ctx->defer_entries.count;

        if (!ir_lower_scope_stack_push(&ctx->scopes)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-016: failed to extend IR lowering scope stack");
            return 0;
        }
        if (!ir_lower_defer_scope_stack_push(&ctx->defer_scopes, defer_base)) {
            ir_lower_scope_stack_pop(&ctx->scopes);
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-070: failed to extend defer scope stack during IR lowering");
            return 0;
        }
        for (i = 0; i < stmt->child_count; ++i) {
            if (!ir_lower_statement(ctx, stmt->children[i])) {
                ir_lower_defer_scope_stack_pop(&ctx->defer_scopes);
                ir_lower_scope_stack_pop(&ctx->scopes);
                return 0;
            }
            if (!ctx->has_block || ir_lower_current_block(ctx)->has_terminator) {
                break;
            }
        }
        if (ctx->has_block && !ir_lower_current_block(ctx)->has_terminator &&
            !ir_lower_emit_defer_entries(ctx, defer_base)) {
            ir_lower_defer_scope_stack_pop(&ctx->defer_scopes);
            ir_lower_scope_stack_pop(&ctx->scopes);
            return 0;
        }
        ir_lower_defer_entry_stack_pop_to(&ctx->defer_entries, defer_base);
        ir_lower_defer_scope_stack_pop(&ctx->defer_scopes);
        ir_lower_scope_stack_pop(&ctx->scopes);
        return 1;
    }
    case AST_STMT_DEFER:
        if (stmt->child_count != 1u || !stmt->children || !stmt->children[0]) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-025: malformed defer statement payload during IR lowering");
            return 0;
        }
        if (!ir_lower_defer_entry_stack_push(&ctx->defer_entries, stmt->children[0])) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-071: failed to register defer payload during IR lowering");
            return 0;
        }
        return 1;
    case AST_STMT_FNDEFER:
        if (stmt->child_count != 1u || !stmt->children || !stmt->children[0]) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-026: malformed fndefer statement payload during IR lowering");
            return 0;
        }
        {
            size_t repeat_count_local_id = (size_t)-1;

            if (!ir_lower_emit_fndefer_site_registration(ctx, stmt, &repeat_count_local_id)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-080: failed to register function-exit defer payload during IR lowering");
                return 0;
            }
            (void)repeat_count_local_id;
        }
        return 1;
    case AST_STMT_CAPDEFER: {
        const AstStatement *payload_stmt;
        const char **capture_names = NULL;
        size_t *capture_local_ids = NULL;
        size_t capture_index;

        if (stmt->child_count != 1u || !stmt->children || !stmt->children[0]) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-049: malformed capdefer statement payload during IR lowering");
            return 0;
        }
        if (stmt->capture_name_count == 0u || !stmt->capture_names ||
            stmt->capture_expression_count != stmt->capture_name_count ||
            !stmt->capture_expressions) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-050: malformed capdefer capture list during IR lowering");
            return 0;
        }

        payload_stmt = stmt->children[0];
        capture_names = (const char **)malloc(stmt->capture_name_count * sizeof(const char *));
        capture_local_ids = (size_t *)malloc(stmt->capture_name_count * sizeof(size_t));
        if (!capture_names || !capture_local_ids) {
            free(capture_names);
            free(capture_local_ids);
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-081: failed to allocate capdefer capture metadata during IR lowering");
            return 0;
        }

        for (capture_index = 0u; capture_index < stmt->capture_name_count; ++capture_index) {
            IrValueRef captured_value;
            size_t local_id;
            char hidden_name[128];

            snprintf(hidden_name,
                sizeof(hidden_name),
                "__capdefer_%zu_%s",
                capture_index,
                stmt->capture_names[capture_index] ? stmt->capture_names[capture_index] : "capture");
            if (!ir_function_append_local(ctx->function, hidden_name, 0, &local_id, ctx->error)) {
                free(capture_names);
                free(capture_local_ids);
                return 0;
            }
            if (!ir_lower_expression(ctx, stmt->capture_expressions[capture_index], &captured_value) ||
                !ir_emit_mov(ctx, ir_value_local(local_id), captured_value)) {
                free(capture_names);
                free(capture_local_ids);
                return 0;
            }
            capture_names[capture_index] = stmt->capture_names[capture_index];
            capture_local_ids[capture_index] = local_id;
        }

        if (!ir_lower_function_defer_entry_stack_push_with_captures(
                &ctx->function_defer_entries,
                payload_stmt,
                capture_names,
                capture_local_ids,
                stmt->capture_name_count)) {
            free(capture_names);
            free(capture_local_ids);
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-082: failed to register capdefer payload during IR lowering");
            return 0;
        }
        return 1;
    }
    case AST_STMT_DECLARATION:
        return ir_lower_declaration_names(ctx, stmt, stmt->expression_count);
    case AST_STMT_EXPRESSION:
        for (i = 0; i < stmt->expression_count; ++i) {
            IrValueRef unused_value;
            int handled = 0;
            if (i == 0u &&
                !ir_try_lower_unused_returned_function_value_passthrough_call_statement(
                    ctx,
                    stmt,
                    &handled)) {
                return 0;
            }
            if (handled) {
                continue;
            }
            if (i == 0u && !ir_try_lower_function_value_assignment_statement(ctx, stmt, &handled)) {
                return 0;
            }
            if (handled) {
                continue;
            }
            if (stmt->expressions[i] && !ir_lower_expression(ctx, stmt->expressions[i], &unused_value)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_IF:
        return ir_lower_if_statement(ctx, stmt);
    case AST_STMT_WHILE:
        return ir_lower_while_statement(ctx, stmt);
    case AST_STMT_FOR:
        return ir_lower_for_statement(ctx, stmt);
    case AST_STMT_RETURN:
        if (!ir_lower_return_statement(ctx, stmt)) {
            if (ctx && ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-114: return-statement lowering failed without a specific diagnostic");
            }
            return 0;
        }
        return 1;
    case AST_STMT_BREAK:
        return ir_lower_loop_control_statement(ctx, stmt, 0);
    case AST_STMT_CONTINUE:
        return ir_lower_loop_control_statement(ctx, stmt, 1);
    default:
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-008: statement kind '%s' is not supported in IR v1",
            ast_statement_kind_name(stmt->kind));
        return 0;
    }
}

#endif
