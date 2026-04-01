#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

typedef struct {
    const char **names;
    size_t count;
    size_t capacity;
} ScopeFrame;

typedef struct {
    ScopeFrame *frames;
    size_t count;
    size_t capacity;
} ScopeStack;

static int scope_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity) {
    size_t next_capacity;

    if (!out_next_capacity || initial == 0 || elem_size == 0) {
        return 0;
    }

    if (current == 0) {
        next_capacity = initial;
    } else {
        if (current > ((size_t)-1) / 2) {
            return 0;
        }
        next_capacity = current * 2;
    }

    if (next_capacity > ((size_t)-1) / elem_size) {
        return 0;
    }

    *out_next_capacity = next_capacity;
    return 1;
}

static void scope_frame_free(ScopeFrame *frame) {
    if (!frame) {
        return;
    }

    free(frame->names);
    frame->names = NULL;
    frame->count = 0;
    frame->capacity = 0;
}

static void scope_stack_free(ScopeStack *stack) {
    size_t i;

    if (!stack) {
        return;
    }

    for (i = 0; i < stack->count; ++i) {
        scope_frame_free(&stack->frames[i]);
    }

    free(stack->frames);
    stack->frames = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static int scope_stack_push(ScopeStack *stack) {
    ScopeFrame *new_frames;
    size_t next_capacity;

    if (!stack) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!scope_next_growth_capacity(stack->capacity,
            8,
            sizeof(ScopeFrame),
            &next_capacity)) {
            return 0;
        }
        new_frames = (ScopeFrame *)realloc(stack->frames, next_capacity * sizeof(ScopeFrame));
        if (!new_frames) {
            return 0;
        }
        stack->frames = new_frames;
        stack->capacity = next_capacity;
    }

    stack->frames[stack->count].names = NULL;
    stack->frames[stack->count].count = 0;
    stack->frames[stack->count].capacity = 0;
    stack->count++;
    return 1;
}

static void scope_stack_pop(ScopeStack *stack) {
    if (!stack || stack->count == 0) {
        return;
    }

    scope_frame_free(&stack->frames[stack->count - 1]);
    stack->count--;
}

static int scope_frame_contains_name(const ScopeFrame *frame, const char *name) {
    size_t i;

    if (!frame || !name || name[0] == '\0') {
        return 0;
    }

    for (i = 0; i < frame->count; ++i) {
        const char *candidate = frame->names[i];
        if (candidate && strcmp(candidate, name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int scope_stack_contains_local_name(const ScopeStack *stack, const char *name) {
    size_t i;

    if (!stack || !name || name[0] == '\0') {
        return 0;
    }

    for (i = stack->count; i > 0; --i) {
        if (scope_frame_contains_name(&stack->frames[i - 1], name)) {
            return 1;
        }
    }

    return 0;
}

static int scope_stack_add_to_current(ScopeStack *stack, const char *name) {
    ScopeFrame *frame;
    const char **new_names;
    size_t next_capacity;

    if (!stack || stack->count == 0 || !name || name[0] == '\0') {
        return 0;
    }

    frame = &stack->frames[stack->count - 1];
    if (frame->count == frame->capacity) {
        if (!scope_next_growth_capacity(frame->capacity,
            8,
            sizeof(const char *),
            &next_capacity)) {
            return 0;
        }
        new_names = (const char **)realloc(frame->names, next_capacity * sizeof(const char *));
        if (!new_names) {
            return 0;
        }
        frame->names = new_names;
        frame->capacity = next_capacity;
    }

    frame->names[frame->count] = name;
    frame->count++;
    return 1;
}

static int semantic_top_level_name_visible(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const char *name) {
    size_t i;
    size_t visible_external_count;

    if (!program || !name || name[0] == '\0') {
        return 0;
    }

    visible_external_count = include_current_external ? (func_index + 1) : func_index;
    if (visible_external_count > program->count) {
        visible_external_count = program->count;
    }

    for (i = 0; i < visible_external_count; ++i) {
        const AstExternal *ext = &program->externals[i];

        if (!ext->name) {
            continue;
        }

        if (strcmp(ext->name, name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int semantic_scope_name_resolved(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    const char *name) {
    if (scope_stack_contains_local_name(scopes, name)) {
        return 1;
    }

    return semantic_top_level_name_visible(program,
        func_index,
        include_current_external,
        name);
}

static int semantic_scope_add_declaration_names(const ScopeStack *scopes,
    const AstProgram *program,
    size_t func_index,
    ScopeStack *mutable_scopes,
    char *const *names,
    size_t name_count,
    int line,
    int column,
    SemanticError *error) {
    size_t i;

    (void)scopes;
    (void)program;
    (void)func_index;

    for (i = 0; i < name_count; ++i) {
        char preview[128];
        const char *name = names ? names[i] : NULL;
        if (!name || name[0] == '\0') {
            continue;
        }

        if (mutable_scopes->count == 0) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-INT-009: scope stack is empty while processing declaration names");
            return 0;
        }

        if (scope_frame_contains_name(&mutable_scopes->frames[mutable_scopes->count - 1], name)) {
            format_callee_preview(name, preview, sizeof(preview));
            if (error) {
                char msg[512];
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-SCOPE-001: identifier=%s; duplicate local declaration in the same scope",
                    preview);
                semantic_set_error(error, line, column, msg);
            }
            return 0;
        }

        if (!scope_stack_add_to_current(mutable_scopes, name)) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }
    }

    return 1;
}

static int semantic_scope_callee_shadowed_by_local_non_function(const AstExpression *callee,
    const ScopeStack *scopes,
    const char **out_name) {
    const AstExpression *base = unwrap_paren_expression(callee);

    if (out_name) {
        *out_name = NULL;
    }

    if (!base || base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name ||
        base->as.identifier.name[0] == '\0') {
        return 0;
    }

    if (!scope_stack_contains_local_name(scopes, base->as.identifier.name)) {
        return 0;
    }

    if (out_name) {
        *out_name = base->as.identifier.name;
    }

    return 1;
}

static int semantic_scope_add_declaration_names_shadow_pass(ScopeStack *scopes,
    char *const *names,
    size_t name_count,
    int line,
    int column,
    SemanticError *error) {
    size_t i;

    if (!scopes) {
        return 1;
    }

    for (i = 0; i < name_count; ++i) {
        const char *name = names ? names[i] : NULL;

        if (!name || name[0] == '\0') {
            continue;
        }

        if (scopes->count == 0) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-INT-009: scope stack is empty while processing declaration names");
            return 0;
        }

        if (scope_frame_contains_name(&scopes->frames[scopes->count - 1], name)) {
            continue;
        }

        if (!scope_stack_add_to_current(scopes, name)) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }
    }

    return 1;
}

static int semantic_scope_check_call_shadow_expression_with_depth(const AstExpression *expr,
    const ScopeStack *scopes,
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
    case AST_EXPR_PAREN:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.inner,
            scopes,
            error,
            depth + 1);
    case AST_EXPR_UNARY:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.unary.operand,
            scopes,
            error,
            depth + 1);
    case AST_EXPR_POSTFIX:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.postfix.operand,
            scopes,
            error,
            depth + 1);
    case AST_EXPR_BINARY:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.binary.left,
            scopes,
            error,
            depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.binary.right,
                scopes,
                error,
                depth + 1);
    case AST_EXPR_TERNARY:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.ternary.condition,
            scopes,
            error,
            depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.ternary.then_expr,
                scopes,
                error,
                depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.ternary.else_expr,
                scopes,
                error,
                depth + 1);
    case AST_EXPR_CALL: {
        const char *shadowed_callee_name = NULL;
        if (semantic_scope_callee_shadowed_by_local_non_function(expr->as.call.callee,
            scopes,
            &shadowed_callee_name)) {
            if (error) {
                char preview[128];
                char msg[512];
                format_callee_preview(shadowed_callee_name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CALL-003: callee=%s; symbol_kind=non_function; call to non-function symbol",
                    preview);
                semantic_set_error(error, expr->line, expr->column, msg);
            }
            return 0;
        }

        if (!semantic_scope_check_call_shadow_expression_with_depth(expr->as.call.callee,
            scopes,
            error,
            depth + 1)) {
            return 0;
        }

        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_scope_check_call_shadow_expression_with_depth(expr->as.call.args[i],
                scopes,
                error,
                depth + 1)) {
                return 0;
            }
        }

        return 1;
    }
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    default:
        return 1;
    }
}

static int semantic_scope_check_call_shadow_expression(const AstExpression *expr,
    const ScopeStack *scopes,
    SemanticError *error) {
    return semantic_scope_check_call_shadow_expression_with_depth(expr, scopes, error, 0);
}

static int semantic_scope_check_call_shadow_statement(const AstStatement *stmt,
    ScopeStack *scopes,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    if (stmt->kind == AST_STMT_COMPOUND) {
        if (!scope_stack_push(scopes)) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        scope_stack_pop(scopes);
        return 1;
    }

    if (stmt->kind == AST_STMT_FOR) {
        size_t expression_start_index = 0;

        if (!scope_stack_push(scopes)) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }

        if (stmt->declaration_name_count > 0) {
            size_t init_expr_limit = stmt->expression_count;

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < init_expr_limit) {
                init_expr_limit = stmt->condition_expression_index;
            } else if (stmt->has_for_step_expression &&
                stmt->for_step_expression_index < init_expr_limit) {
                init_expr_limit = stmt->for_step_expression_index;
            }

            for (i = 0; i < stmt->declaration_name_count; ++i) {
                char *single_name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
                const AstExpression *init_expr =
                    (stmt->expressions && i < init_expr_limit) ? stmt->expressions[i] : NULL;

                if (!semantic_scope_check_call_shadow_expression(init_expr, scopes, error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }

                if (!semantic_scope_add_declaration_names_shadow_pass(scopes,
                    &single_name,
                    1,
                    stmt->line,
                    stmt->column,
                    error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }
            }

            expression_start_index = init_expr_limit;
        }

        for (i = expression_start_index; i < stmt->expression_count; ++i) {
            if (!semantic_scope_check_call_shadow_expression(stmt->expressions[i], scopes, error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        scope_stack_pop(scopes);
        return 1;
    }

    if (stmt->kind == AST_STMT_DECLARATION) {
        size_t expression_start_index = 0;

        for (i = 0; i < stmt->declaration_name_count; ++i) {
            char *single_name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
            const AstExpression *init_expr =
                (stmt->expressions && i < stmt->expression_count) ? stmt->expressions[i] : NULL;

            if (!semantic_scope_check_call_shadow_expression(init_expr, scopes, error)) {
                return 0;
            }

            if (!semantic_scope_add_declaration_names_shadow_pass(scopes,
                &single_name,
                1,
                stmt->line,
                stmt->column,
                error)) {
                return 0;
            }
        }

        expression_start_index = stmt->declaration_name_count;
        if (expression_start_index > stmt->expression_count) {
            expression_start_index = stmt->expression_count;
        }

        for (i = expression_start_index; i < stmt->expression_count; ++i) {
            if (!semantic_scope_check_call_shadow_expression(stmt->expressions[i], scopes, error)) {
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, error)) {
                return 0;
            }
        }

        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (!semantic_scope_check_call_shadow_expression(stmt->expressions[i], scopes, error)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_check_function_callable_scope_shadow_rules(const AstExternal *func,
    SemanticError *error) {
    ScopeStack scopes;
    const AstStatement *body;
    size_t i;

    if (!func || !func->is_function_definition) {
        return 1;
    }

    if (!func->function_body) {
        semantic_set_error(error,
            func->line,
            func->column,
            "SEMA-INT-005: function definition missing statement AST body during AST-primary semantic phase");
        return 0;
    }

    scopes.frames = NULL;
    scopes.count = 0;
    scopes.capacity = 0;

    if (!scope_stack_push(&scopes)) {
        semantic_set_error(error,
            func->line,
            func->column,
            "SEMA-INT-010: failed to extend semantic scope stack");
        return 0;
    }

    if (!semantic_scope_add_declaration_names_shadow_pass(&scopes,
        func->parameter_names,
        func->parameter_count,
        func->line,
        func->column,
        error)) {
        scope_stack_free(&scopes);
        return 0;
    }

    body = func->function_body;
    if (body && body->kind == AST_STMT_COMPOUND) {
        for (i = 0; i < body->child_count; ++i) {
            if (!semantic_scope_check_call_shadow_statement(body->children[i], &scopes, error)) {
                scope_stack_free(&scopes);
                return 0;
            }
        }
    } else if (!semantic_scope_check_call_shadow_statement(body, &scopes, error)) {
        scope_stack_free(&scopes);
        return 0;
    }

    scope_stack_free(&scopes);
    return 1;
}

static int semantic_scope_check_expression_with_visibility_impl(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
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
        if (!in_call_callee && expr->as.identifier.name && expr->as.identifier.name[0] != '\0' &&
            !semantic_scope_name_resolved(program,
                func_index,
                include_current_external,
                scopes,
                expr->as.identifier.name)) {
            if (error) {
                char preview[128];
                char msg[512];
                format_callee_preview(expr->as.identifier.name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-SCOPE-002: identifier=%s; use of undeclared identifier",
                    preview);
                semantic_set_error(error, expr->line, expr->column, msg);
            }
            return 0;
        }
        return 1;
    case AST_EXPR_NUMBER:
        return 1;
    case AST_EXPR_PAREN:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.inner,
            program,
            func_index,
            include_current_external,
            scopes,
            in_call_callee,
            error,
            depth + 1);
    case AST_EXPR_UNARY:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.unary.operand,
            program,
            func_index,
            include_current_external,
            scopes,
            in_call_callee,
            error,
            depth + 1);
    case AST_EXPR_POSTFIX:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.postfix.operand,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                error,
                depth + 1);
    case AST_EXPR_BINARY:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.binary.right,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                error,
                depth + 1);
    case AST_EXPR_TERNARY:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.ternary.condition,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.ternary.then_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                error,
                depth + 1);
    case AST_EXPR_CALL: {
        const char *shadowed_callee_name = NULL;
        if (semantic_scope_callee_shadowed_by_local_non_function(expr->as.call.callee,
            scopes,
            &shadowed_callee_name)) {
            if (error) {
                char preview[128];
                char msg[512];
                format_callee_preview(shadowed_callee_name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CALL-003: callee=%s; symbol_kind=non_function; call to non-function symbol",
                    preview);
                semantic_set_error(error, expr->line, expr->column, msg);
            }
            return 0;
        }
        if (!semantic_scope_check_expression_with_visibility_impl(expr->as.call.callee,
            program,
            func_index,
            include_current_external,
            scopes,
            1,
            error,
            depth + 1)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_scope_check_expression_with_visibility_impl(expr->as.call.args[i],
                program,
                func_index,
                include_current_external,
                scopes,
                0,
                error,
                depth + 1)) {
                return 0;
            }
        }
        return 1;
    }
    default:
        return 1;
    }
}

static int semantic_scope_check_expression_with_visibility(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    int in_call_callee,
    SemanticError *error) {
    return semantic_scope_check_expression_with_visibility_impl(expr,
        program,
        func_index,
        include_current_external,
        scopes,
        in_call_callee,
        error,
        0);
}

static int semantic_scope_check_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    int in_call_callee,
    SemanticError *error) {
    return semantic_scope_check_expression_with_visibility(expr,
        program,
        func_index,
        1,
        scopes,
        in_call_callee,
        error);
}

static int semantic_scope_check_top_level_initializer_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t external_index,
    SemanticError *error) {
    return semantic_scope_check_expression_with_visibility(expr,
        program,
        external_index,
        0,
        NULL,
        0,
        error);
}

static int semantic_scope_check_statement(const AstStatement *stmt,
    const AstProgram *program,
    size_t func_index,
    ScopeStack *scopes,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    if (stmt->kind == AST_STMT_COMPOUND) {
        if (!scope_stack_push(scopes)) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_statement(stmt->children[i],
                program,
                func_index,
                scopes,
                error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        scope_stack_pop(scopes);
        return 1;
    }

    if (stmt->kind == AST_STMT_FOR) {
        size_t expression_start_index = 0;

        if (!scope_stack_push(scopes)) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }

        if (stmt->declaration_name_count > 0) {
            size_t init_expr_limit = stmt->expression_count;

            if (stmt->has_condition_expression &&
                stmt->condition_expression_index < init_expr_limit) {
                init_expr_limit = stmt->condition_expression_index;
            } else if (stmt->has_for_step_expression &&
                stmt->for_step_expression_index < init_expr_limit) {
                init_expr_limit = stmt->for_step_expression_index;
            }

            for (i = 0; i < stmt->declaration_name_count; ++i) {
                char *single_name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
                const AstExpression *init_expr =
                    (stmt->expressions && i < init_expr_limit) ? stmt->expressions[i] : NULL;

                if (!semantic_scope_check_expression(init_expr,
                    program,
                    func_index,
                    scopes,
                    0,
                    error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }

                if (!semantic_scope_add_declaration_names(scopes,
                    program,
                    func_index,
                    scopes,
                    &single_name,
                    1,
                    stmt->line,
                    stmt->column,
                    error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }
            }

            expression_start_index = init_expr_limit;
        }

        for (i = expression_start_index; i < stmt->expression_count; ++i) {
            if (!semantic_scope_check_expression(stmt->expressions[i],
                program,
                func_index,
                scopes,
                0,
                error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_statement(stmt->children[i],
                program,
                func_index,
                scopes,
                error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        scope_stack_pop(scopes);
        return 1;
    }

    if (stmt->kind == AST_STMT_DECLARATION) {
        size_t expression_start_index = 0;

        for (i = 0; i < stmt->declaration_name_count; ++i) {
            char *single_name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
            const AstExpression *init_expr =
                (stmt->expressions && i < stmt->expression_count) ? stmt->expressions[i] : NULL;

            if (!semantic_scope_check_expression(init_expr,
                program,
                func_index,
                scopes,
                0,
                error)) {
                return 0;
            }

            if (!semantic_scope_add_declaration_names(scopes,
                program,
                func_index,
                scopes,
                &single_name,
                1,
                stmt->line,
                stmt->column,
                error)) {
                return 0;
            }
        }

        expression_start_index = stmt->declaration_name_count;
        if (expression_start_index > stmt->expression_count) {
            expression_start_index = stmt->expression_count;
        }

        for (i = expression_start_index; i < stmt->expression_count; ++i) {
            if (!semantic_scope_check_expression(stmt->expressions[i],
                program,
                func_index,
                scopes,
                0,
                error)) {
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_statement(stmt->children[i],
                program,
                func_index,
                scopes,
                error)) {
                return 0;
            }
        }

        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (!semantic_scope_check_expression(stmt->expressions[i],
            program,
            func_index,
            scopes,
            0,
            error)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_scope_check_statement(stmt->children[i],
            program,
            func_index,
            scopes,
            error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_check_function_scope_rules(const AstProgram *program,
    size_t func_index,
    const AstExternal *func,
    SemanticError *error) {
    ScopeStack scopes;
    const AstStatement *body;
    size_t i;

    if (!program || !func || !func->function_body) {
        return 1;
    }

    scopes.frames = NULL;
    scopes.count = 0;
    scopes.capacity = 0;

    if (!scope_stack_push(&scopes)) {
        semantic_set_error(error,
            func->line,
            func->column,
            "SEMA-INT-010: failed to extend semantic scope stack");
        return 0;
    }

    if (func->parameter_names) {
        for (i = 0; i < func->parameter_count; ++i) {
            char *single_name = func->parameter_names[i];
            if (!single_name || single_name[0] == '\0') {
                continue;
            }
            if (!semantic_scope_add_declaration_names(&scopes,
                program,
                func_index,
                &scopes,
                &single_name,
                1,
                func->line,
                func->column,
                error)) {
                scope_stack_free(&scopes);
                return 0;
            }
        }
    }

    body = func->function_body;
    if (body && body->kind == AST_STMT_COMPOUND) {
        for (i = 0; i < body->child_count; ++i) {
            if (!semantic_scope_check_statement(body->children[i],
                program,
                func_index,
                &scopes,
                error)) {
                scope_stack_free(&scopes);
                return 0;
            }
        }
    } else if (!semantic_scope_check_statement(body,
        program,
        func_index,
        &scopes,
        error)) {
        scope_stack_free(&scopes);
        return 0;
    }

    scope_stack_free(&scopes);
    return 1;
}

#endif
