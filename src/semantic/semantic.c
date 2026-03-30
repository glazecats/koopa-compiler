#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALLEE_PREVIEW_HEAD 24
#define CALLEE_PREVIEW_TAIL 16

typedef int (*ExpressionPostVisitor)(const AstExpression *expr, void *ctx);

typedef struct {
    const AstProgram *program;
    size_t func_index;
    SemanticError *error;
} CallableRuleVisitCtx;

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

static int walk_expression_postorder(const AstExpression *expr,
                                     ExpressionPostVisitor visitor,
                                     void *ctx) {
    size_t i;

    if (!expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_PAREN:
        if (!walk_expression_postorder(expr->as.inner, visitor, ctx)) {
            return 0;
        }
        break;
    case AST_EXPR_UNARY:
        if (!walk_expression_postorder(expr->as.unary.operand, visitor, ctx)) {
            return 0;
        }
        break;
    case AST_EXPR_POSTFIX:
        if (!walk_expression_postorder(expr->as.postfix.operand, visitor, ctx)) {
            return 0;
        }
        break;
    case AST_EXPR_BINARY:
        if (!walk_expression_postorder(expr->as.binary.left, visitor, ctx) ||
            !walk_expression_postorder(expr->as.binary.right, visitor, ctx)) {
            return 0;
        }
        break;
    case AST_EXPR_TERNARY:
        if (!walk_expression_postorder(expr->as.ternary.condition, visitor, ctx) ||
            !walk_expression_postorder(expr->as.ternary.then_expr, visitor, ctx) ||
            !walk_expression_postorder(expr->as.ternary.else_expr, visitor, ctx)) {
            return 0;
        }
        break;
    case AST_EXPR_CALL:
        if (!walk_expression_postorder(expr->as.call.callee, visitor, ctx)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!walk_expression_postorder(expr->as.call.args[i], visitor, ctx)) {
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

static int walk_statement_postorder(const AstStatement *stmt,
                                    ExpressionPostVisitor visitor,
                                    void *ctx) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (!walk_expression_postorder(stmt->expressions[i], visitor, ctx)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!walk_statement_postorder(stmt->children[i], visitor, ctx)) {
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

static int expression_is_constant_true_for_flow(const AstExpression *expr) {
    return expr && expr->kind == AST_EXPR_NUMBER && expr->as.number_value != 0;
}

static int semantic_analyze_statement_flow(const AstStatement *stmt,
                                           int *out_guaranteed_return,
                                           int *out_may_fallthrough,
                                           int *out_may_break) {
    size_t i;
    int guaranteed_return = 0;
    int may_fallthrough = 1;
    int may_break = 0;

    if (out_guaranteed_return) {
        *out_guaranteed_return = 0;
    }
    if (out_may_fallthrough) {
        *out_may_fallthrough = 1;
    }
    if (out_may_break) {
        *out_may_break = 0;
    }

    if (!stmt) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_COMPOUND: {
        int block_guaranteed_return = 0;
        int block_may_fallthrough = 1;
        int block_may_break = 0;

        for (i = 0; i < stmt->child_count; ++i) {
            int child_guaranteed_return = 0;
            int child_may_fallthrough = 1;
            int child_may_break = 0;

            if (!semantic_analyze_statement_flow(stmt->children[i],
                                                 &child_guaranteed_return,
                                                 &child_may_fallthrough,
                                                 &child_may_break)) {
                return 0;
            }

            if (block_may_fallthrough && child_may_break) {
                block_may_break = 1;
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
        break;
    }
    case AST_STMT_IF: {
        int then_guaranteed_return = 0;
        int then_may_fallthrough = 1;
        int then_may_break = 0;
        int else_guaranteed_return = 0;
        int else_may_fallthrough = 1;
        int else_may_break = 0;
        int has_else = stmt->child_count >= 2;

        if (stmt->child_count >= 1 &&
            !semantic_analyze_statement_flow(stmt->children[0],
                                             &then_guaranteed_return,
                                             &then_may_fallthrough,
                                             &then_may_break)) {
            return 0;
        }

        if (has_else &&
            !semantic_analyze_statement_flow(stmt->children[1],
                                             &else_guaranteed_return,
                                             &else_may_fallthrough,
                                             &else_may_break)) {
            return 0;
        }

        guaranteed_return = has_else && then_guaranteed_return && else_guaranteed_return;
        may_fallthrough = has_else ? (then_may_fallthrough || else_may_fallthrough) : 1;
        may_break = has_else ? (then_may_break || else_may_break) : then_may_break;
        break;
    }
    case AST_STMT_WHILE: {
        int cond_always_true = expression_is_constant_true_for_flow(
            statement_get_condition_expression(stmt));
        int body_guaranteed_return = 0;
        int body_may_break = 0;

        if (stmt->child_count >= 1 &&
            !semantic_analyze_statement_flow(stmt->children[0],
                                             &body_guaranteed_return,
                                             NULL,
                                             &body_may_break)) {
            return 0;
        }

        if (cond_always_true) {
            if (body_may_break) {
                guaranteed_return = 0;
                may_fallthrough = 1;
            } else {
                guaranteed_return = body_guaranteed_return;
                may_fallthrough = 0;
            }
        } else {
            guaranteed_return = 0;
            may_fallthrough = 1;
        }

        may_break = 0;
        break;
    }
    case AST_STMT_FOR: {
        const AstExpression *cond_expr = statement_get_condition_expression(stmt);
        int cond_always_true = 1;
        int body_guaranteed_return = 0;
        int body_may_break = 0;

        if (stmt->has_condition_expression) {
            cond_always_true = expression_is_constant_true_for_flow(cond_expr);
        }

        if (stmt->child_count >= 1 &&
            !semantic_analyze_statement_flow(stmt->children[0],
                                             &body_guaranteed_return,
                                             NULL,
                                             &body_may_break)) {
            return 0;
        }

        if (cond_always_true) {
            if (body_may_break) {
                guaranteed_return = 0;
                may_fallthrough = 1;
            } else {
                guaranteed_return = body_guaranteed_return;
                may_fallthrough = 0;
            }
        } else {
            guaranteed_return = 0;
            may_fallthrough = 1;
        }

        may_break = 0;
        break;
    }
    case AST_STMT_RETURN:
        guaranteed_return = 1;
        may_fallthrough = 0;
        may_break = 0;
        break;
    case AST_STMT_BREAK:
        guaranteed_return = 0;
        may_fallthrough = 0;
        may_break = 1;
        break;
    case AST_STMT_CONTINUE:
        guaranteed_return = 0;
        may_fallthrough = 0;
        may_break = 0;
        break;
    case AST_STMT_DECLARATION:
    case AST_STMT_EXPRESSION:
    default:
        guaranteed_return = 0;
        may_fallthrough = 1;
        may_break = 0;
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
    return 1;
}

static int semantic_compute_function_returns_all_paths(const AstExternal *func,
                                                       int *out_returns_all_paths,
                                                       SemanticError *error) {
    int guaranteed_return = 0;

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

    if (!semantic_analyze_statement_flow(func->function_body, &guaranteed_return, NULL, NULL)) {
        semantic_set_error(error,
                           func->line,
                           func->column,
                           "SEMA-INT-007: failed to analyze function return-path flow from statement AST");
        return 0;
    }

    if (out_returns_all_paths) {
        *out_returns_all_paths = guaranteed_return;
    }

    return 1;
}

static int semantic_check_single_callable_rule(const AstProgram *program,
                                               size_t func_index,
                                               const char *called_name,
                                               int call_line,
                                               int call_column,
                                               size_t called_arg_count,
                                               int callee_kind,
                                               SemanticError *error) {
    size_t j;
    int found_decl = 0;
    int found_matching_param_count = 0;
    int found_non_function_symbol = 0;
    int has_later_function_decl = 0;
    int later_decl_line = 0;
    int later_decl_column = 0;
    size_t expected_arg_count = 0;
    char callee_preview[128];

    format_callee_preview(called_name, callee_preview, sizeof(callee_preview));

    if (!called_name || called_name[0] == '\0') {
        if (error) {
            if (callee_kind == AST_CALL_CALLEE_CALL_RESULT) {
                semantic_set_error(error,
                                   call_line,
                                   call_column,
                                   "SEMA-CALL-005: call result is not callable in this semantic subset; callee_kind=call_result");
            } else {
                semantic_set_error(error,
                                   call_line,
                                   call_column,
                                   "SEMA-CALL-006: non-identifier callee is not supported in this semantic subset; callee_kind=non_identifier");
            }
        }
        return 0;
    }

    for (j = 0; j <= func_index; ++j) {
        const AstExternal *candidate = &program->externals[j];

        if (!candidate->name) {
            continue;
        }

        if (strcmp(candidate->name, called_name) != 0) {
            continue;
        }

        if (candidate->kind == AST_EXTERNAL_FUNCTION) {
            found_decl = 1;
            expected_arg_count = candidate->parameter_count;
            if (candidate->parameter_count == called_arg_count) {
                found_matching_param_count = 1;
                break;
            }
            continue;
        }

        found_non_function_symbol = 1;
    }

    if (!found_decl) {
        for (j = func_index + 1; j < program->count; ++j) {
            const AstExternal *candidate = &program->externals[j];

            if (!candidate->name || candidate->kind != AST_EXTERNAL_FUNCTION) {
                continue;
            }

            if (strcmp(candidate->name, called_name) == 0) {
                has_later_function_decl = 1;
                later_decl_line = candidate->line;
                later_decl_column = candidate->column;
                break;
            }
        }
    }

    if (!found_decl && found_non_function_symbol) {
        if (error) {
            char msg[512];
            snprintf(msg,
                     sizeof(msg),
                     "SEMA-CALL-003: callee=%s; symbol_kind=non_function; call to non-function symbol",
                     callee_preview);
            semantic_set_error(error, call_line, call_column, msg);
        }
        return 0;
    }

    if (found_decl && !found_matching_param_count) {
        if (error) {
            char msg[512];
            snprintf(msg,
                     sizeof(msg),
                     "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
                     callee_preview,
                     expected_arg_count,
                     called_arg_count);
            semantic_set_error(error, call_line, call_column, msg);
        }
        return 0;
    }

    if (!found_decl) {
        if (error) {
            char msg[512];
            if (has_later_function_decl) {
                snprintf(msg,
                         sizeof(msg),
                         "SEMA-CALL-002: callee=%s; decl_line=%d; decl_col=%d; call before declaration",
                         callee_preview,
                         later_decl_line,
                         later_decl_column);
            } else {
                snprintf(msg,
                         sizeof(msg),
                         "SEMA-CALL-001: callee=%s; call to undeclared function",
                         callee_preview);
            }
            semantic_set_error(error, call_line, call_column, msg);
        }
        return 0;
    }

    return 1;
}

static int semantic_check_call_expr_postorder(const AstExpression *expr, void *ctx) {
    CallableRuleVisitCtx *visit_ctx;
    const char *called_name = NULL;
    int callee_kind;

    if (!expr || expr->kind != AST_EXPR_CALL) {
        return 1;
    }

    if (!ctx) {
        return 0;
    }

    visit_ctx = (CallableRuleVisitCtx *)ctx;
    callee_kind = classify_ast_call_callee(expr->as.call.callee, &called_name);

    return semantic_check_single_callable_rule(visit_ctx->program,
                                               visit_ctx->func_index,
                                               called_name,
                                               expr->line,
                                               expr->column,
                                               expr->as.call.arg_count,
                                               callee_kind,
                                               visit_ctx->error);
}

static int semantic_check_function_callable_rules(const AstProgram *program,
                                                  size_t func_index,
                                                  const AstExternal *func,
                                                  SemanticError *error) {
    CallableRuleVisitCtx visit_ctx;

    if (!program || !func) {
        return 1;
    }

    /* S2-9: function-definition callable checks are AST-primary only. */
    if (!func->is_function_definition) {
        return 1;
    }

    if (!func->function_body) {
        semantic_set_error(error,
                           func->line,
                           func->column,
                           "SEMA-INT-005: function definition missing statement AST body during AST-primary semantic phase");
        return 0;
    }

    visit_ctx.program = program;
    visit_ctx.func_index = func_index;
    visit_ctx.error = error;
    return walk_statement_postorder(func->function_body,
                                    semantic_check_call_expr_postorder,
                                    &visit_ctx);
}

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
        next_capacity = (stack->capacity == 0) ? 8 : (stack->capacity * 2);
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
        next_capacity = (frame->capacity == 0) ? 8 : (frame->capacity * 2);
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
                                           const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0') {
        return 0;
    }

    for (i = 0; i <= func_index && i < program->count; ++i) {
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
                                        const ScopeStack *scopes,
                                        const char *name) {
    if (scope_stack_contains_local_name(scopes, name)) {
        return 1;
    }

    return semantic_top_level_name_visible(program, func_index, name);
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

static int semantic_scope_check_call_shadow_expression(const AstExpression *expr,
                                                       const ScopeStack *scopes,
                                                       SemanticError *error) {
    size_t i;

    if (!expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_PAREN:
        return semantic_scope_check_call_shadow_expression(expr->as.inner, scopes, error);
    case AST_EXPR_UNARY:
        return semantic_scope_check_call_shadow_expression(expr->as.unary.operand,
                                                           scopes,
                                                           error);
    case AST_EXPR_POSTFIX:
        return semantic_scope_check_call_shadow_expression(expr->as.postfix.operand,
                                                           scopes,
                                                           error);
    case AST_EXPR_BINARY:
        return semantic_scope_check_call_shadow_expression(expr->as.binary.left, scopes, error) &&
               semantic_scope_check_call_shadow_expression(expr->as.binary.right, scopes, error);
    case AST_EXPR_TERNARY:
        return semantic_scope_check_call_shadow_expression(expr->as.ternary.condition,
                                                           scopes,
                                                           error) &&
               semantic_scope_check_call_shadow_expression(expr->as.ternary.then_expr,
                                                           scopes,
                                                           error) &&
               semantic_scope_check_call_shadow_expression(expr->as.ternary.else_expr,
                                                           scopes,
                                                           error);
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

        if (!semantic_scope_check_call_shadow_expression(expr->as.call.callee, scopes, error)) {
            return 0;
        }

        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_scope_check_call_shadow_expression(expr->as.call.args[i],
                                                             scopes,
                                                             error)) {
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

static int semantic_scope_check_expression(const AstExpression *expr,
                                           const AstProgram *program,
                                           size_t func_index,
                                           const ScopeStack *scopes,
                                           int in_call_callee,
                                           SemanticError *error) {
    size_t i;

    if (!expr) {
        return 1;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        if (!in_call_callee && expr->as.identifier.name && expr->as.identifier.name[0] != '\0' &&
            !semantic_scope_name_resolved(program, func_index, scopes, expr->as.identifier.name)) {
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
        return semantic_scope_check_expression(expr->as.inner,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error);
    case AST_EXPR_UNARY:
        return semantic_scope_check_expression(expr->as.unary.operand,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error);
    case AST_EXPR_POSTFIX:
        return semantic_scope_check_expression(expr->as.postfix.operand,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error);
    case AST_EXPR_BINARY:
        return semantic_scope_check_expression(expr->as.binary.left,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error) &&
               semantic_scope_check_expression(expr->as.binary.right,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error);
    case AST_EXPR_TERNARY:
        return semantic_scope_check_expression(expr->as.ternary.condition,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error) &&
               semantic_scope_check_expression(expr->as.ternary.then_expr,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error) &&
               semantic_scope_check_expression(expr->as.ternary.else_expr,
                                               program,
                                               func_index,
                                               scopes,
                                               in_call_callee,
                                               error);
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
        if (!semantic_scope_check_expression(expr->as.call.callee,
                                             program,
                                             func_index,
                                             scopes,
                                             1,
                                             error)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_scope_check_expression(expr->as.call.args[i],
                                                 program,
                                                 func_index,
                                                 scopes,
                                                 0,
                                                 error)) {
                return 0;
            }
        }
        return 1;
    }
    default:
        return 1;
    }
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

int semantic_analyze_program(const AstProgram *program, SemanticError *error) {
    size_t i;
    size_t j;

    if (!program) {
        semantic_set_error(error, 0, 0, "Semantic input program is NULL");
        return 0;
    }

    if (error) {
        error->line = 0;
        error->column = 0;
        error->message[0] = '\0';
    }

    for (i = 0; i < program->count; ++i) {
        const AstExternal *lhs = &program->externals[i];
        int returns_all_paths = 0;

        /* AST contract allows unnamed externals (name_token == NULL). */
        if (!lhs->name || lhs->name_length == 0) {
            continue;
        }

        if (lhs->kind == AST_EXTERNAL_FUNCTION && lhs->is_function_definition) {
            if (!semantic_compute_function_returns_all_paths(lhs, &returns_all_paths, error)) {
                return 0;
            }

            if (!returns_all_paths) {
                if (error) {
                    semantic_set_error(error,
                                       lhs->line,
                                       lhs->column,
                                       "Function definition must return on all control-flow paths");
                }
                return 0;
            }

            if (!semantic_check_function_callable_scope_shadow_rules(lhs, error)) {
                return 0;
            }

            if (!semantic_check_function_callable_rules(program, i, lhs, error)) {
                return 0;
            }

            if (!semantic_check_function_scope_rules(program, i, lhs, error)) {
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
                if (lhs->parameter_count != rhs->parameter_count) {
                    if (error) {
                        semantic_set_error(error,
                                           rhs->line,
                                           rhs->column,
                                           "Conflicting function declarations: parameter count mismatch");
                    }
                    return 0;
                }

                if (lhs->is_function_definition && rhs->is_function_definition) {
                    if (error) {
                        semantic_set_error(error,
                                           rhs->line,
                                           rhs->column,
                                           "Duplicate function definition");
                    }
                    return 0;
                }

                /* Multiple compatible declarations are currently accepted. */
                continue;
            }

            if (lhs->kind != rhs->kind) {
                if (error) {
                    semantic_set_error(error,
                                       rhs->line,
                                       rhs->column,
                                       "Function/variable name conflict at top level");
                }
                return 0;
            }
        }
    }

    return 1;
}