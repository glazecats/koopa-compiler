#include "semantic.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CALLEE_PREVIEW_HEAD 24
#define CALLEE_PREVIEW_TAIL 16
#define SEMANTIC_MAX_EXPRESSION_DEPTH 4096

typedef int (*ExpressionPostVisitor)(const AstExpression *expr, void *ctx);

typedef struct {
    const AstProgram *program;
    size_t func_index;
    int include_current_external;
    SemanticError *error;
} CallableRuleVisitCtx;

typedef struct {
    int may_exit_loop;
    int exits_loop_on_all_paths;
} FlowLoopExitSummary;

static void format_callee_preview(const char *name, char *out, size_t out_size);
static void semantic_set_error(SemanticError *error,
    int line,
    int column,
    const char *message);
static int names_equal(const AstExternal *a, const AstExternal *b);
static const AstExpression *unwrap_paren_expression(const AstExpression *expr);
static int classify_ast_call_callee(const AstExpression *callee, const char **out_name);
static int walk_expression_postorder_with_depth(const AstExpression *expr,
    ExpressionPostVisitor visitor,
    void *ctx,
    SemanticError *error,
    size_t depth);
static int walk_expression_postorder(const AstExpression *expr,
    ExpressionPostVisitor visitor,
    void *ctx,
    SemanticError *error);
static int walk_statement_postorder(const AstStatement *stmt,
    ExpressionPostVisitor visitor,
    void *ctx,
    SemanticError *error);
static const AstExpression *statement_get_condition_expression(const AstStatement *stmt);
static const AstExpression *statement_get_for_step_expression(const AstStatement *stmt);
static int expression_is_constant_true_for_flow(const AstExpression *expr);
static int expression_is_constant_false_for_flow(const AstExpression *expr);
static int semantic_analyze_statement_flow(const AstStatement *stmt,
    int *out_guaranteed_return,
    int *out_may_fallthrough,
    int *out_may_break,
    int *out_may_continue_loop,
    int *out_may_non_terminating);

static int semantic_check_single_callable_rule(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const char *called_name,
    int call_line,
    int call_column,
    size_t called_arg_count,
    int callee_kind,
    SemanticError *error);
static int semantic_check_call_expr_postorder(const AstExpression *expr, void *ctx);
static int semantic_check_function_callable_rules(const AstProgram *program,
    size_t func_index,
    const AstExternal *func,
    SemanticError *error);
static int semantic_check_top_level_initializer_callable_rules(const AstProgram *program,
    size_t external_index,
    const AstExpression *expr,
    SemanticError *error);
static int semantic_check_function_callable_scope_shadow_rules(const AstExternal *func,
    SemanticError *error);
static int semantic_scope_check_top_level_initializer_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t external_index,
    SemanticError *error);
static int semantic_check_function_scope_rules(const AstProgram *program,
    size_t func_index,
    const AstExternal *func,
    SemanticError *error);
static int semantic_compute_function_returns_all_paths(const AstExternal *func,
    int *out_returns_all_paths,
    SemanticError *error);

#define SEMANTIC_SPLIT_AGGREGATOR 1
#include "semantic_core_flow.inc"
#include "semantic_callable_rules.inc"
#include "semantic_scope_rules.inc"
#include "semantic_entry.inc"
#undef SEMANTIC_SPLIT_AGGREGATOR
