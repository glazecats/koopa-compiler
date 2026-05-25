#include "semantic.h"

#include <limits.h>
#include <stdint.h>
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
    const AstExternal *current_func;
    SemanticError *error;
} CallableRuleVisitCtx;

typedef struct {
    int skip_all_paths_return_check;
    int allow_extension_features;
} SemanticAnalyzeConfig;

typedef struct {
    int may_exit_loop;
    int exits_loop_on_all_paths;
} FlowLoopExitSummary;

typedef struct {
    const char *name;
    AstFunctionReturnType return_type;
    size_t parameter_count;
} SemanticBuiltinFunctionInfo;

typedef enum {
    SEMANTIC_TYPE_KIND_INVALID = 0,
    SEMANTIC_TYPE_KIND_INT,
    SEMANTIC_TYPE_KIND_FLOAT,
    SEMANTIC_TYPE_KIND_VOID,
    SEMANTIC_TYPE_KIND_FUNCTION,
    SEMANTIC_TYPE_KIND_AGGREGATE,
} SemanticTypeKind;

typedef enum {
    SEMANTIC_AGGREGATE_KIND_NONE = 0,
    SEMANTIC_AGGREGATE_KIND_PAIR,
    SEMANTIC_AGGREGATE_KIND_STRUCT,
} SemanticAggregateKind;

typedef struct SemanticTypeDescriptor SemanticTypeDescriptor;

struct SemanticTypeDescriptor {
    SemanticTypeKind kind;
    SemanticAggregateKind aggregate_kind;
    const char *named_type_name;
    size_t array_rank;
    AstExpression **array_extent_exprs;
    AstFunctionReturnType function_return_type;
    size_t parameter_count;
    const int *parameter_value_kinds;
    const AstFunctionReturnType *parameter_function_return_types;
    const size_t *parameter_function_parameter_counts;
};

typedef enum {
    SEMANTIC_FUNCTION_SIGNATURE_MATCH = 0,
    SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_RETURN_TYPE,
    SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_COUNT,
    SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_PARAMETER_KIND,
    SEMANTIC_FUNCTION_SIGNATURE_MISMATCH_NESTED_SIGNATURE,
} SemanticFunctionSignatureMatchKind;

static void format_callee_preview(const char *name, char *out, size_t out_size);
static void semantic_set_error(SemanticError *error,
    int line,
    int column,
    const char *message);
static long long semantic_normalize_sysy_int_value(long long value);
static const SemanticBuiltinFunctionInfo *semantic_find_builtin_function(const char *name);
static const AstExternal *semantic_find_visible_function_external(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const char *name);
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
static int semantic_statement_flow_has_must_return_boundary(const AstStatement *stmt,
    int *out_must_return);
static int semantic_analyze_loop_statement_flow(const AstExpression *cond_expr,
    int cond_present,
    const AstStatement *body_stmt,
    char **declaration_names,
    size_t declaration_name_count,
    const AstExpression *step_expr,
    int *out_guaranteed_return,
    int *out_may_fallthrough,
    int *out_may_break,
    int *out_may_continue_loop,
    int *out_may_non_terminating);
static int semantic_analyze_statement_flow(const AstStatement *stmt,
    int *out_guaranteed_return,
    int *out_may_fallthrough,
    int *out_may_break,
    int *out_may_continue_loop,
    int *out_may_non_terminating);

static int semantic_check_single_callable_rule(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const AstExternal *current_func,
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
static int semantic_function_parameter_name_is_function_valued(const AstExternal *func, const char *name);
static int semantic_check_function_callable_scope_shadow_rules(const AstExternal *func,
    SemanticError *error);
static int semantic_scope_check_top_level_initializer_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t external_index,
    SemanticError *error);
static int semantic_check_function_scope_rules(const AstProgram *program,
    size_t func_index,
    const AstExternal *func,
    const SemanticAnalyzeConfig *config,
    SemanticError *error);
static int semantic_build_function_return_type_descriptor(const AstExternal *external,
    SemanticTypeDescriptor *out_type);
static int semantic_build_top_level_declaration_type_descriptor(const AstExternal *external,
    SemanticTypeDescriptor *out_type);
static int semantic_build_function_parameter_type_descriptor(const AstExternal *external,
    size_t param_index,
    SemanticTypeDescriptor *out_type);
static int semantic_find_named_function_parameter_type_descriptor(const AstExternal *func,
    const char *name,
    size_t *out_index,
    SemanticTypeDescriptor *out_type);
static int semantic_type_descriptors_are_compatible(const SemanticTypeDescriptor *lhs,
    const SemanticTypeDescriptor *rhs);
static int semantic_top_level_declarations_have_compatible_types(const AstExternal *lhs,
    const AstExternal *rhs);
static SemanticFunctionSignatureMatchKind semantic_function_declaration_signature_match_kind(
    const AstExternal *lhs,
    const AstExternal *rhs);
static int semantic_check_function_return_shape_rules(const AstExternal *func,
    SemanticError *error);
static int semantic_compute_function_returns_all_paths(const AstExternal *func,
    int *out_returns_all_paths,
    SemanticError *error);
static SemanticAggregateKind semantic_aggregate_kind_from_declaration_value_kind(int declaration_value_kind) {
    switch (declaration_value_kind) {
    case AST_DECLARATION_VALUE_FLOAT:
        return SEMANTIC_AGGREGATE_KIND_NONE;
    case AST_DECLARATION_VALUE_PAIR:
        return SEMANTIC_AGGREGATE_KIND_PAIR;
    case AST_DECLARATION_VALUE_STRUCT:
        return SEMANTIC_AGGREGATE_KIND_STRUCT;
    default:
        return SEMANTIC_AGGREGATE_KIND_NONE;
    }
}

static long long semantic_normalize_sysy_int_value(long long value) {
    uint32_t bits = (uint32_t)value;
    return (long long)(int32_t)bits;
}

#define SEMANTIC_SPLIT_AGGREGATOR 1
#include "semantic_core_flow.inc"
#include "semantic_callable_rules.inc"
#include "semantic_scope_rules.inc"
#include "semantic_entry.inc"
#undef SEMANTIC_SPLIT_AGGREGATOR
