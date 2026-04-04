#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_eval_global_initializer_expr(const IrProgram *program,
    const AstExpression *expr,
    long long *out_value,
    IrError *error,
    int *out_invalid_constant);

static int ir_append_global_initializer_dependency(size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    size_t global_id,
    IrError *error) {
    size_t i;
    size_t next_capacity;
    size_t *new_deps;

    if (!deps || !dep_count || !dep_capacity) {
        return 0;
    }

    for (i = 0; i < *dep_count; ++i) {
        if ((*deps)[i] == global_id) {
            return 1;
        }
    }

    if (*dep_count == *dep_capacity) {
        if (!ir_next_growth_capacity(*dep_capacity,
                4,
                sizeof(size_t),
                &next_capacity)) {
            ir_set_error(error,
                0,
                0,
                "IR-INT-050: global dependency storage capacity overflow");
            return 0;
        }
        new_deps = (size_t *)realloc(*deps, next_capacity * sizeof(size_t));
        if (!new_deps) {
            ir_set_error(error,
                0,
                0,
                "IR-INT-051: out of memory while collecting global initializer dependencies");
            return 0;
        }
        *deps = new_deps;
        *dep_capacity = next_capacity;
    }

    (*deps)[*dep_count] = global_id;
    *dep_count += 1;
    return 1;
}

typedef struct {
    const char **names;
    size_t count;
    size_t capacity;
} IrDepScopeFrame;

typedef struct {
    IrDepScopeFrame *frames;
    size_t count;
    size_t capacity;
} IrDepScopeStack;

typedef struct {
    const IrProgram *program;
    const AstProgram *ast_program;
    unsigned char *function_states;
    size_t **function_dep_ids;
    size_t *function_dep_count;
    size_t *function_dep_capacity;
} IrGlobalDepCollector;

typedef struct {
    const char **names;
    size_t count;
    size_t capacity;
} IrDepCallPath;

typedef struct {
    const IrProgram *program;
    const AstProgram *ast_program;
    size_t target_global_id;
    IrDepCallPath call_path;
    unsigned char *function_states;
    size_t function_state_count;
    int found_via_callee;
} IrGlobalDepTrace;

static int ir_dep_scope_push(IrDepScopeStack *stack) {
    size_t next_capacity;
    IrDepScopeFrame *new_frames;

    if (!stack) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity,
                4,
                sizeof(IrDepScopeFrame),
                &next_capacity)) {
            return 0;
        }
        new_frames = (IrDepScopeFrame *)realloc(stack->frames,
            next_capacity * sizeof(IrDepScopeFrame));
        if (!new_frames) {
            return 0;
        }
        stack->frames = new_frames;
        stack->capacity = next_capacity;
    }

    stack->frames[stack->count].names = NULL;
    stack->frames[stack->count].count = 0;
    stack->frames[stack->count].capacity = 0;
    stack->count += 1;
    return 1;
}

static void ir_dep_scope_pop(IrDepScopeStack *stack) {
    IrDepScopeFrame *frame;

    if (!stack || stack->count == 0) {
        return;
    }

    frame = &stack->frames[stack->count - 1];
    free(frame->names);
    frame->names = NULL;
    frame->count = 0;
    frame->capacity = 0;
    stack->count -= 1;
}

static void ir_dep_scope_free(IrDepScopeStack *stack) {
    if (!stack) {
        return;
    }

    while (stack->count > 0) {
        ir_dep_scope_pop(stack);
    }
    free(stack->frames);
    stack->frames = NULL;
    stack->capacity = 0;
}

static int ir_dep_scope_add_name(IrDepScopeStack *stack, const char *name) {
    IrDepScopeFrame *frame;
    size_t next_capacity;
    const char **new_names;

    if (!stack || stack->count == 0 || !name || name[0] == '\0') {
        return 1;
    }

    frame = &stack->frames[stack->count - 1];
    if (frame->count == frame->capacity) {
        if (!ir_next_growth_capacity(frame->capacity,
                4,
                sizeof(const char *),
                &next_capacity)) {
            return 0;
        }
        new_names = (const char **)realloc(frame->names,
            next_capacity * sizeof(const char *));
        if (!new_names) {
            return 0;
        }
        frame->names = new_names;
        frame->capacity = next_capacity;
    }

    frame->names[frame->count] = name;
    frame->count += 1;
    return 1;
}

static int ir_dep_scope_has_name(const IrDepScopeStack *stack, const char *name) {
    size_t frame_index;

    if (!stack || !name || name[0] == '\0') {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        const IrDepScopeFrame *frame = &stack->frames[frame_index - 1];
        size_t i;

        for (i = frame->count; i > 0; --i) {
            if (strcmp(frame->names[i - 1], name) == 0) {
                return 1;
            }
        }
    }

    return 0;
}

static int ir_dep_call_path_push(IrDepCallPath *path, const char *name) {
    size_t next_capacity;
    const char **new_names;

    if (!path || !name || name[0] == '\0') {
        return 0;
    }

    if (path->count == path->capacity) {
        if (!ir_next_growth_capacity(path->capacity,
                4,
                sizeof(const char *),
                &next_capacity)) {
            return 0;
        }
        new_names = (const char **)realloc(path->names, next_capacity * sizeof(const char *));
        if (!new_names) {
            return 0;
        }
        path->names = new_names;
        path->capacity = next_capacity;
    }

    path->names[path->count] = name;
    path->count += 1;
    return 1;
}

static void ir_dep_call_path_pop(IrDepCallPath *path) {
    if (!path || path->count == 0) {
        return;
    }

    path->count -= 1;
}

static void ir_dep_call_path_free(IrDepCallPath *path) {
    if (!path) {
        return;
    }

    free(path->names);
    path->names = NULL;
    path->count = 0;
    path->capacity = 0;
}

static int ir_find_ast_function_definition_index(const AstProgram *ast_program,
    const char *name,
    size_t *out_index) {
    size_t i;

    if (!ast_program || !name || name[0] == '\0') {
        return 0;
    }

    for (i = 0; i < ast_program->count; ++i) {
        const AstExternal *external = &ast_program->externals[i];

        if (external->kind == AST_EXTERNAL_FUNCTION && external->is_function_definition &&
            external->name && strcmp(external->name, name) == 0) {
            if (out_index) {
                *out_index = i;
            }
            return 1;
        }
    }

    return 0;
}

static int ir_trace_global_dependency_from_statement(IrGlobalDepTrace *trace,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    int *out_found,
    int *out_may_fallthrough);

static int ir_trace_global_dependency_from_function(IrGlobalDepTrace *trace,
    size_t function_external_index,
    int *out_found) {
    const AstExternal *external;
    IrDepScopeStack scope_stack;
    size_t i;

    if (!out_found) {
        return 0;
    }
    *out_found = 0;

    if (!trace || !trace->ast_program ||
        function_external_index >= trace->ast_program->count ||
        function_external_index >= trace->function_state_count) {
        return 1;
    }

    if (trace->function_states[function_external_index] == 1) {
        return 1;
    }

    external = &trace->ast_program->externals[function_external_index];
    if (external->kind != AST_EXTERNAL_FUNCTION || !external->is_function_definition ||
        !external->function_body) {
        return 1;
    }

    scope_stack.frames = NULL;
    scope_stack.count = 0;
    scope_stack.capacity = 0;
    trace->function_states[function_external_index] = 1;

    if (!ir_dep_scope_push(&scope_stack)) {
        trace->function_states[function_external_index] = 0;
        return 0;
    }

    for (i = 0; i < external->parameter_count; ++i) {
        const char *parameter_name = external->parameter_names ? external->parameter_names[i] : NULL;

        if (!ir_dep_scope_add_name(&scope_stack, parameter_name)) {
            ir_dep_scope_free(&scope_stack);
            trace->function_states[function_external_index] = 0;
            return 0;
        }
    }

    if (!ir_trace_global_dependency_from_statement(trace,
            external->function_body,
            &scope_stack,
            out_found,
            NULL)) {
        ir_dep_scope_free(&scope_stack);
        trace->function_states[function_external_index] = 0;
        return 0;
    }

    ir_dep_scope_free(&scope_stack);
    trace->function_states[function_external_index] = 0;
    return 1;
}

static int ir_collect_global_dependencies_from_expr(IrGlobalDepCollector *collector,
    const AstExpression *expr,
    IrDepScopeStack *scope_stack,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error);

static int ir_trace_global_dependency_from_expr(IrGlobalDepTrace *trace,
    const AstExpression *expr,
    IrDepScopeStack *scope_stack,
    int *out_found);

static int ir_trace_global_dependency_from_statement(IrGlobalDepTrace *trace,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    int *out_found,
    int *out_may_fallthrough);

static int ir_collect_global_dependencies_from_statement(IrGlobalDepCollector *collector,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    int *out_may_fallthrough,
    IrError *error);

static int ir_dependency_try_eval_condition_truthiness(const AstExpression *expr,
    int *out_known,
    int *out_truthy) {
    long long value;

    if (!out_known || !out_truthy) {
        return 0;
    }

    *out_known = 0;
    *out_truthy = 0;
    if (!expr) {
        return 1;
    }

    if (ir_eval_global_initializer_expr(NULL, expr, &value, NULL, NULL)) {
        *out_known = 1;
        *out_truthy = value != 0;
    }

    return 1;
}

typedef struct {
    int may_fallthrough;
    int may_break;
    int may_continue;
    int may_return;
} IrLoopTransferInfo;

static int ir_analyze_loop_transfer_from_statement(const AstStatement *stmt,
    IrLoopTransferInfo *out_info) {
    size_t i;

    if (!out_info) {
        return 0;
    }

    out_info->may_fallthrough = 1;
    out_info->may_break = 0;
    out_info->may_continue = 0;
    out_info->may_return = 0;

    if (!stmt) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND:
        {
            int may_fallthrough = 1;

            out_info->may_fallthrough = 1;
            out_info->may_break = 0;
            out_info->may_continue = 0;
            out_info->may_return = 0;

            for (i = 0; i < stmt->child_count; ++i) {
                IrLoopTransferInfo child_info;

                if (!may_fallthrough) {
                    break;
                }
                if (!ir_analyze_loop_transfer_from_statement(stmt->children[i], &child_info)) {
                    return 0;
                }

                out_info->may_break = out_info->may_break || child_info.may_break;
                out_info->may_continue = out_info->may_continue || child_info.may_continue;
                out_info->may_return = out_info->may_return || child_info.may_return;
                may_fallthrough = child_info.may_fallthrough;
            }

            out_info->may_fallthrough = may_fallthrough;
            return 1;
        }
    case AST_STMT_DECLARATION:
    case AST_STMT_EXPRESSION:
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
    case AST_STMT_IF:
        {
            IrLoopTransferInfo then_info;
            IrLoopTransferInfo else_info;
            int condition_known = 0;
            int condition_truthy = 0;

            then_info.may_fallthrough = 1;
            then_info.may_break = 0;
            then_info.may_continue = 0;
            then_info.may_return = 0;
            else_info = then_info;

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_dependency_try_eval_condition_truthiness(
                    stmt->expressions[stmt->condition_expression_index],
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known) {
                if (condition_truthy) {
                    if (stmt->child_count >= 1 &&
                        !ir_analyze_loop_transfer_from_statement(stmt->children[0], out_info)) {
                        return 0;
                    }
                    return 1;
                }

                if (stmt->child_count >= 2 &&
                    !ir_analyze_loop_transfer_from_statement(stmt->children[1], out_info)) {
                    return 0;
                }
                return 1;
            }

            if (stmt->child_count >= 1 &&
                !ir_analyze_loop_transfer_from_statement(stmt->children[0], &then_info)) {
                return 0;
            }
            if (stmt->child_count >= 2 &&
                !ir_analyze_loop_transfer_from_statement(stmt->children[1], &else_info)) {
                return 0;
            }

            out_info->may_fallthrough = then_info.may_fallthrough || else_info.may_fallthrough;
            out_info->may_break = then_info.may_break || else_info.may_break;
            out_info->may_continue = then_info.may_continue || else_info.may_continue;
            out_info->may_return = then_info.may_return || else_info.may_return;
            return 1;
        }
    case AST_STMT_WHILE:
    case AST_STMT_FOR:
        {
            IrLoopTransferInfo body_info;
            int condition_known = 0;
            int condition_truthy = 0;
            const AstStatement *body_stmt = stmt->child_count >= 1 ? stmt->children[0] : NULL;

            if (stmt->kind == AST_STMT_FOR && !stmt->has_condition_expression) {
                condition_known = 1;
                condition_truthy = 1;
            } else if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_dependency_try_eval_condition_truthiness(
                    stmt->expressions[stmt->condition_expression_index],
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (!ir_analyze_loop_transfer_from_statement(body_stmt, &body_info)) {
                return 0;
            }

            out_info->may_break = 0;
            out_info->may_continue = 0;
            out_info->may_return = body_info.may_return;

            if (condition_known) {
                out_info->may_fallthrough = condition_truthy ? body_info.may_break : 1;
            } else {
                out_info->may_fallthrough = 1;
            }
            return 1;
        }
    default:
        return 1;
    }
}

static int ir_trace_global_dependency_from_expr(IrGlobalDepTrace *trace,
    const AstExpression *expr,
    IrDepScopeStack *scope_stack,
    int *out_found) {
    const IrGlobal *global;
    size_t i;

    if (!out_found) {
        return 0;
    }
    *out_found = 0;

    if (!expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        if (!trace || !trace->program || !expr->as.identifier.name ||
            expr->as.identifier.name[0] == '\0' ||
            ir_dep_scope_has_name(scope_stack, expr->as.identifier.name)) {
            return 1;
        }
        global = ir_program_find_global(trace->program, expr->as.identifier.name);
        if (!global || global->id != trace->target_global_id) {
            return 1;
        }
        *out_found = 1;
        trace->found_via_callee = trace->call_path.count > 0;
        return 1;
    case AST_EXPR_NUMBER:
        return 1;
    case AST_EXPR_PAREN:
        return ir_trace_global_dependency_from_expr(trace,
            expr->as.inner,
            scope_stack,
            out_found);
    case AST_EXPR_UNARY:
        return ir_trace_global_dependency_from_expr(trace,
            expr->as.unary.operand,
            scope_stack,
            out_found);
    case AST_EXPR_POSTFIX:
        return ir_trace_global_dependency_from_expr(trace,
            expr->as.postfix.operand,
            scope_stack,
            out_found);
    case AST_EXPR_CALL:
        {
            const char *callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
            size_t callee_index;
            int found = 0;

            if (!ir_trace_global_dependency_from_expr(trace,
                    expr->as.call.callee,
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (trace && trace->ast_program && callee_name &&
                ir_find_ast_function_definition_index(trace->ast_program,
                    callee_name,
                    &callee_index)) {
                if (!ir_dep_call_path_push(&trace->call_path, callee_name)) {
                    return 0;
                }
                if (!ir_trace_global_dependency_from_function(trace, callee_index, &found)) {
                    ir_dep_call_path_pop(&trace->call_path);
                    return 0;
                }
                if (found) {
                    *out_found = 1;
                    return 1;
                }
                ir_dep_call_path_pop(&trace->call_path);
            }

            for (i = 0; i < expr->as.call.arg_count; ++i) {
                if (!ir_trace_global_dependency_from_expr(trace,
                        expr->as.call.args[i],
                        scope_stack,
                        &found)) {
                    return 0;
                }
                if (found) {
                    *out_found = 1;
                    return 1;
                }
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        {
            int found = 0;

            if (expr->as.binary.op == TOKEN_AND_AND || expr->as.binary.op == TOKEN_OR_OR) {
                int lhs_known = 0;
                int lhs_truthy = 0;

                if (!ir_trace_global_dependency_from_expr(trace,
                        expr->as.binary.left,
                        scope_stack,
                        &found)) {
                    return 0;
                }
                if (found) {
                    *out_found = 1;
                    return 1;
                }

                if (!ir_dependency_try_eval_condition_truthiness(expr->as.binary.left,
                        &lhs_known,
                        &lhs_truthy)) {
                    return 0;
                }

                if ((expr->as.binary.op == TOKEN_AND_AND && lhs_known && !lhs_truthy) ||
                    (expr->as.binary.op == TOKEN_OR_OR && lhs_known && lhs_truthy)) {
                    return 1;
                }

                if (!ir_trace_global_dependency_from_expr(trace,
                        expr->as.binary.right,
                        scope_stack,
                        &found)) {
                    return 0;
                }
                if (found) {
                    *out_found = 1;
                }
                return 1;
            }

            if (!ir_trace_global_dependency_from_expr(trace,
                    expr->as.binary.left,
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (!ir_trace_global_dependency_from_expr(trace,
                    expr->as.binary.right,
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
            }
            return 1;
        }
    case AST_EXPR_TERNARY:
        {
            int condition_known = 0;
            int condition_truthy = 0;
            int found = 0;

            if (!ir_trace_global_dependency_from_expr(trace,
                    expr->as.ternary.condition,
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (!ir_dependency_try_eval_condition_truthiness(expr->as.ternary.condition,
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known) {
                if (!ir_trace_global_dependency_from_expr(trace,
                        condition_truthy ? expr->as.ternary.then_expr : expr->as.ternary.else_expr,
                        scope_stack,
                        &found)) {
                    return 0;
                }
                if (found) {
                    *out_found = 1;
                }
                return 1;
            }

            if (!ir_trace_global_dependency_from_expr(trace,
                    expr->as.ternary.then_expr,
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (!ir_trace_global_dependency_from_expr(trace,
                    expr->as.ternary.else_expr,
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
            }
            return 1;
        }
    default:
        return 1;
    }
}

static int ir_trace_global_dependency_from_statement(IrGlobalDepTrace *trace,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    int *out_found,
    int *out_may_fallthrough) {
    size_t i;

    if (!out_found) {
        return 0;
    }
    *out_found = 0;

    if (!stmt) {
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND:
        {
            int may_fallthrough = 1;

            if (!ir_dep_scope_push(scope_stack)) {
                return 0;
            }
            for (i = 0; i < stmt->child_count; ++i) {
                int child_found = 0;
                int child_may_fallthrough = 1;

                if (!may_fallthrough) {
                    break;
                }
                if (!ir_trace_global_dependency_from_statement(trace,
                        stmt->children[i],
                        scope_stack,
                        &child_found,
                        &child_may_fallthrough)) {
                    ir_dep_scope_pop(scope_stack);
                    return 0;
                }
                if (child_found) {
                    *out_found = 1;
                    return 1;
                }
                may_fallthrough = child_may_fallthrough;
            }
            ir_dep_scope_pop(scope_stack);
            if (out_may_fallthrough) {
                *out_may_fallthrough = may_fallthrough;
            }
            return 1;
        }
    case AST_STMT_DECLARATION:
        for (i = 0; i < stmt->declaration_name_count; ++i) {
            int found = 0;

            if (stmt->expressions && i < stmt->expression_count && stmt->expressions[i] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[i],
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (!ir_dep_scope_add_name(scope_stack,
                    stmt->declaration_names ? stmt->declaration_names[i] : NULL)) {
                return 0;
            }
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    case AST_STMT_FOR:
        {
            size_t init_expr_limit = stmt->expression_count;
            int condition_known = 0;
            int condition_truthy = 0;
            int found = 0;
            IrLoopTransferInfo body_info;

            if (!ir_dep_scope_push(scope_stack)) {
                return 0;
            }

            if (stmt->has_condition_expression && stmt->condition_expression_index < init_expr_limit) {
                init_expr_limit = stmt->condition_expression_index;
            }
            if (stmt->has_for_step_expression && stmt->for_step_expression_index < init_expr_limit) {
                init_expr_limit = stmt->for_step_expression_index;
            }

            if (stmt->declaration_name_count > 0) {
                for (i = 0; i < stmt->declaration_name_count; ++i) {
                    if (stmt->expressions && i < init_expr_limit && stmt->expressions[i] &&
                        !ir_trace_global_dependency_from_expr(trace,
                            stmt->expressions[i],
                            scope_stack,
                            &found)) {
                        ir_dep_scope_pop(scope_stack);
                        return 0;
                    }
                    if (found) {
                        *out_found = 1;
                        return 1;
                    }
                    if (!ir_dep_scope_add_name(scope_stack,
                            stmt->declaration_names ? stmt->declaration_names[i] : NULL)) {
                        ir_dep_scope_pop(scope_stack);
                        return 0;
                    }
                }
            } else if (stmt->has_for_init_expression &&
                stmt->for_init_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->for_init_expression_index] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[stmt->for_init_expression_index],
                    scope_stack,
                    &found)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            } else if (found) {
                *out_found = 1;
                return 1;
            }

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[stmt->condition_expression_index],
                    scope_stack,
                    &found)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_dependency_try_eval_condition_truthiness(
                    stmt->expressions[stmt->condition_expression_index],
                    &condition_known,
                    &condition_truthy)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }

            if (!stmt->has_condition_expression) {
                condition_known = 1;
                condition_truthy = 1;
            }

            if (condition_known && !condition_truthy) {
                ir_dep_scope_pop(scope_stack);
                if (out_may_fallthrough) {
                    *out_may_fallthrough = 1;
                }
                return 1;
            }

            if (stmt->child_count >= 1 &&
                !ir_trace_global_dependency_from_statement(trace,
                    stmt->children[0],
                    scope_stack,
                    &found,
                    NULL)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (!ir_analyze_loop_transfer_from_statement(
                    stmt->child_count >= 1 ? stmt->children[0] : NULL,
                    &body_info)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }

            if ((body_info.may_fallthrough || body_info.may_continue) &&
                stmt->has_for_step_expression &&
                stmt->for_step_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->for_step_expression_index] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[stmt->for_step_expression_index],
                    scope_stack,
                    &found)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            ir_dep_scope_pop(scope_stack);
            if (out_may_fallthrough) {
                if (condition_known) {
                    *out_may_fallthrough = condition_truthy ? body_info.may_break : 1;
                } else {
                    *out_may_fallthrough = 1;
                }
            }
            return 1;
        }
    case AST_STMT_IF:
        {
            int condition_known = 0;
            int condition_truthy = 0;
            int found = 0;
            int then_may_fallthrough = 1;
            int else_may_fallthrough = 1;

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[stmt->condition_expression_index],
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_dependency_try_eval_condition_truthiness(
                    stmt->expressions[stmt->condition_expression_index],
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known) {
                if (condition_truthy) {
                    if (stmt->child_count >= 1 &&
                        !ir_trace_global_dependency_from_statement(trace,
                            stmt->children[0],
                            scope_stack,
                            &found,
                            &then_may_fallthrough)) {
                        return 0;
                    }
                    if (found) {
                        *out_found = 1;
                        return 1;
                    }
                    if (out_may_fallthrough) {
                        *out_may_fallthrough = stmt->child_count >= 1 ? then_may_fallthrough : 1;
                    }
                    return 1;
                }

                if (stmt->child_count >= 2 &&
                    !ir_trace_global_dependency_from_statement(trace,
                        stmt->children[1],
                        scope_stack,
                        &found,
                        &else_may_fallthrough)) {
                    return 0;
                }
                if (found) {
                    *out_found = 1;
                    return 1;
                }
                if (out_may_fallthrough) {
                    *out_may_fallthrough = stmt->child_count >= 2 ? else_may_fallthrough : 1;
                }
                return 1;
            }

            if (stmt->child_count >= 1 &&
                !ir_trace_global_dependency_from_statement(trace,
                    stmt->children[0],
                    scope_stack,
                    &found,
                    &then_may_fallthrough)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (stmt->child_count >= 2 &&
                !ir_trace_global_dependency_from_statement(trace,
                    stmt->children[1],
                    scope_stack,
                    &found,
                    &else_may_fallthrough)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (out_may_fallthrough) {
                *out_may_fallthrough = stmt->child_count >= 2
                    ? (then_may_fallthrough || else_may_fallthrough)
                    : 1;
            }
            return 1;
        }
    case AST_STMT_WHILE:
        {
            int condition_known = 0;
            int condition_truthy = 0;
            int found = 0;
            IrLoopTransferInfo body_info;

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[stmt->condition_expression_index],
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < stmt->expression_count &&
                stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
                !ir_dependency_try_eval_condition_truthiness(
                    stmt->expressions[stmt->condition_expression_index],
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known && !condition_truthy) {
                if (out_may_fallthrough) {
                    *out_may_fallthrough = 1;
                }
                return 1;
            }

            if (stmt->child_count >= 1 &&
                !ir_trace_global_dependency_from_statement(trace,
                    stmt->children[0],
                    scope_stack,
                    &found,
                    NULL)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }

            if (!ir_analyze_loop_transfer_from_statement(
                    stmt->child_count >= 1 ? stmt->children[0] : NULL,
                    &body_info)) {
                return 0;
            }

            if (out_may_fallthrough) {
                if (condition_known) {
                    *out_may_fallthrough = condition_truthy ? body_info.may_break : 1;
                } else {
                    *out_may_fallthrough = 1;
                }
            }
            return 1;
        }
    case AST_STMT_EXPRESSION:
    case AST_STMT_RETURN:
        for (i = 0; i < stmt->expression_count; ++i) {
            int found = 0;

            if (stmt->expressions && stmt->expressions[i] &&
                !ir_trace_global_dependency_from_expr(trace,
                    stmt->expressions[i],
                    scope_stack,
                    &found)) {
                return 0;
            }
            if (found) {
                *out_found = 1;
                return 1;
            }
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = stmt->kind == AST_STMT_RETURN ? 0 : 1;
        }
        return 1;
    case AST_STMT_BREAK:
    case AST_STMT_CONTINUE:
        if (out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        return 1;
    default:
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }
}

static int ir_try_format_runtime_global_dependency_path(const IrProgram *program,
    const AstProgram *ast_program,
    size_t source_global_id,
    const AstExpression *source_expr,
    size_t blocked_global_id,
    char *out_path,
    size_t out_path_size,
    int *out_via_callee) {
    IrGlobalDepTrace trace;
    int found = 0;
    size_t offset = 0;
    const char *source_name;
    const char *blocked_name;
    size_t i;

    if (!program || !source_expr || !out_path || out_path_size == 0 ||
        source_global_id >= program->global_count || blocked_global_id >= program->global_count) {
        return 0;
    }

    out_path[0] = '\0';
    if (out_via_callee) {
        *out_via_callee = 0;
    }

    trace.program = program;
    trace.ast_program = ast_program;
    trace.target_global_id = blocked_global_id;
    trace.call_path.names = NULL;
    trace.call_path.count = 0;
    trace.call_path.capacity = 0;
    trace.function_states = NULL;
    trace.function_state_count = ast_program ? ast_program->count : 0;
    trace.found_via_callee = 0;

    if (trace.function_state_count > 0) {
        trace.function_states = (unsigned char *)calloc(trace.function_state_count,
            sizeof(unsigned char));
        if (!trace.function_states) {
            ir_dep_call_path_free(&trace.call_path);
            return 0;
        }
    }

    if (!ir_trace_global_dependency_from_expr(&trace, source_expr, NULL, &found)) {
        free(trace.function_states);
        ir_dep_call_path_free(&trace.call_path);
        return 0;
    }
    if (!found) {
        free(trace.function_states);
        ir_dep_call_path_free(&trace.call_path);
        return 0;
    }

    source_name = (program->globals[source_global_id].name &&
        program->globals[source_global_id].name[0] != '\0')
        ? program->globals[source_global_id].name
        : "<global>";
    blocked_name = (program->globals[blocked_global_id].name &&
        program->globals[blocked_global_id].name[0] != '\0')
        ? program->globals[blocked_global_id].name
        : "<global>";

    offset += (size_t)snprintf(out_path + offset,
        offset < out_path_size ? out_path_size - offset : 0,
        "%s",
        source_name);
    for (i = 0; i < trace.call_path.count; ++i) {
        const char *call_name = trace.call_path.names[i] ? trace.call_path.names[i] : "<call>";

        offset += (size_t)snprintf(out_path + offset,
            offset < out_path_size ? out_path_size - offset : 0,
            " -> %s()",
            call_name);
    }
    (void)snprintf(out_path + offset,
        offset < out_path_size ? out_path_size - offset : 0,
        " -> %s",
        blocked_name);

    if (out_via_callee) {
        *out_via_callee = trace.found_via_callee;
    }

    free(trace.function_states);
    ir_dep_call_path_free(&trace.call_path);
    return 1;
}

static int ir_collect_global_dependencies_from_function(IrGlobalDepCollector *collector,
    size_t function_external_index,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error) {
    const AstExternal *external;
    IrDepScopeStack scope_stack;
    size_t *local_dep_ids = NULL;
    size_t local_dep_count = 0;
    size_t local_dep_capacity = 0;
    size_t i;

    if (!collector || !collector->ast_program ||
        function_external_index >= collector->ast_program->count) {
        return 1;
    }

    if (collector->function_states[function_external_index] == 2) {
        for (i = 0; i < collector->function_dep_count[function_external_index]; ++i) {
            if (!ir_append_global_initializer_dependency(deps,
                    dep_count,
                    dep_capacity,
                    collector->function_dep_ids[function_external_index][i],
                    error)) {
                return 0;
            }
        }
        return 1;
    }
    if (collector->function_states[function_external_index] == 1) {
        return 1;
    }

    external = &collector->ast_program->externals[function_external_index];
    if (external->kind != AST_EXTERNAL_FUNCTION || !external->is_function_definition ||
        !external->function_body) {
        collector->function_states[function_external_index] = 2;
        return 1;
    }

    collector->function_states[function_external_index] = 1;
    scope_stack.frames = NULL;
    scope_stack.count = 0;
    scope_stack.capacity = 0;

    if (!ir_dep_scope_push(&scope_stack)) {
        ir_set_error(error,
            external->line,
            external->column,
            "IR-INT-052: out of memory while preparing function dependency scope");
        collector->function_states[function_external_index] = 0;
        return 0;
    }

    for (i = 0; i < external->parameter_count; ++i) {
        const char *parameter_name = external->parameter_names ? external->parameter_names[i] : NULL;

        if (!ir_dep_scope_add_name(&scope_stack, parameter_name)) {
            ir_set_error(error,
                external->line,
                external->column,
                "IR-INT-053: out of memory while recording parameter dependency scope");
            ir_dep_scope_free(&scope_stack);
            collector->function_states[function_external_index] = 0;
            return 0;
        }
    }

    if (!ir_collect_global_dependencies_from_statement(collector,
            external->function_body,
            &scope_stack,
            &local_dep_ids,
            &local_dep_count,
            &local_dep_capacity,
            NULL,
            error)) {
        free(local_dep_ids);
        ir_dep_scope_free(&scope_stack);
        collector->function_states[function_external_index] = 0;
        return 0;
    }

    ir_dep_scope_free(&scope_stack);

    collector->function_dep_ids[function_external_index] = local_dep_ids;
    collector->function_dep_count[function_external_index] = local_dep_count;
    collector->function_dep_capacity[function_external_index] = local_dep_capacity;
    collector->function_states[function_external_index] = 2;

    for (i = 0; i < local_dep_count; ++i) {
        if (!ir_append_global_initializer_dependency(deps,
                dep_count,
                dep_capacity,
                local_dep_ids[i],
                error)) {
            return 0;
        }
    }

    return 1;
}

static int ir_collect_global_dependencies_from_expr(IrGlobalDepCollector *collector,
    const AstExpression *expr,
    IrDepScopeStack *scope_stack,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error) {
    const IrGlobal *global;
    size_t i;

    if (!expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        if (!collector || !collector->program || !expr->as.identifier.name ||
            expr->as.identifier.name[0] == '\0' ||
            ir_dep_scope_has_name(scope_stack, expr->as.identifier.name)) {
            return 1;
        }
        global = ir_program_find_global(collector->program, expr->as.identifier.name);
        if (!global) {
            return 1;
        }
        return ir_append_global_initializer_dependency(deps,
            dep_count,
            dep_capacity,
            global->id,
            error);
    case AST_EXPR_NUMBER:
        return 1;
    case AST_EXPR_PAREN:
        return ir_collect_global_dependencies_from_expr(collector,
            expr->as.inner,
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error);
    case AST_EXPR_UNARY:
        return ir_collect_global_dependencies_from_expr(collector,
            expr->as.unary.operand,
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error);
    case AST_EXPR_POSTFIX:
        return ir_collect_global_dependencies_from_expr(collector,
            expr->as.postfix.operand,
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error);
    case AST_EXPR_CALL:
        if (!ir_collect_global_dependencies_from_expr(collector,
                expr->as.call.callee,
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error)) {
            return 0;
        }

        if (collector && collector->ast_program) {
            const char *callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
            size_t callee_index;

            if (callee_name &&
                ir_find_ast_function_definition_index(collector->ast_program,
                    callee_name,
                    &callee_index) &&
                !ir_collect_global_dependencies_from_function(collector,
                    callee_index,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }
        }

        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!ir_collect_global_dependencies_from_expr(collector,
                    expr->as.call.args[i],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_AND_AND || expr->as.binary.op == TOKEN_OR_OR) {
            int lhs_known = 0;
            int lhs_truthy = 0;

            if (!ir_collect_global_dependencies_from_expr(collector,
                    expr->as.binary.left,
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }

            if (!ir_dependency_try_eval_condition_truthiness(expr->as.binary.left,
                    &lhs_known,
                    &lhs_truthy)) {
                return 0;
            }

            if ((expr->as.binary.op == TOKEN_AND_AND && lhs_known && !lhs_truthy) ||
                (expr->as.binary.op == TOKEN_OR_OR && lhs_known && lhs_truthy)) {
                return 1;
            }

            return ir_collect_global_dependencies_from_expr(collector,
                expr->as.binary.right,
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error);
        }

        if (!ir_collect_global_dependencies_from_expr(collector,
                expr->as.binary.left,
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error)) {
            return 0;
        }
        return ir_collect_global_dependencies_from_expr(collector,
            expr->as.binary.right,
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error);
    case AST_EXPR_TERNARY:
        {
            int condition_known = 0;
            int condition_truthy = 0;

            if (!ir_collect_global_dependencies_from_expr(collector,
                    expr->as.ternary.condition,
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }

            if (!ir_dependency_try_eval_condition_truthiness(expr->as.ternary.condition,
                    &condition_known,
                    &condition_truthy)) {
                return 0;
            }

            if (condition_known) {
                return ir_collect_global_dependencies_from_expr(collector,
                    condition_truthy ? expr->as.ternary.then_expr : expr->as.ternary.else_expr,
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error);
            }

            if (!ir_collect_global_dependencies_from_expr(collector,
                    expr->as.ternary.then_expr,
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }

            return ir_collect_global_dependencies_from_expr(collector,
                expr->as.ternary.else_expr,
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error);
        }
    default:
        return 1;
    }
}

static int ir_collect_global_dependencies_from_declaration_statement(IrGlobalDepCollector *collector,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    for (i = 0; i < stmt->declaration_name_count; ++i) {
        if (stmt->expressions && i < stmt->expression_count && stmt->expressions[i] &&
            !ir_collect_global_dependencies_from_expr(collector,
                stmt->expressions[i],
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error)) {
            return 0;
        }

        if (!ir_dep_scope_add_name(scope_stack,
                stmt->declaration_names ? stmt->declaration_names[i] : NULL)) {
            ir_set_error(error,
                stmt->line,
                stmt->column,
                "IR-INT-054: out of memory while tracking declaration dependency scope");
            return 0;
        }
    }

    return 1;
}

static int ir_collect_global_dependencies_from_for_statement(IrGlobalDepCollector *collector,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    int *out_may_fallthrough,
    IrError *error) {
    size_t init_expr_limit = stmt ? stmt->expression_count : 0;
    int condition_known = 0;
    int condition_truthy = 0;
    int body_may_fallthrough = 1;
    IrLoopTransferInfo body_info;

    if (!stmt) {
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }

    if (!ir_dep_scope_push(scope_stack)) {
        ir_set_error(error,
            stmt->line,
            stmt->column,
            "IR-INT-055: out of memory while tracking for-scope dependencies");
        return 0;
    }

    if (stmt->has_condition_expression && stmt->condition_expression_index < init_expr_limit) {
        init_expr_limit = stmt->condition_expression_index;
    }
    if (stmt->has_for_step_expression && stmt->for_step_expression_index < init_expr_limit) {
        init_expr_limit = stmt->for_step_expression_index;
    }

    if (stmt->declaration_name_count > 0) {
        size_t i;

        for (i = 0; i < stmt->declaration_name_count; ++i) {
            if (stmt->expressions && i < init_expr_limit && stmt->expressions[i] &&
                !ir_collect_global_dependencies_from_expr(collector,
                    stmt->expressions[i],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }

            if (!ir_dep_scope_add_name(scope_stack,
                    stmt->declaration_names ? stmt->declaration_names[i] : NULL)) {
                ir_set_error(error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-054: out of memory while tracking declaration dependency scope");
                ir_dep_scope_pop(scope_stack);
                return 0;
            }
        }
    } else if (stmt->has_for_init_expression &&
        stmt->for_init_expression_index < stmt->expression_count &&
        stmt->expressions && stmt->expressions[stmt->for_init_expression_index] &&
        !ir_collect_global_dependencies_from_expr(collector,
            stmt->expressions[stmt->for_init_expression_index],
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error)) {
        ir_dep_scope_pop(scope_stack);
        return 0;
    }

    if (stmt->has_condition_expression &&
        stmt->condition_expression_index < stmt->expression_count &&
        stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
        !ir_collect_global_dependencies_from_expr(collector,
            stmt->expressions[stmt->condition_expression_index],
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error)) {
        ir_dep_scope_pop(scope_stack);
        return 0;
    }

    if (stmt->has_condition_expression &&
        stmt->condition_expression_index < stmt->expression_count &&
        stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
        !ir_dependency_try_eval_condition_truthiness(
            stmt->expressions[stmt->condition_expression_index],
            &condition_known,
            &condition_truthy)) {
        ir_dep_scope_pop(scope_stack);
        return 0;
    }

    if (!stmt->has_condition_expression) {
        condition_known = 1;
        condition_truthy = 1;
    }

    if (condition_known && !condition_truthy) {
        ir_dep_scope_pop(scope_stack);
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }

    if (stmt->child_count >= 1 &&
        !ir_collect_global_dependencies_from_statement(collector,
            stmt->children[0],
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            &body_may_fallthrough,
            error)) {
        ir_dep_scope_pop(scope_stack);
        return 0;
    }

    if (!ir_analyze_loop_transfer_from_statement(
            stmt->child_count >= 1 ? stmt->children[0] : NULL,
            &body_info)) {
        ir_set_error(error,
            stmt->line,
            stmt->column,
            "IR-INT-057: failed to analyze for-loop dependency reachability");
        ir_dep_scope_pop(scope_stack);
        return 0;
    }

    if ((body_info.may_fallthrough || body_info.may_continue) &&
        stmt->has_for_step_expression &&
        stmt->for_step_expression_index < stmt->expression_count &&
        stmt->expressions && stmt->expressions[stmt->for_step_expression_index] &&
        !ir_collect_global_dependencies_from_expr(collector,
            stmt->expressions[stmt->for_step_expression_index],
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error)) {
        ir_dep_scope_pop(scope_stack);
        return 0;
    }

    ir_dep_scope_pop(scope_stack);
    (void)body_may_fallthrough;
    if (out_may_fallthrough) {
        if (condition_known) {
            *out_may_fallthrough = condition_truthy ? body_info.may_break : 1;
        } else {
            *out_may_fallthrough = 1;
        }
    }
    return 1;
}

static int ir_collect_global_dependencies_from_statement(IrGlobalDepCollector *collector,
    const AstStatement *stmt,
    IrDepScopeStack *scope_stack,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    int *out_may_fallthrough,
    IrError *error) {
    size_t i;

    if (!stmt) {
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND:
        {
            int may_fallthrough = 1;

        if (!ir_dep_scope_push(scope_stack)) {
            ir_set_error(error,
                stmt->line,
                stmt->column,
                "IR-INT-056: out of memory while tracking compound dependency scope");
            return 0;
        }
        for (i = 0; i < stmt->child_count; ++i) {
            int child_may_fallthrough = 1;

            if (!may_fallthrough) {
                break;
            }
            if (!ir_collect_global_dependencies_from_statement(collector,
                    stmt->children[i],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    &child_may_fallthrough,
                    error)) {
                ir_dep_scope_pop(scope_stack);
                return 0;
            }
            may_fallthrough = child_may_fallthrough;
        }
        ir_dep_scope_pop(scope_stack);
        if (out_may_fallthrough) {
            *out_may_fallthrough = may_fallthrough;
        }
        return 1;
        }
    case AST_STMT_DECLARATION:
        if (!ir_collect_global_dependencies_from_declaration_statement(collector,
            stmt,
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            error)) {
            return 0;
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    case AST_STMT_FOR:
        return ir_collect_global_dependencies_from_for_statement(collector,
            stmt,
            scope_stack,
            deps,
            dep_count,
            dep_capacity,
            out_may_fallthrough,
            error);
    case AST_STMT_IF:
        {
            int condition_known = 0;
            int condition_truthy = 0;
            int then_may_fallthrough = 1;
            int else_may_fallthrough = 1;

        if (stmt->has_condition_expression &&
            stmt->condition_expression_index < stmt->expression_count &&
            stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
            !ir_collect_global_dependencies_from_expr(collector,
                stmt->expressions[stmt->condition_expression_index],
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error)) {
            return 0;
        }

        if (stmt->has_condition_expression &&
            stmt->condition_expression_index < stmt->expression_count &&
            stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
            !ir_dependency_try_eval_condition_truthiness(
                stmt->expressions[stmt->condition_expression_index],
                &condition_known,
                &condition_truthy)) {
            return 0;
        }

        if (condition_known) {
            if (condition_truthy) {
                if (stmt->child_count >= 1 &&
                    !ir_collect_global_dependencies_from_statement(collector,
                        stmt->children[0],
                        scope_stack,
                        deps,
                        dep_count,
                        dep_capacity,
                        &then_may_fallthrough,
                        error)) {
                    return 0;
                }
                if (out_may_fallthrough) {
                    *out_may_fallthrough = stmt->child_count >= 1 ? then_may_fallthrough : 1;
                }
                return 1;
            }

            if (stmt->child_count >= 2 &&
                !ir_collect_global_dependencies_from_statement(collector,
                    stmt->children[1],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    &else_may_fallthrough,
                    error)) {
                return 0;
            }
            if (out_may_fallthrough) {
                *out_may_fallthrough = stmt->child_count >= 2 ? else_may_fallthrough : 1;
            }
            return 1;
        }

        if (stmt->child_count >= 1 &&
            !ir_collect_global_dependencies_from_statement(collector,
                    stmt->children[0],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    &then_may_fallthrough,
                    error)) {
                return 0;
        }
        if (stmt->child_count >= 2 &&
            !ir_collect_global_dependencies_from_statement(collector,
                stmt->children[1],
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                &else_may_fallthrough,
                error)) {
            return 0;
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = stmt->child_count >= 2
                ? (then_may_fallthrough || else_may_fallthrough)
                : 1;
        }
        return 1;
        }
    case AST_STMT_WHILE:
        {
            int condition_known = 0;
            int condition_truthy = 0;
            int body_may_fallthrough = 1;
            IrLoopTransferInfo body_info;

        if (stmt->has_condition_expression &&
            stmt->condition_expression_index < stmt->expression_count &&
            stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
            !ir_collect_global_dependencies_from_expr(collector,
                stmt->expressions[stmt->condition_expression_index],
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                error)) {
            return 0;
        }

        if (stmt->has_condition_expression &&
            stmt->condition_expression_index < stmt->expression_count &&
            stmt->expressions && stmt->expressions[stmt->condition_expression_index] &&
            !ir_dependency_try_eval_condition_truthiness(
                stmt->expressions[stmt->condition_expression_index],
                &condition_known,
                &condition_truthy)) {
            return 0;
        }

        if (condition_known && !condition_truthy) {
            if (out_may_fallthrough) {
                *out_may_fallthrough = 1;
            }
            return 1;
        }

        if (stmt->child_count >= 1) {
            if (!ir_collect_global_dependencies_from_statement(collector,
                stmt->children[0],
                scope_stack,
                deps,
                dep_count,
                dep_capacity,
                &body_may_fallthrough,
                error)) {
                return 0;
            }
        }

        if (!ir_analyze_loop_transfer_from_statement(
                stmt->child_count >= 1 ? stmt->children[0] : NULL,
                &body_info)) {
            ir_set_error(error,
                stmt->line,
                stmt->column,
                "IR-INT-058: failed to analyze while-loop dependency reachability");
            return 0;
        }
        (void)body_may_fallthrough;
        if (out_may_fallthrough) {
            if (condition_known) {
                *out_may_fallthrough = condition_truthy ? body_info.may_break : 1;
            } else {
                *out_may_fallthrough = 1;
            }
        }
        return 1;
        }
    case AST_STMT_EXPRESSION:
        for (i = 0; i < stmt->expression_count; ++i) {
            if (stmt->expressions && stmt->expressions[i] &&
                !ir_collect_global_dependencies_from_expr(collector,
                    stmt->expressions[i],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    case AST_STMT_RETURN:
        for (i = 0; i < stmt->expression_count; ++i) {
            if (stmt->expressions && stmt->expressions[i] &&
                !ir_collect_global_dependencies_from_expr(collector,
                    stmt->expressions[i],
                    scope_stack,
                    deps,
                    dep_count,
                    dep_capacity,
                    error)) {
                return 0;
            }
        }
        if (out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        return 1;
    case AST_STMT_BREAK:
    case AST_STMT_CONTINUE:
        if (out_may_fallthrough) {
            *out_may_fallthrough = 0;
        }
        return 1;
    default:
        if (out_may_fallthrough) {
            *out_may_fallthrough = 1;
        }
        return 1;
    }
}

static int ir_collect_global_initializer_dependencies(const IrProgram *program,
    const AstProgram *ast_program,
    const AstExpression *expr,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error) {
    IrGlobalDepCollector collector;
    size_t external_count;
    size_t i;
    int ok;

    if (!expr) {
        return 1;
    }

    external_count = ast_program ? ast_program->count : 0;
    collector.program = program;
    collector.ast_program = ast_program;
    collector.function_states = NULL;
    collector.function_dep_ids = NULL;
    collector.function_dep_count = NULL;
    collector.function_dep_capacity = NULL;

    if (external_count > 0) {
        collector.function_states = (unsigned char *)calloc(external_count, sizeof(unsigned char));
        collector.function_dep_ids = (size_t **)calloc(external_count, sizeof(size_t *));
        collector.function_dep_count = (size_t *)calloc(external_count, sizeof(size_t));
        collector.function_dep_capacity = (size_t *)calloc(external_count, sizeof(size_t));
        if (!collector.function_states || !collector.function_dep_ids ||
            !collector.function_dep_count || !collector.function_dep_capacity) {
            free(collector.function_states);
            free(collector.function_dep_ids);
            free(collector.function_dep_count);
            free(collector.function_dep_capacity);
            ir_set_error(error,
                expr->line,
                expr->column,
                "IR-INT-057: out of memory while preparing global initializer dependency collector");
            return 0;
        }
    }

    ok = ir_collect_global_dependencies_from_expr(&collector,
        expr,
        NULL,
        deps,
        dep_count,
        dep_capacity,
        error);

    if (external_count > 0) {
        for (i = 0; i < external_count; ++i) {
            free(collector.function_dep_ids[i]);
        }
    }
    free(collector.function_states);
    free(collector.function_dep_ids);
    free(collector.function_dep_count);
    free(collector.function_dep_capacity);
    return ok;
}

static const AstExternal *ir_find_global_initializer_external(const IrProgram *program,
    const AstProgram *ast_program,
    size_t global_id) {
    size_t i;

    if (!program || !ast_program || global_id >= program->global_count) {
        return NULL;
    }

    for (i = 0; i < ast_program->count; ++i) {
        const AstExternal *external = &ast_program->externals[i];
        const IrGlobal *global;

        if (external->kind != AST_EXTERNAL_DECLARATION || !external->has_initializer ||
            !external->name || external->name[0] == '\0' || !external->declaration_initializer) {
            continue;
        }

        global = ir_program_find_global(program, external->name);
        if (global && global->id == global_id) {
            return external;
        }
    }

    return NULL;
}

static int ir_try_format_runtime_global_cycle(const IrProgram *program,
    const AstProgram *ast_program,
    size_t source_global_id,
    size_t blocked_global_id,
    char *out_chain,
    size_t out_chain_size) {
    size_t global_count;
    size_t **adj = NULL;
    size_t *adj_count = NULL;
    size_t *adj_capacity = NULL;
    size_t *queue = NULL;
    size_t *parent = NULL;
    unsigned char *visited = NULL;
    size_t queue_head = 0;
    size_t queue_tail = 0;
    int found = 0;
    size_t i;

    if (!program || !ast_program || !out_chain || out_chain_size == 0 ||
        source_global_id >= program->global_count || blocked_global_id >= program->global_count) {
        return 0;
    }

    out_chain[0] = '\0';
    global_count = program->global_count;
    if (global_count == 0) {
        return 0;
    }

    adj = (size_t **)calloc(global_count, sizeof(size_t *));
    adj_count = (size_t *)calloc(global_count, sizeof(size_t));
    adj_capacity = (size_t *)calloc(global_count, sizeof(size_t));
    queue = (size_t *)malloc(global_count * sizeof(size_t));
    parent = (size_t *)malloc(global_count * sizeof(size_t));
    visited = (unsigned char *)calloc(global_count, sizeof(unsigned char));
    if (!adj || !adj_count || !adj_capacity || !queue || !parent || !visited) {
        free(adj);
        free(adj_count);
        free(adj_capacity);
        free(queue);
        free(parent);
        free(visited);
        return 0;
    }

    for (i = 0; i < global_count; ++i) {
        const AstExternal *initializer_external = ir_find_global_initializer_external(program,
            ast_program,
            i);

        parent[i] = (size_t)-1;
        if (!initializer_external) {
            continue;
        }

        if (!ir_collect_global_initializer_dependencies(program,
                ast_program,
                initializer_external->declaration_initializer,
                &adj[i],
                &adj_count[i],
                &adj_capacity[i],
                NULL)) {
            for (size_t free_i = 0; free_i < global_count; ++free_i) {
                free(adj[free_i]);
            }
            free(adj);
            free(adj_count);
            free(adj_capacity);
            free(queue);
            free(parent);
            free(visited);
            return 0;
        }
    }

    visited[blocked_global_id] = 1;
    queue[queue_tail++] = blocked_global_id;
    while (queue_head < queue_tail) {
        size_t node = queue[queue_head++];
        size_t dep_index;

        if (node == source_global_id) {
            found = 1;
            break;
        }

        for (dep_index = 0; dep_index < adj_count[node]; ++dep_index) {
            size_t dep_id = adj[node][dep_index];

            if (dep_id >= global_count || visited[dep_id]) {
                continue;
            }

            visited[dep_id] = 1;
            parent[dep_id] = node;
            queue[queue_tail++] = dep_id;
        }
    }

    if (found) {
        size_t chain_nodes_count = 0;
        size_t cursor = source_global_id;
        size_t *chain_nodes = NULL;
        size_t chain_index;

        while (cursor != (size_t)-1) {
            chain_nodes_count += 1;
            if (cursor == blocked_global_id) {
                break;
            }
            cursor = parent[cursor];
        }

        if (chain_nodes_count > 0 && cursor == blocked_global_id) {
            chain_nodes = (size_t *)malloc(chain_nodes_count * sizeof(size_t));
            if (chain_nodes) {
                cursor = source_global_id;
                for (chain_index = 0; chain_index < chain_nodes_count; ++chain_index) {
                    chain_nodes[chain_index] = cursor;
                    if (cursor == blocked_global_id) {
                        break;
                    }
                    cursor = parent[cursor];
                }

                if (chain_index + 1 == chain_nodes_count) {
                    size_t cycle_node_count = chain_nodes_count + 1;
                    size_t *cycle_nodes = (size_t *)malloc(cycle_node_count * sizeof(size_t));

                    if (cycle_nodes) {
                        size_t cycle_fill = 0;
                        size_t offset = 0;

                        cycle_nodes[cycle_fill++] = source_global_id;
                        for (size_t rev = chain_nodes_count; rev > 0; --rev) {
                            cycle_nodes[cycle_fill++] = chain_nodes[rev - 1];
                        }

                        for (size_t edge_index = 0; edge_index + 1 < cycle_fill; ++edge_index) {
                            size_t edge_source_id = cycle_nodes[edge_index];
                            size_t edge_target_id = cycle_nodes[edge_index + 1];
                            const AstExternal *edge_external = ir_find_global_initializer_external(program,
                                ast_program,
                                edge_source_id);
                            const char *edge_source_name = (program->globals[edge_source_id].name &&
                                program->globals[edge_source_id].name[0] != '\0')
                                ? program->globals[edge_source_id].name
                                : "<global>";
                            const char *edge_target_name = (program->globals[edge_target_id].name &&
                                program->globals[edge_target_id].name[0] != '\0')
                                ? program->globals[edge_target_id].name
                                : "<global>";
                            const char *edge_suffix = NULL;
                            char edge_path[256];

                            edge_path[0] = '\0';
                            if (!(edge_external && edge_external->declaration_initializer &&
                                    ir_try_format_runtime_global_dependency_path(program,
                                        ast_program,
                                        edge_source_id,
                                        edge_external->declaration_initializer,
                                        edge_target_id,
                                        edge_path,
                                        sizeof(edge_path),
                                        NULL))) {
                                (void)snprintf(edge_path,
                                    sizeof(edge_path),
                                    "%s -> %s",
                                    edge_source_name,
                                    edge_target_name);
                            }

                            if (edge_index == 0) {
                                offset += (size_t)snprintf(out_chain + offset,
                                    offset < out_chain_size ? out_chain_size - offset : 0,
                                    "%s",
                                    edge_path);
                                continue;
                            }

                            edge_suffix = strstr(edge_path, " -> ");
                            if (edge_suffix) {
                                edge_suffix += 4;
                            } else {
                                edge_suffix = edge_path;
                            }

                            offset += (size_t)snprintf(out_chain + offset,
                                offset < out_chain_size ? out_chain_size - offset : 0,
                                " -> %s",
                                edge_suffix);
                        }

                        free(cycle_nodes);
                    }
                }

                free(chain_nodes);
            }
        }
    }

    for (i = 0; i < global_count; ++i) {
        free(adj[i]);
    }
    free(adj);
    free(adj_count);
    free(adj_capacity);
    free(queue);
    free(parent);
    free(visited);
    return found && out_chain[0] != '\0';
}

#endif
