#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

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

static int flow_try_eval_constant_int(const AstExpression *expr, long long *out_value) {
    long long operand_value;
    long long lhs_value;
    long long rhs_value;
    long long cond_value;

    if (!expr || !out_value) {
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        *out_value = expr->as.number_value;
        return 1;
    case AST_EXPR_PAREN:
        return flow_try_eval_constant_int(expr->as.inner, out_value);
    case AST_EXPR_UNARY:
        if (!flow_try_eval_constant_int(expr->as.unary.operand, &operand_value)) {
            return 0;
        }
        switch (expr->as.unary.op) {
        case TOKEN_PLUS:
            *out_value = operand_value;
            return 1;
        case TOKEN_MINUS:
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
        if (!flow_try_eval_constant_int(expr->as.ternary.condition, &cond_value)) {
            return 0;
        }
        if (cond_value != 0) {
            return flow_try_eval_constant_int(expr->as.ternary.then_expr, out_value);
        }
        return flow_try_eval_constant_int(expr->as.ternary.else_expr, out_value);
    case AST_EXPR_BINARY:
        switch (expr->as.binary.op) {
        case TOKEN_AND_AND:
            if (!flow_try_eval_constant_int(expr->as.binary.left, &lhs_value)) {
                return 0;
            }
            if (lhs_value == 0) {
                *out_value = 0;
                return 1;
            }
            if (!flow_try_eval_constant_int(expr->as.binary.right, &rhs_value)) {
                return 0;
            }
            *out_value = (rhs_value != 0) ? 1 : 0;
            return 1;
        case TOKEN_OR_OR:
            if (!flow_try_eval_constant_int(expr->as.binary.left, &lhs_value)) {
                return 0;
            }
            if (lhs_value != 0) {
                *out_value = 1;
                return 1;
            }
            if (!flow_try_eval_constant_int(expr->as.binary.right, &rhs_value)) {
                return 0;
            }
            *out_value = (rhs_value != 0) ? 1 : 0;
            return 1;
        default:
            break;
        }

        if (!flow_try_eval_constant_int(expr->as.binary.left, &lhs_value) ||
            !flow_try_eval_constant_int(expr->as.binary.right, &rhs_value)) {
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

static int expression_is_constant_true_for_flow(const AstExpression *expr) {
    long long value;

    return flow_try_eval_constant_int(expr, &value) && value != 0;
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

        guaranteed_return = has_else && then_guaranteed_return && else_guaranteed_return;
        may_fallthrough = has_else ? (then_may_fallthrough || else_may_fallthrough) : 1;
        may_break = has_else ? (then_may_break || else_may_break) : then_may_break;
        may_continue_loop =
            has_else ? (then_may_continue_loop || else_may_continue_loop) : then_may_continue_loop;
        may_non_terminating =
            has_else ? (then_may_non_terminating || else_may_non_terminating)
                     : then_may_non_terminating;
        break;
    }
    case AST_STMT_WHILE: {
        int cond_always_true = expression_is_constant_true_for_flow(
            statement_get_condition_expression(stmt));
        int body_guaranteed_return = 0;
        int body_may_fallthrough = 1;
        int body_may_break = 0;
        int body_may_continue_loop = 0;
        int body_may_non_terminating = 0;

        if (stmt->child_count >= 1 &&
            !semantic_analyze_statement_flow(stmt->children[0],
                                             &body_guaranteed_return,
                                             &body_may_fallthrough,
                                             &body_may_break,
                                             &body_may_continue_loop,
                                             &body_may_non_terminating)) {
            return 0;
        }

        if (cond_always_true) {
            if (body_may_break) {
                int body_can_reiterate = body_may_fallthrough || body_may_continue_loop;
                int body_can_nonterminate_without_break =
                    body_can_reiterate || body_may_non_terminating;
                guaranteed_return = 0;
                may_fallthrough = body_can_nonterminate_without_break ? 0 : 1;
                may_non_terminating = body_can_nonterminate_without_break ? 1 : 0;
            } else {
                guaranteed_return = body_guaranteed_return;
                may_fallthrough = 0;
                may_non_terminating = body_guaranteed_return ? 0 : 1;
            }
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
        int cond_always_true = 1;
        int body_guaranteed_return = 0;
        int body_may_fallthrough = 1;
        int body_may_break = 0;
        int body_may_continue_loop = 0;
        int body_may_non_terminating = 0;

        if (stmt->has_condition_expression) {
            cond_always_true = expression_is_constant_true_for_flow(cond_expr);
        }

        if (stmt->child_count >= 1 &&
            !semantic_analyze_statement_flow(stmt->children[0],
                                             &body_guaranteed_return,
                                             &body_may_fallthrough,
                                             &body_may_break,
                                             &body_may_continue_loop,
                                             &body_may_non_terminating)) {
            return 0;
        }

        if (cond_always_true) {
            if (body_may_break) {
                int body_can_reiterate = body_may_fallthrough || body_may_continue_loop;
                int body_can_nonterminate_without_break =
                    body_can_reiterate || body_may_non_terminating;
                guaranteed_return = 0;
                may_fallthrough = body_can_nonterminate_without_break ? 0 : 1;
                may_non_terminating = body_can_nonterminate_without_break ? 1 : 0;
            } else {
                guaranteed_return = body_guaranteed_return;
                may_fallthrough = 0;
                may_non_terminating = body_guaranteed_return ? 0 : 1;
            }
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
                                         NULL)) {
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

#endif
