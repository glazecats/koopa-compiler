#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

static void format_callee_preview(const char *name, char *out, size_t out_size) {
    size_t len;

    if (!out || out_size == 0) {
        return;
    }

    if (!name) {
        snprintf(out, out_size, "(null)");
        return;
    }

    len = strlen(name);
    if (len <= (CALLEE_PREVIEW_HEAD + CALLEE_PREVIEW_TAIL + 3)) {
        snprintf(out, out_size, "%s", name);
        return;
    }

    snprintf(out,
        out_size,
        "%.*s...%.*s(len=%zu)",
        CALLEE_PREVIEW_HEAD,
        name,
        CALLEE_PREVIEW_TAIL,
        name + (len - CALLEE_PREVIEW_TAIL),
        len);
}

static void semantic_set_error(SemanticError *error,
    int line,
    int column,
    const char *message) {
    if (!error) {
        return;
    }

    error->line = line;
    error->column = column;
    snprintf(error->message, sizeof(error->message), "%s", message);
}

static int names_equal(const AstExternal *a, const AstExternal *b) {
    if (!a->name || !b->name) {
        return 0;
    }
    if (a->name_length != b->name_length) {
        return 0;
    }
    return strncmp(a->name, b->name, a->name_length) == 0;
}

static const AstExpression *unwrap_paren_expression(const AstExpression *expr) {
    const AstExpression *cur = expr;

    while (cur && cur->kind == AST_EXPR_PAREN) {
        cur = cur->as.inner;
    }
    return cur;
}

static int classify_ast_call_callee(const AstExpression *callee, const char **out_name) {
    const AstExpression *base = unwrap_paren_expression(callee);

    if (out_name) {
        *out_name = NULL;
    }

    if (!base) {
        return AST_CALL_CALLEE_NON_IDENTIFIER;
    }

    if (base->kind == AST_EXPR_IDENTIFIER) {
        if (out_name) {
            *out_name = base->as.identifier.name;
        }
        return AST_CALL_CALLEE_DIRECT_IDENTIFIER;
    }

    if (base->kind == AST_EXPR_CALL) {
        return AST_CALL_CALLEE_CALL_RESULT;
    }

    return AST_CALL_CALLEE_NON_IDENTIFIER;
}

static int walk_expression_postorder_with_depth(const AstExpression *expr,
    ExpressionPostVisitor visitor,
    void *ctx,
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
        if (!walk_expression_postorder_with_depth(expr->as.inner,
            visitor,
            ctx,
            error,
            depth + 1)) {
            return 0;
        }
        break;
    case AST_EXPR_UNARY:
        if (!walk_expression_postorder_with_depth(expr->as.unary.operand,
            visitor,
            ctx,
            error,
            depth + 1)) {
            return 0;
        }
        break;
    case AST_EXPR_POSTFIX:
        if (!walk_expression_postorder_with_depth(expr->as.postfix.operand,
            visitor,
            ctx,
            error,
            depth + 1)) {
            return 0;
        }
        break;
    case AST_EXPR_BINARY:
        if (!walk_expression_postorder_with_depth(expr->as.binary.left,
            visitor,
            ctx,
            error,
            depth + 1) ||
            !walk_expression_postorder_with_depth(expr->as.binary.right,
            visitor,
            ctx,
            error,
            depth + 1)) {
            return 0;
        }
        break;
    case AST_EXPR_TERNARY:
        if (!walk_expression_postorder_with_depth(expr->as.ternary.condition,
            visitor,
            ctx,
            error,
            depth + 1) ||
            !walk_expression_postorder_with_depth(expr->as.ternary.then_expr,
            visitor,
            ctx,
            error,
            depth + 1) ||
            !walk_expression_postorder_with_depth(expr->as.ternary.else_expr,
            visitor,
            ctx,
            error,
            depth + 1)) {
            return 0;
        }
        break;
    case AST_EXPR_CALL:
        if (!walk_expression_postorder_with_depth(expr->as.call.callee,
            visitor,
            ctx,
            error,
            depth + 1)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!walk_expression_postorder_with_depth(expr->as.call.args[i],
                visitor,
                ctx,
                error,
                depth + 1)) {
                return 0;
            }
        }
        break;
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    default:
        break;
    }

    if (visitor && !visitor(expr, ctx)) {
        return 0;
    }

    return 1;
}

static int walk_expression_postorder(const AstExpression *expr,
    ExpressionPostVisitor visitor,
    void *ctx,
    SemanticError *error) {
    return walk_expression_postorder_with_depth(expr, visitor, ctx, error, 0);
}

static int walk_statement_postorder(const AstStatement *stmt,
    ExpressionPostVisitor visitor,
    void *ctx,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (!walk_expression_postorder(stmt->expressions[i], visitor, ctx, error)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!walk_statement_postorder(stmt->children[i], visitor, ctx, error)) {
            return 0;
        }
    }

    return 1;
}

static const AstExpression *statement_get_condition_expression(const AstStatement *stmt) {
    if (!stmt || !stmt->has_condition_expression) {
        return NULL;
    }

    if (stmt->condition_expression_index >= stmt->expression_count) {
        return NULL;
    }

    return stmt->expressions[stmt->condition_expression_index];
}

static const AstExpression *statement_get_for_step_expression(const AstStatement *stmt) {
    if (!stmt || !stmt->has_for_step_expression) {
        return NULL;
    }

    if (stmt->for_step_expression_index >= stmt->expression_count) {
        return NULL;
    }

    return stmt->expressions[stmt->for_step_expression_index];
}

static int expression_is_constant_true_for_flow(const AstExpression *expr);
static int expression_is_constant_false_for_flow(const AstExpression *expr);
static int semantic_analyze_statement_flow(const AstStatement *stmt,
    int *out_guaranteed_return,
    int *out_may_fallthrough,
    int *out_may_break,
    int *out_may_continue_loop,
    int *out_may_non_terminating);

#define FLOW_GUARD_IDENTIFIER_LIMIT 64
#define FLOW_GUARD_SCOPE_LIMIT 32

typedef struct {
    const char *names[FLOW_GUARD_IDENTIFIER_LIMIT];
    size_t lengths[FLOW_GUARD_IDENTIFIER_LIMIT];
    size_t bindings[FLOW_GUARD_IDENTIFIER_LIMIT];
    size_t count;
    int overflowed;
} FlowGuardIdentifierSet;

typedef struct {
    const char *names[FLOW_GUARD_IDENTIFIER_LIMIT];
    size_t lengths[FLOW_GUARD_IDENTIFIER_LIMIT];
    size_t scope_offsets[FLOW_GUARD_SCOPE_LIMIT];
    size_t count;
    size_t scope_count;
    int overflowed;
} FlowIdentifierScopeStack;

static void flow_guard_identifier_set_add(FlowGuardIdentifierSet *set,
    const char *name,
    size_t name_length,
    size_t binding) {
    size_t i;

    if (!set || !name || name_length == 0 || set->overflowed) {
        return;
    }

    for (i = 0; i < set->count; ++i) {
        if (set->lengths[i] == name_length && set->bindings[i] == binding &&
            strncmp(set->names[i], name, name_length) == 0) {
            return;
        }
    }

    if (set->count >= FLOW_GUARD_IDENTIFIER_LIMIT) {
        set->overflowed = 1;
        return;
    }

    set->names[set->count] = name;
    set->lengths[set->count] = name_length;
    set->bindings[set->count] = binding;
    set->count++;
}

static int flow_guard_identifier_set_contains(const FlowGuardIdentifierSet *set,
    const char *name,
    size_t name_length,
    size_t binding) {
    size_t i;

    if (!set || !name || name_length == 0) {
        return 0;
    }

    for (i = 0; i < set->count; ++i) {
        if (set->lengths[i] == name_length && set->bindings[i] == binding &&
            strncmp(set->names[i], name, name_length) == 0) {
            return 1;
        }
    }

    return 0;
}

static int flow_scope_stack_push(FlowIdentifierScopeStack *stack) {
    if (!stack || stack->overflowed) {
        return 0;
    }

    if (stack->scope_count >= FLOW_GUARD_SCOPE_LIMIT) {
        stack->overflowed = 1;
        return 0;
    }

    stack->scope_offsets[stack->scope_count++] = stack->count;
    return 1;
}

static void flow_scope_stack_add_name(FlowIdentifierScopeStack *stack,
    const char *name,
    size_t name_length) {
    if (!stack || !name || name_length == 0 || stack->overflowed) {
        return;
    }

    if (stack->count >= FLOW_GUARD_IDENTIFIER_LIMIT) {
        stack->overflowed = 1;
        return;
    }

    stack->names[stack->count] = name;
    stack->lengths[stack->count] = name_length;
    stack->count++;
}

static void flow_scope_stack_add_declaration_names(FlowIdentifierScopeStack *stack,
    char **names,
    size_t name_count) {
    size_t i;

    for (i = 0; i < name_count; ++i) {
        const char *name = names ? names[i] : NULL;

        if (!name || name[0] == '\0') {
            continue;
        }

        flow_scope_stack_add_name(stack, name, strlen(name));
    }
}

static size_t flow_scope_stack_resolve_binding(const FlowIdentifierScopeStack *stack,
    const char *name,
    size_t name_length) {
    size_t i;

    if (!stack || !name || name_length == 0) {
        return 0;
    }

    for (i = stack->count; i > 0; --i) {
        size_t index = i - 1;

        if (stack->lengths[index] == name_length &&
            strncmp(stack->names[index], name, name_length) == 0) {
            return index + 1;
        }
    }

    return 0;
}

static const AstExpression *flow_unwrap_paren_expression(const AstExpression *expr) {
    const AstExpression *current = expr;

    while (current && current->kind == AST_EXPR_PAREN) {
        current = current->as.inner;
    }

    return current;
}

static void flow_collect_expression_identifiers(const AstExpression *expr,
    FlowGuardIdentifierSet *set,
    const FlowIdentifierScopeStack *scopes) {
    size_t i;

    if (!expr || !set || set->overflowed) {
        return;
    }

    if (scopes && scopes->overflowed) {
        set->overflowed = 1;
        return;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        flow_guard_identifier_set_add(set,
            expr->as.identifier.name,
            expr->as.identifier.name_length,
            flow_scope_stack_resolve_binding(scopes,
                expr->as.identifier.name,
                expr->as.identifier.name_length));
        return;
    case AST_EXPR_PAREN:
        flow_collect_expression_identifiers(expr->as.inner, set, scopes);
        return;
    case AST_EXPR_UNARY:
        flow_collect_expression_identifiers(expr->as.unary.operand, set, scopes);
        return;
    case AST_EXPR_POSTFIX:
        flow_collect_expression_identifiers(expr->as.postfix.operand, set, scopes);
        return;
    case AST_EXPR_CALL:
        flow_collect_expression_identifiers(expr->as.call.callee, set, scopes);
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            flow_collect_expression_identifiers(expr->as.call.args[i], set, scopes);
        }
        return;
    case AST_EXPR_BINARY:
        flow_collect_expression_identifiers(expr->as.binary.left, set, scopes);
        flow_collect_expression_identifiers(expr->as.binary.right, set, scopes);
        return;
    case AST_EXPR_TERNARY:
        flow_collect_expression_identifiers(expr->as.ternary.condition, set, scopes);
        flow_collect_expression_identifiers(expr->as.ternary.then_expr, set, scopes);
        flow_collect_expression_identifiers(expr->as.ternary.else_expr, set, scopes);
        return;
    case AST_EXPR_NUMBER:
    default:
        return;
    }
}

static void flow_collect_guard_dependencies_from_declaration(const AstStatement *stmt,
    FlowGuardIdentifierSet *set,
    const FlowIdentifierScopeStack *scopes) {
    size_t i;
    size_t scope_base_count;

    if (!stmt || stmt->kind != AST_STMT_DECLARATION || !set || set->overflowed) {
        return;
    }

    if (scopes && scopes->overflowed) {
        set->overflowed = 1;
        return;
    }

    scope_base_count = scopes ? scopes->count : 0;

    for (i = 0; i < stmt->declaration_name_count; ++i) {
        const char *name = stmt->declaration_names ? stmt->declaration_names[i] : NULL;
        const AstExpression *initializer_expr =
            (stmt->expressions && i < stmt->expression_count) ? stmt->expressions[i] : NULL;
        size_t binding;

        if (!name || name[0] == '\0' || !initializer_expr) {
            continue;
        }

        binding = scope_base_count + i + 1;
        if (!flow_guard_identifier_set_contains(set, name, strlen(name), binding)) {
            continue;
        }

        flow_collect_expression_identifiers(initializer_expr, set, scopes);
        if (set->overflowed) {
            return;
        }
    }
}

static void flow_collect_guard_dependencies_in_compound(const AstStatement *stmt,
    FlowGuardIdentifierSet *set,
    const FlowIdentifierScopeStack *scopes) {
    size_t pass;

    if (!stmt || stmt->kind != AST_STMT_COMPOUND || !set || set->overflowed) {
        return;
    }

    for (pass = 0; pass < FLOW_GUARD_IDENTIFIER_LIMIT; ++pass) {
        FlowIdentifierScopeStack block_scopes = {0};
        size_t previous_count = set->count;
        size_t i;

        if (scopes) {
            block_scopes = *scopes;
        }
        if (!flow_scope_stack_push(&block_scopes)) {
            set->overflowed = 1;
            return;
        }

        for (i = 0; i < stmt->child_count; ++i) {
            const AstStatement *child = stmt->children[i];

            if (child && child->kind == AST_STMT_DECLARATION) {
                flow_collect_guard_dependencies_from_declaration(child, set, &block_scopes);
                if (set->overflowed) {
                    return;
                }

                flow_scope_stack_add_declaration_names(&block_scopes,
                    child->declaration_names,
                    child->declaration_name_count);
                if (block_scopes.overflowed) {
                    set->overflowed = 1;
                    return;
                }
            }
        }

        if (set->count == previous_count) {
            return;
        }
    }

    set->overflowed = 1;
}

static int flow_expression_targets_guard_identifier(const AstExpression *expr,
    const FlowGuardIdentifierSet *set,
    const FlowIdentifierScopeStack *scopes) {
    const AstExpression *base = flow_unwrap_paren_expression(expr);

    if (!base || base->kind != AST_EXPR_IDENTIFIER) {
        return 0;
    }

    return flow_guard_identifier_set_contains(set,
        base->as.identifier.name,
        base->as.identifier.name_length,
        flow_scope_stack_resolve_binding(scopes,
            base->as.identifier.name,
            base->as.identifier.name_length));
}

static int flow_expression_may_change_guard_state(const AstExpression *expr,
        const FlowGuardIdentifierSet *set,
        const FlowIdentifierScopeStack *scopes) {
    if (!expr || !set || set->count == 0 || set->overflowed) {
        return 0;
    }

    if (scopes && scopes->overflowed) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_PAREN:
        return flow_expression_may_change_guard_state(expr->as.inner, set, scopes);
    case AST_EXPR_UNARY:
        if ((expr->as.unary.op == TOKEN_PLUS_PLUS || expr->as.unary.op == TOKEN_MINUS_MINUS) &&
            flow_expression_targets_guard_identifier(expr->as.unary.operand, set, scopes)) {
            return 1;
        }
        return flow_expression_may_change_guard_state(expr->as.unary.operand, set, scopes);
    case AST_EXPR_POSTFIX:
        if ((expr->as.postfix.op == TOKEN_PLUS_PLUS || expr->as.postfix.op == TOKEN_MINUS_MINUS) &&
            flow_expression_targets_guard_identifier(expr->as.postfix.operand, set, scopes)) {
            return 1;
        }
        return flow_expression_may_change_guard_state(expr->as.postfix.operand, set, scopes);
    case AST_EXPR_CALL:
        return 1;
    case AST_EXPR_BINARY:
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
            if (flow_expression_targets_guard_identifier(expr->as.binary.left, set, scopes)) {
                return 1;
            }
            break;
        default:
            break;
        }
        return flow_expression_may_change_guard_state(expr->as.binary.left, set, scopes) ||
            flow_expression_may_change_guard_state(expr->as.binary.right, set, scopes);
    case AST_EXPR_TERNARY:
        return flow_expression_may_change_guard_state(expr->as.ternary.condition, set, scopes) ||
            flow_expression_may_change_guard_state(expr->as.ternary.then_expr, set, scopes) ||
            flow_expression_may_change_guard_state(expr->as.ternary.else_expr, set, scopes);
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    default:
        return 0;
    }
}

static int flow_expression_range_may_change_guard_state(const AstStatement *stmt,
    size_t start_index,
    size_t end_index,
    const FlowGuardIdentifierSet *set,
    const FlowIdentifierScopeStack *scopes) {
    size_t i;

    if (!stmt) {
        return 0;
    }

    if (end_index > stmt->expression_count) {
        end_index = stmt->expression_count;
    }

    for (i = start_index; i < end_index; ++i) {
        if (flow_expression_may_change_guard_state(stmt->expressions[i], set, scopes)) {
            return 1;
        }
    }

    return 0;
}

static int flow_statement_may_change_guard_state(const AstStatement *stmt,
    FlowGuardIdentifierSet *set,
    const FlowIdentifierScopeStack *scopes) {
    size_t i;

    if (!stmt || !set || set->count == 0 || set->overflowed) {
        return 0;
    }

    if (scopes && scopes->overflowed) {
        return 1;
    }

    if (stmt->kind == AST_STMT_COMPOUND) {
        FlowIdentifierScopeStack block_scopes = {0};

        if (scopes) {
            block_scopes = *scopes;
        }
        if (!flow_scope_stack_push(&block_scopes)) {
            return 1;
        }

        flow_collect_guard_dependencies_in_compound(stmt, set, scopes);
        if (set->overflowed) {
            return 1;
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (flow_statement_may_change_guard_state(stmt->children[i], set, &block_scopes)) {
                return 1;
            }
            if (stmt->children[i] && stmt->children[i]->kind == AST_STMT_DECLARATION) {
                flow_scope_stack_add_declaration_names(&block_scopes,
                    stmt->children[i]->declaration_names,
                    stmt->children[i]->declaration_name_count);
                if (block_scopes.overflowed) {
                    return 1;
                }
            }
        }

        return 0;
    }

    if (stmt->kind == AST_STMT_FOR) {
        FlowIdentifierScopeStack loop_scopes = {0};
        size_t declaration_expr_count = stmt->declaration_name_count;

        if (scopes) {
            loop_scopes = *scopes;
        }

        if (flow_expression_range_may_change_guard_state(stmt,
            0,
            declaration_expr_count,
            set,
            &loop_scopes)) {
            return 1;
        }

        if (stmt->declaration_name_count > 0) {
            if (!flow_scope_stack_push(&loop_scopes)) {
                return 1;
            }
            flow_scope_stack_add_declaration_names(&loop_scopes,
                stmt->declaration_names,
                stmt->declaration_name_count);
            if (loop_scopes.overflowed) {
                return 1;
            }
        }

        if (flow_expression_range_may_change_guard_state(stmt,
            declaration_expr_count,
            stmt->expression_count,
            set,
            &loop_scopes)) {
            return 1;
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (flow_statement_may_change_guard_state(stmt->children[i], set, &loop_scopes)) {
                return 1;
            }
        }

        return 0;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (flow_expression_may_change_guard_state(stmt->expressions[i], set, scopes)) {
            return 1;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (flow_statement_may_change_guard_state(stmt->children[i], set, scopes)) {
            return 1;
        }
    }

    return 0;
}

static FlowLoopExitSummary flow_collect_loop_exit_summary(const AstStatement *stmt,
    FlowGuardIdentifierSet *guard_ids,
    const FlowIdentifierScopeStack *scopes);

static FlowLoopExitSummary flow_collect_loop_exit_summary(const AstStatement *stmt,
    FlowGuardIdentifierSet *guard_ids,
    const FlowIdentifierScopeStack *scopes) {
    FlowLoopExitSummary summary = {0, 0};

    if (!stmt) {
        return summary;
    }

    switch (stmt->kind) {
    case AST_STMT_BREAK:
    case AST_STMT_RETURN:
        summary.may_exit_loop = 1;
        summary.exits_loop_on_all_paths = 1;
        return summary;
    case AST_STMT_IF: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_is_const_true = expression_is_constant_true_for_flow(cond_expr);
        int cond_is_const_false = expression_is_constant_false_for_flow(cond_expr);
        FlowLoopExitSummary then_summary = {0, 0};
        FlowLoopExitSummary else_summary = {0, 0};
        int has_else = stmt->child_count >= 2;

        if (stmt->child_count >= 1) {
            then_summary = flow_collect_loop_exit_summary(stmt->children[0], guard_ids, scopes);
        }
        if (has_else) {
            else_summary = flow_collect_loop_exit_summary(stmt->children[1], guard_ids, scopes);
        }

        if (cond_is_const_true) {
            return then_summary;
        }
        if (cond_is_const_false) {
            return has_else ? else_summary : summary;
        }

        summary.may_exit_loop = then_summary.may_exit_loop || else_summary.may_exit_loop;
        summary.exits_loop_on_all_paths = has_else && then_summary.exits_loop_on_all_paths &&
            else_summary.exits_loop_on_all_paths;

        if (summary.may_exit_loop && !summary.exits_loop_on_all_paths) {
            flow_collect_expression_identifiers(cond_expr, guard_ids, scopes);
        }
        return summary;
    }
    case AST_STMT_COMPOUND: {
        size_t i;
        int path_reaches_child = 1;
        FlowIdentifierScopeStack block_scopes = {0};

        if (scopes) {
            block_scopes = *scopes;
        }
        if (!flow_scope_stack_push(&block_scopes)) {
            guard_ids->overflowed = 1;
            return summary;
        }

        for (i = 0; i < stmt->child_count; ++i) {
            int child_may_fallthrough = 1;
            FlowLoopExitSummary child_summary =
                flow_collect_loop_exit_summary(stmt->children[i], guard_ids, &block_scopes);

            if (path_reaches_child && child_summary.may_exit_loop) {
                summary.may_exit_loop = 1;
            }

            if (path_reaches_child && child_summary.exits_loop_on_all_paths) {
                summary.exits_loop_on_all_paths = 1;
                return summary;
            }

            if (!semantic_analyze_statement_flow(stmt->children[i],
                NULL,
                &child_may_fallthrough,
                NULL,
                NULL,
                NULL)) {
                return summary;
            }

            if (path_reaches_child && !child_may_fallthrough) {
                path_reaches_child = 0;
            }

            if (stmt->children[i] && stmt->children[i]->kind == AST_STMT_DECLARATION) {
                flow_scope_stack_add_declaration_names(&block_scopes,
                    stmt->children[i]->declaration_names,
                    stmt->children[i]->declaration_name_count);
                if (block_scopes.overflowed) {
                    guard_ids->overflowed = 1;
                    return summary;
                }
            }
        }

        return summary;
    }
    case AST_STMT_WHILE:
    case AST_STMT_FOR: {
        int child_guaranteed_return = 0;

        if (!semantic_analyze_statement_flow(stmt,
            &child_guaranteed_return,
            NULL,
            NULL,
            NULL,
            NULL)) {
            return summary;
        }

        summary.may_exit_loop = child_guaranteed_return;
        summary.exits_loop_on_all_paths = child_guaranteed_return;
        return summary;
    }
    case AST_STMT_DECLARATION:
    case AST_STMT_EXPRESSION:
    case AST_STMT_CONTINUE:
    default:
        return summary;
    }
}

static int flow_try_eval_constant_arithmetic(TokenType op,
    long long lhs,
    long long rhs,
    long long *out_value) {
    const int shift_width = (int)(sizeof(long long) * 8u);

    if (!out_value) {
        return 0;
    }

    switch (op) {
    case TOKEN_PLUS:
        return !__builtin_add_overflow(lhs, rhs, out_value);
    case TOKEN_MINUS:
        return !__builtin_sub_overflow(lhs, rhs, out_value);
    case TOKEN_STAR:
        return !__builtin_mul_overflow(lhs, rhs, out_value);
    case TOKEN_SLASH:
        if (rhs == 0) {
            return 0;
        }
        if (lhs == LLONG_MIN && rhs == -1) {
            return 0;
        }
        *out_value = lhs / rhs;
        return 1;
    case TOKEN_PERCENT:
        if (rhs == 0) {
            return 0;
        }
        if (lhs == LLONG_MIN && rhs == -1) {
            return 0;
        }
        *out_value = lhs % rhs;
        return 1;
    case TOKEN_SHIFT_LEFT:
        if (rhs < 0 || rhs >= shift_width || lhs < 0) {
            return 0;
        }
        if ((unsigned long long)lhs > ((unsigned long long)LLONG_MAX >> (unsigned long long)rhs)) {
            return 0;
        }
        *out_value = lhs << rhs;
        return 1;
    case TOKEN_SHIFT_RIGHT:
        if (rhs < 0 || rhs >= shift_width || lhs < 0) {
            return 0;
        }
        *out_value = lhs >> rhs;
        return 1;
    default:
        return 0;
    }
}

static int flow_try_eval_constant_int_with_depth(const AstExpression *expr,
    long long *out_value,
    size_t depth) {
    long long operand_value;
    long long lhs_value;
    long long rhs_value;
    long long cond_value;

    if (!expr || !out_value) {
        return 0;
    }

    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        *out_value = expr->as.number_value;
        return 1;
    case AST_EXPR_PAREN:
        return flow_try_eval_constant_int_with_depth(expr->as.inner, out_value, depth + 1);
    case AST_EXPR_UNARY:
        if (!flow_try_eval_constant_int_with_depth(expr->as.unary.operand,
            &operand_value,
            depth + 1)) {
            return 0;
        }
        switch (expr->as.unary.op) {
        case TOKEN_PLUS:
            *out_value = operand_value;
            return 1;
        case TOKEN_MINUS:
            if (operand_value == LLONG_MIN) {
                return 0;
            }
            *out_value = -operand_value;
            return 1;
        case TOKEN_BANG:
            *out_value = !operand_value;
            return 1;
        case TOKEN_TILDE:
            *out_value = ~operand_value;
            return 1;
        default:
            return 0;
        }
    case AST_EXPR_TERNARY:
        if (!flow_try_eval_constant_int_with_depth(expr->as.ternary.condition,
            &cond_value,
            depth + 1)) {
            return 0;
        }
        if (cond_value != 0) {
            return flow_try_eval_constant_int_with_depth(expr->as.ternary.then_expr,
                out_value,
                depth + 1);
        }
        return flow_try_eval_constant_int_with_depth(expr->as.ternary.else_expr,
            out_value,
            depth + 1);
    case AST_EXPR_BINARY:
        switch (expr->as.binary.op) {
        case TOKEN_AND_AND:
            if (!flow_try_eval_constant_int_with_depth(expr->as.binary.left,
                &lhs_value,
                depth + 1)) {
                return 0;
            }
            if (lhs_value == 0) {
                *out_value = 0;
                return 1;
            }
            if (!flow_try_eval_constant_int_with_depth(expr->as.binary.right,
                &rhs_value,
                depth + 1)) {
                return 0;
            }
            *out_value = (rhs_value != 0) ? 1 : 0;
            return 1;
        case TOKEN_OR_OR:
            if (!flow_try_eval_constant_int_with_depth(expr->as.binary.left,
                &lhs_value,
                depth + 1)) {
                return 0;
            }
            if (lhs_value != 0) {
                *out_value = 1;
                return 1;
            }
            if (!flow_try_eval_constant_int_with_depth(expr->as.binary.right,
                &rhs_value,
                depth + 1)) {
                return 0;
            }
            *out_value = (rhs_value != 0) ? 1 : 0;
            return 1;
        default:
            break;
        }

        if (!flow_try_eval_constant_int_with_depth(expr->as.binary.left,
            &lhs_value,
            depth + 1) ||
            !flow_try_eval_constant_int_with_depth(expr->as.binary.right,
                &rhs_value,
                depth + 1)) {
            return 0;
        }

        switch (expr->as.binary.op) {
        case TOKEN_COMMA:
            *out_value = rhs_value;
            return 1;
        case TOKEN_EQ:
            *out_value = (lhs_value == rhs_value) ? 1 : 0;
            return 1;
        case TOKEN_NE:
            *out_value = (lhs_value != rhs_value) ? 1 : 0;
            return 1;
        case TOKEN_LT:
            *out_value = (lhs_value < rhs_value) ? 1 : 0;
            return 1;
        case TOKEN_LE:
            *out_value = (lhs_value <= rhs_value) ? 1 : 0;
            return 1;
        case TOKEN_GT:
            *out_value = (lhs_value > rhs_value) ? 1 : 0;
            return 1;
        case TOKEN_GE:
            *out_value = (lhs_value >= rhs_value) ? 1 : 0;
            return 1;
        case TOKEN_AMP:
            *out_value = lhs_value & rhs_value;
            return 1;
        case TOKEN_CARET:
            *out_value = lhs_value ^ rhs_value;
            return 1;
        case TOKEN_PIPE:
            *out_value = lhs_value | rhs_value;
            return 1;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_PERCENT:
        case TOKEN_SHIFT_LEFT:
        case TOKEN_SHIFT_RIGHT:
            return flow_try_eval_constant_arithmetic(expr->as.binary.op,
                lhs_value,
                rhs_value,
                out_value);
        default:
            return 0;
        }
    default:
        return 0;
    }
}

static int flow_try_eval_constant_int(const AstExpression *expr, long long *out_value) {
    return flow_try_eval_constant_int_with_depth(expr, out_value, 0);
}

static int expression_is_constant_true_for_flow(const AstExpression *expr) {
    long long value;

    return flow_try_eval_constant_int(expr, &value) && value != 0;
}

static int expression_is_constant_false_for_flow(const AstExpression *expr) {
    long long value;

    return flow_try_eval_constant_int(expr, &value) && value == 0;
}

static int semantic_analyze_statement_flow(const AstStatement *stmt,
    int *out_guaranteed_return,
    int *out_may_fallthrough,
    int *out_may_break,
    int *out_may_continue_loop,
    int *out_may_non_terminating) {
    size_t i;
    int guaranteed_return = 0;
    int may_fallthrough = 1;
    int may_break = 0;
    int may_continue_loop = 0;
    int may_non_terminating = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }
    if (out_may_continue_loop) {
        *out_may_continue_loop = 0;
    }
    if (out_may_non_terminating) {
        *out_may_non_terminating = 0;
    }

    if (!stmt) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND: {
        int block_guaranteed_return = 0;
        int block_may_fallthrough = 1;
        int block_may_break = 0;
        int block_may_continue_loop = 0;
        int block_may_non_terminating = 0;

        for (i = 0; i < stmt->child_count; ++i) {
            int child_guaranteed_return = 0;
            int child_may_fallthrough = 1;
            int child_may_break = 0;
            int child_may_continue_loop = 0;
            int child_may_non_terminating = 0;

            if (!semantic_analyze_statement_flow(stmt->children[i],
                &child_guaranteed_return,
                &child_may_fallthrough,
                &child_may_break,
                &child_may_continue_loop,
                &child_may_non_terminating)) {
                return 0;
            }

            if (block_may_fallthrough && child_may_break) {
                block_may_break = 1;
            }
            if (block_may_fallthrough && child_may_continue_loop) {
                block_may_continue_loop = 1;
            }
            if (block_may_fallthrough && child_may_non_terminating) {
                block_may_non_terminating = 1;
            }

            if (block_may_fallthrough) {
                if (child_guaranteed_return) {
                    block_guaranteed_return = 1;
                    block_may_fallthrough = 0;
                } else if (!child_may_fallthrough) {
                    block_guaranteed_return = 0;
                    block_may_fallthrough = 0;
                } else {
                    block_guaranteed_return = 0;
                }
            }
        }

        guaranteed_return = block_guaranteed_return;
        may_fallthrough = block_may_fallthrough;
        may_break = block_may_break;
        may_continue_loop = block_may_continue_loop;
        may_non_terminating = block_may_non_terminating;
        break;
    }
    case AST_STMT_IF: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_is_const_true = expression_is_constant_true_for_flow(cond_expr);
        int cond_is_const_false = expression_is_constant_false_for_flow(cond_expr);
        int then_guaranteed_return = 0;
        int then_may_fallthrough = 1;
        int then_may_break = 0;
        int then_may_continue_loop = 0;
        int then_may_non_terminating = 0;
        int else_guaranteed_return = 0;
        int else_may_fallthrough = 1;
        int else_may_break = 0;
        int else_may_continue_loop = 0;
        int else_may_non_terminating = 0;
        int has_else = stmt->child_count >= 2;

        if (stmt->child_count >= 1 &&
            !semantic_analyze_statement_flow(stmt->children[0],
                &then_guaranteed_return,
                &then_may_fallthrough,
                &then_may_break,
                &then_may_continue_loop,
                &then_may_non_terminating)) {
            return 0;
        }

        if (has_else &&
            !semantic_analyze_statement_flow(stmt->children[1],
                &else_guaranteed_return,
                &else_may_fallthrough,
                &else_may_break,
                &else_may_continue_loop,
                &else_may_non_terminating)) {
            return 0;
        }

        if (cond_is_const_true) {
            guaranteed_return = then_guaranteed_return;
            may_fallthrough = then_may_fallthrough;
            may_break = then_may_break;
            may_continue_loop = then_may_continue_loop;
            may_non_terminating = then_may_non_terminating;
        } else if (cond_is_const_false) {
            guaranteed_return = has_else ? else_guaranteed_return : 0;
            may_fallthrough = has_else ? else_may_fallthrough : 1;
            may_break = has_else ? else_may_break : 0;
            may_continue_loop = has_else ? else_may_continue_loop : 0;
            may_non_terminating = has_else ? else_may_non_terminating : 0;
        } else {
            guaranteed_return = has_else && then_guaranteed_return && else_guaranteed_return;
            may_fallthrough = has_else ? (then_may_fallthrough || else_may_fallthrough) : 1;
            may_break = has_else ? (then_may_break || else_may_break) : then_may_break;
            may_continue_loop = has_else ? (then_may_continue_loop || else_may_continue_loop)
                : then_may_continue_loop;
            may_non_terminating = has_else ? (then_may_non_terminating || else_may_non_terminating)
                : then_may_non_terminating;
        }
        break;
    }
    case AST_STMT_WHILE: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_always_true = expression_is_constant_true_for_flow(
            cond_expr);
        int cond_always_false = expression_is_constant_false_for_flow(
            cond_expr);
        const AstStatement *body_stmt = stmt->child_count >= 1 ? stmt->children[0] : NULL;
        FlowGuardIdentifierSet guard_ids = {{0}, {0}, {0}, 0, 0};
        FlowIdentifierScopeStack loop_scopes = {0};
        FlowLoopExitSummary exit_summary = {0, 0};
        int body_guaranteed_return = 0;
        int body_may_break = 0;
        int body_changes_guard_state = 0;

        if (body_stmt &&
            !semantic_analyze_statement_flow(body_stmt,
                &body_guaranteed_return,
                NULL,
                &body_may_break,
                NULL,
                NULL)) {
            return 0;
        }

        if (!cond_always_true && !cond_always_false) {
            flow_collect_expression_identifiers(cond_expr, &guard_ids, &loop_scopes);
        }
        if (body_stmt) {
            exit_summary = flow_collect_loop_exit_summary(body_stmt, &guard_ids, &loop_scopes);
            body_changes_guard_state = flow_statement_may_change_guard_state(body_stmt,
                &guard_ids,
                &loop_scopes);
        }

        if (cond_always_false) {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        } else if (body_guaranteed_return && cond_always_true) {
            guaranteed_return = 1;
            may_fallthrough = 0;
            may_non_terminating = 0;
        } else if (body_guaranteed_return) {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        } else if (exit_summary.exits_loop_on_all_paths) {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        } else if (!guard_ids.overflowed && guard_ids.count > 0 && !body_changes_guard_state) {
            guaranteed_return = 0;
            may_fallthrough = 0;
            may_non_terminating = 1;
        } else if (cond_always_true && !body_may_break) {
            guaranteed_return = 0;
            may_fallthrough = 0;
            may_non_terminating = 1;
        } else {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        }

        may_break = 0;
        may_continue_loop = 0;
        break;
    }
    case AST_STMT_FOR: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        const AstExpression *step_expr = statement_get_for_step_expression(stmt);
        int cond_always_true = 1;
        int cond_always_false = 0;
        const AstStatement *body_stmt = stmt->child_count >= 1 ? stmt->children[0] : NULL;
        FlowGuardIdentifierSet guard_ids = {{0}, {0}, {0}, 0, 0};
        FlowIdentifierScopeStack loop_scopes = {0};
        FlowLoopExitSummary exit_summary = {0, 0};
        int body_guaranteed_return = 0;
        int body_may_break = 0;
        int body_changes_guard_state = 0;
        int step_changes_guard_state = 0;

        if (stmt->has_condition_expression) {
            cond_always_true = expression_is_constant_true_for_flow(cond_expr);
            cond_always_false = expression_is_constant_false_for_flow(cond_expr);
        }

        if (body_stmt &&
            !semantic_analyze_statement_flow(body_stmt,
                &body_guaranteed_return,
                NULL,
                &body_may_break,
                NULL,
                NULL)) {
            return 0;
        }

        if (stmt->declaration_name_count > 0) {
            if (!flow_scope_stack_push(&loop_scopes)) {
                guard_ids.overflowed = 1;
            }
            flow_scope_stack_add_declaration_names(&loop_scopes,
                stmt->declaration_names,
                stmt->declaration_name_count);
            if (loop_scopes.overflowed) {
                guard_ids.overflowed = 1;
            }
        }

        if (!cond_always_true && !cond_always_false) {
            flow_collect_expression_identifiers(cond_expr, &guard_ids, &loop_scopes);
        }
        if (body_stmt) {
            exit_summary = flow_collect_loop_exit_summary(body_stmt, &guard_ids, &loop_scopes);
            body_changes_guard_state = flow_statement_may_change_guard_state(body_stmt,
                &guard_ids,
                &loop_scopes);
        }
        if (step_expr) {
            step_changes_guard_state = flow_expression_may_change_guard_state(step_expr,
                &guard_ids,
                &loop_scopes);
        }

        if (cond_always_false) {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        } else if (body_guaranteed_return && cond_always_true) {
            guaranteed_return = 1;
            may_fallthrough = 0;
            may_non_terminating = 0;
        } else if (body_guaranteed_return) {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        } else if (exit_summary.exits_loop_on_all_paths) {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        } else if (!guard_ids.overflowed && guard_ids.count > 0 &&
            !body_changes_guard_state && !step_changes_guard_state) {
            guaranteed_return = 0;
            may_fallthrough = 0;
            may_non_terminating = 1;
        } else if (cond_always_true && !body_may_break) {
            guaranteed_return = 0;
            may_fallthrough = 0;
            may_non_terminating = 1;
        } else {
            guaranteed_return = 0;
            may_fallthrough = 1;
            may_non_terminating = 0;
        }

        may_break = 0;
        may_continue_loop = 0;
        break;
    }
    case AST_STMT_RETURN:
        guaranteed_return = 1;
        may_fallthrough = 0;
        may_break = 0;
        may_continue_loop = 0;
        may_non_terminating = 0;
        break;
    case AST_STMT_BREAK:
        guaranteed_return = 0;
        may_fallthrough = 0;
        may_break = 1;
        may_continue_loop = 0;
        may_non_terminating = 0;
        break;
    case AST_STMT_CONTINUE:
        guaranteed_return = 0;
        may_fallthrough = 0;
        may_break = 0;
        may_continue_loop = 1;
        may_non_terminating = 0;
        break;
    case AST_STMT_DECLARATION:
    case AST_STMT_EXPRESSION:
    default:
        guaranteed_return = 0;
        may_fallthrough = 1;
        may_break = 0;
        may_continue_loop = 0;
        may_non_terminating = 0;
        break;
    }

    if (out_guaranteed_return) {
        *out_guaranteed_return = guaranteed_return;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = may_fallthrough;
    }
    if (out_may_break) {
        *out_may_break = may_break;
    }
    if (out_may_continue_loop) {
        *out_may_continue_loop = may_continue_loop;
    }
    if (out_may_non_terminating) {
        *out_may_non_terminating = may_non_terminating;
    }
    return 1;
}

static int semantic_compute_function_returns_all_paths(const AstExternal *func,
    int *out_returns_all_paths,
    SemanticError *error) {
    int guaranteed_return = 0;
    int may_non_terminating = 0;

    if (out_returns_all_paths) {
        *out_returns_all_paths = 0;
    }

    if (!func) {
        return 1;
    }

    if (!func->function_body) {
        semantic_set_error(error,
            func->line,
            func->column,
            "SEMA-INT-005: function definition missing statement AST body during AST-primary semantic phase");
        return 0;
    }

    if (!semantic_analyze_statement_flow(func->function_body,
            &guaranteed_return,
            NULL,
            NULL,
            NULL,
            &may_non_terminating)) {
        semantic_set_error(error,
            func->line,
            func->column,
            "SEMA-INT-007: failed to analyze function return-path flow from statement AST");
        return 0;
    }

    if (out_returns_all_paths) {
        *out_returns_all_paths = guaranteed_return && !may_non_terminating;
    }

    return 1;
}

#endif
