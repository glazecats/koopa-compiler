#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_dependency_try_eval_condition_truthiness(const AstExpression *expr,
    int *out_known,
    int *out_truthy);

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
        size_t local_id;
        IrValueRef init_value;

        if (!name || name[0] == '\0') {
            continue;
        }
        if (!ir_function_append_local(ctx->function, name, 0, &local_id, ctx->error) ||
            !ir_lower_scope_add_binding(&ctx->scopes,
                ctx->function->locals[local_id].source_name,
                local_id)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-017: failed to record local '%s' during IR lowering",
                name);
            return 0;
        }
        if (stmt->expressions && i < init_expr_limit && stmt->expressions[i]) {
            if (!ir_lower_expression(ctx, stmt->expressions[i], &init_value) ||
                !ir_emit_mov(ctx, ir_value_local(local_id), init_value)) {
                return 0;
            }
        }
    }

    return 1;
}

static int ir_statement_has_loop_control_for_current_loop(const AstStatement *stmt,
    int nested_loop_depth,
    int is_continue) {
    size_t i;

    if (!stmt) {
        return 0;
    }

    switch (stmt->kind) {
    case AST_STMT_BREAK:
        return !is_continue && nested_loop_depth == 0;
    case AST_STMT_CONTINUE:
        return is_continue && nested_loop_depth == 0;
    case AST_STMT_WHILE:
    case AST_STMT_FOR:
        if (stmt->child_count >= 1) {
            return ir_statement_has_loop_control_for_current_loop(stmt->children[0],
                nested_loop_depth + 1,
                is_continue);
        }
        return 0;
    default:
        for (i = 0; i < stmt->child_count; ++i) {
            if (ir_statement_has_loop_control_for_current_loop(stmt->children[i],
                    nested_loop_depth,
                    is_continue)) {
                return 1;
            }
        }
        return 0;
    }
}

static int ir_statement_has_break_targeting_current_loop(const AstStatement *stmt) {
    return ir_statement_has_loop_control_for_current_loop(stmt, 0, 0);
}

static int ir_statement_has_continue_targeting_current_loop(const AstStatement *stmt) {
    return ir_statement_has_loop_control_for_current_loop(stmt, 0, 1);
}

static int ir_statement_may_fallthrough_for_lowering(const AstStatement *stmt,
    int *out_may_fallthrough) {
    size_t i;

    if (!out_may_fallthrough) {
        return 0;
    }
    if (!stmt) {
        *out_may_fallthrough = 1;
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND:
        {
            int may_fallthrough = 1;

            for (i = 0; i < stmt->child_count && may_fallthrough; ++i) {
                if (!ir_statement_may_fallthrough_for_lowering(stmt->children[i],
                        &may_fallthrough)) {
                    return 0;
                }
            }
            *out_may_fallthrough = may_fallthrough;
            return 1;
        }
    case AST_STMT_RETURN:
    case AST_STMT_BREAK:
    case AST_STMT_CONTINUE:
        *out_may_fallthrough = 0;
        return 1;
    case AST_STMT_IF:
        {
            const AstExpression *condition_expr = (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count)
                ? stmt->expressions[stmt->condition_expression_index]
                : NULL;
            int condition_known = 0;
            int condition_truthy = 0;
            int then_may_fallthrough = 1;
            int else_may_fallthrough = 1;

            if (condition_expr &&
                !ir_dependency_try_eval_condition_truthiness(condition_expr,
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known) {
                if (condition_truthy) {
                    if (stmt->child_count >= 1) {
                        return ir_statement_may_fallthrough_for_lowering(stmt->children[0],
                            out_may_fallthrough);
                    }
                    *out_may_fallthrough = 1;
                    return 1;
                }

                if (stmt->child_count >= 2) {
                    return ir_statement_may_fallthrough_for_lowering(stmt->children[1],
                        out_may_fallthrough);
                }
                *out_may_fallthrough = 1;
                return 1;
            }

            if (stmt->child_count >= 1 &&
                !ir_statement_may_fallthrough_for_lowering(stmt->children[0],
                    &then_may_fallthrough)) {
                return 0;
            }
            if (stmt->child_count >= 2 &&
                !ir_statement_may_fallthrough_for_lowering(stmt->children[1],
                    &else_may_fallthrough)) {
                return 0;
            }

            *out_may_fallthrough = stmt->child_count >= 2
                ? (then_may_fallthrough || else_may_fallthrough)
                : 1;
            return 1;
        }
    case AST_STMT_WHILE:
        {
            const AstExpression *condition_expr = (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count)
                ? stmt->expressions[stmt->condition_expression_index]
                : NULL;
            int condition_known = 0;
            int condition_truthy = 0;

            if (condition_expr &&
                !ir_dependency_try_eval_condition_truthiness(condition_expr,
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known && !condition_truthy) {
                *out_may_fallthrough = 1;
                return 1;
            }
            if (condition_known && condition_truthy &&
                !(stmt->child_count >= 1 &&
                    ir_statement_has_break_targeting_current_loop(stmt->children[0]))) {
                *out_may_fallthrough = 0;
                return 1;
            }

            *out_may_fallthrough = 1;
            return 1;
        }
    case AST_STMT_FOR:
        {
            const AstExpression *condition_expr = NULL;
            int condition_known = 1;
            int condition_truthy = 1;

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count) {
                condition_expr = stmt->expressions[stmt->condition_expression_index];
                condition_known = 0;
                condition_truthy = 0;
                if (condition_expr &&
                    !ir_dependency_try_eval_condition_truthiness(condition_expr,
                        &condition_known,
                        &condition_truthy)) {
                    return 0;
                }
            }

            if (condition_known && !condition_truthy) {
                *out_may_fallthrough = 1;
                return 1;
            }
            if (condition_known && condition_truthy &&
                !(stmt->child_count >= 1 &&
                    ir_statement_has_break_targeting_current_loop(stmt->children[0]))) {
                *out_may_fallthrough = 0;
                return 1;
            }

            *out_may_fallthrough = 1;
            return 1;
        }
    default:
        *out_may_fallthrough = 1;
        return 1;
    }
}

static int ir_lower_while_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *condition_expr;
    size_t entry_block_id;
    size_t header_block_id;
    size_t body_block_id;
    size_t exit_block_id = 0;
    int condition_known = 0;
    int condition_truthy = 0;
    int has_break_to_exit = 0;
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

    if (!ir_dependency_try_eval_condition_truthiness(condition_expr,
            &condition_known,
            &condition_truthy)) {
        return 0;
    }
    if (condition_known && !condition_truthy) {
        return 1;
    }

    has_break_to_exit = stmt->child_count >= 1 &&
        ir_statement_has_break_targeting_current_loop(stmt->children[0]);
    needs_exit_block = !(condition_known && condition_truthy && !has_break_to_exit);

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
    if (needs_exit_block) {
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
    int body_has_break = 0;
    int body_has_continue = 0;
    int body_may_fallthrough = 1;
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
        if (!ir_dependency_try_eval_condition_truthiness(condition_expr,
                &condition_known,
                &condition_truthy)) {
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
        body_has_break = ir_statement_has_break_targeting_current_loop(stmt->children[0]);
        body_has_continue = ir_statement_has_continue_targeting_current_loop(stmt->children[0]);
        if (!ir_statement_may_fallthrough_for_lowering(stmt->children[0], &body_may_fallthrough)) {
            if (scope_pushed) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
    }

    needs_exit_block = !(condition_known && condition_truthy && !body_has_break);
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
    if (needs_exit_block && condition_expr) {
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
    int has_else;
    int condition_known = 0;
    int condition_truthy = 0;
    int then_fallthrough;
    int else_fallthrough;

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
            "IR-LOWER-007: if statement missing condition expression");
        return 0;
    }

    has_else = stmt->child_count >= 2;

    if (!ir_dependency_try_eval_condition_truthiness(condition_expr,
            &condition_known,
            &condition_truthy)) {
        return 0;
    }

    if (condition_known) {
        if (condition_truthy) {
            return stmt->child_count >= 1 ? ir_lower_statement(ctx, stmt->children[0]) : 1;
        }
        return has_else ? ir_lower_statement(ctx, stmt->children[1]) : 1;
    }

    if (!ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error)) {
        return 0;
    }

    if (has_else) {
        if (!ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
            !ir_lower_boolean_branch(ctx, condition_expr, then_block_id, else_block_id)) {
            return 0;
        }
    } else {
        if (!ir_function_append_block(ctx->function, &continue_block_id, NULL, ctx->error) ||
            !ir_lower_boolean_branch(ctx, condition_expr, then_block_id, continue_block_id)) {
            return 0;
        }
        else_block_id = continue_block_id;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (stmt->child_count >= 1 && !ir_lower_statement(ctx, stmt->children[0])) {
        return 0;
    }
    then_fallthrough = ctx->has_block && !ir_lower_current_block(ctx)->has_terminator;
    if (ctx->has_block) {
        then_exit_block_id = ctx->block_id;
    }

    if (has_else) {
        ctx->block_id = else_block_id;
        ctx->has_block = 1;
        if (!ir_lower_statement(ctx, stmt->children[1])) {
            return 0;
        }
        else_fallthrough = ctx->has_block && !ir_lower_current_block(ctx)->has_terminator;
        if (ctx->has_block) {
            else_exit_block_id = ctx->block_id;
        }

        if (then_fallthrough || else_fallthrough) {
            IrBasicBlock *then_exit_block;
            IrBasicBlock *else_exit_block;

            if (!ir_function_append_block(ctx->function, &continue_block_id, NULL, ctx->error)) {
                return 0;
            }
            then_exit_block = ir_function_get_block_mut(ctx->function, then_exit_block_id);
            else_exit_block = ir_function_get_block_mut(ctx->function, else_exit_block_id);
            if (then_fallthrough &&
                !ir_block_set_jump(then_exit_block, continue_block_id, ctx->error)) {
                return 0;
            }
            if (else_fallthrough &&
                !ir_block_set_jump(else_exit_block, continue_block_id, ctx->error)) {
                return 0;
            }
            ctx->block_id = continue_block_id;
            ctx->has_block = 1;
        } else {
            ctx->has_block = 0;
        }
    } else {
        IrBasicBlock *then_exit_block = ir_function_get_block_mut(ctx->function, then_exit_block_id);

        if (then_fallthrough &&
            !ir_block_set_jump(then_exit_block, continue_block_id, ctx->error)) {
            return 0;
        }
        ctx->block_id = continue_block_id;
        ctx->has_block = 1;
    }

    return 1;
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
