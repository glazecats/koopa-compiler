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
    const char *names[64];
    size_t lengths[64];
    unsigned char known[64];
    long long values[64];
    size_t count;
    size_t scope_offsets[32];
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

        if (cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
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

        if (!cond_known && cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
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

        if (cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
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

        if (cond_expr &&
            ir_lower_flow_state_eval_expr(ctx, cond_expr, state, &cond_truthy)) {
            cond_known = 1;
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
    size_t continue_target) {
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

    target_block_id = is_continue ? loop_target->continue_target : loop_target->break_target;
    return ir_block_set_jump(block, target_block_id, ctx->error);
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

        if (!name || name[0] == '\0') {
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
                    stmt->declaration_is_const,
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

                if (!ir_expand_array_initializer_slots(initializer_expr,
                        stmt->declaration_array_ranks[i],
                        array_extents,
                        &initializer_slots,
                        &slot_count,
                        ctx->error)) {
                    free(array_extents);
                    return 0;
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
        if (stmt->expressions && i < init_expr_limit && stmt->expressions[i]) {
            long long const_value = 0;
            int has_const_value = ir_eval_constant_int_expression(ctx, stmt->expressions[i], &const_value);

            if (!ir_lower_scope_add_binding_with_metadata(&ctx->scopes,
                    ctx->function->locals[local_id].source_name,
                    local_id,
                    IR_LOWER_BINDING_SCALAR_LOCAL,
                    0u,
                    NULL,
                    stmt->declaration_is_const,
                    has_const_value,
                    const_value)) {
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
                stmt->declaration_is_const,
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

static int ir_lower_while_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *condition_expr;
    size_t entry_block_id;
    size_t header_block_id;
    size_t body_block_id;
    size_t exit_block_id = 0;
    int condition_known = 0;
    int condition_truthy = 0;
    int condition_constant_known = 0;
    int condition_constant_truthy = 0;
    IrLowerLoopTransferInfo body_info = {1, 0, 0, 0};
    int body_has_nested_loop = 0;
    int needs_exit_block = 1;
    int loop_target_pushed = 0;

    if (!ctx || !stmt || !ctx->function || !ctx->has_block) {
        return 0;
    }

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
    if (condition_known && !condition_truthy) {
        return 1;
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
            !body_has_nested_loop && ir_lower_flow_state_proves_loop_condition_stays_true(ctx, stmt);
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
            header_block_id)) {
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
    } else {
        ctx->has_block = 0;
    }
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
    int needs_step_block = 0;
    int needs_exit_block = 1;
    int scope_pushed = 0;
    int loop_target_pushed = 0;

    if (!ctx || !stmt || !ctx->function || !ctx->has_block) {
        return 0;
    }

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
    }

    if (condition_known && !condition_truthy) {
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 1;
    }

    if (stmt->child_count >= 1) {
        IrLowerFlowState body_state;

        ir_lower_flow_state_init(&body_state);
        if (!ir_lower_flow_state_analyze_loop_transfer(ctx, stmt->children[0], &body_state, &body_info)) {
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
            !body_has_nested_loop && ir_lower_flow_state_proves_loop_condition_stays_true(ctx, stmt);
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
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    } else if (needs_exit_block && condition_expr) {
        if (!ir_lower_boolean_branch(ctx, condition_expr, body_block_id, exit_block_id)) {
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    } else if (!ir_block_set_jump(ir_lower_current_block(ctx), body_block_id, ctx->error)) {
        if (scope_pushed) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        return 0;
    }

    ctx->block_id = body_block_id;
    ctx->has_block = 1;
    if (!ir_loop_target_stack_push(&ctx->loop_targets,
            needs_exit_block ? exit_block_id : header_block_id,
            continue_target_block_id)) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-INT-024: failed to record loop targets for for-statement lowering");
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
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        if (ctx->has_block && !ir_lower_current_block(ctx)->has_terminator &&
            !ir_block_set_jump(ir_lower_current_block(ctx), header_block_id, ctx->error)) {
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    }

    if (needs_exit_block) {
        ctx->block_id = exit_block_id;
        ctx->has_block = 1;
    } else {
        ctx->has_block = 0;
    }
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

    if (condition_known) {
        if (condition_truthy) {
            ok = stmt->child_count >= 1 ? ir_lower_statement(ctx, stmt->children[0]) : 1;
            goto cleanup;
        }
        ok = has_else ? ir_lower_statement(ctx, stmt->children[1]) : 1;
        goto cleanup;
    }

    if (!ir_lower_scope_stack_clone(&ctx->scopes, &entry_scopes) ||
        !ir_lower_scope_stack_clone(&ctx->scopes, &then_scopes) ||
        !ir_lower_scope_stack_clone(&ctx->scopes, &else_scopes)) {
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
            ctx->scopes = then_scopes;
            memset(&then_scopes, 0, sizeof(then_scopes));
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
    case AST_STMT_COMPOUND:
        if (!ir_lower_scope_stack_push(&ctx->scopes)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-016: failed to extend IR lowering scope stack");
            return 0;
        }
        for (i = 0; i < stmt->child_count; ++i) {
            if (!ir_lower_statement(ctx, stmt->children[i])) {
                ir_lower_scope_stack_pop(&ctx->scopes);
                return 0;
            }
            if (!ctx->has_block || ir_lower_current_block(ctx)->has_terminator) {
                break;
            }
        }
        ir_lower_scope_stack_pop(&ctx->scopes);
        return 1;
    case AST_STMT_DECLARATION:
        return ir_lower_declaration_names(ctx, stmt, stmt->expression_count);
    case AST_STMT_EXPRESSION:
        for (i = 0; i < stmt->expression_count; ++i) {
            IrValueRef unused_value;
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
        return ir_lower_return_statement(ctx, stmt);
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
