#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_lower_scope_clear_mutated_scalar_locals_in_expr(IrLowerScopeStack *stack,
    const AstExpression *expr);
static int ir_expression_is_direct_float_condition(const IrLowerContext *ctx,
    const AstExpression *expr);
static IrBinaryOp ir_binary_op_from_assignment_token(TokenType token_type,
    int *out_supported);
static int ir_expression_is_signed_float_literal(const AstExpression *expr);
static int ir_expression_is_direct_float_transport_root(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_lower_signed_float_literal_expression(const AstExpression *expr,
    IrValueRef *out_value);
static int ir_expression_is_float_arithmetic(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_lower_float_arithmetic_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
static int ir_expression_is_float_direct_compare(const IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_lower_float_compare_normalize_zero(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_normalized);
static int ir_lower_float_compare_order_key(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_key);
static int ir_lower_float_compare_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
static int ir_lower_refresh_current_function(IrLowerContext *ctx,
    const char *current_function_name,
    int line,
    int column);
static int ir_function_lowered_return_slot_count(const AstProgram *ast_program,
    IrProgram *program,
    const AstExternal *external,
    size_t *out_slot_count);
static int ir_aggregate_slot_count_for_kind(const AstProgram *program,
    AstDeclarationValueKind value_kind,
    const char *type_name,
    size_t *out_slot_count);
static int ir_struct_field_slot_offset(const AstProgram *program,
    const AstStructType *struct_type,
    size_t field_index,
    size_t *out_offset);
static int ir_resolve_local_aggregate_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerBindingInfo *out_info);
static int ir_lower_initialize_aggregate_slots(IrLowerContext *ctx,
    IrLowerBindingKind kind,
    const char *type_name,
    size_t dest_first_local_id,
    size_t dest_slot_count,
    const AstExpression *initializer_expr,
    int line,
    int column);
static int ir_resolve_aggregate_lvalue_ref(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_lvalue);
static int ir_call_argument_resolve_extension_function_value_target(const AstProgram *program,
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t arg_index,
    const AstExpression *arg_expr,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name);
static int ir_try_get_recursive_returned_function_value_parameter_argument_expr(
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal **out_return_function_external,
    size_t *out_return_param_index,
    const AstExpression **out_arg_expr);
static int ir_lower_append_call_argument(IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t param_index,
    const AstExpression *arg_expr,
    IrValueRef **io_args,
    size_t *io_arg_count);
static int ir_lower_call_into_aggregate_slots(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrLowerBindingKind expected_kind,
    const char *expected_type_name,
    size_t dest_first_local_id,
    size_t dest_slot_count,
    int dest_slots_are_addresses);
static int ir_emit_copy_local_slot_range(IrLowerContext *ctx,
    size_t dest_first_local_id,
    size_t source_first_local_id,
    size_t slot_count);
static int ir_emit_store_slot_range_to_hidden_return_addresses(IrLowerContext *ctx,
    size_t source_first_local_id,
    size_t slot_count);
static int ir_materialize_returned_function_value_payload_locals(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    const char *local_name_prefix,
    size_t payload_slot_count,
    size_t **out_payload_local_ids);
static int ir_resolve_function_value_descriptor(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerFunctionObjectDescriptor *out_descriptor);
static int ir_resolve_identifier_function_value_descriptor(IrLowerContext *ctx,
    const char *identifier_name,
    IrLowerFunctionObjectDescriptor *out_descriptor);
static int ir_resolve_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view);
static int ir_resolve_any_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view);
static int ir_callee_is_supported_direct_identifier_shape(IrLowerContext *ctx,
    const AstExpression *expr);
static int ir_try_resolve_direct_callee_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view);
static int ir_try_resolve_static_actual_argument_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view);
static int ir_resolve_returned_function_value_call_producer_info(
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_base_call_expr,
    const AstExternal **out_producer_external,
    int *out_return_family_is_closure);
static int ir_try_resolve_returned_binding_callee_info(IrLowerContext *ctx,
    const AstExpression *callee_base_expr,
    IrLowerBindingInfo *out_binding_info);
static int ir_try_emit_dynamic_closure_callable_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    const char *oom_error_message,
    const char *missing_capture_error_message,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_dynamic_noncapturing_callable_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_direct_dynamic_function_value_specialized_call(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external_a,
    const AstExternal *callee_external_b,
    IrLowerCallableObjectView *callee_view,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_direct_target_static_callee_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *resolved_callee_view,
    const AstExpression *callee_base_expr,
    const char *callee_name,
    int callee_is_closure_local,
    int callee_from_static_returned_parameter_closure,
    const size_t *closure_capture_local_ids,
    size_t closure_prefix_arg_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_passthrough_callable_dispatch_from_function_args(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    IrValueRef *out_value,
    int *out_applied);
static int ir_emit_callable_object_indirect_call_into(IrLowerContext *ctx,
    IrValueRef result_value,
    const char *callee_name,
    IrValueRef env_value,
    size_t shape_id,
    const IrValueRef *args,
    size_t arg_count);
static int ir_emit_callable_object_indirect_call(IrLowerContext *ctx,
    const char *callee_name,
    IrValueRef env_value,
    size_t shape_id,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value);
static int ir_build_callable_shape_from_function_signature(IrLowerContext *ctx,
    const IrFunction *target_function,
    size_t hidden_prefix_param_count,
    size_t *out_shape_id);
static int ir_emit_two_target_callable_object_indirect_call_bridged(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    IrValueRef then_env_value,
    IrValueRef else_env_value,
    size_t shape_id,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value);
static int ir_build_two_target_callable_shape_bridge(IrLowerContext *ctx,
    const IrFunction *then_target_function,
    const IrFunction *else_target_function,
    size_t hidden_prefix_param_count,
    const char *shape_error_message,
    size_t *out_shape_id);
static int ir_build_two_target_callable_shape_bridge_from_names(IrLowerContext *ctx,
    const char *then_target_name,
    const char *else_target_name,
    size_t hidden_prefix_param_count,
    const char *shape_error_message,
    size_t *out_shape_id);
static int ir_build_single_target_callable_shape_bridge_from_name(IrLowerContext *ctx,
    const char *target_name,
    size_t hidden_prefix_param_count,
    size_t expected_callable_arg_count,
    size_t *out_shape_id,
    const IrFunction **out_target_function);
static int ir_build_single_visible_callable_arg(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_arg);
static int ir_build_required_single_visible_callable_arg(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *error_message,
    IrValueRef *out_arg);
static int ir_build_required_single_visible_callable_args(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *error_message,
    IrValueRef *out_args,
    size_t *out_arg_count);
static int ir_build_required_single_visible_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *error_message,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static int ir_build_dynamic_visible_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *oom_error_message,
    IrValueRef **out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static int ir_build_optional_single_visible_callable_args(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_args,
    size_t *out_arg_count);
static int ir_build_optional_single_indexed_callable_arg(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t arg_index,
    IrValueRef *out_arg);
static int ir_build_optional_single_indexed_callable_args(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t visible_arg_count,
    size_t arg_index,
    IrValueRef *out_args,
    size_t *out_arg_count);
static int ir_build_required_single_indexed_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t arg_index,
    const char *error_message,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static int ir_build_dynamic_indexed_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t visible_arg_count,
    size_t arg_index,
    const char *oom_error_message,
    IrValueRef **out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static int ir_build_dynamic_non_function_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t hidden_prefix_param_count,
    size_t function_arg_index,
    const char *oom_error_message,
    IrValueRef **out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static int ir_build_optional_single_indexed_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t visible_arg_count,
    size_t arg_index,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static int ir_build_optional_single_visible_callable_args_from_value(IrValueRef value,
    size_t visible_arg_count,
    IrValueRef *out_args,
    size_t *out_arg_count);
static int ir_build_optional_single_visible_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count);
static const IrValueRef *ir_optional_single_callable_args_view(const IrValueRef *args,
    size_t arg_count);
static int ir_mark_callable_dispatch_applied(int *out_applied);
static int ir_finish_callable_bridge_result_value(IrValueRef result_value,
    IrValueRef *out_value);
static int ir_finish_returned_callable_capture_cleanup(size_t *owned_capture_local_ids);
static int ir_clear_returned_callable_capture_slots(size_t **owned_capture_local_ids);
static int ir_finish_returned_callable_capture_cleanup_applied(
    size_t **owned_capture_local_ids,
    int *out_applied);
static int ir_finish_callable_dispatch_result(IrValueRef result_value,
    IrValueRef *out_value,
    int *out_applied);
static int ir_finish_optional_callable_dispatch_result(int applied,
    IrValueRef result_value,
    IrValueRef *out_value,
    int *out_applied);
static int ir_finish_optional_callable_dispatch_applied(int applied,
    int *out_applied);
static int ir_finish_void_or_value_callable_dispatch_result(int is_void_result,
    IrValueRef result_value,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_two_target_callable_object_indirect_dispatch_from_call_expr(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_dynamic_callable_object_dispatch_from_call_expr(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    const char *oom_error_message,
    const char *missing_capture_error_message,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_static_callable_object_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_static_noncapturing_target_callable_indirect_call_with_args(
    IrLowerContext *ctx,
    const char *target_name,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_direct_callee_callable_object_dynamic_dispatch(IrLowerContext *ctx,
    IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    int *io_callee_is_closure_local,
    const size_t **io_closure_capture_local_ids,
    size_t *io_closure_prefix_arg_count,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_lower_plain_direct_call(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_ternary_function_value_argument_dispatch(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    size_t non_function_arg_index,
    size_t non_function_arg_count,
    IrValueRef *out_value,
    int *out_applied);
static int ir_materialize_ternary_function_value_argument_callable_object_view(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const AstExpression *function_arg_base,
    IrLowerCallableObjectView *out_view,
    const char *const **out_owned_dynamic_target_names,
    long long *out_then_tag_value);
static int ir_try_resolve_dynamic_function_value_specialization_callable_object_view(
    IrLowerContext *ctx,
    const AstExpression *function_arg_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    IrLowerCallableObjectView *out_view,
    const char *const **out_owned_dynamic_target_names,
    int *out_requires_materialized_capture_forwarding,
    long long *out_then_tag_value);
static int ir_try_emit_dynamic_function_value_specialized_call_from_callable_object_view(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const IrLowerCallableObjectView *dynamic_callable_view,
    const char *const *owned_dynamic_target_names,
    int requires_materialized_capture_forwarding,
    long long dynamic_then_tag_value,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_function_parameter_passthrough_callable_dispatch(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const IrLowerCallableObjectView *callable_view,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied);
static int ir_can_use_direct_function_parameter_passthrough_dispatch(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t function_arg_index);
static int ir_find_external_parameter_index_by_name(const AstExternal *external,
    const char *name,
    int require_function_kind,
    size_t *out_index);
typedef struct {
    const char *local_name;
    const char *source_parameter_name;
} IrFunctionParameterPassthroughAlias;
typedef struct {
    const char *local_name;
    const char *source_parameter_name;
} IrScalarParameterPassthroughAlias;

typedef struct {
    const AstExternal *target_external;
    AstExpression **mapped_args;
    size_t function_arg_index;
    IrLowerCallableObjectView function_callable_view;
} IrFunctionParameterRecursiveTargetPlan;

typedef struct {
    const char *name;
    int has_scalar_value;
    IrValueRef scalar_value;
    int has_callable_view;
    IrLowerCallableObjectView callable_view;
} IrFunctionBodyEvalBinding;
static int ir_try_resolve_function_parameter_passthrough_alias_name(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const AstExpression *expr,
    const char **out_parameter_name);
static int ir_statement_is_zero_return(const AstStatement *stmt);
static int ir_try_collect_function_parameter_passthrough_aliases(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    const AstStatement *body,
    IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_capacity,
    IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_capacity,
    size_t *out_alias_count,
    size_t *out_scalar_alias_count,
    const AstStatement **out_passthrough_stmt,
    int *out_returns_zero_after_void_call);
static int ir_try_lower_passthrough_bound_value_expr(IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_count,
    const AstExpression *call_expr,
    const AstExpression *expr,
    IrValueRef *out_value);
static size_t ir_find_function_parameter_passthrough_alias_index(
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const char *name);
static size_t ir_find_scalar_parameter_passthrough_alias_index(
    const IrScalarParameterPassthroughAlias *aliases,
    size_t alias_count,
    const char *name);
static int ir_try_resolve_scalar_parameter_passthrough_alias_name(
    const AstExternal *callee_external,
    const IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_count,
    const AstExpression *expr,
    const char **out_parameter_name);
static int ir_passthrough_stmt_uses_only_function_arguments(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const AstStatement *passthrough_stmt);
static int ir_try_emit_simple_function_body_eval_dispatch(IrLowerContext *ctx,
    const AstExternal *callee_external,
    int is_closure_family,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_two_target_dynamic_closure_body_eval_dispatch(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external_a,
    const AstExternal *callee_external_b,
    const IrLowerCallableObjectView *dynamic_callable_view,
    long long dynamic_then_tag_value,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_static_callable_view_dispatch_with_args(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callable_view,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_static_direct_target_callable_dispatch(IrLowerContext *ctx,
    const char *direct_target_name,
    int is_closure_family,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_emit_static_direct_target_specialized_call(IrLowerContext *ctx,
    const char *direct_target_name,
    int is_closure_family,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied);
static int ir_try_build_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    IrLowerFunctionValueReturnResolution *out_resolution);
static int ir_try_build_identifier_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    int *io_is_closure_return,
    size_t *out_payload_slot_count,
    IrLowerFunctionValueReturnResolution *out_resolution);
static int ir_try_build_direct_call_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    int *io_is_closure_return,
    size_t *out_payload_slot_count,
    IrLowerFunctionValueReturnResolution *out_resolution);
static int ir_try_build_ternary_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    int *io_is_closure_return,
    size_t *out_payload_slot_count,
    IrLowerFunctionValueReturnResolution *out_resolution);
static int ir_emit_function_value_return_payload_from_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    const IrLowerFunctionValueReturnResolution *return_resolution,
    int use_snapshot,
    size_t snapshot_first_local_id,
    size_t capture_count,
    int *out_payload_already_lowered);
static int ir_try_resolve_returned_function_value_family_payload_prefix(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    const AstExpression **out_base_call_expr,
    int *out_return_family_is_closure,
    const char **out_payload_local_name_prefix);
static int ir_try_resolve_returned_function_object_descriptor_with_family_prefixes(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t required_dynamic_target_count,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor,
    int *out_return_family_is_closure);
static int ir_init_function_object_descriptor_from_returned_lowered_payload(
    const AstExternal *producer_external,
    int is_closure_family,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count,
    size_t *payload_local_ids,
    size_t payload_slot_count,
    IrLowerFunctionObjectDescriptor *out_descriptor);
static int ir_resolve_returned_function_object_descriptor_with_prefix(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t required_dynamic_target_count,
    const char *payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor);
static int ir_materialize_returned_function_object_descriptor(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    int is_closure_family,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count,
    const char *payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor);
static int ir_init_function_object_descriptor_from_returned_call_materialization(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    int is_closure_family,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count,
    const char *payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor);
static int ir_ensure_noncapturing_callable_wrapper_function(IrLowerContext *ctx,
    const char *target_name,
    const char **out_wrapper_name);
static int ir_ensure_closure_callable_wrapper_function(IrLowerContext *ctx,
    const char *target_name,
    size_t capture_count,
    const char **out_wrapper_name);
static int ir_try_get_returned_function_value_parameter_argument_expr(const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal **out_return_function_external,
    size_t *out_return_param_index,
    const AstExpression **out_arg_expr);
static int ir_try_resolve_exact_returned_function_value_parameter_passthrough_base_expr(
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_arg_base);
static int ir_try_resolve_returned_function_value_parameter_argument_target(const AstProgram *program,
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name);
static int ir_try_resolve_returned_function_value_parameter_argument_info(
    const AstProgram *program,
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_arg_expr,
    const AstExpression **out_arg_base,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name,
    int *out_is_static_single_capture_closure_local);
static int ir_collect_returned_function_value_target_names_with_parameter_fallback(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *return_function_external,
    const char ***out_target_names,
    size_t *out_target_count);
static int ir_resolve_returned_closure_target_info(
    const AstProgram *program,
    IrLowerContext *ctx,
    const AstExternal *return_function_external,
    const char **out_helper_name,
    const char ***out_target_names,
    size_t *out_target_count,
    const char **out_primary_target_name);
static int ir_resolve_returned_function_value_target_info(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *return_function_external,
    int return_family_is_closure,
    const char ***out_target_names,
    size_t *out_target_count,
    const char **out_primary_target_name);
static int ir_try_preserve_returned_function_value_parameter_closure_captures(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *binding_name,
    size_t *io_closure_capture_count,
    size_t **out_closure_capture_local_ids,
    int *out_preserved);
static int ir_try_materialize_two_target_returned_callable_object_view_with_family_prefixes(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    IrLowerCallableObjectView *out_view);

static const AstExpression *ir_unwrap_function_value_wrapper_expression(const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!base) {
        return NULL;
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_COMMA) {
        return ir_unwrap_function_value_wrapper_expression(base->as.binary.right);
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_ASSIGN) {
        return ir_unwrap_function_value_wrapper_expression(base->as.binary.left);
    }

    return base;
}

static int ir_lower_emit_function_value_wrapper_side_effects(IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrValueRef ignored_value;

    if (!ctx || !base) {
        return 1;
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_COMMA) {
        if (!ir_lower_expression(ctx, base->as.binary.left, &ignored_value)) {
            return 0;
        }
        return ir_lower_emit_function_value_wrapper_side_effects(ctx, base->as.binary.right);
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_ASSIGN) {
        AstStatement synthetic_stmt;
        int handled = 0;

        memset(&synthetic_stmt, 0, sizeof(synthetic_stmt));
        synthetic_stmt.kind = AST_STMT_EXPRESSION;
        synthetic_stmt.line = base->line;
        synthetic_stmt.column = base->column;
        synthetic_stmt.expressions = (AstExpression **)&base;
        synthetic_stmt.expression_count = 1u;
        if (!ir_try_lower_function_value_assignment_statement(ctx, &synthetic_stmt, &handled)) {
            return 0;
        }
        if (handled) {
            return 1;
        }
        return ir_lower_expression(ctx, base, &ignored_value);
    }

    return 1;
}

static int ir_function_value_wrapper_expression_has_side_effects(const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!base) {
        return 0;
    }

    return base->kind == AST_EXPR_BINARY &&
        (base->as.binary.op == TOKEN_COMMA || base->as.binary.op == TOKEN_ASSIGN);
}

static int ir_resolve_function_value_wrapper_direct_identifier(const IrLowerContext *ctx,
    const AstExpression *expr,
    const char **out_name) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    (void)ctx;

    if (out_name) {
        *out_name = NULL;
    }
    if (!base) {
        return 0;
    }

    if (base->kind == AST_EXPR_IDENTIFIER) {
        if (out_name) {
            *out_name = base->as.identifier.name;
        }
        return base->as.identifier.name && base->as.identifier.name[0] != '\0';
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_COMMA) {
        return ir_resolve_function_value_wrapper_direct_identifier(ctx, base->as.binary.right, out_name);
    }

    if (base->kind == AST_EXPR_BINARY && base->as.binary.op == TOKEN_ASSIGN) {
        return ir_resolve_function_value_wrapper_direct_identifier(ctx, base->as.binary.left, out_name);
    }

    return 0;
}

static int ir_resolve_function_value_initializer_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    const char **out_target_name,
    size_t **out_capture_local_ids,
    size_t *out_capture_count,
    size_t *out_tag_local_id,
    int *out_has_const_value,
    long long *out_const_value,
    int *out_is_closure_local) {
    IrLowerCallableObjectView view;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (out_tag_local_id) {
        *out_tag_local_id = (size_t)-1;
    }
    if (out_has_const_value) {
        *out_has_const_value = 0;
    }
    if (out_const_value) {
        *out_const_value = 0;
    }
    if (out_is_closure_local) {
        *out_is_closure_local = 0;
    }
    if (!ctx || !ir_resolve_callable_object_view(ctx, expr, &view) || !view.direct_target_name) {
        return 0;
    }

    if (out_target_name) {
        *out_target_name = view.direct_target_name;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = view.is_closure_family
            ? (size_t *)view.capture_local_ids
            : NULL;
    }
    if (out_capture_count) {
        *out_capture_count = view.is_closure_family
            ? view.capture_count
            : 0u;
    }
    if (out_tag_local_id) {
        *out_tag_local_id = view.dynamic_tag_local_id;
    }
    if (out_has_const_value) {
        *out_has_const_value = view.has_const_tag_value;
    }
    if (out_const_value) {
        *out_const_value = view.const_tag_value;
    }
    if (out_is_closure_local) {
        *out_is_closure_local = view.is_closure_family;
    }
    return 1;
}

static int ir_project_static_callable_object_view_target_binding(
    const IrLowerCallableObjectView *view,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name) {
    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (!view ||
        !view->direct_target_name ||
        view->direct_target_name[0] == '\0' ||
        (view->has_dynamic_family && view->dynamic_tag_local_id != (size_t)-1)) {
        return 0;
    }

    if (out_target_name) {
        *out_target_name = view->direct_target_name;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = view->is_closure_family
            ? view->capture_local_ids
            : NULL;
    }
    if (out_capture_count) {
        *out_capture_count = view->is_closure_family
            ? view->capture_count
            : 0u;
    }
    return 1;
}

static int ir_resolve_identifier_function_value_descriptor(IrLowerContext *ctx,
    const char *identifier_name,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const char *parameter_target = NULL;
    IrLowerBindingInfo binding_info;

    ir_init_function_object_descriptor(out_descriptor);
    if (!ctx || !identifier_name || identifier_name[0] == '\0' || !out_descriptor) {
        return 0;
    }

    memset(&binding_info, 0, sizeof(binding_info));
    if (ir_lower_scope_lookup_binding(&ctx->scopes, identifier_name, &binding_info) &&
        binding_info.type_name && binding_info.type_name[0] != '\0' &&
        (binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
            binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL)) {
        return ir_init_function_object_descriptor_from_binding_info(
            &binding_info,
            ir_lower_find_function_value_target_set(ctx, identifier_name),
            out_descriptor);
    }

    parameter_target = ir_lower_lookup_function_value_target(ctx, identifier_name);
    if (parameter_target && parameter_target[0] != '\0') {
        const size_t *capture_local_ids = NULL;
        size_t capture_count = 0u;

        out_descriptor->direct_target_name = parameter_target;
        out_descriptor->is_closure_family =
            ir_lower_lookup_function_value_capture_binding(
                ctx,
                identifier_name,
                &capture_local_ids,
                &capture_count) &&
            capture_count > 0u;
        out_descriptor->capture_local_ids = capture_local_ids;
        out_descriptor->capture_count = capture_count;
        out_descriptor->payload_slot_count = capture_count;
        return 1;
    }

    if (ctx->program && ir_program_find_function_mut(ctx->program, identifier_name)) {
        return ir_init_function_object_descriptor_from_static_target(
            identifier_name,
            0,
            0,
            0,
            0,
            out_descriptor);
    }

    if (ctx->program) {
        IrFunction *builtin_function = NULL;
        if (ir_ensure_builtin_function_signature(
                ctx->program,
                identifier_name,
                &builtin_function,
                ctx->error)) {
            return ir_init_function_object_descriptor_from_static_target(
                identifier_name,
                0,
                0,
                0,
                0,
                out_descriptor);
        }
    }

    return 0;
}

static int ir_resolve_function_value_descriptor(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);

    ir_init_function_object_descriptor(out_descriptor);
    if (!ctx || !base || !out_descriptor ||
        base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }
    return ir_resolve_identifier_function_value_descriptor(
        ctx,
        base->as.identifier.name,
        out_descriptor);
}

static int ir_try_materialize_identifier_function_value_capture_slot(
    IrLowerContext *ctx,
    const char *capture_name,
    size_t dest_local_id,
    int *out_applied) {
    IrLowerFunctionObjectDescriptor descriptor;
    long long tag_value = 0;
    IrValueRef source_value;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !capture_name || capture_name[0] == '\0' ||
        dest_local_id == (size_t)-1) {
        return 0;
    }

    ir_init_function_object_descriptor(&descriptor);
    if (!ir_resolve_identifier_function_value_descriptor(
            ctx,
            capture_name,
            &descriptor) ||
        !descriptor.direct_target_name ||
        descriptor.direct_target_name[0] == '\0') {
        return 1;
    }

    if (descriptor.capture_count > 0u || descriptor.has_dynamic_family) {
        return 1;
    }

    if (descriptor.dynamic_tag_local_id != (size_t)-1) {
        source_value = ir_value_local(descriptor.dynamic_tag_local_id);
    } else if (descriptor.has_const_tag_value) {
        source_value = ir_value_immediate(descriptor.const_tag_value);
    } else if (ir_lower_register_function_value_target(
            ctx,
            capture_name,
            descriptor.direct_target_name,
            &tag_value)) {
        source_value = ir_value_immediate(tag_value);
    } else {
        return 0;
    }

    if (!ir_emit_mov(
            ctx,
            ir_value_local(dest_local_id),
            source_value)) {
        return 0;
    }
    if (out_applied) {
        *out_applied = 1;
    }
    return 1;
}

static int ir_resolve_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view) {
    IrLowerFunctionObjectDescriptor descriptor;

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (!ctx || !out_view ||
        !ir_resolve_function_value_descriptor(ctx, expr, &descriptor)) {
        return 0;
    }
    return ir_init_callable_object_view_from_function_object_descriptor(
        &descriptor,
        out_view);
}

static int ir_resolve_any_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (!ctx || !expr || !out_view) {
        return 0;
    }
    if (ir_resolve_callable_object_view(ctx, expr, out_view)) {
        return 1;
    }

    if (!base || base->kind != AST_EXPR_CALL || !ctx->ast_program) {
        return 0;
    }
    return ir_try_materialize_two_target_returned_callable_object_view_with_family_prefixes(
        ctx,
        base,
        "__retfn_anyview",
        "__retclosure_anyview",
        out_view);
}

static int ir_callee_is_supported_direct_identifier_shape(IrLowerContext *ctx,
    const AstExpression *expr) {
    const char *direct_identifier_name = NULL;

    if (ir_extract_direct_call_callee_name(expr)) {
        return 1;
    }
    if (!ctx) {
        return 0;
    }
    return ir_resolve_function_value_wrapper_direct_identifier(ctx, expr, &direct_identifier_name) &&
        direct_identifier_name && direct_identifier_name[0] != '\0';
}

static int ir_try_resolve_direct_callee_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view) {
    const char *classified_callee_name = NULL;

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (!ctx || !expr || !out_view) {
        return 0;
    }
    if (!ir_classify_direct_identifier_expr(expr, &classified_callee_name) ||
        !classified_callee_name || classified_callee_name[0] == '\0') {
        return 0;
    }
    return ir_resolve_callable_object_view(ctx, expr, out_view) &&
        out_view->direct_target_name &&
        out_view->direct_target_name[0] != '\0';
}

static int ir_try_resolve_static_actual_argument_callable_object_view(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerCallableObjectView *out_view) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);
    IrLowerFunctionObjectDescriptor descriptor;
    IrLowerBindingInfo binding_info;
    IrLowerCallableObjectView resolved_view;

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (!ctx || !expr || !out_view || !base) {
        return 0;
    }
    memset(&resolved_view, 0, sizeof(resolved_view));
    resolved_view.dynamic_tag_local_id = (size_t)-1;
    if (ir_resolve_callable_object_view(ctx, expr, &resolved_view) &&
        resolved_view.direct_target_name &&
        resolved_view.direct_target_name[0] != '\0') {
        if (resolved_view.has_dynamic_family &&
            resolved_view.dynamic_tag_local_id != (size_t)-1 &&
            resolved_view.dynamic_target_count > 1u &&
            resolved_view.dynamic_target_names) {
            *out_view = resolved_view;
            return 1;
        }
        if (resolved_view.dynamic_target_count > 1u &&
            resolved_view.has_const_tag_value) {
            const char *const_target_name = resolved_view.direct_target_name;

            if (resolved_view.const_tag_value >= 1 &&
                (size_t)resolved_view.const_tag_value <= resolved_view.dynamic_target_count &&
                resolved_view.dynamic_target_names &&
                resolved_view.dynamic_target_names[(size_t)resolved_view.const_tag_value - 1u] &&
                resolved_view.dynamic_target_names[(size_t)resolved_view.const_tag_value - 1u][0] != '\0') {
                const_target_name =
                    resolved_view.dynamic_target_names[(size_t)resolved_view.const_tag_value - 1u];
            }
            if (ir_init_function_object_descriptor_from_static_target_with_payload(
                    const_target_name,
                    resolved_view.is_closure_family,
                    resolved_view.has_const_tag_value,
                    resolved_view.const_tag_value,
                    0,
                    resolved_view.capture_local_ids,
                    resolved_view.capture_count,
                    resolved_view.dynamic_tag_local_id,
                    &descriptor) &&
                ir_init_callable_object_view_from_function_object_descriptor(
                    &descriptor,
                    out_view)) {
                out_view->owned_payload_local_ids = resolved_view.owned_payload_local_ids;
                return 1;
            }

            resolved_view.dynamic_target_names = NULL;
            resolved_view.dynamic_target_count = 0u;
        }

        *out_view = resolved_view;
        return 1;
    }

    if (base->kind == AST_EXPR_CLOSURE) {
        char *closure_helper_name = NULL;
        IrFunction *helper_function = NULL;
        size_t closure_capture_count = base->as.closure.capture_name_count;
        size_t closure_capture_index;
        size_t *closure_capture_local_ids = NULL;

        if (closure_capture_count > 0u) {
            closure_capture_local_ids =
                (size_t *)malloc(closure_capture_count * sizeof(size_t));
            if (!closure_capture_local_ids) {
                return 0;
            }
        }
        for (closure_capture_index = 0u; closure_capture_index < closure_capture_count; ++closure_capture_index) {
            const char *capture_name =
                base->as.closure.capture_names
                    ? base->as.closure.capture_names[closure_capture_index]
                    : NULL;
            AstExpression capture_expr;
            IrValueRef capture_value;
            char capture_local_name[256];
            int applied_function_capture = 0;

            if (!capture_name || capture_name[0] == '\0') {
                free(closure_capture_local_ids);
                return 0;
            }
            memset(&capture_expr, 0, sizeof(capture_expr));
            capture_expr.kind = AST_EXPR_IDENTIFIER;
            capture_expr.line = base->line;
            capture_expr.column = base->column;
            capture_expr.as.identifier.name = (char *)capture_name;
            capture_expr.as.identifier.name_length = strlen(capture_name);
            if (!ir_lower_expression(ctx, &capture_expr, &capture_value)) {
                free(closure_capture_local_ids);
                return 0;
            }
            snprintf(capture_local_name,
                sizeof(capture_local_name),
                "__argclosure_cap_%zu",
                closure_capture_index);
            if (!ir_function_append_local(
                    ctx->function,
                    capture_local_name,
                    0,
                    &closure_capture_local_ids[closure_capture_index],
                    ctx->error)) {
                free(closure_capture_local_ids);
                return 0;
            }
            if (!ir_try_materialize_identifier_function_value_capture_slot(
                    ctx,
                    capture_name,
                    closure_capture_local_ids[closure_capture_index],
                    &applied_function_capture)) {
                free(closure_capture_local_ids);
                return 0;
            }
            if (!applied_function_capture &&
                !ir_emit_mov(ctx,
                    ir_value_local(closure_capture_local_ids[closure_capture_index]),
                    capture_value)) {
                free(closure_capture_local_ids);
                return 0;
            }
        }
        if (!ir_build_closure_helper_name(
                ctx->function && ctx->function->name ? ctx->function->name : "fn",
                "__argclosure",
                ((size_t)(base->line > 0 ? base->line : 0) << 16) |
                    (size_t)(base->column > 0 ? base->column : 0),
                &closure_helper_name,
                ctx->error) ||
            !ir_lower_closure_helper_function_from_expr(
                ctx,
                base,
                closure_helper_name,
                (const char *const *)base->as.closure.capture_names,
                closure_capture_local_ids,
                closure_capture_count,
                ctx->error) ||
            !(helper_function =
                (ctx->program && closure_helper_name)
                    ? ir_program_find_function_mut(ctx->program, closure_helper_name)
                    : NULL) ||
            !ir_init_function_object_descriptor_from_static_target_with_payload(
                helper_function->name,
                1,
                1,
                1,
                0,
                closure_capture_local_ids,
                closure_capture_count,
                (size_t)-1,
                &descriptor)) {
            free(closure_capture_local_ids);
            free(closure_helper_name);
            return 0;
        }
        descriptor.owned_payload_local_ids = closure_capture_local_ids;
        free(closure_helper_name);
        return ir_init_callable_object_view_from_function_object_descriptor(
            &descriptor,
            out_view);
    }

    memset(&binding_info, 0, sizeof(binding_info));
    if (base->kind == AST_EXPR_IDENTIFIER &&
        base->as.identifier.name &&
        ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info) &&
        binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL &&
        binding_info.type_name &&
        binding_info.type_name[0] != '\0' &&
        binding_info.function_target_tag_local_id != (size_t)-1 &&
        binding_info.has_const_value) {
        IrFunctionValueTargetSet *target_set =
            ir_lower_find_function_value_target_set(ctx, base->as.identifier.name);
        const char *const_target_name = NULL;

        if (target_set &&
            binding_info.const_value >= 1 &&
            (size_t)binding_info.const_value <= target_set->target_count &&
            target_set->target_names &&
            target_set->target_names[(size_t)binding_info.const_value - 1u] &&
            target_set->target_names[(size_t)binding_info.const_value - 1u][0] != '\0') {
            const_target_name = target_set->target_names[(size_t)binding_info.const_value - 1u];
        }
        if (!const_target_name) {
            const_target_name = binding_info.type_name;
        }
        if (!ir_init_function_object_descriptor_from_static_target_with_payload(
                const_target_name,
                1,
                1,
                binding_info.const_value,
                binding_info.from_returned_function_value_call,
                binding_info.closure_capture_local_ids,
                binding_info.closure_capture_count,
                binding_info.function_target_tag_local_id,
                &descriptor)) {
            return 0;
        }
        return ir_init_callable_object_view_from_function_object_descriptor(
            &descriptor,
            out_view);
    }

    if (base->kind == AST_EXPR_CALL &&
        ctx->ast_program) {
        const AstExpression *returned_base_call_expr = NULL;
        const AstExternal *returned_producer_external = NULL;
        int return_family_is_closure = 0;
        const char **returned_target_names = NULL;
        size_t returned_target_count = 0u;
        const char *returned_primary_target_name = NULL;
        int can_materialize_static_returned_view = 0;

        if (ir_resolve_returned_function_value_call_producer_info(
                ctx,
                base,
                &returned_base_call_expr,
                &returned_producer_external,
                &return_family_is_closure) &&
            returned_base_call_expr &&
            returned_producer_external &&
            ir_resolve_returned_function_value_target_info(
                ctx,
                returned_base_call_expr,
                returned_producer_external,
                return_family_is_closure,
                &returned_target_names,
                &returned_target_count,
                &returned_primary_target_name) &&
            ((!return_family_is_closure && returned_target_count == 1u) ||
                (return_family_is_closure && returned_target_count <= 1u)) &&
            returned_primary_target_name &&
            returned_primary_target_name[0] != '\0') {
            can_materialize_static_returned_view = 1;
        }
        free((void *)returned_target_names);
        if (can_materialize_static_returned_view &&
            ir_try_resolve_returned_function_object_descriptor_with_family_prefixes(
                ctx,
                base,
                0u,
                "__retfn_argslot",
                "__retclosure_argslot",
                &descriptor,
                NULL) &&
            descriptor.direct_target_name &&
            descriptor.direct_target_name[0] != '\0' &&
            !descriptor.has_dynamic_family) {
            return ir_init_callable_object_view_from_function_object_descriptor(
                &descriptor,
                out_view);
        }
    }

    return 0;
}

static int ir_try_resolve_returned_binding_callee_info(IrLowerContext *ctx,
    const AstExpression *callee_base_expr,
    IrLowerBindingInfo *out_binding_info) {
    if (out_binding_info) {
        memset(out_binding_info, 0, sizeof(*out_binding_info));
    }
    if (!ctx || !callee_base_expr || !out_binding_info ||
        callee_base_expr->kind != AST_EXPR_IDENTIFIER ||
        !callee_base_expr->as.identifier.name) {
        return 0;
    }
    return ir_lower_scope_lookup_binding(
               &ctx->scopes,
               callee_base_expr->as.identifier.name,
               out_binding_info) &&
        out_binding_info->from_returned_function_value_call;
}

static int ir_init_function_object_descriptor_from_returned_call_materialization(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    int is_closure_family,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count,
    const char *payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    ir_init_function_object_descriptor(out_descriptor);
    if (!ctx || !call_expr || !producer_external || !direct_target_name ||
        direct_target_name[0] == '\0' || !payload_local_name_prefix ||
        payload_local_name_prefix[0] == '\0' || !out_descriptor) {
        return 0;
    }
    return ir_materialize_returned_function_object_descriptor(
            ctx,
            call_expr,
            producer_external,
            is_closure_family,
            direct_target_name,
            dynamic_target_names,
            dynamic_target_count,
            payload_local_name_prefix,
            out_descriptor);
}

static int ir_resolve_returned_function_value_call_producer_info(
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_base_call_expr,
    const AstExternal **out_producer_external,
    int *out_return_family_is_closure) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(call_expr);
    const AstExpression *callee_base = NULL;
    const AstExternal *producer_external = NULL;
    int return_family_is_closure = 0;
    size_t hop_count = 0u;

    if (out_base_call_expr) {
        *out_base_call_expr = NULL;
    }
    if (out_producer_external) {
        *out_producer_external = NULL;
    }
    if (out_return_family_is_closure) {
        *out_return_family_is_closure = 0;
    }
    if (!ctx || !call_expr || !ctx->ast_program ||
        !base || base->kind != AST_EXPR_CALL) {
        return 0;
    }

    while (base && base->kind == AST_EXPR_CALL && hop_count < 64u) {
        const AstExpression *returned_arg_expr = NULL;
        size_t passthrough_index = (size_t)-1;

        callee_base = ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
            return 0;
        }
        producer_external =
            ir_find_named_function_external_prefer_definition(
                ctx->ast_program,
                callee_base->as.identifier.name);
        if (!producer_external || !producer_external->has_function_return_signature) {
            return 0;
        }
        if (!ir_function_value_return_is_closure_family(
                ctx->ast_program,
                producer_external,
                &return_family_is_closure)) {
            return 0;
        }
        if (!ir_function_value_return_exact_parameter_passthrough_index(
                ctx->ast_program,
                producer_external,
                &passthrough_index) ||
            passthrough_index == (size_t)-1 ||
            !base->as.call.args ||
            passthrough_index >= base->as.call.arg_count) {
            break;
        }
        returned_arg_expr =
            ir_unwrap_function_value_wrapper_expression(base->as.call.args[passthrough_index]);
        if (!returned_arg_expr || returned_arg_expr->kind != AST_EXPR_CALL) {
            break;
        }
        base = returned_arg_expr;
        ++hop_count;
    }

    if (out_base_call_expr) {
        *out_base_call_expr = base;
    }
    if (out_producer_external) {
        *out_producer_external = producer_external;
    }
    if (out_return_family_is_closure) {
        *out_return_family_is_closure = return_family_is_closure;
    }
    return 1;
}

static int ir_callable_object_view_is_two_target_dynamic_family(
    const IrLowerCallableObjectView *view) {
    return view &&
        view->has_dynamic_family &&
        view->dynamic_tag_local_id != (size_t)-1 &&
        view->dynamic_target_count == 2u &&
        view->dynamic_target_names &&
        view->dynamic_target_names[0] &&
        view->dynamic_target_names[1];
}

static int ir_finish_dynamic_function_value_specialization_view_resolution(
    IrLowerCallableObjectView *out_view,
    const char *const **out_owned_dynamic_target_names,
    int *out_requires_materialized_capture_forwarding) {
    if (out_owned_dynamic_target_names) {
        *out_owned_dynamic_target_names = NULL;
    }
    if (out_requires_materialized_capture_forwarding) {
        *out_requires_materialized_capture_forwarding = 0;
    }
    if (!out_view ||
        (out_view->is_closure_family && out_view->capture_count < 1u) ||
        !ir_callable_object_view_is_two_target_dynamic_family(out_view)) {
        return 0;
    }
    if (out_requires_materialized_capture_forwarding) {
        *out_requires_materialized_capture_forwarding =
            out_view->is_closure_family && out_view->capture_count > 0u;
    }
    return 1;
}

static int ir_finish_dynamic_function_value_specialization_materialized_view(
    IrLowerCallableObjectView *out_view,
    const char *const **out_owned_dynamic_target_names,
    int *out_requires_materialized_capture_forwarding,
    int materialize_ok) {
    return materialize_ok &&
        ir_finish_dynamic_function_value_specialization_view_resolution(
            out_view,
            out_owned_dynamic_target_names,
            out_requires_materialized_capture_forwarding);
}

static int ir_prepare_dynamic_function_value_specialization_dispatch(
    const IrLowerCallableObjectView *dynamic_callable_view,
    const char *const *owned_dynamic_target_names,
    int requires_materialized_capture_forwarding) {
    if (!ir_callable_object_view_is_two_target_dynamic_family(
            dynamic_callable_view)) {
        free((void *)owned_dynamic_target_names);
        return 1;
    }
    if (!requires_materialized_capture_forwarding &&
        dynamic_callable_view->is_closure_family) {
        free((void *)owned_dynamic_target_names);
        return 1;
    }
    return 0;
}

static int ir_try_finish_dynamic_function_value_specialization_indirect_fast_path(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrLowerCallableObjectView *dynamic_callable_view,
    const IrValueRef *call_args_view,
    size_t callable_arg_count,
    const char *const *owned_dynamic_target_names,
    IrValueRef *out_value,
    int *out_applied,
    int *out_applied_indirect_dispatch) {
    IrValueRef ignored_result = ir_value_immediate(0);
    IrValueRef indirect_result = ir_value_immediate(0);
    IrValueRef *call_result_target =
        callee_external && callee_external->function_return_type == AST_FUNCTION_RETURN_VOID
            ? &ignored_result
            : &indirect_result;
    int applied_indirect_dispatch = 0;

    if (out_applied_indirect_dispatch) {
        *out_applied_indirect_dispatch = 0;
    }
    if (!ctx || !callee_external || !dynamic_callable_view ||
        !ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
            ctx,
            dynamic_callable_view,
            call_args_view,
            callable_arg_count,
            call_result_target,
            &applied_indirect_dispatch)) {
        return 0;
    }
    if (out_applied_indirect_dispatch) {
        *out_applied_indirect_dispatch = applied_indirect_dispatch;
    }
    if (!applied_indirect_dispatch) {
        return 1;
    }

    free((void *)owned_dynamic_target_names);
    return ir_finish_void_or_value_callable_dispatch_result(
        callee_external->function_return_type == AST_FUNCTION_RETURN_VOID,
        indirect_result,
        out_value,
        out_applied);
}

static int ir_init_function_object_descriptor_from_returned_lowered_payload(
    const AstExternal *producer_external,
    int is_closure_family,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count,
    size_t *payload_local_ids,
    size_t payload_slot_count,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    int payload_has_dynamic_tag = 0;

    ir_init_function_object_descriptor(out_descriptor);
    if (!producer_external || !producer_external->has_function_return_signature ||
        !direct_target_name || direct_target_name[0] == '\0' || !out_descriptor) {
        return 0;
    }

    payload_has_dynamic_tag =
        payload_slot_count > 0u &&
        (dynamic_target_count > 1u || !is_closure_family)
            ? 1
            : 0;
    out_descriptor->producer_external = producer_external;
    out_descriptor->direct_target_name = direct_target_name;
    out_descriptor->dynamic_target_names = dynamic_target_names;
    out_descriptor->dynamic_target_count = dynamic_target_count;
    out_descriptor->owned_dynamic_target_names = (const char **)dynamic_target_names;
    out_descriptor->owned_payload_local_ids = payload_local_ids;
    out_descriptor->payload_slot_count = payload_slot_count;
    out_descriptor->dynamic_tag_local_id =
        payload_has_dynamic_tag && payload_local_ids
            ? payload_local_ids[0]
            : (size_t)-1;
    out_descriptor->capture_local_ids =
        payload_has_dynamic_tag
            ? (payload_slot_count > 1u && payload_local_ids ? &payload_local_ids[1] : NULL)
            : payload_local_ids;
    out_descriptor->capture_count =
        payload_has_dynamic_tag
            ? (payload_slot_count > 0u ? payload_slot_count - 1u : 0u)
            : payload_slot_count;
    out_descriptor->is_closure_family = is_closure_family ? 1 : 0;
    out_descriptor->has_dynamic_family = dynamic_target_count > 1u;
    out_descriptor->payload_has_dynamic_tag = payload_has_dynamic_tag;
    return 1;
}

typedef struct {
    const char *helper_name;
    int is_closure_family;
    int is_dynamic_two_target;
    IrLowerFunctionObjectDescriptor descriptor;
} IrLowerReturnedFunctionObjectResolution;

static int ir_try_resolve_returned_function_object_resolution(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    IrLowerReturnedFunctionObjectResolution *out_resolution) {
    const AstExpression *base = NULL;
    int return_family_is_closure = 0;
    const char *payload_local_name_prefix = NULL;
    const char *helper_name = NULL;

    if (out_resolution) {
        memset(out_resolution, 0, sizeof(*out_resolution));
        ir_init_function_object_descriptor(&out_resolution->descriptor);
    }
    if (!ctx || !call_expr || !out_resolution ||
        !nonclosure_payload_local_name_prefix ||
        nonclosure_payload_local_name_prefix[0] == '\0' ||
        !closure_payload_local_name_prefix ||
        closure_payload_local_name_prefix[0] == '\0') {
        return 0;
    }
    if (!ir_try_resolve_returned_function_value_family_payload_prefix(
            ctx,
            call_expr,
            nonclosure_payload_local_name_prefix,
            closure_payload_local_name_prefix,
            &base,
            &return_family_is_closure,
            &payload_local_name_prefix) ||
        !ir_resolve_returned_function_object_descriptor_with_prefix(
            ctx,
            base,
            0u,
            payload_local_name_prefix,
            &out_resolution->descriptor)) {
        return 0;
    }

    out_resolution->is_closure_family = return_family_is_closure ? 1 : 0;
    out_resolution->is_dynamic_two_target =
        out_resolution->descriptor.dynamic_target_count == 2u &&
        out_resolution->descriptor.dynamic_target_names &&
        out_resolution->descriptor.dynamic_target_names[0] &&
        out_resolution->descriptor.dynamic_target_names[1] &&
        out_resolution->descriptor.dynamic_tag_local_id != (size_t)-1
            ? 1
            : 0;

    if (out_resolution->is_closure_family) {
        if (!ir_resolve_returned_closure_target_info(
                ctx->ast_program,
                ctx,
                out_resolution->descriptor.producer_external,
                &helper_name,
                NULL,
                NULL,
                NULL) ||
            !helper_name ||
            helper_name[0] == '\0') {
            return 0;
        }
        out_resolution->helper_name = helper_name;
        return 1;
    }

    if (!out_resolution->descriptor.direct_target_name ||
        out_resolution->descriptor.direct_target_name[0] == '\0') {
        return 0;
    }
    return 1;
}

static int ir_resolve_returned_function_object_descriptor_with_prefix(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t required_dynamic_target_count,
    const char *payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(call_expr);
    const AstExternal *producer_external = NULL;
    const char **returned_target_names = NULL;
    size_t returned_target_count = 0u;
    const char *returned_primary_target_name = NULL;
    int return_family_is_closure = 0;

    ir_init_function_object_descriptor(out_descriptor);
    if (!ctx || !call_expr || !payload_local_name_prefix ||
        payload_local_name_prefix[0] == '\0' || !out_descriptor ||
        !base || base->kind != AST_EXPR_CALL || !ctx->ast_program || !ctx->program) {
        return 0;
    }

    if (!ir_resolve_returned_function_value_call_producer_info(
            ctx,
            base,
            NULL,
            &producer_external,
            &return_family_is_closure)) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-139: failed while recovering returned function-value producer info");
        }
        return 0;
    }
    if (!ir_resolve_returned_function_value_target_info(
            ctx,
            base,
            producer_external,
            return_family_is_closure,
            &returned_target_names,
            &returned_target_count,
            &returned_primary_target_name) ||
        !returned_primary_target_name ||
        returned_primary_target_name[0] == '\0') {
        free((void *)returned_target_names);
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-140: failed while recovering returned function-value target set");
        }
        return 0;
    }
    if (!return_family_is_closure &&
        returned_target_count == 0u) {
        free((void *)returned_target_names);
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-142: noncapturing returned function-value recovery produced no target set");
        }
        return 0;
    }
    if (required_dynamic_target_count > 0u &&
        returned_target_count != required_dynamic_target_count) {
        free((void *)returned_target_names);
        return 0;
    }

    if (!ir_init_function_object_descriptor_from_returned_call_materialization(
            ctx,
            base,
            producer_external,
            return_family_is_closure,
            returned_primary_target_name,
            returned_target_names,
            returned_target_count,
            payload_local_name_prefix,
            out_descriptor)) {
        free((void *)returned_target_names);
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-141: failed while materializing returned function-object descriptor payload");
        }
        return 0;
    }
    return 1;
}

static int ir_try_resolve_returned_function_value_family_payload_prefix(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    const AstExpression **out_base_call_expr,
    int *out_return_family_is_closure,
    const char **out_payload_local_name_prefix) {
    const AstExpression *base = NULL;
    int return_family_is_closure = 0;
    const char *payload_local_name_prefix = NULL;

    if (out_base_call_expr) {
        *out_base_call_expr = NULL;
    }
    if (out_return_family_is_closure) {
        *out_return_family_is_closure = 0;
    }
    if (out_payload_local_name_prefix) {
        *out_payload_local_name_prefix = NULL;
    }
    if (!ctx || !call_expr ||
        !nonclosure_payload_local_name_prefix ||
        nonclosure_payload_local_name_prefix[0] == '\0' ||
        !closure_payload_local_name_prefix ||
        closure_payload_local_name_prefix[0] == '\0' ||
        !ctx->ast_program) {
        return 0;
    }
    if (!ir_resolve_returned_function_value_call_producer_info(
            ctx,
            call_expr,
            &base,
            NULL,
            &return_family_is_closure)) {
        return 0;
    }

    payload_local_name_prefix =
        return_family_is_closure ? closure_payload_local_name_prefix
                                 : nonclosure_payload_local_name_prefix;
    if (out_base_call_expr) {
        *out_base_call_expr = base;
    }
    if (out_return_family_is_closure) {
        *out_return_family_is_closure = return_family_is_closure;
    }
    if (out_payload_local_name_prefix) {
        *out_payload_local_name_prefix = payload_local_name_prefix;
    }
    return 1;
}

static int ir_try_resolve_returned_function_object_descriptor_with_family_prefixes(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t required_dynamic_target_count,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor,
    int *out_return_family_is_closure) {
    const AstExpression *base = NULL;
    int return_family_is_closure = 0;
    const char *payload_local_name_prefix = NULL;

    ir_init_function_object_descriptor(out_descriptor);
    if (out_return_family_is_closure) {
        *out_return_family_is_closure = 0;
    }
    if (!ctx || !call_expr || !out_descriptor ||
        !nonclosure_payload_local_name_prefix ||
        nonclosure_payload_local_name_prefix[0] == '\0' ||
        !closure_payload_local_name_prefix ||
        closure_payload_local_name_prefix[0] == '\0') {
        return 0;
    }
    if (!ir_try_resolve_returned_function_value_family_payload_prefix(
            ctx,
            call_expr,
            nonclosure_payload_local_name_prefix,
            closure_payload_local_name_prefix,
            &base,
            &return_family_is_closure,
            &payload_local_name_prefix) ||
        !ir_resolve_returned_function_object_descriptor_with_prefix(
            ctx,
            base,
            required_dynamic_target_count,
            payload_local_name_prefix,
            out_descriptor)) {
        return 0;
    }
    if (out_return_family_is_closure) {
        *out_return_family_is_closure = return_family_is_closure;
    }
    return 1;
}

static int ir_resolve_identifier_ternary_function_value_info(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerFunctionValueTernaryInfo *out_info) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const AstExpression *then_passthrough_expr = NULL;
    const AstExpression *else_passthrough_expr = NULL;
    IrLowerBindingInfo then_binding_info;
    IrLowerBindingInfo else_binding_info;

    if (out_info) {
        memset(out_info, 0, sizeof(*out_info));
        out_info->then_tag_local_id = (size_t)-1;
        out_info->else_tag_local_id = (size_t)-1;
    }
    if (!ctx || !base || base->kind != AST_EXPR_TERNARY || !out_info) {
        return 0;
    }
    memset(&then_binding_info, 0, sizeof(then_binding_info));
    memset(&else_binding_info, 0, sizeof(else_binding_info));

    out_info->then_base = ir_unwrap_paren_expression(base->as.ternary.then_expr);
    out_info->else_base = ir_unwrap_paren_expression(base->as.ternary.else_expr);
    if (out_info->then_base && out_info->then_base->kind == AST_EXPR_CALL &&
        ir_try_resolve_exact_returned_function_value_parameter_passthrough_base_expr(
            ctx,
            out_info->then_base,
            &then_passthrough_expr) &&
        then_passthrough_expr) {
        out_info->then_base = then_passthrough_expr;
    }
    if (out_info->else_base && out_info->else_base->kind == AST_EXPR_CALL &&
        ir_try_resolve_exact_returned_function_value_parameter_passthrough_base_expr(
            ctx,
            out_info->else_base,
            &else_passthrough_expr) &&
        else_passthrough_expr) {
        out_info->else_base = else_passthrough_expr;
    }
    if (!out_info->then_base || !out_info->else_base ||
        out_info->then_base->kind != AST_EXPR_IDENTIFIER ||
        out_info->else_base->kind != AST_EXPR_IDENTIFIER) {
        return 0;
    }

    if (!ir_resolve_function_value_initializer_binding(
               ctx,
               out_info->then_base,
               &out_info->then_target_name,
               &out_info->then_capture_local_ids,
               &out_info->then_capture_count,
               &out_info->then_tag_local_id,
               &out_info->then_has_const_value,
               &out_info->then_const_value,
               &out_info->then_is_closure_local) ||
        !ir_resolve_function_value_initializer_binding(
               ctx,
               out_info->else_base,
               &out_info->else_target_name,
               &out_info->else_capture_local_ids,
               &out_info->else_capture_count,
               &out_info->else_tag_local_id,
               &out_info->else_has_const_value,
               &out_info->else_const_value,
               &out_info->else_is_closure_local)) {
        return 0;
    }

    if (ir_lower_scope_lookup_binding(
            &ctx->scopes,
            out_info->then_base->as.identifier.name,
            &then_binding_info) &&
        (then_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
            then_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) &&
        then_binding_info.function_target_tag_local_id != (size_t)-1) {
        out_info->then_tag_local_id = then_binding_info.function_target_tag_local_id;
        out_info->then_has_const_value = then_binding_info.has_const_value;
        out_info->then_const_value = then_binding_info.const_value;
    }
    if (ir_lower_scope_lookup_binding(
            &ctx->scopes,
            out_info->else_base->as.identifier.name,
            &else_binding_info) &&
        (else_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
            else_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) &&
        else_binding_info.function_target_tag_local_id != (size_t)-1) {
        out_info->else_tag_local_id = else_binding_info.function_target_tag_local_id;
        out_info->else_has_const_value = else_binding_info.has_const_value;
        out_info->else_const_value = else_binding_info.const_value;
    }

    return 1;
}

static int ir_emit_materialized_ternary_function_value_selection(IrLowerContext *ctx,
    const AstExpression *condition_expr,
    long long then_tag_value,
    long long else_tag_value,
    size_t then_tag_local_id,
    size_t else_tag_local_id,
    int then_has_const_value,
    int else_has_const_value,
    const size_t *then_capture_local_ids,
    const size_t *else_capture_local_ids,
    size_t capture_count,
    size_t tag_local_id,
    const size_t *capture_local_ids) {
    IrValueRef cond_value;
    IrValueRef compare_value;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;
    size_t capture_index;

    if (!ctx || !condition_expr) {
        return 0;
    }

    if (!ir_lower_expression(ctx, condition_expr, &cond_value)) {
        return 0;
    }
    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    if (!ir_emit_binary(ctx,
            compare_value,
            IR_BINARY_NE,
            cond_value,
            ir_value_immediate(0)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_emit_mov(ctx,
            ir_value_local(tag_local_id),
            (then_tag_local_id != (size_t)-1 && !then_has_const_value)
                ? ir_value_local(then_tag_local_id)
                : ir_value_immediate(then_tag_value))) {
        return 0;
    }
    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        if (!then_capture_local_ids || !capture_local_ids ||
            !ir_emit_mov(ctx,
                ir_value_local(capture_local_ids[capture_index]),
                ir_value_local(then_capture_local_ids[capture_index]))) {
            return 0;
        }
    }
    if (!ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_emit_mov(ctx,
            ir_value_local(tag_local_id),
            (else_tag_local_id != (size_t)-1 && !else_has_const_value)
                ? ir_value_local(else_tag_local_id)
                : ir_value_immediate(else_tag_value))) {
        return 0;
    }
    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        if (!else_capture_local_ids || !capture_local_ids ||
            !ir_emit_mov(ctx,
                ir_value_local(capture_local_ids[capture_index]),
                ir_value_local(else_capture_local_ids[capture_index]))) {
            return 0;
        }
    }
    if (!ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    return 1;
}

static int ir_emit_materialized_ternary_function_value_assignment(IrLowerContext *ctx,
    const AstExpression *condition_expr,
    const IrLowerFunctionValueTernaryInfo *ternary_info,
    size_t tag_local_id,
    const size_t *capture_local_ids,
    size_t capture_count) {
    if (!ctx || !condition_expr || !ternary_info ||
        !ternary_info->then_target_name || !ternary_info->else_target_name ||
        ternary_info->then_capture_count != ternary_info->else_capture_count) {
        return 0;
    }

    return ir_emit_materialized_ternary_function_value_selection(
        ctx,
        condition_expr,
        ternary_info->then_const_value,
        ternary_info->else_const_value,
        ternary_info->then_tag_local_id,
        ternary_info->else_tag_local_id,
        ternary_info->then_has_const_value,
        ternary_info->else_has_const_value,
        ternary_info->then_capture_local_ids,
        ternary_info->else_capture_local_ids,
        capture_count,
        tag_local_id,
        capture_local_ids);
}

static int ir_emit_materialized_ternary_function_value_payload_slot(IrLowerContext *ctx,
    size_t dest_local_id,
    IrValueRef value,
    int dest_is_indirect) {
    if (!ctx) {
        return 0;
    }

    if (dest_is_indirect) {
        return ir_emit_store_indirect(ctx, ir_value_local(dest_local_id), value);
    }

    return ir_emit_mov(ctx, ir_value_local(dest_local_id), value);
}

static int ir_emit_materialized_ternary_function_value_return_payload(IrLowerContext *ctx,
    const AstExpression *condition_expr,
    const IrLowerFunctionValueTernaryInfo *ternary_info,
    int is_closure_return,
    size_t dest_first_local_id,
    size_t slot_count,
    int dest_is_indirect) {
    IrValueRef cond_value;
    IrValueRef compare_value;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;
    size_t slot_index;

    if (!ctx || !condition_expr || !ternary_info ||
        !ternary_info->then_target_name || !ternary_info->else_target_name ||
        ternary_info->then_is_closure_local != ternary_info->else_is_closure_local ||
        ternary_info->then_capture_count != ternary_info->else_capture_count) {
        return 0;
    }

    if (!ir_lower_expression(ctx, condition_expr, &cond_value)) {
        return 0;
    }
    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    if (!ir_emit_binary(ctx,
            compare_value,
            IR_BINARY_NE,
            cond_value,
            ir_value_immediate(0)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!is_closure_return) {
        if (!ir_emit_materialized_ternary_function_value_payload_slot(
                ctx,
                dest_first_local_id,
                ir_value_immediate(ternary_info->then_const_value),
                dest_is_indirect) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return 0;
        }
    } else {
        size_t capture_slot_offset = 0u;

        if (slot_count == ternary_info->then_capture_count + 1u) {
            if (!ir_emit_materialized_ternary_function_value_payload_slot(
                    ctx,
                    dest_first_local_id,
                    (ternary_info->then_tag_local_id != (size_t)-1 &&
                        !ternary_info->then_has_const_value)
                        ? ir_value_local(ternary_info->then_tag_local_id)
                        : ir_value_immediate(ternary_info->then_const_value),
                    dest_is_indirect)) {
                return 0;
            }
            capture_slot_offset = 1u;
        }
        for (slot_index = 0u; slot_index < ternary_info->then_capture_count; ++slot_index) {
            if (!ternary_info->then_capture_local_ids ||
                !ir_emit_materialized_ternary_function_value_payload_slot(
                    ctx,
                    dest_first_local_id + capture_slot_offset + slot_index,
                    ir_value_local(ternary_info->then_capture_local_ids[slot_index]),
                    dest_is_indirect)) {
                return 0;
            }
        }
        if (!ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return 0;
        }
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!is_closure_return) {
        if (!ir_emit_materialized_ternary_function_value_payload_slot(
                ctx,
                dest_first_local_id,
                ir_value_immediate(ternary_info->else_const_value),
                dest_is_indirect) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return 0;
        }
    } else {
        size_t capture_slot_offset = 0u;

        if (slot_count == ternary_info->else_capture_count + 1u) {
            if (!ir_emit_materialized_ternary_function_value_payload_slot(
                    ctx,
                    dest_first_local_id,
                    (ternary_info->else_tag_local_id != (size_t)-1 &&
                        !ternary_info->else_has_const_value)
                        ? ir_value_local(ternary_info->else_tag_local_id)
                        : ir_value_immediate(ternary_info->else_const_value),
                    dest_is_indirect)) {
                return 0;
            }
            capture_slot_offset = 1u;
        }
        for (slot_index = 0u; slot_index < ternary_info->else_capture_count; ++slot_index) {
            if (!ternary_info->else_capture_local_ids ||
                !ir_emit_materialized_ternary_function_value_payload_slot(
                    ctx,
                    dest_first_local_id + capture_slot_offset + slot_index,
                    ir_value_local(ternary_info->else_capture_local_ids[slot_index]),
                    dest_is_indirect)) {
                return 0;
            }
        }
        if (!ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return 0;
        }
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    return 1;
}

static int ir_emit_two_target_dispatch_call(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value) {
    IrValueRef compare_value;
    IrValueRef result_value;
    size_t result_temp_id;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !then_target_name || !else_target_name || !out_value) {
        return 0;
    }

    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_binary(ctx,
            compare_value,
            IR_BINARY_EQ,
            tag_value,
            ir_value_immediate(then_tag_value)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_emit_call(ctx, result_value, then_target_name, args, arg_count) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_emit_call(ctx, result_value, else_target_name, args, arg_count) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    return ir_finish_callable_bridge_result_value(result_value, out_value);
}

static int ir_emit_two_target_noncapturing_callable_indirect_call(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value) {
    const char *then_wrapper_name = NULL;
    const char *else_wrapper_name = NULL;
    size_t shape_id;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !ctx->program || !then_target_name || !else_target_name || !out_value ||
        (arg_count > 0u && !args)) {
        return 0;
    }

    if (!ir_ensure_noncapturing_callable_wrapper_function(ctx, then_target_name, &then_wrapper_name) ||
        !ir_ensure_noncapturing_callable_wrapper_function(ctx, else_target_name, &else_wrapper_name)) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-091: failed while synthesizing returned callable wrappers '%s'/'%s'",
                then_target_name ? then_target_name : "<null>",
                else_target_name ? else_target_name : "<null>");
        }
        return 0;
    }

    if (!ir_build_two_target_callable_shape_bridge_from_names(
            ctx,
            then_wrapper_name,
            else_wrapper_name,
            1u,
            "IR-LOWER-071: returned callable-wrapper dispatch bridge shape mismatch",
            &shape_id)) {
        return 0;
    }

    if (!ir_emit_two_target_callable_object_indirect_call_bridged(
            ctx,
            tag_value,
            then_tag_value,
            then_wrapper_name,
            else_wrapper_name,
            ir_value_immediate(0),
            ir_value_immediate(0),
            shape_id,
            args,
            arg_count,
            out_value)) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-094: failed while lowering callable dispatch bridge");
        }
        return 0;
    }
    return 1;
}

static int ir_emit_callable_object_indirect_call_into(IrLowerContext *ctx,
    IrValueRef result_value,
    const char *callee_name,
    IrValueRef env_value,
    size_t shape_id,
    const IrValueRef *args,
    size_t arg_count) {
    size_t callable_temp_id;
    IrValueRef callable_value;

    if (!ctx || !callee_name || callee_name[0] == '\0' ||
        (arg_count > 0u && !args)) {
        return 0;
    }

    callable_temp_id = ir_allocate_temp(ctx->function);
    callable_value = ir_value_temp(callable_temp_id);
    if (!ir_emit_fn_make(
            ctx,
            callable_value,
            callee_name,
            env_value,
            shape_id) ||
        !ir_emit_call_indirect(
            ctx,
            result_value,
            callable_value,
            args,
            arg_count)) {
        return 0;
    }

    return 1;
}

static int ir_emit_callable_object_indirect_call(IrLowerContext *ctx,
    const char *callee_name,
    IrValueRef env_value,
    size_t shape_id,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value) {
    IrValueRef result_value;
    size_t callable_temp_id;
    size_t result_temp_id;
    IrValueRef callable_value;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !out_value) {
        return 0;
    }

    callable_temp_id = ir_allocate_temp(ctx->function);
    result_temp_id = ir_allocate_temp(ctx->function);
    callable_value = ir_value_temp(callable_temp_id);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_fn_make(
            ctx,
            callable_value,
            callee_name,
            env_value,
            shape_id) ||
        !ir_emit_call_indirect(
            ctx,
            result_value,
            callable_value,
            args,
            arg_count)) {
        return 0;
    }

    return ir_finish_callable_bridge_result_value(result_value, out_value);
}

static int ir_build_callable_shape_from_function_signature(IrLowerContext *ctx,
    const IrFunction *target_function,
    size_t hidden_prefix_param_count,
    size_t *out_shape_id) {
    AstFunctionReturnType *parameter_types = NULL;
    size_t callable_arg_count;
    size_t arg_index;

    if (out_shape_id) {
        *out_shape_id = (size_t)-1;
    }
    if (!ctx || !ctx->program || !target_function || !out_shape_id ||
        target_function->parameter_count < hidden_prefix_param_count) {
        return 0;
    }

    callable_arg_count = target_function->parameter_count - hidden_prefix_param_count;
    if (callable_arg_count > 0u) {
        parameter_types =
            (AstFunctionReturnType *)malloc(callable_arg_count * sizeof(AstFunctionReturnType));
        if (!parameter_types) {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-092: out of memory while building callable shape parameter types");
            return 0;
        }
        for (arg_index = 0u; arg_index < callable_arg_count; ++arg_index) {
            const IrLocal *parameter_local =
                ir_function_get_local(target_function, hidden_prefix_param_count + arg_index);
            if (!parameter_local) {
                free(parameter_types);
                return 0;
            }
            parameter_types[arg_index] = parameter_local->value_type;
        }
    }
    if (!ir_program_append_function_shape(
            ctx->program,
            target_function->return_type,
            parameter_types,
            callable_arg_count,
            out_shape_id,
            ctx->error)) {
        free(parameter_types);
        return 0;
    }
    free(parameter_types);
    return 1;
}

static int ir_emit_two_target_callable_object_indirect_call_bridged(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    IrValueRef then_env_value,
    IrValueRef else_env_value,
    size_t shape_id,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value) {
    IrValueRef compare_value;
    IrValueRef result_value;
    size_t result_temp_id;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !then_target_name || !else_target_name || !out_value ||
        (arg_count > 0u && !args)) {
        return 0;
    }

    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_binary(ctx,
            compare_value,
            IR_BINARY_EQ,
            tag_value,
            ir_value_immediate(then_tag_value)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_emit_callable_object_indirect_call_into(
            ctx,
            result_value,
            then_target_name,
            then_env_value,
            shape_id,
            args,
            arg_count) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_emit_callable_object_indirect_call_into(
            ctx,
            result_value,
            else_target_name,
            else_env_value,
            shape_id,
            args,
            arg_count) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    return ir_finish_callable_bridge_result_value(result_value, out_value);
}

static int ir_build_two_target_callable_shape_bridge(IrLowerContext *ctx,
    const IrFunction *then_target_function,
    const IrFunction *else_target_function,
    size_t hidden_prefix_param_count,
    const char *shape_error_message,
    size_t *out_shape_id) {
    AstFunctionReturnType *parameter_types = NULL;
    size_t callable_arg_count;
    size_t arg_index;

    if (out_shape_id) {
        *out_shape_id = (size_t)-1;
    }
    if (!ctx || !ctx->program || !then_target_function || !else_target_function ||
        !out_shape_id ||
        then_target_function->parameter_count < hidden_prefix_param_count ||
        else_target_function->parameter_count < hidden_prefix_param_count ||
        then_target_function->parameter_count != else_target_function->parameter_count ||
        then_target_function->return_type != else_target_function->return_type) {
        if (ctx && ctx->error) {
            ir_set_error(ctx->error,
                0,
                0,
                "%s",
                shape_error_message
                    ? shape_error_message
                    : "IR-LOWER-071: callable dispatch bridge shape mismatch");
        }
        return 0;
    }

    callable_arg_count = then_target_function->parameter_count - hidden_prefix_param_count;
    if (callable_arg_count > 0u) {
        parameter_types =
            (AstFunctionReturnType *)malloc(callable_arg_count * sizeof(AstFunctionReturnType));
        if (!parameter_types) {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-092: out of memory while building callable shape parameter types");
            return 0;
        }
        for (arg_index = 0u; arg_index < callable_arg_count; ++arg_index) {
            const IrLocal *then_param =
                ir_function_get_local(then_target_function, hidden_prefix_param_count + arg_index);
            const IrLocal *else_param =
                ir_function_get_local(else_target_function, hidden_prefix_param_count + arg_index);
            if (!then_param || !else_param || then_param->value_type != else_param->value_type) {
                free(parameter_types);
                ir_set_error(ctx->error,
                    0,
                    0,
                    "%s",
                    shape_error_message
                        ? shape_error_message
                        : "IR-LOWER-071: callable dispatch bridge shape mismatch");
                return 0;
            }
            parameter_types[arg_index] = then_param->value_type;
        }
    }

    if (!ir_program_append_function_shape(
            ctx->program,
            then_target_function->return_type,
            parameter_types,
            callable_arg_count,
            out_shape_id,
            ctx->error)) {
        free(parameter_types);
        return 0;
    }
    free(parameter_types);
    return 1;
}

static int ir_build_two_target_callable_shape_bridge_from_names(IrLowerContext *ctx,
    const char *then_target_name,
    const char *else_target_name,
    size_t hidden_prefix_param_count,
    const char *shape_error_message,
    size_t *out_shape_id) {
    const IrFunction *then_target_function;
    const IrFunction *else_target_function;

    if (out_shape_id) {
        *out_shape_id = (size_t)-1;
    }
    if (!ctx || !ctx->program || !then_target_name || !else_target_name || !out_shape_id) {
        return 0;
    }

    then_target_function = ir_program_find_function_mut(ctx->program, then_target_name);
    else_target_function = ir_program_find_function_mut(ctx->program, else_target_name);
    if (!then_target_function || !else_target_function) {
        if (ctx->error) {
            ir_set_error(ctx->error,
                0,
                0,
                "%s",
                shape_error_message
                    ? shape_error_message
                    : "IR-LOWER-071: callable dispatch bridge shape mismatch");
        }
        return 0;
    }

    return ir_build_two_target_callable_shape_bridge(
        ctx,
        then_target_function,
        else_target_function,
        hidden_prefix_param_count,
        shape_error_message,
        out_shape_id);
}

static int ir_build_single_target_callable_shape_bridge_from_name(IrLowerContext *ctx,
    const char *target_name,
    size_t hidden_prefix_param_count,
    size_t expected_callable_arg_count,
    size_t *out_shape_id,
    const IrFunction **out_target_function) {
    const IrFunction *target_function;

    if (out_shape_id) {
        *out_shape_id = (size_t)-1;
    }
    if (out_target_function) {
        *out_target_function = NULL;
    }
    if (!ctx || !ctx->program || !target_name || !out_shape_id) {
        return 0;
    }

    target_function = ir_program_find_function_mut(ctx->program, target_name);
    if (!target_function ||
        target_function->parameter_count < hidden_prefix_param_count ||
        target_function->parameter_count - hidden_prefix_param_count != expected_callable_arg_count) {
        return 0;
    }
    if (out_target_function) {
        *out_target_function = target_function;
    }

    return ir_build_callable_shape_from_function_signature(
        ctx,
        target_function,
        hidden_prefix_param_count,
        out_shape_id);
}

static int ir_build_single_visible_callable_arg(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_arg) {
    if (out_arg) {
        *out_arg = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !out_arg ||
        call_expr->kind != AST_EXPR_CALL ||
        call_expr->as.call.arg_count != 1u ||
        !call_expr->as.call.args) {
        return 0;
    }

    return ir_lower_expression(ctx, call_expr->as.call.args[0], out_arg);
}

static int ir_build_required_single_visible_callable_arg(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *error_message,
    IrValueRef *out_arg) {
    if (out_arg) {
        *out_arg = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !out_arg) {
        return 0;
    }
    if (!ir_build_single_visible_callable_arg(ctx, call_expr, out_arg)) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "%s",
                error_message
                    ? error_message
                    : "IR-LOWER-064: callable-object bridge currently supports one visible argument");
        }
        return 0;
    }
    return 1;
}

static int ir_build_required_single_visible_callable_args(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *error_message,
    IrValueRef *out_args,
    size_t *out_arg_count) {
    IrValueRef arg_value;

    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (out_args) {
        out_args[0] = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !out_args || !out_arg_count) {
        return 0;
    }
    if (!ir_build_required_single_visible_callable_arg(
            ctx,
            call_expr,
            error_message,
            &arg_value)) {
        return 0;
    }
    return ir_build_optional_single_visible_callable_args_from_value(
        arg_value,
        1u,
        out_args,
        out_arg_count);
}

static int ir_build_required_single_visible_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *error_message,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count) {
        return 0;
    }
    if (!ir_build_required_single_visible_callable_args(
            ctx,
            call_expr,
            error_message,
            out_args,
            out_arg_count)) {
        return 0;
    }
    *out_args_view = ir_optional_single_callable_args_view(out_args, *out_arg_count);
    return 1;
}

static int ir_build_dynamic_visible_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *oom_error_message,
    IrValueRef **out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    IrValueRef *args = NULL;
    size_t arg_index;

    if (out_args) {
        *out_args = NULL;
    }
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count ||
        call_expr->kind != AST_EXPR_CALL) {
        return 0;
    }

    if (call_expr->as.call.arg_count == 0u) {
        return 1;
    }

    args = (IrValueRef *)malloc(call_expr->as.call.arg_count * sizeof(IrValueRef));
    if (!args) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "%s",
            oom_error_message
                ? oom_error_message
                : "IR-INT-126: out of memory while lowering callable-object arguments");
        return 0;
    }

    for (arg_index = 0u; arg_index < call_expr->as.call.arg_count; ++arg_index) {
        if (!ir_lower_expression(ctx, call_expr->as.call.args[arg_index], &args[arg_index])) {
            free(args);
            return 0;
        }
    }

    *out_args = args;
    *out_args_view = args;
    *out_arg_count = call_expr->as.call.arg_count;
    return 1;
}

static int ir_build_optional_single_visible_callable_args(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_args,
    size_t *out_arg_count) {
    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (out_args) {
        out_args[0] = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !out_arg_count || !out_args ||
        call_expr->kind != AST_EXPR_CALL) {
        return 0;
    }
    if (call_expr->as.call.arg_count == 0u) {
        return 1;
    }
    if (call_expr->as.call.arg_count != 1u) {
        *out_arg_count = call_expr->as.call.arg_count;
        return 1;
    }
    if (!ir_build_single_visible_callable_arg(ctx, call_expr, &out_args[0])) {
        return 0;
    }
    *out_arg_count = 1u;
    return 1;
}

static int ir_build_optional_single_indexed_callable_arg(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t arg_index,
    IrValueRef *out_arg) {
    if (out_arg) {
        *out_arg = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !out_arg || call_expr->kind != AST_EXPR_CALL) {
        return 0;
    }
    if (!call_expr->as.call.args || arg_index >= call_expr->as.call.arg_count) {
        return 0;
    }
    return ir_lower_expression(ctx, call_expr->as.call.args[arg_index], out_arg);
}

static int ir_build_optional_single_indexed_callable_args(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t visible_arg_count,
    size_t arg_index,
    IrValueRef *out_args,
    size_t *out_arg_count) {
    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (out_args) {
        out_args[0] = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !out_arg_count || (visible_arg_count > 0u && !out_args)) {
        return 0;
    }
    if (visible_arg_count == 0u) {
        return 1;
    }
    if (visible_arg_count != 1u) {
        *out_arg_count = visible_arg_count;
        return 1;
    }
    if (!ir_build_optional_single_indexed_callable_arg(
            ctx,
            call_expr,
            arg_index,
            &out_args[0])) {
        return 0;
    }
    *out_arg_count = 1u;
    return 1;
}

static int ir_build_required_single_indexed_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t arg_index,
    const char *error_message,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count) {
        return 0;
    }
    if (!ir_build_optional_single_indexed_callable_args(
            ctx,
            call_expr,
            1u,
            arg_index,
            out_args,
            out_arg_count)) {
        return 0;
    }
    if (*out_arg_count != 1u) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "%s",
                error_message
                    ? error_message
                    : "IR-LOWER-064: callable-object bridge currently supports one visible argument");
        }
        return 0;
    }
    *out_args_view = ir_optional_single_callable_args_view(out_args, *out_arg_count);
    return 1;
}

static int ir_build_dynamic_indexed_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t visible_arg_count,
    size_t arg_index,
    const char *oom_error_message,
    IrValueRef **out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    IrValueRef *args = NULL;
    size_t local_arg_index;

    if (out_args) {
        *out_args = NULL;
    }
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count ||
        call_expr->kind != AST_EXPR_CALL) {
        return 0;
    }
    if (visible_arg_count == 0u) {
        return 1;
    }

    args = (IrValueRef *)malloc(visible_arg_count * sizeof(IrValueRef));
    if (!args) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "%s",
            oom_error_message
                ? oom_error_message
                : "IR-INT-126: out of memory while lowering callable-object arguments");
        return 0;
    }

    for (local_arg_index = 0u; local_arg_index < visible_arg_count; ++local_arg_index) {
        if (!ir_build_optional_single_indexed_callable_arg(
                ctx,
                call_expr,
                arg_index + local_arg_index,
                &args[local_arg_index])) {
            free(args);
            return 0;
        }
    }

    *out_args = args;
    *out_args_view = args;
    *out_arg_count = visible_arg_count;
    return 1;
}

static int ir_build_dynamic_non_function_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t hidden_prefix_param_count,
    size_t function_arg_index,
    const char *oom_error_message,
    IrValueRef **out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    IrValueRef *args = NULL;
    size_t source_arg_index;
    size_t lowered_arg_index = 0u;
    size_t visible_arg_count = 0u;

    if (out_args) {
        *out_args = NULL;
    }
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (!ctx || !call_expr || !callee_external || !out_args || !out_args_view || !out_arg_count ||
        call_expr->kind != AST_EXPR_CALL) {
        return 0;
    }

    for (source_arg_index = 0u; source_arg_index < call_expr->as.call.arg_count; ++source_arg_index) {
        size_t callee_param_index = hidden_prefix_param_count + source_arg_index;

        if (callee_param_index >= callee_external->parameter_count) {
            continue;
        }
        if (callee_external->parameter_value_kinds &&
            callee_external->parameter_value_kinds[callee_param_index] == AST_PARAMETER_VALUE_FUNCTION) {
            continue;
        }
        visible_arg_count += 1u;
    }
    if (visible_arg_count == 0u) {
        return 1;
    }

    args = (IrValueRef *)malloc(visible_arg_count * sizeof(IrValueRef));
    if (!args) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "%s",
            oom_error_message
                ? oom_error_message
                : "IR-INT-126: out of memory while lowering callable-object arguments");
        return 0;
    }

    for (source_arg_index = 0u; source_arg_index < call_expr->as.call.arg_count; ++source_arg_index) {
        size_t callee_param_index = hidden_prefix_param_count + source_arg_index;

        if (callee_param_index >= callee_external->parameter_count) {
            continue;
        }
        if (callee_param_index == function_arg_index ||
            (callee_external->parameter_value_kinds &&
                callee_external->parameter_value_kinds[callee_param_index] == AST_PARAMETER_VALUE_FUNCTION)) {
            continue;
        }
        if (!ir_lower_expression(ctx, call_expr->as.call.args[source_arg_index], &args[lowered_arg_index])) {
            free(args);
            return 0;
        }
        lowered_arg_index += 1u;
    }

    *out_args = args;
    *out_args_view = args;
    *out_arg_count = visible_arg_count;
    return 1;
}

static int ir_build_optional_single_indexed_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    size_t visible_arg_count,
    size_t arg_index,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count) {
        return 0;
    }
    if (!ir_build_optional_single_indexed_callable_args(
            ctx,
            call_expr,
            visible_arg_count,
            arg_index,
            out_args,
            out_arg_count)) {
        return 0;
    }
    *out_args_view = ir_optional_single_callable_args_view(out_args, *out_arg_count);
    return 1;
}

static int ir_build_optional_single_visible_callable_args_from_value(IrValueRef value,
    size_t visible_arg_count,
    IrValueRef *out_args,
    size_t *out_arg_count) {
    if (out_arg_count) {
        *out_arg_count = 0u;
    }
    if (out_args) {
        out_args[0] = ir_value_immediate(0);
    }
    if (!out_args || !out_arg_count) {
        return 0;
    }
    if (visible_arg_count == 0u) {
        return 1;
    }
    if (visible_arg_count != 1u) {
        *out_arg_count = visible_arg_count;
        return 1;
    }
    out_args[0] = value;
    *out_arg_count = 1u;
    return 1;
}

static int ir_build_optional_single_visible_callable_args_view(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_args,
    const IrValueRef **out_args_view,
    size_t *out_arg_count) {
    if (out_args_view) {
        *out_args_view = NULL;
    }
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count) {
        return 0;
    }
    if (!ir_build_optional_single_visible_callable_args(
            ctx,
            call_expr,
            out_args,
            out_arg_count)) {
        return 0;
    }
    *out_args_view = ir_optional_single_callable_args_view(out_args, *out_arg_count);
    return 1;
}

static const IrValueRef *ir_optional_single_callable_args_view(const IrValueRef *args,
    size_t arg_count) {
    if (arg_count == 1u && args) {
        return args;
    }
    return NULL;
}

static int ir_mark_callable_dispatch_applied(int *out_applied) {
    if (out_applied) {
        *out_applied = 1;
    }
    return 1;
}

static int ir_finish_callable_bridge_result_value(IrValueRef result_value,
    IrValueRef *out_value) {
    if (out_value) {
        *out_value = result_value;
    }
    return 1;
}

static int ir_finish_returned_callable_capture_cleanup(size_t *owned_capture_local_ids) {
    free(owned_capture_local_ids);
    return 1;
}

static int ir_finish_dynamic_specialization_cleanup(
    IrValueRef *dynamic_forward_args,
    const char *const *owned_dynamic_target_names) {
    free(dynamic_forward_args);
    free((void *)owned_dynamic_target_names);
    return 1;
}

static int ir_finish_dynamic_specialization_call_cleanup(
    IrValueRef *call_args,
    IrValueRef *dynamic_forward_args,
    const char *const *owned_dynamic_target_names) {
    free(call_args);
    return ir_finish_dynamic_specialization_cleanup(
        dynamic_forward_args,
        owned_dynamic_target_names);
}

static int ir_finish_function_value_specialization_binding_cleanup(
    const char **bound_targets,
    const size_t **bound_capture_local_ids,
    size_t *bound_capture_counts) {
    free(bound_targets);
    free(bound_capture_local_ids);
    free(bound_capture_counts);
    return 1;
}

static int ir_finish_dynamic_function_value_specialization_name_cleanup(
    const char **bound_targets_a,
    const char **bound_targets_b,
    const size_t **bound_capture_local_ids,
    size_t *bound_capture_counts) {
    ir_finish_function_value_specialization_binding_cleanup(
        bound_targets_a,
        bound_capture_local_ids,
        bound_capture_counts);
    ir_finish_function_value_specialization_binding_cleanup(
        bound_targets_b,
        NULL,
        NULL);
    return 1;
}

static int ir_resolve_hidden_function_value_specialization_binding(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t parameter_index,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name) {
    const char *parameter_name = NULL;
    IrLowerFunctionObjectDescriptor descriptor;

    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ctx || !callee_external ||
        parameter_index >= callee_external->parameter_count ||
        !callee_external->parameter_names) {
        return 0;
    }

    parameter_name = callee_external->parameter_names[parameter_index];
    if (!parameter_name || parameter_name[0] == '\0') {
        return 0;
    }

    ir_init_function_object_descriptor(&descriptor);
    if (!ir_resolve_identifier_function_value_descriptor(
            ctx,
            parameter_name,
            &descriptor) ||
        !descriptor.direct_target_name ||
        descriptor.direct_target_name[0] == '\0') {
        return 0;
    }

    if (out_capture_local_ids) {
        *out_capture_local_ids = descriptor.capture_local_ids;
    }
    if (out_capture_count) {
        *out_capture_count = descriptor.capture_count;
    }
    if (out_target_name) {
        *out_target_name = descriptor.direct_target_name;
    }
    return 1;
}

static int ir_build_dynamic_function_value_specialization_forward_args(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    const AstExpression *call_expr,
    size_t function_arg_index,
    const IrLowerCallableObjectView *dynamic_callable_view,
    const IrValueRef *call_args,
    size_t callable_arg_count,
    int line,
    int column,
    IrValueRef **out_dynamic_forward_args,
    size_t *out_dynamic_forward_arg_count) {
    IrValueRef *dynamic_forward_args = NULL;
    size_t dynamic_forward_arg_count = 0u;

    if (out_dynamic_forward_args) {
        *out_dynamic_forward_args = NULL;
    }
    if (out_dynamic_forward_arg_count) {
        *out_dynamic_forward_arg_count = 0u;
    }
    if (!ctx || !dynamic_callable_view || !out_dynamic_forward_args ||
        !out_dynamic_forward_arg_count || !callee_external || !call_expr ||
        call_expr->kind != AST_EXPR_CALL) {
        return 0;
    }
    {
        size_t static_capture_count = 0u;
        size_t visible_index = 0u;
        size_t dynamic_capture_count = dynamic_callable_view->capture_count;

        if (callee_external->parameter_value_kinds) {
            for (size_t i = 0u; i < callee_external->parameter_count && i < call_expr->as.call.arg_count; ++i) {
                const AstExpression *arg_expr = NULL;
                const AstExpression *arg_base = NULL;
                IrLowerBindingInfo binding_info;

                if (i == function_arg_index ||
                    callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                    continue;
                }
                arg_expr = call_expr->as.call.args[i];
                arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
                memset(&binding_info, 0, sizeof(binding_info));
                if (arg_base &&
                    arg_base->kind == AST_EXPR_IDENTIFIER &&
                    arg_base->as.identifier.name &&
                    ir_lower_scope_lookup_binding(&ctx->scopes, arg_base->as.identifier.name, &binding_info) &&
                    binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL &&
                    binding_info.closure_capture_count > 0u &&
                    binding_info.closure_capture_local_ids) {
                    static_capture_count += binding_info.closure_capture_count;
                }
            }
        }

        if (dynamic_capture_count + static_capture_count == 0u) {
            return 1;
        }

        dynamic_forward_arg_count =
            dynamic_capture_count + static_capture_count + callable_arg_count;
        dynamic_forward_args = (IrValueRef *)malloc(dynamic_forward_arg_count * sizeof(IrValueRef));
        if (!dynamic_forward_args) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-INT-029: out of memory while preparing dynamic returned closure forwarding args");
            return 0;
        }

        for (size_t i = 0u; i < dynamic_capture_count; ++i) {
            dynamic_forward_args[visible_index++] =
                ir_value_local(dynamic_callable_view->capture_local_ids[i]);
        }

        if (callee_external->parameter_value_kinds) {
            for (size_t i = 0u; i < callee_external->parameter_count && i < call_expr->as.call.arg_count; ++i) {
                const AstExpression *arg_expr = NULL;
                const AstExpression *arg_base = NULL;
                IrLowerBindingInfo binding_info;

                if (i == function_arg_index ||
                    callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                    continue;
                }
                arg_expr = call_expr->as.call.args[i];
                arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
                memset(&binding_info, 0, sizeof(binding_info));
                if (arg_base &&
                    arg_base->kind == AST_EXPR_IDENTIFIER &&
                    arg_base->as.identifier.name &&
                    ir_lower_scope_lookup_binding(&ctx->scopes, arg_base->as.identifier.name, &binding_info) &&
                    binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL &&
                    binding_info.closure_capture_count > 0u &&
                    binding_info.closure_capture_local_ids) {
                    for (size_t capture_index = 0u;
                         capture_index < binding_info.closure_capture_count;
                         ++capture_index) {
                        dynamic_forward_args[visible_index++] =
                            ir_value_local(binding_info.closure_capture_local_ids[capture_index]);
                    }
                }
            }
        }

        for (size_t i = 0u; i < callable_arg_count; ++i) {
            dynamic_forward_args[visible_index++] = call_args[i];
        }
    }

    *out_dynamic_forward_args = dynamic_forward_args;
    *out_dynamic_forward_arg_count = dynamic_forward_arg_count;
    return 1;
}

static const IrValueRef *ir_dynamic_function_value_specialization_args_view(
    const IrValueRef *call_args,
    size_t callable_arg_count,
    const IrValueRef *dynamic_forward_args,
    size_t dynamic_forward_arg_count,
    size_t *out_arg_count) {
    if (out_arg_count) {
        *out_arg_count = dynamic_forward_args ? dynamic_forward_arg_count : callable_arg_count;
    }
    return dynamic_forward_args ? dynamic_forward_args : call_args;
}

static int ir_build_dynamic_function_value_specialization_names(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const IrLowerCallableObjectView *dynamic_callable_view,
    const AstExpression *const *call_args,
    size_t call_arg_count,
    int requires_materialized_capture_forwarding,
    int line,
    int column,
    const char **out_specialized_name_a,
    const char **out_specialized_name_b) {
    const char **bound_targets_a = NULL;
    const char **bound_targets_b = NULL;
    const size_t **bound_capture_local_ids = NULL;
    size_t *bound_capture_counts = NULL;
    const char *current_function_name = NULL;

    if (out_specialized_name_a) {
        *out_specialized_name_a = NULL;
    }
    if (out_specialized_name_b) {
        *out_specialized_name_b = NULL;
    }
    if (!ctx || !callee_external || !dynamic_callable_view ||
        !out_specialized_name_a || !out_specialized_name_b) {
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;

    bound_targets_a = (const char **)calloc(callee_external->parameter_count, sizeof(const char *));
    bound_targets_b = (const char **)calloc(callee_external->parameter_count, sizeof(const char *));
    bound_capture_local_ids =
        (const size_t **)calloc(callee_external->parameter_count, sizeof(const size_t *));
    bound_capture_counts =
        (size_t *)calloc(callee_external->parameter_count, sizeof(size_t));
    if (!bound_targets_a || !bound_targets_b || !bound_capture_local_ids || !bound_capture_counts) {
        ir_finish_dynamic_function_value_specialization_name_cleanup(
            bound_targets_a,
            bound_targets_b,
            bound_capture_local_ids,
            bound_capture_counts);
        ir_set_error(ctx->error,
            line,
            column,
            "IR-INT-029: out of memory while preparing dynamic function-value specialization bridge");
        return 0;
    }

    bound_targets_a[function_arg_index] = dynamic_callable_view->dynamic_target_names[0];
    bound_targets_b[function_arg_index] = dynamic_callable_view->dynamic_target_names[1];
    if (requires_materialized_capture_forwarding &&
        dynamic_callable_view->capture_count > 0u) {
        bound_capture_local_ids[function_arg_index] = dynamic_callable_view->capture_local_ids;
        bound_capture_counts[function_arg_index] = dynamic_callable_view->capture_count;
    }
    for (size_t i = 0u; i < callee_external->parameter_count && i < call_arg_count; ++i) {
        if (i == function_arg_index ||
            !callee_external->parameter_value_kinds ||
            callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
            continue;
        }
        if ((!call_args[i] &&
                !ir_resolve_hidden_function_value_specialization_binding(
                    ctx,
                    callee_external,
                    i,
                    &bound_capture_local_ids[i],
                    &bound_capture_counts[i],
                    &bound_targets_a[i])) ||
            (call_args[i] &&
                !ir_call_argument_resolve_extension_function_value_target(
                    ctx->ast_program,
                    ctx,
                    callee_external,
                    i,
                    call_args[i],
                    &bound_capture_local_ids[i],
                    &bound_capture_counts[i],
                    &bound_targets_a[i])) ||
            !bound_targets_a[i]) {
            ir_finish_dynamic_function_value_specialization_name_cleanup(
                bound_targets_a,
                bound_targets_b,
                bound_capture_local_ids,
                bound_capture_counts);
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    line,
                    column,
                    "IR-LOWER-083: failed while resolving secondary static function-value specialization argument");
            }
            return 0;
        }
        bound_targets_b[i] = bound_targets_a[i];
    }
    if (!ir_lower_specialized_function_for_function_value_call(
            ctx->ast_program,
            ctx->program,
            NULL,
            callee_external,
            bound_targets_a,
            bound_capture_local_ids,
            bound_capture_counts,
            out_specialized_name_a,
            ctx->error) ||
        !ir_lower_specialized_function_for_function_value_call(
            ctx->ast_program,
            ctx->program,
            NULL,
            callee_external,
            bound_targets_b,
            bound_capture_local_ids,
            bound_capture_counts,
            out_specialized_name_b,
            ctx->error)) {
        ir_finish_dynamic_function_value_specialization_name_cleanup(
            bound_targets_a,
            bound_targets_b,
            bound_capture_local_ids,
            bound_capture_counts);
        return 0;
    }
    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            line,
            column)) {
        ir_finish_dynamic_function_value_specialization_name_cleanup(
            bound_targets_a,
            bound_targets_b,
            bound_capture_local_ids,
            bound_capture_counts);
        return 0;
    }

    ir_finish_dynamic_function_value_specialization_name_cleanup(
        bound_targets_a,
        bound_targets_b,
        bound_capture_local_ids,
        bound_capture_counts);
    return 1;
}

static int ir_build_function_value_specialization_name_from_call_args(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    const AstExpression *const *call_args,
    size_t call_arg_count,
    int line,
    int column,
    const char *oom_error_message,
    const char *resolve_error_message,
    const char *build_error_message,
    const char **out_specialized_name) {
    const char *current_function_name = NULL;
    const char **bound_targets = NULL;
    const size_t **bound_capture_local_ids = NULL;
    size_t *bound_capture_counts = NULL;
    size_t i;

    if (out_specialized_name) {
        *out_specialized_name = NULL;
    }
    if (!ctx || !callee_external || !call_args || !out_specialized_name) {
        return 0;
    }
    current_function_name =
        (ctx->function && ctx->function->name && ctx->function->name[0] != '\0')
            ? ctx->function->name
            : NULL;

    bound_targets = (const char **)calloc(callee_external->parameter_count, sizeof(const char *));
    bound_capture_local_ids =
        (const size_t **)calloc(callee_external->parameter_count, sizeof(const size_t *));
    bound_capture_counts =
        (size_t *)calloc(callee_external->parameter_count, sizeof(size_t));
    if (!bound_targets || !bound_capture_local_ids || !bound_capture_counts) {
        ir_finish_function_value_specialization_binding_cleanup(
            bound_targets,
            bound_capture_local_ids,
            bound_capture_counts);
        if (oom_error_message && oom_error_message[0] != '\0') {
            ir_set_error(ctx->error, line, column, "%s", oom_error_message);
        }
        return 0;
    }

    for (i = 0u; i < call_arg_count; ++i) {
        if (callee_external->parameter_value_kinds &&
            callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
            if ((!call_args[i] &&
                    !ir_resolve_hidden_function_value_specialization_binding(
                        ctx,
                        callee_external,
                        i,
                        &bound_capture_local_ids[i],
                        &bound_capture_counts[i],
                        &bound_targets[i])) ||
                (call_args[i] &&
                    !ir_call_argument_resolve_extension_function_value_target(
                        ctx->ast_program,
                        ctx,
                        callee_external,
                        i,
                        call_args[i],
                        &bound_capture_local_ids[i],
                        &bound_capture_counts[i],
                        &bound_targets[i]))) {
                ir_finish_function_value_specialization_binding_cleanup(
                    bound_targets,
                    bound_capture_local_ids,
                    bound_capture_counts);
                if (resolve_error_message && resolve_error_message[0] != '\0') {
                    ir_set_error(ctx->error, line, column, "%s", resolve_error_message);
                }
                return 0;
            }
        }
    }

    if (!ir_lower_specialized_function_for_function_value_call(
            ctx->ast_program,
            ctx->program,
            NULL,
            callee_external,
            bound_targets,
            bound_capture_local_ids,
            bound_capture_counts,
            out_specialized_name,
            ctx->error)) {
        ir_finish_function_value_specialization_binding_cleanup(
            bound_targets,
            bound_capture_local_ids,
            bound_capture_counts);
        if (ctx->error && ctx->error->message[0] == '\0' &&
            build_error_message && build_error_message[0] != '\0') {
            ir_set_error(ctx->error, line, column, "%s", build_error_message);
        }
        return 0;
    }

    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            line,
            column)) {
        ir_finish_function_value_specialization_binding_cleanup(
            bound_targets,
            bound_capture_local_ids,
            bound_capture_counts);
        if (ctx->error && ctx->error->message[0] == '\0' &&
            build_error_message && build_error_message[0] != '\0') {
            ir_set_error(ctx->error, line, column, "%s", build_error_message);
        }
        return 0;
    }

    ir_finish_function_value_specialization_binding_cleanup(
        bound_targets,
        bound_capture_local_ids,
        bound_capture_counts);
    return 1;
}

static int ir_finish_function_value_specialization_callee_adoption(
    IrLowerContext *ctx,
    const char *current_function_name,
    const char *specialized_name,
    int line,
    int column,
    const char *refresh_error_message,
    const char **io_callee_name,
    IrFunction **io_callee_function,
    int *out_call_uses_function_value_specialization) {
    if (out_call_uses_function_value_specialization) {
        *out_call_uses_function_value_specialization = 0;
    }
    if (!ctx || !io_callee_name || !io_callee_function) {
        return 0;
    }
    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            line,
            column)) {
        if (ctx->error && ctx->error->message[0] == '\0' &&
            refresh_error_message && refresh_error_message[0] != '\0') {
            if (strstr(refresh_error_message, "%s")) {
                ir_set_error(
                    ctx->error,
                    line,
                    column,
                    refresh_error_message,
                    current_function_name ? current_function_name : "<unknown>");
            } else {
                ir_set_error(ctx->error, line, column, "%s", refresh_error_message);
            }
        }
        return 0;
    }
    if (specialized_name && specialized_name[0] != '\0') {
        *io_callee_name = specialized_name;
        *io_callee_function = ir_program_find_function_mut(ctx->program, specialized_name);
        if (out_call_uses_function_value_specialization) {
            *out_call_uses_function_value_specialization = 1;
        }
    }
    return 1;
}

static int ir_try_append_specialized_function_value_capture_args(
    const AstProgram *program,
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t parameter_index,
    const AstExpression *arg_expr,
    int line,
    int column,
    const char *resolve_error_message,
    const char *missing_capture_error_message,
    const char *oom_error_message,
    IrValueRef **io_call_args,
    size_t *io_arg_count,
    int *out_applied) {
    const size_t *capture_local_ids = NULL;
    size_t capture_count = 0u;
    size_t capture_index;
    const AstExpression *arg_base = NULL;
    IrLowerBindingInfo binding_info;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!program || !ctx || !callee_external || !arg_expr ||
        !io_call_args || !io_arg_count) {
        return 0;
    }
    if (!callee_external->parameter_value_kinds ||
        parameter_index >= callee_external->parameter_count ||
        callee_external->parameter_value_kinds[parameter_index] != AST_PARAMETER_VALUE_FUNCTION) {
        return 1;
    }

    memset(&binding_info, 0, sizeof(binding_info));
    arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
    if (arg_base &&
        arg_base->kind == AST_EXPR_IDENTIFIER &&
        arg_base->as.identifier.name &&
        ir_lower_scope_lookup_binding(&ctx->scopes, arg_base->as.identifier.name, &binding_info) &&
        binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL &&
        binding_info.closure_capture_count > 0u &&
        binding_info.closure_capture_local_ids) {
        capture_local_ids = binding_info.closure_capture_local_ids;
        capture_count = binding_info.closure_capture_count;
    } else if (!ir_call_argument_resolve_extension_function_value_target(
            program,
            ctx,
            callee_external,
            parameter_index,
            arg_expr,
            &capture_local_ids,
            &capture_count,
            NULL)) {
        if (resolve_error_message && resolve_error_message[0] != '\0') {
            ir_set_error(ctx->error, line, column, "%s", resolve_error_message);
        }
        return 0;
    }
    if (capture_count == 0u) {
        if (out_applied) {
            *out_applied = 1;
        }
        return 1;
    }
    if (!capture_local_ids) {
        if (missing_capture_error_message && missing_capture_error_message[0] != '\0') {
            ir_set_error(ctx->error, line, column, "%s", missing_capture_error_message);
        }
        return 0;
    }

    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        IrValueRef *new_args =
            (IrValueRef *)realloc(*io_call_args, (*io_arg_count + 1u) * sizeof(IrValueRef));
        if (!new_args) {
            if (oom_error_message && oom_error_message[0] != '\0') {
                ir_set_error(ctx->error, line, column, "%s", oom_error_message);
            }
            return 0;
        }
        *io_call_args = new_args;
        (*io_call_args)[(*io_arg_count)++] = ir_value_local(capture_local_ids[capture_index]);
    }
    if (out_applied) {
        *out_applied = 1;
    }
    return 1;
}

static int ir_try_append_maybe_specialized_call_argument(
    const AstProgram *program,
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t parameter_index,
    const AstExpression *arg_expr,
    int call_uses_function_value_specialization,
    int line,
    int column,
    const char *specialized_resolve_error_message,
    const char *specialized_missing_capture_error_message,
    const char *specialized_oom_error_message,
    const char *ordinary_error_format,
    const char *ordinary_error_name,
    IrValueRef **io_call_args,
    size_t *io_arg_count) {
    int appended_specialized_capture_args = 0;

    if (!ctx || !callee_external || !arg_expr || !io_call_args || !io_arg_count) {
        return 0;
    }
    if (call_uses_function_value_specialization &&
        callee_external->parameter_value_kinds &&
        parameter_index < callee_external->parameter_count &&
        callee_external->parameter_value_kinds[parameter_index] == AST_PARAMETER_VALUE_FUNCTION) {
        if (!ir_try_append_specialized_function_value_capture_args(
                program,
                ctx,
                callee_external,
                parameter_index,
                arg_expr,
                line,
                column,
                specialized_resolve_error_message,
                specialized_missing_capture_error_message,
                specialized_oom_error_message,
                io_call_args,
                io_arg_count,
                &appended_specialized_capture_args)) {
            return 0;
        }
        if (appended_specialized_capture_args) {
            return 1;
        }
    }

    if (!ir_lower_append_call_argument(
            ctx,
            callee_external,
            parameter_index,
            arg_expr,
            io_call_args,
            io_arg_count)) {
        if (ctx->error && ctx->error->message[0] == '\0' &&
            ordinary_error_format && ordinary_error_format[0] != '\0') {
            ir_set_error(
                ctx->error,
                line,
                column,
                ordinary_error_format,
                parameter_index,
                ordinary_error_name ? ordinary_error_name : "<unknown>");
        }
        return 0;
    }
    return 1;
}

static int ir_finish_optional_returned_callable_dispatch(
    int applied_callable_dispatch,
    size_t *immediate_returned_closure_capture_local_ids,
    int *out_handled) {
    if (out_handled) {
        *out_handled = 0;
    }
    if (!applied_callable_dispatch) {
        return 1;
    }
    if (out_handled) {
        *out_handled = 1;
    }
    return ir_finish_returned_callable_capture_cleanup(
        immediate_returned_closure_capture_local_ids);
}

static int ir_finish_failed_callable_probe_with_default_diagnostic(
    IrLowerContext *ctx,
    int line,
    int column,
    const char *default_error_message) {
    if (ctx && ctx->error && ctx->error->message[0] == '\0' &&
        default_error_message && default_error_message[0] != '\0') {
        ir_set_error(ctx->error, line, column, "%s", default_error_message);
    }
    return 0;
}

static int ir_clear_call_lowering_probe_error(IrLowerContext *ctx) {
    if (ctx && ctx->error) {
        ctx->error->line = 0;
        ctx->error->column = 0;
        ctx->error->message[0] = '\0';
    }
    return 1;
}

static int ir_finish_call_lowering_probe_error_if_present(
    IrLowerContext *ctx,
    size_t *immediate_returned_closure_capture_local_ids) {
    if (ctx && ctx->error && ctx->error->message[0] != '\0') {
        free(immediate_returned_closure_capture_local_ids);
        return 0;
    }
    return 1;
}

static int ir_emit_function_value_specialization_branch_call(
    IrLowerContext *ctx,
    size_t block_id,
    size_t join_block_id,
    IrValueRef result_value,
    const char *specialized_name,
    const IrValueRef *call_args,
    size_t call_arg_count) {
    if (!ctx || !specialized_name || specialized_name[0] == '\0') {
        return 0;
    }
    ctx->block_id = block_id;
    ctx->has_block = 1;
    return ir_emit_call(
               ctx,
               result_value,
               specialized_name,
               call_args,
               call_arg_count) &&
        ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error);
}

static int ir_clear_returned_callable_capture_slots(size_t **owned_capture_local_ids) {
    if (owned_capture_local_ids) {
        free(*owned_capture_local_ids);
        *owned_capture_local_ids = NULL;
    }
    return 1;
}

static int ir_finish_returned_callable_capture_cleanup_applied(
    size_t **owned_capture_local_ids,
    int *out_applied) {
    if (!ir_clear_returned_callable_capture_slots(owned_capture_local_ids)) {
        return 0;
    }
    return ir_mark_callable_dispatch_applied(out_applied);
}

static int ir_finish_callable_dispatch_result(IrValueRef result_value,
    IrValueRef *out_value,
    int *out_applied) {
    if (!ir_finish_callable_bridge_result_value(result_value, out_value)) {
        return 0;
    }
    return ir_mark_callable_dispatch_applied(out_applied);
}

static int ir_finish_optional_callable_dispatch_result(int applied,
    IrValueRef result_value,
    IrValueRef *out_value,
    int *out_applied) {
    if (!applied) {
        return 1;
    }
    return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
}

static int ir_finish_optional_callable_dispatch_applied(int applied,
    int *out_applied) {
    if (!applied) {
        return 1;
    }
    return ir_mark_callable_dispatch_applied(out_applied);
}

static int ir_finish_void_or_value_callable_dispatch_result(int is_void_result,
    IrValueRef result_value,
    IrValueRef *out_value,
    int *out_applied) {
    return is_void_result
        ? ir_mark_callable_dispatch_applied(out_applied)
        : ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
}

static int ir_materialize_closure_callable_wrapper_env_addr(IrLowerContext *ctx,
    const size_t *capture_local_ids,
    size_t capture_count,
    IrValueRef *out_env_addr) {
    size_t env_first_local_id = (size_t)-1;
    size_t capture_index;
    IrValueRef env_addr;

    if (out_env_addr) {
        *out_env_addr = ir_value_immediate(0);
    }
    if (!ctx || !ctx->function || !out_env_addr ||
        capture_count == 0u || !capture_local_ids) {
        return 0;
    }

    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        char env_local_name[64];
        size_t local_id = (size_t)-1;

        snprintf(env_local_name, sizeof(env_local_name), "__closure_env_%zu", capture_index);
        if (!ir_function_append_local(
                ctx->function,
                env_local_name,
                0,
                &local_id,
                ctx->error)) {
            return 0;
        }
        if (capture_index == 0u) {
            env_first_local_id = local_id;
        }
    }

    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        if (!ir_emit_mov(
                ctx,
                ir_value_local(env_first_local_id + capture_index),
                ir_value_local(capture_local_ids[capture_index]))) {
            return 0;
        }
    }

    env_addr = ir_value_temp(ir_allocate_temp(ctx->function));
    if (!ir_emit_addr_local(ctx, env_addr, env_first_local_id)) {
        return 0;
    }

    *out_env_addr = env_addr;
    return 1;
}

static int ir_emit_two_target_wrapped_closure_callable_indirect_call(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    const size_t *capture_local_ids,
    size_t capture_count,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value) {
    const char *then_wrapper_name = NULL;
    const char *else_wrapper_name = NULL;
    IrValueRef env_addr = ir_value_immediate(0);
    size_t shape_id = (size_t)-1;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !ctx->program || !then_target_name || !else_target_name || !out_value ||
        capture_count == 0u || !capture_local_ids ||
        (arg_count > 0u && !args)) {
        return 0;
    }

    if (!ir_ensure_closure_callable_wrapper_function(
            ctx,
            then_target_name,
            capture_count,
            &then_wrapper_name) ||
        !ir_ensure_closure_callable_wrapper_function(
            ctx,
            else_target_name,
            capture_count,
            &else_wrapper_name) ||
        !ir_materialize_closure_callable_wrapper_env_addr(
            ctx,
            capture_local_ids,
            capture_count,
            &env_addr)) {
        return 0;
    }

    if (!ir_build_two_target_callable_shape_bridge_from_names(
            ctx,
            then_wrapper_name,
            else_wrapper_name,
            1u,
            "IR-LOWER-071: returned callable-wrapper dispatch bridge shape mismatch",
            &shape_id)) {
        return 0;
    }

    if (!ir_emit_two_target_callable_object_indirect_call_bridged(
            ctx,
            tag_value,
            then_tag_value,
            then_wrapper_name,
            else_wrapper_name,
            env_addr,
            env_addr,
            shape_id,
            args,
            arg_count,
            out_value)) {
        ir_set_error(ctx->error, 0, 0, "IR-INT-094: failed while lowering callable dispatch bridge");
        return 0;
    }
    return 1;
}

static int ir_try_emit_two_target_dynamic_closure_callable_indirect_call_with_args(
    IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    const size_t *capture_local_ids,
    size_t capture_count,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !then_target_name || !else_target_name || !out_value ||
        capture_count == 0u || !capture_local_ids ||
        (arg_count > 0u && !args)) {
        return 1;
    }

    if (!ir_emit_two_target_wrapped_closure_callable_indirect_call(
            ctx,
            tag_value,
            then_tag_value,
            then_target_name,
            else_target_name,
            capture_local_ids,
            capture_count,
            args,
            arg_count,
            out_value)) {
        return 0;
    }
    return ir_mark_callable_dispatch_applied(out_applied);
}

static int ir_try_emit_static_single_capture_closure_callable_indirect_call_with_args(
    IrLowerContext *ctx,
    const char *target_name,
    IrValueRef env_value,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    size_t shape_id;
    IrValueRef result_value;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !target_name || target_name[0] == '\0' || !out_value ||
        (arg_count > 0u && !args)) {
        return 1;
    }

    if (!ir_build_single_target_callable_shape_bridge_from_name(
            ctx,
            target_name,
            1u,
            arg_count,
            &shape_id,
            NULL)) {
        return 1;
    }

    if (!ir_emit_callable_object_indirect_call(
            ctx,
            target_name,
            env_value,
            shape_id,
            args,
            arg_count,
            &result_value)) {
        return 0;
    }
    return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
}

static int ir_try_emit_static_closure_callable_indirect_call_with_args(IrLowerContext *ctx,
    const char *target_name,
    const size_t *capture_local_ids,
    size_t capture_count,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !target_name || target_name[0] == '\0' || !out_value ||
        capture_count == 0u || !capture_local_ids ||
        (arg_count > 0u && !args)) {
        return 1;
    }

    {
        const char *wrapper_name = NULL;
        size_t shape_id = (size_t)-1;
        IrValueRef env_addr = ir_value_immediate(0);
        IrValueRef result_value;

        if (!ir_ensure_closure_callable_wrapper_function(
                ctx,
                target_name,
                capture_count,
                &wrapper_name) ||
            !ir_materialize_closure_callable_wrapper_env_addr(
                ctx,
                capture_local_ids,
                capture_count,
                &env_addr)) {
            return 0;
        }
        if (!ir_build_single_target_callable_shape_bridge_from_name(
                ctx,
                wrapper_name,
                1u,
                arg_count,
                &shape_id,
                NULL)) {
            return 1;
        }
        if (!ir_emit_callable_object_indirect_call(
                ctx,
                wrapper_name,
                env_addr,
                shape_id,
                args,
                arg_count,
                &result_value)) {
            return 0;
        }
        return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
    }
}

static int ir_try_emit_static_returned_closure_callable_indirect_call(IrLowerContext *ctx,
    const AstExpression *function_arg_expr,
    size_t non_function_arg_count,
    size_t non_function_arg_index,
    AstFunctionReturnType expected_return_type,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *returned_base_call_expr = NULL;
    const AstExternal *returned_producer_external = NULL;
    int returned_family_is_closure = 0;
    const char **returned_target_names = NULL;
    size_t returned_target_count = 0u;
    const char *returned_primary_target_name = NULL;
    IrLowerReturnedFunctionObjectResolution returned_resolution;
    int have_returned_resolution = 0;
    int can_attempt_static_returned_dispatch = 0;
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    size_t callable_arg_count = 0u;
    IrValueRef result_value;
    int applied = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !function_arg_expr || !out_value) {
        return 1;
    }
    if (ctx->ast_program &&
        ir_resolve_returned_function_value_call_producer_info(
            ctx,
            function_arg_expr,
            &returned_base_call_expr,
            &returned_producer_external,
            &returned_family_is_closure) &&
        returned_base_call_expr &&
        returned_producer_external &&
        ir_resolve_returned_function_value_target_info(
            ctx,
            returned_base_call_expr,
            returned_producer_external,
            returned_family_is_closure,
            &returned_target_names,
            &returned_target_count,
            &returned_primary_target_name)) {
        if ((!returned_family_is_closure && returned_target_count != 1u) ||
            (returned_family_is_closure && returned_target_count > 1u) ||
            !returned_primary_target_name ||
            returned_primary_target_name[0] == '\0') {
            free((void *)returned_target_names);
            return 1;
        }
        can_attempt_static_returned_dispatch = 1;
        free((void *)returned_target_names);
    }
    if (!can_attempt_static_returned_dispatch) {
        return 1;
    }
    if (!ir_try_resolve_returned_function_object_resolution(
            ctx,
            function_arg_expr,
            "__retfn_argslot",
            "__retclosure_argslot",
            &returned_resolution)) {
        return 0;
    }
    have_returned_resolution = 1;
    if (!have_returned_resolution ||
        !returned_resolution.is_closure_family ||
        returned_resolution.is_dynamic_two_target ||
        !returned_resolution.helper_name ||
        returned_resolution.helper_name[0] == '\0' ||
        !returned_resolution.descriptor.capture_local_ids ||
        returned_resolution.descriptor.capture_count == 0u) {
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        ir_clear_returned_callable_capture_slots(
            &returned_resolution.descriptor.owned_payload_local_ids);
        return 1;
    }
    if (!ir_build_dynamic_indexed_callable_args_view(
            ctx,
            call_expr,
            non_function_arg_count,
            non_function_arg_index,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        ir_clear_returned_callable_capture_slots(
            &returned_resolution.descriptor.owned_payload_local_ids);
        return 0;
    }
    if (!ir_try_emit_static_closure_callable_indirect_call_with_args(
            ctx,
            returned_resolution.helper_name,
            returned_resolution.descriptor.capture_local_ids,
            returned_resolution.descriptor.capture_count,
            call_args_view,
            callable_arg_count,
            &result_value,
            &applied)) {
        free(call_args);
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        ir_clear_returned_callable_capture_slots(
            &returned_resolution.descriptor.owned_payload_local_ids);
        return 0;
    }
    free(call_args);
    free((void *)returned_resolution.descriptor.dynamic_target_names);
    ir_clear_returned_callable_capture_slots(
        &returned_resolution.descriptor.owned_payload_local_ids);
    if (!applied) {
        return 1;
    }
    return ir_finish_void_or_value_callable_dispatch_result(
        expected_return_type == AST_FUNCTION_RETURN_VOID,
        result_value,
        out_value,
        out_applied);
}

static int ir_try_emit_static_returned_single_capture_closure_callable_indirect_call(IrLowerContext *ctx,
    const AstExpression *function_arg_expr,
    size_t non_function_arg_count,
    size_t non_function_arg_index,
    AstFunctionReturnType expected_return_type,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    return ir_try_emit_static_returned_closure_callable_indirect_call(
        ctx,
        function_arg_expr,
        non_function_arg_count,
        non_function_arg_index,
        expected_return_type,
        call_expr,
        out_value,
        out_applied);
}

static int ir_try_emit_dynamic_single_capture_closure_callable_indirect_call(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    IrFunction *then_target_function = NULL;
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    size_t callable_arg_count = 0u;
    IrValueRef result_value;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !then_target_name || !else_target_name || !call_expr || !out_value) {
        return 1;
    }
    if (capture_count != 1u || !capture_local_ids) {
        return 1;
    }
    if (!ctx->program || !ctx->function) {
        return 1;
    }
    then_target_function = ir_program_find_function_mut(ctx->program, then_target_name);
    if (!then_target_function || then_target_function->parameter_count == 0u) {
        return 1;
    }
    if (!ir_build_dynamic_visible_callable_args_view(
            ctx,
            call_expr,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        return 0;
    }
    if (then_target_function->parameter_count - 1u != callable_arg_count) {
        free(call_args);
        return 1;
    }
    result_value = ir_value_immediate(0);
    if (!ir_emit_two_target_wrapped_closure_callable_indirect_call(
            ctx,
            tag_value,
            then_tag_value,
            then_target_name,
            else_target_name,
            capture_local_ids,
            capture_count,
            call_args_view,
            callable_arg_count,
            &result_value)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    return ir_finish_void_or_value_callable_dispatch_result(
        then_target_function->return_type == AST_FUNCTION_RETURN_VOID,
        result_value,
        out_value,
        out_applied);
}

static int ir_emit_two_target_dynamic_closure_dispatch_call(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    const char *oom_error_message,
    const char *missing_capture_error_message,
    IrValueRef *out_value) {
    IrValueRef *dynamic_args = NULL;
    IrValueRef result_value;
    size_t expected_arg_count;
    size_t arg_index;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !then_target_name || !else_target_name || !call_expr || !out_value) {
        return 0;
    }

    expected_arg_count = capture_count + call_expr->as.call.arg_count;
    if (expected_arg_count > 0u) {
        dynamic_args = (IrValueRef *)malloc(expected_arg_count * sizeof(IrValueRef));
        if (!dynamic_args) {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "%s",
                oom_error_message ? oom_error_message
                                  : "IR-INT-027: out of memory while lowering dynamic closure call arguments");
            return 0;
        }
    }

    for (arg_index = 0u; arg_index < capture_count; ++arg_index) {
        if (!capture_local_ids) {
            free(dynamic_args);
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "%s",
                missing_capture_error_message
                    ? missing_capture_error_message
                    : "IR-LOWER-058: dynamic closure call is missing captured-value lowering metadata");
            return 0;
        }
        dynamic_args[arg_index] = ir_value_local(capture_local_ids[arg_index]);
    }
    for (arg_index = 0u; arg_index < call_expr->as.call.arg_count; ++arg_index) {
        if (!ir_lower_expression(
                ctx,
                call_expr->as.call.args[arg_index],
                &dynamic_args[capture_count + arg_index])) {
            free(dynamic_args);
            return 0;
        }
    }

    if (!ir_emit_two_target_dispatch_call(
            ctx,
            tag_value,
            then_tag_value,
            then_target_name,
            else_target_name,
            dynamic_args,
            expected_arg_count,
            &result_value)) {
        free(dynamic_args);
        return 0;
    }

    free(dynamic_args);
    return ir_finish_callable_bridge_result_value(result_value, out_value);
}

static int ir_try_emit_dynamic_closure_callable_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    const char *oom_error_message,
    const char *missing_capture_error_message,
    IrValueRef *out_value,
    int *out_applied) {
    IrValueRef tag_value;
    IrValueRef result_value;
    int applied_callable_indirect = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_view || !call_expr || !out_value) {
        return 1;
    }
    if (!callee_view->is_closure_family ||
        !ir_callable_object_view_is_two_target_dynamic_family(callee_view)) {
        return 1;
    }

    if (!ir_try_emit_two_target_callable_object_indirect_dispatch_from_call_expr(
            ctx,
            callee_view,
            call_expr,
            out_value,
            &applied_callable_indirect)) {
        return 0;
    }
    if (applied_callable_indirect) {
        return ir_mark_callable_dispatch_applied(out_applied);
    }

    tag_value = ir_value_local(callee_view->dynamic_tag_local_id);
    if (!ir_emit_two_target_dynamic_closure_dispatch_call(
            ctx,
            tag_value,
            1,
            callee_view->dynamic_target_names[0],
            callee_view->dynamic_target_names[1],
            callee_view->capture_local_ids,
            callee_view->capture_count,
            call_expr,
            oom_error_message,
            missing_capture_error_message,
            &result_value)) {
        return 0;
    }

    return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
}

static int ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !callee_view || !out_value || (arg_count > 0u && !args) ||
        !ir_callable_object_view_is_two_target_dynamic_family(callee_view)) {
        return 1;
    }

    if (!callee_view->is_closure_family && callee_view->capture_count == 0u) {
        if (!ir_emit_two_target_noncapturing_callable_indirect_call(
                ctx,
                ir_value_local(callee_view->dynamic_tag_local_id),
                1,
                callee_view->dynamic_target_names[0],
                callee_view->dynamic_target_names[1],
                args,
                arg_count,
                out_value)) {
            return 0;
        }
        return ir_mark_callable_dispatch_applied(out_applied);
    }

    if (callee_view->is_closure_family &&
        callee_view->capture_count > 0u &&
        callee_view->capture_local_ids) {
        if (!ir_try_emit_two_target_dynamic_closure_callable_indirect_call_with_args(
                ctx,
                ir_value_local(callee_view->dynamic_tag_local_id),
                1,
                callee_view->dynamic_target_names[0],
                callee_view->dynamic_target_names[1],
                callee_view->capture_local_ids,
                callee_view->capture_count,
                args,
                arg_count,
                out_value,
                out_applied)) {
            return 0;
        }
        return 1;
    }

    return 1;
}

static int ir_try_emit_two_target_callable_object_indirect_dispatch_from_call_expr(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    size_t callable_arg_count = 0u;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_view || !call_expr || !out_value) {
        return 1;
    }
    if (call_expr->kind != AST_EXPR_CALL) {
        return 1;
    }
    if (!ir_build_dynamic_visible_callable_args_view(
            ctx,
            call_expr,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        return 0;
    }
    {
        int result = ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
            ctx,
            callee_view,
            call_args_view,
            callable_arg_count,
            out_value,
            out_applied);
        free(call_args);
        return result;
    }
}

static int ir_try_emit_dynamic_callable_object_dispatch_from_call_expr(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    const char *oom_error_message,
    const char *missing_capture_error_message,
    IrValueRef *out_value,
    int *out_applied) {
    int applied_dispatch = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_view || !call_expr || !out_value) {
        return 1;
    }
    if (!callee_view->has_dynamic_family) {
        return 1;
    }

    if (!ir_try_emit_two_target_callable_object_indirect_dispatch_from_call_expr(
            ctx,
            callee_view,
            call_expr,
            out_value,
            &applied_dispatch)) {
        return 0;
    }
    if (!applied_dispatch) {
        if (callee_view->is_closure_family) {
            if (!ir_try_emit_dynamic_closure_callable_dispatch(
                    ctx,
                    callee_view,
                    call_expr,
                    oom_error_message,
                    missing_capture_error_message,
                    out_value,
                    &applied_dispatch)) {
                return 0;
            }
        } else {
            if (!ir_try_emit_dynamic_noncapturing_callable_dispatch(
                    ctx,
                    callee_view,
                    call_expr,
                    out_value,
                    &applied_dispatch)) {
                return 0;
            }
        }
    }

    return ir_finish_optional_callable_dispatch_applied(
        applied_dispatch,
        out_applied);
}

static int ir_try_emit_dynamic_noncapturing_callable_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    int applied_indirect_dispatch = 0;
    IrValueRef call_args[1];
    const IrValueRef *call_args_view = NULL;
    size_t callable_arg_count = 0u;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_view || !call_expr || !out_value) {
        return 1;
    }
    if (callee_view->is_closure_family ||
        !ir_callable_object_view_is_two_target_dynamic_family(callee_view)) {
        return 1;
    }
    if (!ir_build_required_single_visible_callable_args_view(
            ctx,
            call_expr,
            "IR-LOWER-064: first dynamic function-value dispatch slice currently supports one-argument noncapturing calls only",
            call_args,
            &call_args_view,
            &callable_arg_count)) {
        return 0;
    }
    if (!ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
            ctx,
            callee_view,
            call_args_view,
            callable_arg_count,
            out_value,
            &applied_indirect_dispatch)) {
        return 0;
    }
    return ir_finish_optional_callable_dispatch_applied(
        applied_indirect_dispatch,
        out_applied);
}

static int ir_try_emit_direct_callee_callable_object_dynamic_dispatch(IrLowerContext *ctx,
    IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    int *io_callee_is_closure_local,
    const size_t **io_closure_capture_local_ids,
    size_t *io_closure_prefix_arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExternal *callee_external_a = NULL;
    const AstExternal *callee_external_b = NULL;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_view || !call_expr || !out_value) {
        return 1;
    }
    if (!callee_view->direct_target_name || callee_view->direct_target_name[0] == '\0') {
        return 1;
    }

    if (callee_view->is_closure_family) {
        if (io_callee_is_closure_local) {
            *io_callee_is_closure_local = 1;
        }
        if (io_closure_capture_local_ids) {
            *io_closure_capture_local_ids = callee_view->capture_local_ids;
        }
        if (io_closure_prefix_arg_count) {
            *io_closure_prefix_arg_count = callee_view->capture_count;
        }
    }
    if (ir_callable_object_view_is_two_target_dynamic_family(callee_view) &&
        ctx->ast_program &&
        callee_view->dynamic_target_names &&
        callee_view->dynamic_target_names[0] &&
        callee_view->dynamic_target_names[1]) {
        callee_external_a = ir_find_named_function_external_prefer_definition(
            ctx->ast_program,
            callee_view->dynamic_target_names[0]);
        callee_external_b = ir_find_named_function_external_prefer_definition(
            ctx->ast_program,
            callee_view->dynamic_target_names[1]);
        if (!callee_external_a) {
            callee_external_a =
                ir_program_find_function_external_ref(
                    ctx->program,
                    callee_view->dynamic_target_names[0]);
        }
        if (!callee_external_b) {
            callee_external_b =
                ir_program_find_function_external_ref(
                    ctx->program,
                    callee_view->dynamic_target_names[1]);
        }
        if (callee_external_a &&
            callee_external_b &&
            callee_external_a->parameter_count ==
                call_expr->as.call.arg_count +
                    (callee_view->is_closure_family ? callee_view->capture_count : 0u) &&
            callee_external_b->parameter_count ==
                call_expr->as.call.arg_count +
                    (callee_view->is_closure_family ? callee_view->capture_count : 0u) &&
            callee_external_a->parameter_value_kinds &&
            callee_external_b->parameter_value_kinds &&
            ir_external_body_needs_function_value_specialization(
                ctx->ast_program,
                callee_external_a,
                callee_external_a->function_body) &&
            ir_external_body_needs_function_value_specialization(
                ctx->ast_program,
                callee_external_b,
                callee_external_b->function_body)) {
            return ir_try_emit_direct_dynamic_function_value_specialized_call(
                ctx,
                call_expr,
                callee_external_a,
                callee_external_b,
                callee_view,
                out_value,
                out_applied);
        }
    }
    return ir_try_emit_dynamic_callable_object_dispatch_from_call_expr(
        ctx,
        callee_view,
        call_expr,
        "IR-INT-027: out of memory while lowering dynamic closure call arguments",
        "IR-LOWER-058: dynamic closure call is missing captured-value lowering metadata",
        out_value,
        out_applied);
}

static int ir_try_lower_plain_direct_call(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *callee_base = NULL;
    const char *callee_name = NULL;
    const AstExternal *callee_external = NULL;
    IrFunction *callee_function = NULL;
    IrValueRef *call_args = NULL;
    IrLowerBindingInfo binding_info;
    size_t arg_count = 0u;
    size_t i;
    size_t temp_id;
    const char *current_function_name = NULL;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !ctx->program || !call_expr || !out_value || call_expr->kind != AST_EXPR_CALL) {
        return 1;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;

    callee_base = ir_unwrap_paren_expression(call_expr->as.call.callee);
    callee_name = ir_extract_direct_call_callee_name(call_expr->as.call.callee);
    if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_name || callee_name[0] == '\0') {
        return 1;
    }
    memset(&binding_info, 0, sizeof(binding_info));
        if (ir_lower_scope_lookup_binding(&ctx->scopes, callee_name, &binding_info)) {
            int applied_static_binding_call = 0;
            int applied_dynamic_binding_call = 0;
            IrLowerCallableObjectView callee_view;

        if (!binding_info.type_name || binding_info.type_name[0] == '\0' ||
            (binding_info.kind != IR_LOWER_BINDING_FUNCTION_LOCAL &&
                binding_info.kind != IR_LOWER_BINDING_CLOSURE_LOCAL)) {
            return 1;
        }
        if (binding_info.function_target_tag_local_id != (size_t)-1 &&
            !binding_info.has_const_value) {
            memset(&callee_view, 0, sizeof(callee_view));
            if (!ir_resolve_any_callable_object_view(ctx, call_expr->as.call.callee, &callee_view) ||
                !callee_view.direct_target_name || callee_view.direct_target_name[0] == '\0') {
                return 1;
            }
            if (!ir_try_emit_direct_callee_callable_object_dynamic_dispatch(
                    ctx,
                    &callee_view,
                    call_expr,
                    NULL,
                    NULL,
                    NULL,
                    out_value,
                    &applied_dynamic_binding_call)) {
                return 0;
            }
            if (applied_dynamic_binding_call) {
                return ir_mark_callable_dispatch_applied(out_applied);
            }
            return 1;
        }
        if (binding_info.function_target_tag_local_id != (size_t)-1 &&
            binding_info.has_const_value &&
            binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL) {
            const IrFunctionValueTargetSet *target_set =
                ir_lower_find_function_value_target_set(ctx, callee_name);

            if (target_set &&
                target_set->target_count > 1u) {
                memset(&callee_view, 0, sizeof(callee_view));
                if (!ir_resolve_any_callable_object_view(ctx, call_expr->as.call.callee, &callee_view) ||
                    !callee_view.direct_target_name || callee_view.direct_target_name[0] == '\0') {
                    return 1;
                }
                if (!ir_try_emit_direct_callee_callable_object_dynamic_dispatch(
                        ctx,
                        &callee_view,
                        call_expr,
                        NULL,
                        NULL,
                        NULL,
                        out_value,
                        &applied_dynamic_binding_call)) {
                    return 0;
                }
                if (applied_dynamic_binding_call) {
                    return ir_mark_callable_dispatch_applied(out_applied);
                }
            }
        }
        if (!ir_try_emit_static_direct_target_callable_dispatch(
                ctx,
                binding_info.type_name,
                binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL,
                binding_info.closure_capture_local_ids,
                binding_info.closure_capture_count,
                call_expr,
                out_value,
                &applied_static_binding_call)) {
            return 0;
        }
        if (applied_static_binding_call) {
            return ir_mark_callable_dispatch_applied(out_applied);
        }
        return 1;
    }
    {
        const char *bound_target_name = ir_lower_lookup_function_value_target(ctx, callee_name);
        const size_t *bound_capture_local_ids = NULL;
        size_t bound_capture_count = 0u;
        int applied_bound_static_dispatch = 0;

        if (bound_target_name && bound_target_name[0] != '\0') {
            (void)ir_lower_lookup_function_value_capture_binding(
                ctx,
                callee_name,
                &bound_capture_local_ids,
                &bound_capture_count);
            if (!ir_try_emit_static_direct_target_callable_dispatch(
                    ctx,
                    bound_target_name,
                    bound_capture_count > 0u,
                    bound_capture_local_ids,
                    bound_capture_count,
                    call_expr,
                    out_value,
                    &applied_bound_static_dispatch)) {
                return 0;
            }
            if (applied_bound_static_dispatch) {
                return ir_mark_callable_dispatch_applied(out_applied);
            }
            return 1;
        }
    }

    callee_function = ir_program_find_function_mut(ctx->program, callee_name);
    if (!callee_function &&
        !ir_ensure_builtin_function_signature(ctx->program, callee_name, &callee_function, ctx->error)) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-LOWER-025: call target '%s' is not declared in IR lowering context",
            callee_name);
        return 0;
    }
    if (!ir_lower_refresh_current_function(ctx, current_function_name, call_expr->line, call_expr->column)) {
        return 0;
    }
    if (!callee_function) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-LOWER-025: call target '%s' is not declared in IR lowering context",
            callee_name);
        return 0;
    }

    if (ctx->ast_program) {
        for (i = 0u; i < ctx->ast_program->count; ++i) {
            const AstExternal *candidate = &ctx->ast_program->externals[i];
            if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                candidate->name &&
                strcmp(candidate->name, callee_name) == 0) {
                callee_external = candidate;
                break;
            }
        }
    }

    if (callee_external) {
        size_t function_value_param_count = 0u;

        if (callee_external->parameter_value_kinds) {
            for (i = 0u; i < callee_external->parameter_count; ++i) {
                if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
                    function_value_param_count += 1u;
                }
            }
        }

        /* Let the full callable-object / specialization path handle any
           function-valued surface instead of prematurely freezing it into a
           plain direct call with erased zero placeholders. */
        if (function_value_param_count > 0u ||
            callee_external->has_function_return_signature) {
            return 1;
        }
    }

    if (!ir_try_emit_passthrough_callable_dispatch_from_function_args(
            ctx,
            call_expr,
            callee_external,
            out_value,
            out_applied)) {
        return 0;
    }
    if (out_applied && *out_applied) {
        return 1;
    }

    arg_count = call_expr->as.call.arg_count;
    if (callee_external && callee_external->kind == AST_EXTERNAL_FUNCTION) {
        size_t lowered_arg_count = 0u;

        if (call_expr->as.call.arg_count != callee_external->parameter_count) {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-026: call to '%s' has %zu args but callee source signature expects %zu",
                callee_name,
                call_expr->as.call.arg_count,
                callee_external->parameter_count);
            return 0;
        }
        for (i = 0u; i < call_expr->as.call.arg_count; ++i) {
            if (!ir_lower_append_call_argument(
                    ctx,
                    callee_external,
                    i,
                    call_expr->as.call.args[i],
                    &call_args,
                    &lowered_arg_count)) {
                free(call_args);
                return 0;
            }
        }
        arg_count = lowered_arg_count;
    } else {
        if (arg_count != callee_function->parameter_count) {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-026: call to '%s' has %zu args but callee signature expects %zu",
                callee_name,
                arg_count,
                callee_function->parameter_count);
            return 0;
        }
        if (arg_count > 0u) {
            call_args = (IrValueRef *)malloc(arg_count * sizeof(IrValueRef));
            if (!call_args) {
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-INT-027: out of memory while lowering plain direct call arguments");
                return 0;
            }
        }

        for (i = 0u; i < arg_count; ++i) {
            if (!call_expr->as.call.args ||
                !ir_lower_expression(ctx, call_expr->as.call.args[i], &call_args[i])) {
                free(call_args);
                return 0;
            }
        }
    }

    if (arg_count != callee_function->parameter_count) {
        free(call_args);
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-LOWER-026: call to '%s' lowers to %zu args but callee signature expects %zu",
            callee_name,
            arg_count,
            callee_function->parameter_count);
        return 0;
    }

    if (callee_function->return_type == AST_FUNCTION_RETURN_VOID) {
        IrValueRef ignored_result = ir_value_temp(ir_allocate_temp(ctx->function));

        *out_value = ir_value_immediate(0);
        if (!ir_emit_call(ctx, ignored_result, callee_name, call_args, arg_count)) {
            free(call_args);
            return 0;
        }

        free(call_args);
        return ir_mark_callable_dispatch_applied(out_applied);
    }

    temp_id = ir_allocate_temp(ctx->function);
    *out_value = ir_value_temp(temp_id);
    if (!ir_emit_call(ctx, *out_value, callee_name, call_args, arg_count)) {
        free(call_args);
        return 0;
    }

    free(call_args);
    return ir_mark_callable_dispatch_applied(out_applied);
}

static int ir_try_emit_ternary_function_value_argument_dispatch(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    size_t non_function_arg_index,
    size_t non_function_arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *function_arg_base = NULL;
    IrLowerFunctionValueTernaryInfo ternary_info;
    IrValueRef *forwarded_scalar_args = NULL;
    const IrValueRef *forwarded_scalar_args_view = NULL;
    size_t forwarded_scalar_arg_count = 0u;
    IrValueRef ignored_result = ir_value_immediate(0);
    IrValueRef *call_result_target = NULL;
    size_t tag_local_id = (size_t)-1;
    long long then_tag_value = 0;
    long long else_tag_value = 0;
    size_t *payload_local_ids = NULL;
    size_t capture_index;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !call_expr || !callee_external || !out_value || function_arg_index >= call_expr->as.call.arg_count) {
        return 1;
    }

    function_arg_base =
        ir_unwrap_paren_expression(
            call_expr->as.call.args[function_arg_index]);
    if (!function_arg_base || function_arg_base->kind != AST_EXPR_TERNARY) {
        return 1;
    }
    if (!ir_resolve_identifier_ternary_function_value_info(ctx, function_arg_base, &ternary_info) ||
        !ternary_info.then_target_name ||
        !ternary_info.else_target_name ||
        ternary_info.then_capture_count != ternary_info.else_capture_count ||
        ternary_info.then_is_closure_local != ternary_info.else_is_closure_local ||
        !ir_function_append_local(
            ctx->function,
            "__ternary_fn_argtag",
            0,
            &tag_local_id,
            ctx->error) ||
        !ir_lower_register_function_value_target(
            ctx,
            "__ternary_fn_arg",
            ternary_info.then_target_name,
            &then_tag_value) ||
        !ir_lower_register_function_value_target(
            ctx,
            "__ternary_fn_arg",
            ternary_info.else_target_name,
            &else_tag_value)) {
        return 1;
    }

    if (ternary_info.then_capture_count > 0u) {
        payload_local_ids = (size_t *)malloc(ternary_info.then_capture_count * sizeof(size_t));
        if (!payload_local_ids) {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-029: out of memory while preparing ternary function-value actual-argument payload");
            return 0;
        }
        for (capture_index = 0u; capture_index < ternary_info.then_capture_count; ++capture_index) {
            char payload_local_name[256];
            snprintf(payload_local_name,
                sizeof(payload_local_name),
                "__ternary_fn_argcap_%zu",
                capture_index);
            if (!ir_function_append_local(
                    ctx->function,
                    payload_local_name,
                    0,
                    &payload_local_ids[capture_index],
                    ctx->error)) {
                free(payload_local_ids);
                return 0;
            }
        }
    }

    if (!ir_emit_materialized_ternary_function_value_selection(
            ctx,
            function_arg_base->as.ternary.condition,
            then_tag_value,
            else_tag_value,
            ternary_info.then_tag_local_id,
            ternary_info.else_tag_local_id,
            ternary_info.then_has_const_value,
            ternary_info.else_has_const_value,
            ternary_info.then_capture_local_ids,
            ternary_info.else_capture_local_ids,
            ternary_info.then_capture_count,
            tag_local_id,
            payload_local_ids)) {
        free(payload_local_ids);
        return 0;
    }

    call_result_target = callee_external->function_return_type == AST_FUNCTION_RETURN_VOID
        ? &ignored_result
        : out_value;
    {
        if (!ir_build_dynamic_indexed_callable_args_view(
                ctx,
                call_expr,
                non_function_arg_count,
                non_function_arg_index,
                "IR-INT-126: out of memory while lowering callable-object arguments",
                &forwarded_scalar_args,
                &forwarded_scalar_args_view,
                &forwarded_scalar_arg_count)) {
            return 0;
        }

        if (ternary_info.then_capture_count == 0u) {
            if (!ir_emit_two_target_noncapturing_callable_indirect_call(
                    ctx,
                    ir_value_local(tag_local_id),
                    then_tag_value,
                    ternary_info.then_target_name,
                    ternary_info.else_target_name,
                    forwarded_scalar_args_view,
                    forwarded_scalar_arg_count,
                    call_result_target)) {
                free(forwarded_scalar_args);
                free(payload_local_ids);
                return 0;
            }
            free(forwarded_scalar_args);
            free(payload_local_ids);
            return ir_mark_callable_dispatch_applied(out_applied);
        }

        if (ternary_info.then_capture_count == 1u &&
            payload_local_ids) {
            if (!ir_emit_two_target_wrapped_closure_callable_indirect_call(
                    ctx,
                    ir_value_local(tag_local_id),
                    then_tag_value,
                    ternary_info.then_target_name,
                    ternary_info.else_target_name,
                    payload_local_ids,
                    ternary_info.then_capture_count,
                    forwarded_scalar_args_view,
                    forwarded_scalar_arg_count,
                    call_result_target)) {
                free(forwarded_scalar_args);
                free(payload_local_ids);
                return 0;
            }
            free(forwarded_scalar_args);
            free(payload_local_ids);
            return ir_mark_callable_dispatch_applied(out_applied);
        }
        free(forwarded_scalar_args);
    }

    free(payload_local_ids);
    return 1;
}

static int ir_try_emit_static_single_capture_closure_callable_indirect_call(IrLowerContext *ctx,
    const char *target_name,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    size_t callable_arg_count = 0u;
    IrValueRef result_value;
    int applied = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !ctx->program || !ctx->function || !target_name || !call_expr || !out_value) {
        return 1;
    }
    if (capture_count != 1u || !capture_local_ids) {
        return 1;
    }
    if (!ir_build_dynamic_visible_callable_args_view(
            ctx,
            call_expr,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        return 0;
    }
    if (!ir_try_emit_static_closure_callable_indirect_call_with_args(
            ctx,
            target_name,
            capture_local_ids,
            capture_count,
            call_args_view,
            callable_arg_count,
            &result_value,
            &applied)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    return ir_finish_optional_callable_dispatch_result(
        applied,
        result_value,
        out_value,
        out_applied);
}

static int ir_build_noncapturing_callable_wrapper_name(const char *target_name,
    char **out_name,
    IrError *error) {
    const char *prefix = "__fnwrap_";
    size_t prefix_length;
    size_t target_length;
    char *name;

    if (out_name) {
        *out_name = NULL;
    }
    if (!target_name || target_name[0] == '\0' || !out_name) {
        ir_set_error(error, 0, 0, "IR-INT-048: invalid noncapturing callable-wrapper name contract");
        return 0;
    }

    prefix_length = strlen(prefix);
    target_length = strlen(target_name);
    name = (char *)malloc(prefix_length + target_length + 1u);
    if (!name) {
        ir_set_error(error, 0, 0, "IR-INT-049: out of memory while building callable-wrapper name");
        return 0;
    }

    memcpy(name, prefix, prefix_length);
    memcpy(name + prefix_length, target_name, target_length + 1u);
    *out_name = name;
    return 1;
}

static int ir_ensure_noncapturing_callable_wrapper_function(IrLowerContext *ctx,
    const char *target_name,
    const char **out_wrapper_name) {
    char *wrapper_name = NULL;
    const char *current_function_name = NULL;
    IrFunction *target_function = NULL;
    IrFunction *wrapper_function = NULL;
    IrBasicBlock *wrapper_block = NULL;
    IrLowerContext wrapper_ctx;
    IrValueRef *forward_args = NULL;
    IrValueRef call_result;
    size_t param_index;
    size_t target_parameter_count = 0u;
    AstFunctionReturnType target_return_type;
    AstFunctionReturnType *target_parameter_types = NULL;
    const char **target_parameter_names = NULL;

    if (out_wrapper_name) {
        *out_wrapper_name = NULL;
    }
    if (!ctx || !ctx->program || !target_name || target_name[0] == '\0') {
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;

    target_function = ir_program_find_function_mut(ctx->program, target_name);
    if (!target_function) {
        const AstExternal *target_external =
            ir_find_named_function_external_prefer_definition(ctx->ast_program, target_name);

        if (target_external) {
            if (!ir_ensure_function_signature(
                    ctx->ast_program,
                    ctx->program,
                    target_external,
                    &target_function,
                    ctx->error)) {
                return 0;
            }
        } else if (!ir_ensure_builtin_function_signature(
                ctx->program,
                target_name,
                &target_function,
                ctx->error)) {
            return 0;
        }
    }

    if (!target_function ||
        target_function->parameter_count > target_function->local_count) {
        ir_set_error(ctx->error,
            0,
            0,
            "IR-LOWER-069: callable-wrapper bridge currently supports only scalar int/float parameters with explicit parameter locals");
        return 0;
    }

    for (param_index = 0u; param_index < target_function->parameter_count; ++param_index) {
        const IrLocal *parameter_local = ir_function_get_local(target_function, param_index);

        if (!parameter_local ||
            (parameter_local->value_type != AST_FUNCTION_RETURN_INT &&
                parameter_local->value_type != AST_FUNCTION_RETURN_FLOAT)) {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-LOWER-070: callable-wrapper bridge currently supports only scalar int/float parameters");
            return 0;
        }
    }

    target_return_type = target_function->return_type;
    target_parameter_count = target_function->parameter_count;
    if (target_parameter_count > 0u) {
        target_parameter_types =
            (AstFunctionReturnType *)malloc(target_parameter_count * sizeof(AstFunctionReturnType));
        target_parameter_names =
            (const char **)malloc(target_parameter_count * sizeof(const char *));
        if (!target_parameter_types || !target_parameter_names) {
            free(target_parameter_types);
            free(target_parameter_names);
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-124: out of memory while snapshotting callable-wrapper target signature");
            return 0;
        }
        for (param_index = 0u; param_index < target_parameter_count; ++param_index) {
            const IrLocal *target_local = ir_function_get_local(target_function, param_index);
            target_parameter_types[param_index] =
                target_local ? target_local->value_type : AST_FUNCTION_RETURN_INT;
            target_parameter_names[param_index] =
                (target_local && target_local->source_name && target_local->source_name[0] != '\0')
                    ? target_local->source_name
                    : "arg";
        }
    }

    if (!ir_build_noncapturing_callable_wrapper_name(target_name, &wrapper_name, ctx->error)) {
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }

    wrapper_function = ir_program_find_function_mut(ctx->program, wrapper_name);
    if (wrapper_function) {
        if (!ir_lower_refresh_current_function(ctx,
                current_function_name,
                0,
                0)) {
            free(wrapper_name);
            free(target_parameter_types);
            free(target_parameter_names);
            return 0;
        }
        if (out_wrapper_name) {
            *out_wrapper_name = wrapper_function->name;
        }
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 1;
    }

    if (!ir_program_append_function(ctx->program, wrapper_name, &wrapper_function, ctx->error)) {
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }
    if (!ir_lower_refresh_current_function(ctx,
            current_function_name,
            0,
            0)) {
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }
    wrapper_function->has_body = 1;
    wrapper_function->return_type = target_return_type;

    if (!ir_function_append_local(wrapper_function, "__env", 1, NULL, ctx->error)) {
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }
    wrapper_function->locals[0].value_type = AST_FUNCTION_RETURN_INT;

    for (param_index = 0u; param_index < target_parameter_count; ++param_index) {
        if (!ir_function_append_local(wrapper_function,
                target_parameter_names ? target_parameter_names[param_index] : "arg",
                1,
                NULL,
                ctx->error)) {
            free(wrapper_name);
            free(target_parameter_types);
            free(target_parameter_names);
            return 0;
        }
        wrapper_function->locals[param_index + 1u].value_type =
            target_parameter_types ? target_parameter_types[param_index] : AST_FUNCTION_RETURN_INT;
    }

    if (!ir_function_append_block(wrapper_function, NULL, &wrapper_block, ctx->error)) {
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }

    memset(&wrapper_ctx, 0, sizeof(wrapper_ctx));
    wrapper_ctx.program = ctx->program;
    wrapper_ctx.function = wrapper_function;
    wrapper_ctx.function_return_type = wrapper_function->return_type;
    wrapper_ctx.block_id = wrapper_block->id;
    wrapper_ctx.has_block = 1;
    wrapper_ctx.error = ctx->error;

    if (target_parameter_count > 0u) {
        forward_args = (IrValueRef *)malloc(target_parameter_count * sizeof(IrValueRef));
        if (!forward_args) {
            free(wrapper_name);
            free(target_parameter_types);
            free(target_parameter_names);
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-050: out of memory while building callable-wrapper args");
            return 0;
        }
    }

    for (param_index = 0u; param_index < target_parameter_count; ++param_index) {
        forward_args[param_index] = ir_value_local(param_index + 1u);
    }

    call_result = ir_value_temp(ir_allocate_temp(wrapper_function));
    if (!ir_emit_call(
            &wrapper_ctx,
            call_result,
            target_name,
            forward_args,
            target_parameter_count)) {
        free(forward_args);
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }
    if (target_return_type == AST_FUNCTION_RETURN_VOID) {
        if (!ir_block_set_void_return(wrapper_block, ctx->error)) {
            free(forward_args);
            free(wrapper_name);
            free(target_parameter_types);
            free(target_parameter_names);
            return 0;
        }
    } else if (!ir_block_set_return(wrapper_block, call_result, ctx->error)) {
        free(forward_args);
        free(wrapper_name);
        free(target_parameter_types);
        free(target_parameter_names);
        return 0;
    }

    free(forward_args);
    free(target_parameter_types);
    free(target_parameter_names);
    if (!ir_lower_refresh_current_function(ctx,
            current_function_name,
            0,
            0)) {
        free(wrapper_name);
        return 0;
    }
    if (out_wrapper_name) {
        *out_wrapper_name = wrapper_function->name;
    }
    free(wrapper_name);
    return 1;
}

static int ir_ensure_closure_callable_wrapper_function(IrLowerContext *ctx,
    const char *target_name,
    size_t capture_count,
    const char **out_wrapper_name) {
    char *wrapper_name = NULL;
    const char *current_function_name = NULL;
    IrFunction *target_function = NULL;
    IrFunction *wrapper_function = NULL;
    IrBasicBlock *wrapper_block = NULL;
    IrLowerContext wrapper_ctx;
    IrValueRef *forward_args = NULL;
    IrValueRef call_result;
    size_t param_index;
    size_t target_parameter_count = 0u;
    size_t visible_parameter_count = 0u;
    AstFunctionReturnType target_return_type;
    AstFunctionReturnType *visible_parameter_types = NULL;
    const char **visible_parameter_names = NULL;
    const char *prefix = "__fnwrap_closure_";
    size_t prefix_length;
    size_t target_length;

    if (out_wrapper_name) {
        *out_wrapper_name = NULL;
    }
    if (!ctx || !ctx->program || !target_name || target_name[0] == '\0' ||
        capture_count == 0u) {
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;

    target_function = ir_program_find_function_mut(ctx->program, target_name);
    if (!target_function ||
        target_function->parameter_count < capture_count ||
        target_function->parameter_count > target_function->local_count) {
        return 0;
    }

    target_parameter_count = target_function->parameter_count;
    visible_parameter_count = target_parameter_count - capture_count;
    target_return_type = target_function->return_type;

    if (visible_parameter_count > 0u) {
        visible_parameter_types =
            (AstFunctionReturnType *)malloc(visible_parameter_count * sizeof(AstFunctionReturnType));
        visible_parameter_names =
            (const char **)malloc(visible_parameter_count * sizeof(const char *));
        if (!visible_parameter_types || !visible_parameter_names) {
            free(visible_parameter_types);
            free(visible_parameter_names);
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-124: out of memory while snapshotting closure-wrapper target signature");
            return 0;
        }
        for (param_index = 0u; param_index < visible_parameter_count; ++param_index) {
            const IrLocal *target_local = ir_function_get_local(target_function, capture_count + param_index);
            if (!target_local ||
                (target_local->value_type != AST_FUNCTION_RETURN_INT &&
                    target_local->value_type != AST_FUNCTION_RETURN_FLOAT)) {
                free(visible_parameter_types);
                free(visible_parameter_names);
                return 0;
            }
            visible_parameter_types[param_index] = target_local->value_type;
            visible_parameter_names[param_index] =
                (target_local->source_name && target_local->source_name[0] != '\0')
                    ? target_local->source_name
                    : "arg";
        }
    }

    prefix_length = strlen(prefix);
    target_length = strlen(target_name);
    wrapper_name = (char *)malloc(prefix_length + target_length + 32u);
    if (!wrapper_name) {
        free(visible_parameter_types);
        free(visible_parameter_names);
        ir_set_error(ctx->error, 0, 0, "IR-INT-049: out of memory while building closure-wrapper name");
        return 0;
    }
    snprintf(wrapper_name,
        prefix_length + target_length + 32u,
        "%s%s_%zu",
        prefix,
        target_name,
        capture_count);

    wrapper_function = ir_program_find_function_mut(ctx->program, wrapper_name);
    if (wrapper_function) {
        if (!ir_lower_refresh_current_function(ctx, current_function_name, 0, 0)) {
            free(wrapper_name);
            free(visible_parameter_types);
            free(visible_parameter_names);
            return 0;
        }
        if (out_wrapper_name) {
            *out_wrapper_name = wrapper_function->name;
        }
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 1;
    }

    if (!ir_program_append_function(ctx->program, wrapper_name, &wrapper_function, ctx->error)) {
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 0;
    }
    if (!ir_lower_refresh_current_function(ctx, current_function_name, 0, 0)) {
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 0;
    }
    wrapper_function->has_body = 1;
    wrapper_function->return_type = target_return_type;

    if (!ir_function_append_local(wrapper_function, "__env", 1, NULL, ctx->error)) {
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 0;
    }
    wrapper_function->locals[0].value_type = AST_FUNCTION_RETURN_INT;

    for (param_index = 0u; param_index < visible_parameter_count; ++param_index) {
        if (!ir_function_append_local(wrapper_function,
                visible_parameter_names ? visible_parameter_names[param_index] : "arg",
                1,
                NULL,
                ctx->error)) {
            free(wrapper_name);
            free(visible_parameter_types);
            free(visible_parameter_names);
            return 0;
        }
        wrapper_function->locals[param_index + 1u].value_type =
            visible_parameter_types ? visible_parameter_types[param_index] : AST_FUNCTION_RETURN_INT;
    }

    if (!ir_function_append_block(wrapper_function, NULL, &wrapper_block, ctx->error)) {
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 0;
    }

    memset(&wrapper_ctx, 0, sizeof(wrapper_ctx));
    wrapper_ctx.program = ctx->program;
    wrapper_ctx.function = wrapper_function;
    wrapper_ctx.function_return_type = wrapper_function->return_type;
    wrapper_ctx.block_id = wrapper_block->id;
    wrapper_ctx.has_block = 1;
    wrapper_ctx.error = ctx->error;

    if (target_parameter_count > 0u) {
        forward_args = (IrValueRef *)malloc(target_parameter_count * sizeof(IrValueRef));
        if (!forward_args) {
            free(wrapper_name);
            free(visible_parameter_types);
            free(visible_parameter_names);
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-050: out of memory while building closure-wrapper args");
            return 0;
        }
    }

    for (param_index = 0u; param_index < capture_count; ++param_index) {
        IrValueRef env_base = ir_value_local(0u);
        IrValueRef slot_addr = ir_value_temp(ir_allocate_temp(wrapper_function));
        IrValueRef slot_value = ir_value_temp(ir_allocate_temp(wrapper_function));

        if (!ir_emit_binary(&wrapper_ctx,
                slot_addr,
                IR_BINARY_ADD,
                env_base,
                ir_value_immediate((long long)(param_index * 4u))) ||
            !ir_emit_load_indirect(&wrapper_ctx, slot_value, slot_addr)) {
            free(forward_args);
            free(wrapper_name);
            free(visible_parameter_types);
            free(visible_parameter_names);
            return 0;
        }
        forward_args[param_index] = slot_value;
    }
    for (param_index = 0u; param_index < visible_parameter_count; ++param_index) {
        forward_args[capture_count + param_index] = ir_value_local(param_index + 1u);
    }

    call_result = ir_value_temp(ir_allocate_temp(wrapper_function));
    if (!ir_emit_call(
            &wrapper_ctx,
            call_result,
            target_name,
            forward_args,
            target_parameter_count)) {
        free(forward_args);
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 0;
    }
    if (target_return_type == AST_FUNCTION_RETURN_VOID) {
        if (!ir_block_set_void_return(wrapper_block, ctx->error)) {
            free(forward_args);
            free(wrapper_name);
            free(visible_parameter_types);
            free(visible_parameter_names);
            return 0;
        }
    } else if (!ir_block_set_return(wrapper_block, call_result, ctx->error)) {
        free(forward_args);
        free(wrapper_name);
        free(visible_parameter_types);
        free(visible_parameter_names);
        return 0;
    }

    free(forward_args);
    free(visible_parameter_types);
    free(visible_parameter_names);
    if (!ir_lower_refresh_current_function(ctx, current_function_name, 0, 0)) {
        free(wrapper_name);
        return 0;
    }
    if (out_wrapper_name) {
        *out_wrapper_name = wrapper_function->name;
    }
    free(wrapper_name);
    return 1;
}

static int ir_try_emit_static_noncapturing_callable_indirect_call_with_args(
    IrLowerContext *ctx,
    const char *wrapper_name,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    const IrFunction *wrapper_function = NULL;
    size_t shape_id;
    IrValueRef result_value;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !ctx->program || !wrapper_name || wrapper_name[0] == '\0' || !out_value ||
        (arg_count > 0u && !args)) {
        return 1;
    }

    wrapper_function = ir_program_find_function_mut(ctx->program, wrapper_name);
    if (!wrapper_function ||
        wrapper_function->parameter_count == 0u ||
        wrapper_function->parameter_count - 1u != arg_count) {
        return 1;
    }

    if (!ir_build_callable_shape_from_function_signature(
            ctx,
            wrapper_function,
            1u,
            &shape_id)) {
        return 0;
    }

    if (!ir_emit_callable_object_indirect_call(
            ctx,
            wrapper_name,
            ir_value_immediate(0),
            shape_id,
            args,
            arg_count,
            &result_value)) {
        return 0;
    }
    return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
}

static int ir_try_emit_static_noncapturing_target_callable_indirect_call_with_args(
    IrLowerContext *ctx,
    const char *target_name,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    const char *wrapper_name = NULL;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !target_name || target_name[0] == '\0' || !out_value ||
        (arg_count > 0u && !args)) {
        return 1;
    }
    if (!ir_ensure_noncapturing_callable_wrapper_function(ctx, target_name, &wrapper_name)) {
        return 0;
    }
    return ir_try_emit_static_noncapturing_callable_indirect_call_with_args(
        ctx,
        wrapper_name,
        args,
        arg_count,
        out_value,
        out_applied);
}

static int ir_try_emit_static_noncapturing_callable_indirect_call(IrLowerContext *ctx,
    const char *target_name,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    const char *wrapper_name = NULL;
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    size_t callable_arg_count = 0u;
    IrValueRef result_value;
    int applied = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !ctx->program || !ctx->function || !target_name || !call_expr || !out_value) {
        return 1;
    }
    if (!ir_ensure_noncapturing_callable_wrapper_function(ctx, target_name, &wrapper_name)) {
        return 0;
    }
    if (!ir_build_dynamic_visible_callable_args_view(
            ctx,
            call_expr,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        return 0;
    }
    if (!ir_try_emit_static_noncapturing_callable_indirect_call_with_args(
            ctx,
            wrapper_name,
            call_args_view,
            callable_arg_count,
            &result_value,
            &applied)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    return ir_finish_optional_callable_dispatch_result(
        applied,
        result_value,
        out_value,
        out_applied);
}

static int ir_try_emit_static_callable_object_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callee_view,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_view || !call_expr || !out_value ||
        !callee_view->direct_target_name || callee_view->direct_target_name[0] == '\0') {
        return 1;
    }

    if (!ir_try_emit_static_direct_target_specialized_call(
            ctx,
            callee_view->direct_target_name,
            callee_view->is_closure_family,
            callee_view->capture_local_ids,
            callee_view->capture_count,
            call_expr,
            out_value,
            out_applied)) {
        return 0;
    }
    if (out_applied && *out_applied) {
        return 1;
    }

    if (callee_view->is_closure_family) {
        IrValueRef *call_args = NULL;
        const IrValueRef *call_args_view = NULL;
        size_t callable_arg_count = 0u;
        IrValueRef result_value;
        int applied = 0;

        if (!ir_build_dynamic_visible_callable_args_view(
                ctx,
                call_expr,
                "IR-INT-126: out of memory while lowering callable-object arguments",
                &call_args,
                &call_args_view,
                &callable_arg_count)) {
            return 0;
        }
        if (!ir_try_emit_static_closure_callable_indirect_call_with_args(
                ctx,
                callee_view->direct_target_name,
                callee_view->capture_local_ids,
                callee_view->capture_count,
                call_args_view,
                callable_arg_count,
                &result_value,
                &applied)) {
            free(call_args);
            return 0;
        }
        free(call_args);
        return ir_finish_optional_callable_dispatch_result(
            applied,
            result_value,
            out_value,
            out_applied);
    }

    return ir_try_emit_static_noncapturing_callable_indirect_call(
        ctx,
        callee_view->direct_target_name,
        call_expr,
        out_value,
        out_applied);
}

static int ir_try_emit_static_direct_target_specialized_call(IrLowerContext *ctx,
    const char *direct_target_name,
    int is_closure_family,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExternal *callee_external = NULL;
    const char *specialized_name = NULL;
    const char *callee_name = direct_target_name;
    IrFunction *callee_function = NULL;
    IrValueRef *call_args = NULL;
    const AstExpression **specialization_args = NULL;
    size_t arg_count = 0u;
    size_t source_arg_count = 0u;
    size_t hidden_prefix_param_count = 0u;
    size_t i;
    const char *current_function_name = NULL;
    int call_uses_function_value_specialization = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !direct_target_name || direct_target_name[0] == '\0' ||
        !call_expr || call_expr->kind != AST_EXPR_CALL || !out_value) {
        return 1;
    }

    if (!ctx->ast_program || !ctx->program) {
        return 1;
    }

    for (i = 0u; i < ctx->ast_program->count; ++i) {
        const AstExternal *candidate = &ctx->ast_program->externals[i];
        if (candidate->kind == AST_EXTERNAL_FUNCTION &&
            candidate->name &&
            strcmp(candidate->name, direct_target_name) == 0) {
            callee_external = candidate;
            break;
        }
    }
    if (!callee_external) {
        callee_external = ir_program_find_function_external_ref(ctx->program, direct_target_name);
    }
    if (!callee_external || !callee_external->parameter_value_kinds) {
        return 1;
    }

    for (i = 0u; i < callee_external->parameter_count; ++i) {
        if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
            break;
        }
    }
    if (i == callee_external->parameter_count) {
        return 1;
    }

    hidden_prefix_param_count = is_closure_family ? capture_count : 0u;
    if (is_closure_family &&
        hidden_prefix_param_count > 0u &&
        !capture_local_ids) {
        return 1;
    }
    if (!call_expr->as.call.args ||
        call_expr->as.call.arg_count + hidden_prefix_param_count != callee_external->parameter_count) {
        return 1;
    }

    if (!is_closure_family &&
        hidden_prefix_param_count == 0u &&
        !ir_try_emit_passthrough_callable_dispatch_from_function_args(
            ctx,
            call_expr,
            callee_external,
            out_value,
            out_applied)) {
        return 0;
    }
    if (out_applied && *out_applied) {
        return 1;
    }
    if (!ir_try_emit_simple_function_body_eval_dispatch(
            ctx,
            callee_external,
            is_closure_family,
            capture_local_ids,
            capture_count,
            call_expr,
            out_value,
            out_applied)) {
        return 0;
    }
    if (out_applied && *out_applied) {
        return 1;
    }

    for (i = hidden_prefix_param_count; i < callee_external->parameter_count; ++i) {
        const AstExpression *arg_base;
        const AstExpression *returned_base_call_expr = NULL;
        const AstExternal *returned_producer_external = NULL;
        int returned_family_is_closure = 0;
        const char **returned_target_names = NULL;
        size_t returned_target_count = 0u;
        size_t source_arg_index = i - hidden_prefix_param_count;

        if (!callee_external->parameter_value_kinds ||
            callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
            continue;
        }
        arg_base = ir_unwrap_function_value_wrapper_expression(call_expr->as.call.args[source_arg_index]);
        if (arg_base && arg_base->kind == AST_EXPR_TERNARY) {
            return 1;
        }
        if (arg_base && arg_base->kind == AST_EXPR_CALL &&
            ir_resolve_returned_function_value_call_producer_info(
                ctx,
                arg_base,
                &returned_base_call_expr,
                &returned_producer_external,
                &returned_family_is_closure) &&
            returned_base_call_expr &&
            returned_producer_external &&
            ir_resolve_returned_function_value_target_info(
                ctx,
                returned_base_call_expr,
                returned_producer_external,
                returned_family_is_closure,
                &returned_target_names,
                &returned_target_count,
                NULL)) {
            free((void *)returned_target_names);
            if (returned_target_count == 2u) {
                return 1;
            }
            continue;
        }
    }

    current_function_name = ctx->function ? ctx->function->name : NULL;
    source_arg_count = call_expr->as.call.arg_count;
    specialization_args =
        (const AstExpression **)calloc(callee_external->parameter_count, sizeof(AstExpression *));
    if (!specialization_args) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-029: out of memory while preparing higher-order function-value specialization");
        return 0;
    }
    for (i = 0u; i < source_arg_count; ++i) {
        specialization_args[hidden_prefix_param_count + i] = call_expr->as.call.args[i];
    }
    if (!ir_build_function_value_specialization_name_from_call_args(
            ctx,
            callee_external,
            specialization_args,
            callee_external->parameter_count,
            call_expr->line,
            call_expr->column,
            "IR-INT-029: out of memory while preparing higher-order function-value specialization",
            "IR-LOWER-028: failed to resolve higher-order function-value specialization target",
            "IR-INT-134: higher-order static function-value specialization materialization failed without a diagnostic",
            &specialized_name)) {
        free(specialization_args);
        return 0;
    }
    free(specialization_args);
    specialization_args = NULL;
    if (!ir_finish_function_value_specialization_callee_adoption(
            ctx,
            current_function_name,
            specialized_name,
            call_expr->line,
            call_expr->column,
            "IR-INT-083: current lowering function disappeared after higher-order specialization",
            &callee_name,
            &callee_function,
            &call_uses_function_value_specialization)) {
        return 0;
    }
    if (!callee_function) {
        return 1;
    }

    for (i = 0u; i < hidden_prefix_param_count; ++i) {
        if (callee_external->parameter_value_kinds &&
            callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
            const size_t *bound_capture_local_ids = NULL;
            size_t bound_capture_count = 0u;

            if (!ir_resolve_hidden_function_value_specialization_binding(
                    ctx,
                    callee_external,
                    i,
                    &bound_capture_local_ids,
                    &bound_capture_count,
                    NULL)) {
                free(call_args);
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-LOWER-028: failed to resolve higher-order function-value specialization target");
                return 0;
            }
            for (size_t capture_index = 0u; capture_index < bound_capture_count; ++capture_index) {
                IrValueRef *new_args =
                    (IrValueRef *)realloc(call_args, (arg_count + 1u) * sizeof(IrValueRef));
                if (!new_args) {
                    free(call_args);
                    ir_set_error(ctx->error,
                        call_expr->line,
                        call_expr->column,
                        "IR-INT-027: out of memory while lowering higher-order closure capture prefix arguments");
                    return 0;
                }
                call_args = new_args;
                call_args[arg_count++] = ir_value_local(bound_capture_local_ids[capture_index]);
            }
            continue;
        }

        {
            IrValueRef *new_args =
                (IrValueRef *)realloc(call_args, (arg_count + 1u) * sizeof(IrValueRef));
            if (!new_args) {
                free(call_args);
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-INT-027: out of memory while lowering higher-order closure capture prefix arguments");
                return 0;
            }
            call_args = new_args;
            call_args[arg_count++] = ir_value_local(capture_local_ids[i]);
        }
    }

    for (i = 0u; i < source_arg_count; ++i) {
        if (!ir_try_append_maybe_specialized_call_argument(
                ctx->ast_program,
                ctx,
                callee_external,
                hidden_prefix_param_count + i,
                call_expr->as.call.args[i],
                call_uses_function_value_specialization,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-028: failed to resolve higher-order function-value specialization target",
                "IR-LOWER-058: higher-order closure forwarding call is missing captured-value lowering metadata",
                "IR-INT-027: out of memory while lowering higher-order specialized closure capture arguments",
                "IR-INT-128: higher-order call argument %zu for '%s' failed to lower without a diagnostic",
                callee_name,
                &call_args,
                &arg_count)) {
            free(call_args);
            return 0;
        }
    }

    if (arg_count != callee_function->parameter_count) {
        free(call_args);
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-LOWER-026: higher-order call to '%s' lowers to %zu args but callee signature expects %zu",
            callee_name,
            arg_count,
            callee_function->parameter_count);
        return 0;
    }

    if (callee_function->return_type == AST_FUNCTION_RETURN_VOID) {
        IrValueRef ignored_result = ir_value_temp(ir_allocate_temp(ctx->function));

        *out_value = ir_value_immediate(0);
        if (!ir_emit_call(ctx, ignored_result, callee_name, call_args, arg_count)) {
            free(call_args);
            return 0;
        }
        free(call_args);
        return ir_mark_callable_dispatch_applied(out_applied);
    }

    *out_value = ir_value_temp(ir_allocate_temp(ctx->function));
    if (!ir_emit_call(ctx, *out_value, callee_name, call_args, arg_count)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    return ir_mark_callable_dispatch_applied(out_applied);
}

static int ir_try_emit_passthrough_callable_dispatch_from_function_args(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    IrValueRef *out_value,
    int *out_applied) {
    size_t i;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !call_expr || !callee_external || !out_value ||
        !callee_external->parameter_value_kinds ||
        call_expr->kind != AST_EXPR_CALL ||
        !call_expr->as.call.args ||
        call_expr->as.call.arg_count != callee_external->parameter_count) {
        return 1;
    }

    for (i = 0u; i < callee_external->parameter_count; ++i) {
        const AstExpression *function_arg_base;
        IrLowerCallableObjectView dynamic_function_arg_view;
        const char *const *owned_dynamic_target_names = NULL;
        int requires_materialized_capture_forwarding = 0;
        long long dynamic_then_tag_value = 1;
        int applied_dynamic_function_arg_dispatch = 0;
        IrLowerCallableObjectView static_function_arg_view;
        int applied_static_function_arg_dispatch = 0;

        if (callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
            continue;
        }

        if (!ir_lower_emit_function_value_wrapper_side_effects(
                ctx,
                call_expr->as.call.args[i])) {
            return 0;
        }

        function_arg_base =
            ir_unwrap_function_value_wrapper_expression(call_expr->as.call.args[i]);

        memset(&dynamic_function_arg_view, 0, sizeof(dynamic_function_arg_view));
        dynamic_function_arg_view.dynamic_tag_local_id = (size_t)-1;
        if (ir_try_resolve_dynamic_function_value_specialization_callable_object_view(
                ctx,
                call_expr->as.call.args[i],
                callee_external,
                i,
                &dynamic_function_arg_view,
                &owned_dynamic_target_names,
                &requires_materialized_capture_forwarding,
                &dynamic_then_tag_value)) {
            (void)requires_materialized_capture_forwarding;
            (void)dynamic_then_tag_value;
            if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                    ctx,
                    call_expr,
                    callee_external,
                    i,
                    &dynamic_function_arg_view,
                    0u,
                    out_value,
                    &applied_dynamic_function_arg_dispatch)) {
                ir_free_callable_object_view_storage(&dynamic_function_arg_view);
                free((void *)owned_dynamic_target_names);
                return 0;
            }
            ir_free_callable_object_view_storage(&dynamic_function_arg_view);
            free((void *)owned_dynamic_target_names);
            if (applied_dynamic_function_arg_dispatch) {
                return ir_mark_callable_dispatch_applied(out_applied);
            }
            if (function_arg_base && function_arg_base->kind == AST_EXPR_CALL) {
                return 1;
            }
        }

        memset(&static_function_arg_view, 0, sizeof(static_function_arg_view));
        static_function_arg_view.dynamic_tag_local_id = (size_t)-1;
        if (ir_try_resolve_static_actual_argument_callable_object_view(
                ctx,
                call_expr->as.call.args[i],
                &static_function_arg_view) &&
            static_function_arg_view.direct_target_name &&
            static_function_arg_view.direct_target_name[0] != '\0') {
            if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                    ctx,
                    call_expr,
                    callee_external,
                    i,
                    &static_function_arg_view,
                    0u,
                    out_value,
                    &applied_static_function_arg_dispatch)) {
                ir_free_callable_object_view_storage(&static_function_arg_view);
                return 0;
            }
            ir_free_callable_object_view_storage(&static_function_arg_view);
            if (applied_static_function_arg_dispatch) {
                return ir_mark_callable_dispatch_applied(out_applied);
            }
        }
        return 1;
    }

    return 1;
}

static int ir_try_emit_static_direct_target_callable_dispatch(IrLowerContext *ctx,
    const char *direct_target_name,
    int is_closure_family,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    IrLowerCallableObjectView callee_view;

    memset(&callee_view, 0, sizeof(callee_view));
    callee_view.direct_target_name = direct_target_name;
    callee_view.capture_local_ids = capture_local_ids;
    callee_view.capture_count = capture_count;
    callee_view.is_closure_family = is_closure_family ? 1 : 0;
    return ir_try_emit_static_callable_object_dispatch(
        ctx,
        &callee_view,
        call_expr,
        out_value,
        out_applied);
}

static int ir_materialize_ternary_function_value_argument_callable_object_view(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const AstExpression *function_arg_base,
    IrLowerCallableObjectView *out_view,
    const char *const **out_owned_dynamic_target_names,
    long long *out_then_tag_value) {
    const AstExpression *then_base = NULL;
    const AstExpression *else_base = NULL;
    const char *then_target_name = NULL;
    const char *else_target_name = NULL;
    const size_t *then_capture_local_ids = NULL;
    const size_t *else_capture_local_ids = NULL;
    size_t then_capture_count = 0u;
    size_t else_capture_count = 0u;
    long long then_tag_value = 0;
    long long else_tag_value = 0;
    size_t dynamic_tag_local_id = (size_t)-1;
    size_t *payload_local_ids = NULL;
    const char **dynamic_target_names = NULL;
    size_t capture_index;

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (out_owned_dynamic_target_names) {
        *out_owned_dynamic_target_names = NULL;
    }
    if (out_then_tag_value) {
        *out_then_tag_value = 1;
    }
    if (!ctx || !callee_external || !function_arg_base || !out_view ||
        function_arg_base->kind != AST_EXPR_TERNARY) {
        return 0;
    }

    then_base = ir_unwrap_paren_expression(function_arg_base->as.ternary.then_expr);
    else_base = ir_unwrap_paren_expression(function_arg_base->as.ternary.else_expr);
    if (then_base && else_base &&
        then_base->kind == AST_EXPR_CALL &&
        else_base->kind == AST_EXPR_CALL &&
        ir_call_argument_resolve_extension_function_value_target(
            ctx->ast_program,
            ctx,
            callee_external,
            function_arg_index,
            then_base,
            &then_capture_local_ids,
            &then_capture_count,
            &then_target_name) &&
        ir_call_argument_resolve_extension_function_value_target(
            ctx->ast_program,
            ctx,
            callee_external,
            function_arg_index,
            else_base,
            &else_capture_local_ids,
            &else_capture_count,
            &else_target_name) &&
        then_target_name && else_target_name &&
        then_capture_count == else_capture_count) {
        if (!ir_function_append_local(
                ctx->function,
                "__ternary_fn_argtag",
                0,
                &dynamic_tag_local_id,
                ctx->error)) {
            return 0;
        }
        goto build_materialized_ternary_callable_view;
    }
    if (!then_base || !else_base ||
        then_base->kind != AST_EXPR_IDENTIFIER ||
        else_base->kind != AST_EXPR_IDENTIFIER ||
        !ir_call_argument_resolve_extension_function_value_target(
            ctx->ast_program,
            ctx,
            callee_external,
            function_arg_index,
            then_base,
            &then_capture_local_ids,
            &then_capture_count,
            &then_target_name) ||
        !ir_call_argument_resolve_extension_function_value_target(
            ctx->ast_program,
            ctx,
            callee_external,
            function_arg_index,
            else_base,
            &else_capture_local_ids,
            &else_capture_count,
            &else_target_name) ||
        !then_target_name || !else_target_name ||
        then_capture_count != else_capture_count ||
        !ir_function_append_local(
            ctx->function,
            "__ternary_fn_argtag",
            0,
            &dynamic_tag_local_id,
            ctx->error) ||
        !ir_lower_register_function_value_target(
            ctx,
            "__ternary_fn_arg",
            then_target_name,
            &then_tag_value) ||
        !ir_lower_register_function_value_target(
            ctx,
            "__ternary_fn_arg",
            else_target_name,
            &else_tag_value)) {
        return 0;
    }

build_materialized_ternary_callable_view:
    if (then_tag_value == 0 &&
        (!ir_lower_register_function_value_target(
                ctx,
                "__ternary_fn_arg",
                then_target_name,
                &then_tag_value) ||
            !ir_lower_register_function_value_target(
                ctx,
                "__ternary_fn_arg",
                else_target_name,
                &else_tag_value))) {
        return 0;
    }

    dynamic_target_names = (const char **)malloc(2u * sizeof(const char *));
    if (!dynamic_target_names) {
        ir_set_error(ctx->error,
            function_arg_base->line,
            function_arg_base->column,
            "IR-INT-029: out of memory while preparing ternary callable-object targets");
        return 0;
    }
    dynamic_target_names[0] = then_target_name;
    dynamic_target_names[1] = else_target_name;

    if (then_capture_count > 0u) {
        payload_local_ids = (size_t *)malloc(then_capture_count * sizeof(size_t));
        if (!payload_local_ids) {
            free((void *)dynamic_target_names);
            ir_set_error(ctx->error,
                function_arg_base->line,
                function_arg_base->column,
                "IR-INT-029: out of memory while preparing ternary closure forwarding args");
            return 0;
        }
        for (capture_index = 0u; capture_index < then_capture_count; ++capture_index) {
            char payload_local_name[256];

            snprintf(payload_local_name,
                sizeof(payload_local_name),
                "__ternary_fn_argcap_%zu",
                capture_index);
            if (!ir_function_append_local(
                    ctx->function,
                    payload_local_name,
                    0,
                    &payload_local_ids[capture_index],
                    ctx->error)) {
                free(payload_local_ids);
                free((void *)dynamic_target_names);
                return 0;
            }
        }
    }

    if (!ir_emit_materialized_ternary_function_value_selection(
            ctx,
            function_arg_base->as.ternary.condition,
            then_tag_value,
            else_tag_value,
            (size_t)-1,
            (size_t)-1,
            1,
            1,
            then_capture_local_ids,
            else_capture_local_ids,
            then_capture_count,
            dynamic_tag_local_id,
            payload_local_ids)) {
        free(payload_local_ids);
        free((void *)dynamic_target_names);
        return 0;
    }

    out_view->direct_target_name = then_target_name;
    out_view->dynamic_target_names = dynamic_target_names;
    out_view->dynamic_target_count = 2u;
    out_view->owned_payload_local_ids = payload_local_ids;
    out_view->capture_local_ids = payload_local_ids;
    out_view->capture_count = then_capture_count;
    out_view->dynamic_tag_local_id = dynamic_tag_local_id;
    out_view->is_closure_family = then_capture_count > 0u ? 1 : 0;
    out_view->has_dynamic_family = 1;
    if (out_owned_dynamic_target_names) {
        *out_owned_dynamic_target_names = dynamic_target_names;
    }
    if (out_then_tag_value) {
        *out_then_tag_value = then_tag_value;
    }
    return 1;
}

static int ir_try_resolve_dynamic_function_value_specialization_callable_object_view(
    IrLowerContext *ctx,
    const AstExpression *function_arg_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    IrLowerCallableObjectView *out_view,
    const char *const **out_owned_dynamic_target_names,
    int *out_requires_materialized_capture_forwarding,
    long long *out_then_tag_value) {
    const AstExpression *function_arg_base =
        ir_unwrap_function_value_wrapper_expression(function_arg_expr);

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (out_owned_dynamic_target_names) {
        *out_owned_dynamic_target_names = NULL;
    }
    if (out_requires_materialized_capture_forwarding) {
        *out_requires_materialized_capture_forwarding = 0;
    }
    if (out_then_tag_value) {
        *out_then_tag_value = 1;
    }
    if (!ctx || !function_arg_expr || !callee_external || !out_view) {
        return 0;
    }

    if (function_arg_base && function_arg_base->kind == AST_EXPR_TERNARY) {
        if (!ir_finish_dynamic_function_value_specialization_materialized_view(
                out_view,
                out_owned_dynamic_target_names,
                out_requires_materialized_capture_forwarding,
                ir_materialize_ternary_function_value_argument_callable_object_view(
                    ctx,
                    callee_external,
                    function_arg_index,
                    function_arg_base,
                    out_view,
                    out_owned_dynamic_target_names,
                    out_then_tag_value))) {
            return 0;
        }
        return 1;
    }

    if (function_arg_base && function_arg_base->kind == AST_EXPR_CALL) {
        if (!ir_finish_dynamic_function_value_specialization_materialized_view(
                out_view,
                out_owned_dynamic_target_names,
                out_requires_materialized_capture_forwarding,
                ir_try_materialize_two_target_returned_callable_object_view_with_family_prefixes(
                    ctx,
                    function_arg_base,
                    "__retfn_argslot",
                    "__retclosure_argslot",
                    out_view))) {
            return 0;
        }
        return 1;
    }

    if (!ir_resolve_any_callable_object_view(ctx, function_arg_expr, out_view)) {
        return 0;
    }
    if (!ir_callable_object_view_is_two_target_dynamic_family(out_view)) {
        return 0;
    }
    if (out_requires_materialized_capture_forwarding) {
        *out_requires_materialized_capture_forwarding =
            out_view->is_closure_family && out_view->capture_count > 0u;
    }
    return 1;
}

static int ir_try_emit_dynamic_function_value_specialized_call_from_callable_object_view(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const IrLowerCallableObjectView *dynamic_callable_view,
    const char *const *owned_dynamic_target_names,
    int requires_materialized_capture_forwarding,
    long long dynamic_then_tag_value,
    IrValueRef *out_value,
    int *out_applied) {
    const char *specialized_name_a = NULL;
    const char *specialized_name_b = NULL;
    IrValueRef *call_args = NULL;
    IrValueRef compare_value;
    IrValueRef result_value;
    size_t result_temp_id;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;
    IrValueRef *dynamic_forward_args = NULL;
    size_t dynamic_forward_arg_count = 0u;
    size_t forwarded_visible_arg_count = 0u;
    size_t callable_arg_count = 0u;
    size_t branch_call_arg_count = 0u;
    size_t function_value_param_count = 0u;
    int applied_indirect_dispatch = 0;
    const IrValueRef *call_args_view = NULL;
    const IrValueRef *branch_call_args = NULL;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !call_expr || !callee_external || !dynamic_callable_view || !out_value) {
        free((void *)owned_dynamic_target_names);
        return 1;
    }

    if (callee_external->parameter_value_kinds) {
        for (size_t i = 0u; i < callee_external->parameter_count; ++i) {
            if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
                function_value_param_count += 1u;
            }
        }
    }
    if (ir_prepare_dynamic_function_value_specialization_dispatch(
            dynamic_callable_view,
            owned_dynamic_target_names,
            requires_materialized_capture_forwarding)) {
        return 1;
    }

    if (!ir_build_dynamic_non_function_callable_args_view(
            ctx,
            call_expr,
            callee_external,
            requires_materialized_capture_forwarding
                ? dynamic_callable_view->capture_count
                : 0u,
            function_arg_index,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        free((void *)owned_dynamic_target_names);
        return 0;
    }
    forwarded_visible_arg_count = callable_arg_count;

    if (function_value_param_count <= 1u) {
        if (!ir_try_finish_dynamic_function_value_specialization_indirect_fast_path(
                ctx,
                callee_external,
                dynamic_callable_view,
                call_args_view,
                callable_arg_count,
                owned_dynamic_target_names,
                out_value,
                out_applied,
                &applied_indirect_dispatch)) {
            ir_finish_dynamic_specialization_call_cleanup(
                call_args,
                NULL,
                NULL);
            return 0;
        }
        if (applied_indirect_dispatch) {
            ir_finish_dynamic_specialization_call_cleanup(
                call_args,
                NULL,
                NULL);
            return 1;
        }
    }

    if ((requires_materialized_capture_forwarding ||
            function_value_param_count > 1u) &&
        !ir_build_dynamic_function_value_specialization_forward_args(
            ctx,
            callee_external,
            call_expr,
            function_arg_index,
            dynamic_callable_view,
            call_args,
            forwarded_visible_arg_count,
            call_expr->line,
            call_expr->column,
            &dynamic_forward_args,
            &dynamic_forward_arg_count)) {
        ir_finish_dynamic_specialization_call_cleanup(
            call_args,
            NULL,
            owned_dynamic_target_names);
        return 0;
    }

    if (!ir_build_dynamic_function_value_specialization_names(
            ctx,
            callee_external,
            function_arg_index,
            dynamic_callable_view,
            (const AstExpression *const *)call_expr->as.call.args,
            call_expr->as.call.arg_count,
            requires_materialized_capture_forwarding,
            call_expr->line,
            call_expr->column,
            &specialized_name_a,
            &specialized_name_b)) {
        ir_finish_dynamic_specialization_cleanup(
            dynamic_forward_args,
            owned_dynamic_target_names);
        return 0;
    }

    branch_call_args = ir_dynamic_function_value_specialization_args_view(
        call_args,
        callable_arg_count,
        dynamic_forward_args,
        dynamic_forward_arg_count,
        &branch_call_arg_count);

    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_binary(ctx,
            compare_value,
            IR_BINARY_EQ,
            ir_value_local(dynamic_callable_view->dynamic_tag_local_id),
            ir_value_immediate(dynamic_then_tag_value)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        ir_finish_dynamic_specialization_call_cleanup(
            call_args,
            dynamic_forward_args,
            owned_dynamic_target_names);
        return 0;
    }

    if (!ir_emit_function_value_specialization_branch_call(
            ctx,
            then_block_id,
            join_block_id,
            result_value,
            specialized_name_a,
            branch_call_args,
            branch_call_arg_count)) {
        ir_finish_dynamic_specialization_call_cleanup(
            call_args,
            dynamic_forward_args,
            owned_dynamic_target_names);
        return 0;
    }

    if (!ir_emit_function_value_specialization_branch_call(
            ctx,
            else_block_id,
            join_block_id,
            result_value,
            specialized_name_b,
            branch_call_args,
            branch_call_arg_count)) {
        ir_finish_dynamic_specialization_call_cleanup(
            call_args,
            dynamic_forward_args,
            owned_dynamic_target_names);
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    ir_finish_dynamic_specialization_call_cleanup(
        call_args,
        dynamic_forward_args,
        owned_dynamic_target_names);
    return ir_finish_void_or_value_callable_dispatch_result(
        callee_external->function_return_type == AST_FUNCTION_RETURN_VOID,
        result_value,
        out_value,
        out_applied);
}

static int ir_try_emit_returned_dynamic_function_value_specialized_call(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external_a,
    const AstExternal *callee_external_b,
    const IrLowerCallableObjectView *dynamic_callable_view,
    const char *const *owned_dynamic_target_names,
    long long dynamic_then_tag_value,
    IrValueRef *out_value,
    int *out_applied) {
    const char *specialized_name_a = NULL;
    const char *specialized_name_b = NULL;
    const char *current_function_name = NULL;
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    const AstExpression **specialization_args_a = NULL;
    const AstExpression **specialization_args_b = NULL;
    IrValueRef *dynamic_forward_args = NULL;
    IrValueRef compare_value;
    IrValueRef result_value;
    size_t callable_arg_count = 0u;
    size_t dynamic_forward_arg_count = 0u;
    size_t branch_call_arg_count = 0u;
    size_t result_temp_id = 0u;
    size_t then_block_id = 0u;
    size_t else_block_id = 0u;
    size_t join_block_id = 0u;
    size_t hidden_prefix_param_count_a = 0u;
    size_t hidden_prefix_param_count_b = 0u;
    size_t source_arg_index = 0u;
    const IrValueRef *branch_call_args = NULL;
    size_t function_arg_index = (size_t)-1;
    size_t function_param_count_a = 0u;
    size_t function_param_count_b = 0u;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !call_expr || !callee_external_a || !callee_external_b ||
        !dynamic_callable_view || !out_value) {
        return 1;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;
    hidden_prefix_param_count_a = dynamic_callable_view->is_closure_family
        ? dynamic_callable_view->capture_count
        : 0u;
    hidden_prefix_param_count_b = hidden_prefix_param_count_a;

    if (call_expr->as.call.arg_count + hidden_prefix_param_count_a != callee_external_a->parameter_count ||
        call_expr->as.call.arg_count + hidden_prefix_param_count_b != callee_external_b->parameter_count) {
        return 1;
    }

    if (dynamic_callable_view->is_closure_family &&
        !ir_try_emit_two_target_dynamic_closure_body_eval_dispatch(
            ctx,
            call_expr,
            callee_external_a,
            callee_external_b,
            dynamic_callable_view,
            dynamic_then_tag_value,
            out_value,
            out_applied)) {
        return 0;
    }
    if (out_applied && *out_applied) {
        return 1;
    }

    if (!dynamic_callable_view->is_closure_family &&
        call_expr->as.call.arg_count == callee_external_a->parameter_count &&
        call_expr->as.call.arg_count == callee_external_b->parameter_count &&
        callee_external_a->parameter_value_kinds &&
        callee_external_b->parameter_value_kinds) {
        size_t param_index;
        IrLowerCallableObjectView static_actual_argument_view;
        IrValueRef branch_then_value = ir_value_immediate(0);
        IrValueRef branch_else_value = ir_value_immediate(0);
        int branch_then_applied = 0;
        int branch_else_applied = 0;

        for (param_index = 0u; param_index < callee_external_a->parameter_count; ++param_index) {
            int is_function_param_a =
                callee_external_a->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION;
            int is_function_param_b =
                callee_external_b->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION;

            if (is_function_param_a != is_function_param_b) {
                function_param_count_a = 0u;
                function_param_count_b = 0u;
                function_arg_index = (size_t)-1;
                break;
            }
            if (!is_function_param_a) {
                continue;
            }
            function_param_count_a += 1u;
            function_param_count_b += 1u;
            if (function_arg_index == (size_t)-1) {
                function_arg_index = param_index;
            }
        }

        memset(&static_actual_argument_view, 0, sizeof(static_actual_argument_view));
        static_actual_argument_view.dynamic_tag_local_id = (size_t)-1;
        if (function_param_count_a == 1u &&
            function_param_count_b == 1u &&
            function_arg_index != (size_t)-1 &&
            call_expr->as.call.arg_count > 1u &&
            ir_try_resolve_static_actual_argument_callable_object_view(
                ctx,
                call_expr->as.call.args[function_arg_index],
                &static_actual_argument_view) &&
            static_actual_argument_view.direct_target_name &&
            static_actual_argument_view.direct_target_name[0] != '\0' &&
            !static_actual_argument_view.has_dynamic_family &&
            ir_can_use_direct_function_parameter_passthrough_dispatch(
                ctx,
                callee_external_a,
                function_arg_index) &&
            ir_can_use_direct_function_parameter_passthrough_dispatch(
                ctx,
                callee_external_b,
                function_arg_index)) {
            compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
            result_temp_id = ir_allocate_temp(ctx->function);
            result_value = ir_value_temp(result_temp_id);
            if (!ir_emit_binary(ctx,
                    compare_value,
                    IR_BINARY_EQ,
                    ir_value_local(dynamic_callable_view->dynamic_tag_local_id),
                    ir_value_immediate(dynamic_then_tag_value)) ||
                !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
                !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
                !ir_block_set_branch(ir_lower_current_block(ctx),
                    compare_value,
                    then_block_id,
                    else_block_id,
                    ctx->error) ||
                !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }

            ctx->block_id = then_block_id;
            ctx->has_block = 1;
            if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                    ctx,
                    call_expr,
                    callee_external_a,
                    function_arg_index,
                    &static_actual_argument_view,
                    0u,
                    &branch_then_value,
                    &branch_then_applied)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }
            if (!branch_then_applied) {
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-INT-148: branch-local returned dynamic passthrough dispatch failed for target '%s'",
                    callee_external_a->name ? callee_external_a->name : "<unknown>");
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }
            if (callee_external_a->function_return_type != AST_FUNCTION_RETURN_VOID &&
                !ir_emit_mov(ctx, result_value, branch_then_value)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }
            if (!ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }

            ctx->block_id = else_block_id;
            ctx->has_block = 1;
            if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                    ctx,
                    call_expr,
                    callee_external_b,
                    function_arg_index,
                    &static_actual_argument_view,
                    0u,
                    &branch_else_value,
                    &branch_else_applied)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }
            if (!branch_else_applied) {
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-INT-149: branch-local returned dynamic passthrough dispatch failed for target '%s'",
                    callee_external_b->name ? callee_external_b->name : "<unknown>");
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }
            if (callee_external_b->function_return_type != AST_FUNCTION_RETURN_VOID &&
                !ir_emit_mov(ctx, result_value, branch_else_value)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }
            if (!ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
                ir_free_callable_object_view_storage(&static_actual_argument_view);
                return 0;
            }

            ctx->block_id = join_block_id;
            ctx->has_block = 1;
            ir_free_callable_object_view_storage(&static_actual_argument_view);
            return ir_finish_void_or_value_callable_dispatch_result(
                callee_external_a->function_return_type == AST_FUNCTION_RETURN_VOID,
                result_value,
                out_value,
                out_applied);
        }
        ir_free_callable_object_view_storage(&static_actual_argument_view);
    }

    specialization_args_a =
        (const AstExpression **)calloc(callee_external_a->parameter_count, sizeof(const AstExpression *));
    specialization_args_b =
        (const AstExpression **)calloc(callee_external_b->parameter_count, sizeof(const AstExpression *));
    if (!specialization_args_a || !specialization_args_b) {
        free(specialization_args_a);
        free(specialization_args_b);
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-029: out of memory while preparing returned dynamic function-value specialization");
        return 0;
    }
    for (source_arg_index = 0u; source_arg_index < call_expr->as.call.arg_count; ++source_arg_index) {
        const AstExpression *arg_expr = call_expr->as.call.args[source_arg_index];
        specialization_args_a[hidden_prefix_param_count_a + source_arg_index] = arg_expr;
        specialization_args_b[hidden_prefix_param_count_b + source_arg_index] = arg_expr;
    }

    if (!ir_build_function_value_specialization_name_from_call_args(
            ctx,
            callee_external_a,
            specialization_args_a,
            callee_external_a->parameter_count,
            call_expr->line,
            call_expr->column,
            "IR-INT-029: out of memory while preparing returned dynamic function-value specialization",
            "IR-LOWER-028: failed to resolve returned dynamic function-value specialization target",
            "IR-INT-106: failed while building specialized returned-call helper",
            &specialized_name_a)) {
        free(specialization_args_a);
        free(specialization_args_b);
        return 0;
    }
    free(specialization_args_a);
    specialization_args_a = NULL;
    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            call_expr->line,
            call_expr->column)) {
        free(specialization_args_b);
        return 0;
    }

    if (!ir_build_function_value_specialization_name_from_call_args(
            ctx,
            callee_external_b,
            specialization_args_b,
            callee_external_b->parameter_count,
            call_expr->line,
            call_expr->column,
            "IR-INT-029: out of memory while preparing returned dynamic function-value specialization",
            "IR-LOWER-028: failed to resolve returned dynamic function-value specialization target",
            "IR-INT-106: failed while building specialized returned-call helper",
            &specialized_name_b)) {
        free(specialization_args_b);
        return 0;
    }
    free(specialization_args_b);
    specialization_args_b = NULL;
    if (!ir_lower_refresh_current_function(
            ctx,
            current_function_name,
            call_expr->line,
            call_expr->column)) {
        return 0;
    }

    if (!ir_build_dynamic_non_function_callable_args_view(
            ctx,
            call_expr,
            callee_external_a,
            dynamic_callable_view->is_closure_family
                ? dynamic_callable_view->capture_count
                : 0u,
            (size_t)-1,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        return 0;
    }

    if (dynamic_callable_view->is_closure_family &&
        dynamic_callable_view->capture_count > 0u &&
        !ir_build_dynamic_function_value_specialization_forward_args(
            ctx,
            callee_external_a,
            call_expr,
            (size_t)-1,
            dynamic_callable_view,
            call_args,
            callable_arg_count,
            call_expr->line,
            call_expr->column,
            &dynamic_forward_args,
            &dynamic_forward_arg_count)) {
        free(call_args);
        return 0;
    }

    branch_call_args = ir_dynamic_function_value_specialization_args_view(
        call_args,
        callable_arg_count,
        dynamic_forward_args,
        dynamic_forward_arg_count,
        &branch_call_arg_count);

    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_binary(ctx,
            compare_value,
            IR_BINARY_EQ,
            ir_value_local(dynamic_callable_view->dynamic_tag_local_id),
            ir_value_immediate(dynamic_then_tag_value)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        free(call_args);
        free((void *)owned_dynamic_target_names);
        return 0;
    }

    if (!ir_emit_function_value_specialization_branch_call(
            ctx,
            then_block_id,
            join_block_id,
            result_value,
            specialized_name_a,
            branch_call_args,
            branch_call_arg_count) ||
        !ir_emit_function_value_specialization_branch_call(
            ctx,
            else_block_id,
            join_block_id,
            result_value,
            specialized_name_b,
            branch_call_args,
            branch_call_arg_count)) {
        ir_finish_dynamic_specialization_call_cleanup(
            call_args,
            dynamic_forward_args,
            owned_dynamic_target_names);
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    ir_finish_dynamic_specialization_call_cleanup(
        call_args,
        dynamic_forward_args,
        owned_dynamic_target_names);
    return ir_finish_void_or_value_callable_dispatch_result(
        callee_external_a->function_return_type == AST_FUNCTION_RETURN_VOID,
        result_value,
        out_value,
        out_applied);
}

static int ir_try_emit_direct_dynamic_function_value_specialized_call(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external_a,
    const AstExternal *callee_external_b,
    IrLowerCallableObjectView *callee_view,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !call_expr || !callee_external_a || !callee_external_b ||
        !callee_view || !out_value) {
        return 1;
    }
    return ir_try_emit_returned_dynamic_function_value_specialized_call(
        ctx,
        call_expr,
        callee_external_a,
        callee_external_b,
        callee_view,
        NULL,
        1,
        out_value,
        out_applied);
}

static int ir_try_emit_returned_binding_static_callable_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *resolved_callee_view,
    const AstExpression *callee_expr,
    const AstExpression *call_expr,
    const IrLowerBindingInfo *binding_info,
    IrValueRef *out_value,
    int *out_applied) {
    IrLowerCallableObjectView callee_view;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callee_expr || !call_expr || !binding_info || !out_value ||
        !binding_info->from_returned_function_value_call) {
        return 1;
    }

    if (resolved_callee_view &&
        resolved_callee_view->has_dynamic_family &&
        ir_callable_object_view_is_two_target_dynamic_family(resolved_callee_view)) {
        return ir_try_emit_direct_callee_callable_object_dynamic_dispatch(
            ctx,
            (IrLowerCallableObjectView *)resolved_callee_view,
            call_expr,
            NULL,
            NULL,
            NULL,
            out_value,
            out_applied);
    }

    if (binding_info->kind == IR_LOWER_BINDING_FUNCTION_LOCAL &&
        binding_info->type_name && binding_info->type_name[0] != '\0' &&
        binding_info->function_target_tag_local_id != (size_t)-1 &&
        binding_info->has_const_value) {
        return ir_try_emit_static_direct_target_callable_dispatch(
            ctx,
            binding_info->type_name,
            0,
            NULL,
            0u,
            call_expr,
            out_value,
            out_applied);
    }

    if (resolved_callee_view &&
        resolved_callee_view->direct_target_name &&
        resolved_callee_view->direct_target_name[0] != '\0') {
        callee_view = *resolved_callee_view;
    } else {
        if (!ir_resolve_callable_object_view(ctx, callee_expr, &callee_view) ||
            !callee_view.direct_target_name ||
            callee_view.direct_target_name[0] == '\0') {
            return 1;
        }
    }

    return ir_try_emit_static_direct_target_callable_dispatch(
        ctx,
        callee_view.direct_target_name,
        callee_view.is_closure_family,
        callee_view.capture_local_ids,
        callee_view.capture_count,
        call_expr,
        out_value,
        out_applied);
}

static int ir_try_handle_direct_callee_callable_object_dynamic_dispatch(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *resolved_callee_view,
    const AstExpression *call_expr,
    int *io_callee_is_closure_local,
    const size_t **io_closure_capture_local_ids,
    size_t *io_closure_prefix_arg_count,
    size_t *immediate_returned_closure_capture_local_ids,
    const char **io_callee_name,
    IrValueRef *out_value,
    int *out_handled) {
    IrLowerCallableObjectView callee_view;
    int applied_dynamic_dispatch = 0;
    int handled_dynamic_dispatch = 0;

    if (out_handled) {
        *out_handled = 0;
    }
    if (!ctx || !call_expr || !io_callee_is_closure_local ||
        !io_closure_capture_local_ids || !io_closure_prefix_arg_count ||
        !io_callee_name || !out_value) {
        return 1;
    }
    if (*io_callee_is_closure_local ||
        !resolved_callee_view ||
        !resolved_callee_view->direct_target_name ||
        resolved_callee_view->direct_target_name[0] == '\0') {
        return 1;
    }

    callee_view = *resolved_callee_view;
    *io_callee_name = callee_view.direct_target_name;
    if (!ir_try_emit_direct_callee_callable_object_dynamic_dispatch(
            ctx,
            &callee_view,
            call_expr,
            io_callee_is_closure_local,
            io_closure_capture_local_ids,
            io_closure_prefix_arg_count,
            out_value,
            &applied_dynamic_dispatch)) {
        return 0;
    }
    if (!ir_finish_optional_returned_callable_dispatch(
            applied_dynamic_dispatch,
            immediate_returned_closure_capture_local_ids,
            &handled_dynamic_dispatch)) {
        return 0;
    }
    if (out_handled) {
        *out_handled = handled_dynamic_dispatch;
    }
    return 1;
}

static int ir_try_handle_returned_binding_static_callable_dispatch(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *resolved_callee_view,
    const AstExpression *callee_expr,
    const AstExpression *call_expr,
    const AstExpression *callee_base_expr,
    size_t *immediate_returned_closure_capture_local_ids,
    IrValueRef *out_value,
    int *out_handled) {
    IrLowerBindingInfo binding_info;
    int applied_callable_dispatch = 0;
    int handled_callable_dispatch = 0;

    if (out_handled) {
        *out_handled = 0;
    }
    if (!ctx || !callee_base_expr || !callee_expr || !call_expr || !out_value) {
        return 1;
    }
    if (!ir_try_resolve_returned_binding_callee_info(
            ctx,
            callee_base_expr,
            &binding_info)) {
        return 1;
    }
    if (!ir_try_emit_returned_binding_static_callable_dispatch(
            ctx,
            resolved_callee_view,
            callee_expr,
            call_expr,
            &binding_info,
            out_value,
            &applied_callable_dispatch)) {
        return 0;
    }
    if (!ir_finish_optional_returned_callable_dispatch(
            applied_callable_dispatch,
            immediate_returned_closure_capture_local_ids,
            &handled_callable_dispatch)) {
        return 0;
    }
    if (out_handled) {
        *out_handled = handled_callable_dispatch;
    }
    return 1;
}

static int ir_try_handle_returned_binding_probe(
    IrLowerContext *ctx,
    const IrLowerCallableObjectView *resolved_callee_view,
    const AstExpression *callee_expr,
    const AstExpression *call_expr,
    const AstExpression *callee_base_expr,
    size_t *immediate_returned_closure_capture_local_ids,
    IrValueRef *out_value,
    int *out_handled) {
    int handled_callable_dispatch = 0;

    if (out_handled) {
        *out_handled = 0;
    }
    if (!ctx || !callee_expr || !call_expr || !callee_base_expr || !out_value) {
        return 1;
    }
    if (!ir_try_handle_returned_binding_static_callable_dispatch(
            ctx,
            resolved_callee_view,
            callee_expr,
            call_expr,
            callee_base_expr,
            immediate_returned_closure_capture_local_ids,
            out_value,
            &handled_callable_dispatch)) {
        return 0;
    }
    if (out_handled) {
        *out_handled = handled_callable_dispatch;
    }
    return 1;
}

typedef struct {
    int handled;
    int stop_outer_chain;
} IrReturnedCallCalleeResult;

static int ir_try_handle_returned_call_callee(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression *callee_base_expr,
    const char **io_callee_name,
    int *io_callee_is_closure_local,
    int *io_callee_from_static_returned_parameter_closure,
    const size_t **io_closure_capture_local_ids,
    size_t *io_closure_prefix_arg_count,
    size_t **io_immediate_returned_closure_capture_local_ids,
    IrValueRef *out_value,
    IrReturnedCallCalleeResult *out_result) {
    const char *returned_callee_name = NULL;
    const AstExternal *return_function_external = NULL;
    IrLowerReturnedFunctionObjectResolution returned_resolution;
    int have_returned_resolution = 0;
    const size_t *returned_capture_local_ids = NULL;
    size_t returned_capture_count = 0u;
    const char *returned_arg_target_name = NULL;
    int returned_is_static_single_capture_closure_local = 0;

    if (out_result) {
        out_result->handled = 0;
        out_result->stop_outer_chain = 0;
    }
    if (!ctx || !call_expr || !callee_base_expr || !out_value ||
        !io_callee_name || !io_callee_is_closure_local ||
        !io_callee_from_static_returned_parameter_closure ||
        !io_closure_capture_local_ids || !io_closure_prefix_arg_count ||
        !io_immediate_returned_closure_capture_local_ids) {
        return 1;
    }
    if (callee_base_expr->kind != AST_EXPR_CALL || !ctx->ast_program) {
        return 1;
    }
    returned_callee_name =
        ir_extract_direct_call_callee_name(callee_base_expr->as.call.callee);
    if (returned_callee_name && returned_callee_name[0] != '\0') {
        return_function_external =
            ir_find_named_function_external_prefer_definition(
                ctx->ast_program,
                returned_callee_name);
    }
    if (!return_function_external || !return_function_external->has_function_return_signature) {
        return 1;
    }

    {
        const AstExpression *returned_arg_expr = NULL;
        const AstExpression *returned_arg_base = NULL;
        IrLowerCallableObjectView returned_arg_view;
        IrLowerReturnedFunctionObjectResolution returned_arg_resolution;
        int applied_returned_parameter_dispatch = 0;

        memset(&returned_arg_view, 0, sizeof(returned_arg_view));
        returned_arg_view.dynamic_tag_local_id = (size_t)-1;
        memset(&returned_arg_resolution, 0, sizeof(returned_arg_resolution));
        ir_init_function_object_descriptor(&returned_arg_resolution.descriptor);
        if (ir_try_get_recursive_returned_function_value_parameter_argument_expr(
                ctx,
                callee_base_expr,
                NULL,
                NULL,
                &returned_arg_expr) &&
            returned_arg_expr &&
            (returned_arg_base =
                ir_unwrap_function_value_wrapper_expression(returned_arg_expr)) &&
            ((returned_arg_base &&
                returned_arg_base->kind == AST_EXPR_CALL &&
                ir_try_resolve_returned_function_object_resolution(
                    ctx,
                    returned_arg_base,
                    "__retfn_argcap",
                    "__retclosure_argcap",
                    &returned_arg_resolution) &&
                ir_init_callable_object_view_from_function_object_descriptor(
                    &returned_arg_resolution.descriptor,
                    &returned_arg_view)) ||
                ((!returned_arg_base || returned_arg_base->kind != AST_EXPR_CALL) &&
                    ir_resolve_callable_object_view(ctx, returned_arg_expr, &returned_arg_view) &&
                    returned_arg_view.direct_target_name &&
                    returned_arg_view.direct_target_name[0] != '\0')) &&
            returned_arg_view.direct_target_name &&
            returned_arg_view.direct_target_name[0] != '\0') {
            if (returned_arg_base &&
                returned_arg_base->kind == AST_EXPR_CALL &&
                ir_callable_object_view_is_two_target_dynamic_family(&returned_arg_view) &&
                returned_arg_resolution.descriptor.dynamic_target_names &&
                returned_arg_resolution.descriptor.dynamic_target_names[0] &&
                returned_arg_resolution.descriptor.dynamic_target_names[1]) {
                const AstExternal *returned_target_external_a =
                    ir_find_named_function_external_prefer_definition(
                        ctx->ast_program,
                        returned_arg_resolution.descriptor.dynamic_target_names[0]);
                const AstExternal *returned_target_external_b =
                    ir_find_named_function_external_prefer_definition(
                        ctx->ast_program,
                        returned_arg_resolution.descriptor.dynamic_target_names[1]);
                size_t returned_hidden_prefix_count =
                    returned_arg_view.is_closure_family
                        ? returned_arg_view.capture_count
                        : 0u;

                if (!returned_target_external_a) {
                    returned_target_external_a =
                        ir_program_find_function_external_ref(
                            ctx->program,
                            returned_arg_resolution.descriptor.dynamic_target_names[0]);
                }
                if (!returned_target_external_b) {
                    returned_target_external_b =
                        ir_program_find_function_external_ref(
                            ctx->program,
                            returned_arg_resolution.descriptor.dynamic_target_names[1]);
                }

                if (returned_target_external_a &&
                    returned_target_external_b &&
                    returned_target_external_a->parameter_count ==
                        call_expr->as.call.arg_count + returned_hidden_prefix_count &&
                    returned_target_external_b->parameter_count ==
                        call_expr->as.call.arg_count + returned_hidden_prefix_count &&
                    returned_target_external_a->parameter_value_kinds &&
                    returned_target_external_b->parameter_value_kinds &&
                    ir_external_body_needs_function_value_specialization(
                        ctx->ast_program,
                        returned_target_external_a,
                        returned_target_external_a->function_body) &&
                    ir_external_body_needs_function_value_specialization(
                        ctx->ast_program,
                        returned_target_external_b,
                        returned_target_external_b->function_body)) {
                    if (!ir_try_emit_returned_dynamic_function_value_specialized_call(
                            ctx,
                            call_expr,
                            returned_target_external_a,
                            returned_target_external_b,
                            &returned_arg_view,
                            returned_arg_resolution.descriptor.dynamic_target_names,
                            1,
                            out_value,
                            &applied_returned_parameter_dispatch)) {
                        returned_arg_resolution.descriptor.dynamic_target_names = NULL;
                        return 0;
                    }
                    returned_arg_resolution.descriptor.dynamic_target_names = NULL;
                    if (applied_returned_parameter_dispatch) {
                        (void)ir_clear_returned_callable_capture_slots(
                            &returned_arg_resolution.descriptor.owned_payload_local_ids);
                        if (out_result) {
                            out_result->handled = 1;
                            out_result->stop_outer_chain = 1;
                        }
                        return 1;
                    }
                }
            }
            if (!ir_try_emit_dynamic_callable_object_dispatch_from_call_expr(
                    ctx,
                    &returned_arg_view,
                    call_expr,
                    "IR-INT-027: out of memory while lowering returned function-value parameter call arguments",
                    "IR-LOWER-058: returned function-value parameter call is missing captured-value lowering metadata",
                    out_value,
                    &applied_returned_parameter_dispatch)) {
                return 0;
            }
            if (applied_returned_parameter_dispatch) {
                free((void *)returned_arg_resolution.descriptor.dynamic_target_names);
                returned_arg_resolution.descriptor.dynamic_target_names = NULL;
                (void)ir_clear_returned_callable_capture_slots(
                    &returned_arg_resolution.descriptor.owned_payload_local_ids);
                if (out_result) {
                    out_result->handled = 1;
                    out_result->stop_outer_chain = 1;
                }
                return 1;
            }
            if (!ir_try_emit_static_callable_object_dispatch(
                    ctx,
                    &returned_arg_view,
                    call_expr,
                    out_value,
                    &applied_returned_parameter_dispatch)) {
                free((void *)returned_arg_resolution.descriptor.dynamic_target_names);
                returned_arg_resolution.descriptor.dynamic_target_names = NULL;
                (void)ir_clear_returned_callable_capture_slots(
                    &returned_arg_resolution.descriptor.owned_payload_local_ids);
                return 0;
            }
            if (applied_returned_parameter_dispatch) {
                free((void *)returned_arg_resolution.descriptor.dynamic_target_names);
                returned_arg_resolution.descriptor.dynamic_target_names = NULL;
                (void)ir_clear_returned_callable_capture_slots(
                    &returned_arg_resolution.descriptor.owned_payload_local_ids);
                if (out_result) {
                    out_result->handled = 1;
                    out_result->stop_outer_chain = 1;
                }
                return 1;
            }
        }
        free((void *)returned_arg_resolution.descriptor.dynamic_target_names);
        returned_arg_resolution.descriptor.dynamic_target_names = NULL;
        (void)ir_clear_returned_callable_capture_slots(
            &returned_arg_resolution.descriptor.owned_payload_local_ids);
    }

    if (ir_try_resolve_returned_function_value_parameter_argument_info(
            ctx->ast_program,
            ctx,
            callee_base_expr,
            NULL,
            NULL,
            &returned_capture_local_ids,
            &returned_capture_count,
            &returned_arg_target_name,
            &returned_is_static_single_capture_closure_local) &&
        returned_arg_target_name &&
        returned_arg_target_name[0] != '\0') {
        *io_callee_name = returned_arg_target_name;
        ir_clear_returned_callable_capture_slots(
            io_immediate_returned_closure_capture_local_ids);
        *io_closure_prefix_arg_count = returned_capture_count;
        *io_closure_capture_local_ids = returned_capture_local_ids;
        *io_callee_is_closure_local = returned_capture_count > 0u;
        if (returned_is_static_single_capture_closure_local) {
            *io_callee_from_static_returned_parameter_closure = 1;
        }
        if (out_result) {
            out_result->stop_outer_chain = 0;
        }
        return 1;
    }

    if (!ir_try_resolve_returned_function_object_resolution(
            ctx,
            callee_base_expr,
            "__retfn_immediate",
            "__retclosure_immediate",
            &returned_resolution)) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-096: failed while resolving returned function-value object for '%s'",
                returned_callee_name ? returned_callee_name : "<null>");
        }
        return 0;
    }
    have_returned_resolution = 1;

    if (returned_resolution.is_closure_family) {
        int applied_dynamic_closure_dispatch = 0;
        IrLowerCallableObjectView returned_view;
        const AstExternal *returned_target_external_a = NULL;
        const AstExternal *returned_target_external_b = NULL;

        if (!ir_function_returned_closure_payload_slot_count(
                ctx->ast_program,
                ctx->program,
                return_function_external,
                io_closure_prefix_arg_count)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-INT-097: failed while resolving returned function-value payload slot count");
            }
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-059: returned closure payload shape is not supported in IR lowering");
            return 0;
        }

        *io_immediate_returned_closure_capture_local_ids = returned_resolution.descriptor.owned_payload_local_ids;
        *io_closure_prefix_arg_count = returned_resolution.descriptor.payload_slot_count;
        if (!returned_resolution.is_dynamic_two_target) {
            *io_callee_name = returned_resolution.helper_name;
            *io_callee_is_closure_local = 1;
            *io_closure_capture_local_ids = returned_resolution.descriptor.capture_local_ids;
            *io_closure_prefix_arg_count = returned_resolution.descriptor.capture_count;
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 1;
        }
        if (!(returned_resolution.descriptor.dynamic_target_names &&
                returned_resolution.descriptor.dynamic_target_names[0] &&
                returned_resolution.descriptor.dynamic_target_names[1] &&
                *io_closure_prefix_arg_count >= 1u)) {
            ir_clear_returned_callable_capture_slots(
                io_immediate_returned_closure_capture_local_ids);
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-070: returned dynamic closure family currently supports at most two targets");
            return 0;
        }

        if (!ir_init_callable_object_view_from_function_object_descriptor(
                &returned_resolution.descriptor,
                &returned_view)) {
            ir_clear_returned_callable_capture_slots(
                io_immediate_returned_closure_capture_local_ids);
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }

        if (returned_resolution.descriptor.dynamic_target_names &&
            returned_resolution.descriptor.dynamic_target_names[0] &&
            returned_resolution.descriptor.dynamic_target_names[1]) {
            returned_target_external_a =
                ir_find_named_function_external_prefer_definition(
                    ctx->ast_program,
                    returned_resolution.descriptor.dynamic_target_names[0]);
            returned_target_external_b =
                ir_find_named_function_external_prefer_definition(
                    ctx->ast_program,
                    returned_resolution.descriptor.dynamic_target_names[1]);
            if (!returned_target_external_a) {
                returned_target_external_a =
                    ir_program_find_function_external_ref(
                        ctx->program,
                        returned_resolution.descriptor.dynamic_target_names[0]);
            }
            if (!returned_target_external_b) {
                returned_target_external_b =
                    ir_program_find_function_external_ref(
                        ctx->program,
                        returned_resolution.descriptor.dynamic_target_names[1]);
            }
            if (returned_target_external_a &&
                returned_target_external_b &&
                returned_target_external_a->parameter_value_kinds &&
                returned_target_external_b->parameter_value_kinds &&
                ir_external_body_needs_function_value_specialization(
                    ctx->ast_program,
                    returned_target_external_a,
                    returned_target_external_a->function_body) &&
                ir_external_body_needs_function_value_specialization(
                    ctx->ast_program,
                    returned_target_external_b,
                    returned_target_external_b->function_body)) {
                if (!ir_try_emit_returned_dynamic_function_value_specialized_call(
                        ctx,
                        call_expr,
                        returned_target_external_a,
                        returned_target_external_b,
                        &returned_view,
                        returned_resolution.descriptor.dynamic_target_names,
                        1,
                        out_value,
                        &applied_dynamic_closure_dispatch)) {
                    ir_clear_returned_callable_capture_slots(
                        io_immediate_returned_closure_capture_local_ids);
                    returned_resolution.descriptor.dynamic_target_names = NULL;
                    return 0;
                }
                returned_resolution.descriptor.dynamic_target_names = NULL;
                if (applied_dynamic_closure_dispatch) {
                    if (!ir_finish_returned_callable_capture_cleanup_applied(
                            io_immediate_returned_closure_capture_local_ids,
                            &applied_dynamic_closure_dispatch)) {
                        return 0;
                    }
                    if (out_result) {
                        out_result->handled = applied_dynamic_closure_dispatch;
                        out_result->stop_outer_chain = applied_dynamic_closure_dispatch;
                    }
                    return 1;
                }
            }
        }

        if (!ir_try_emit_dynamic_closure_callable_dispatch(
                ctx,
                &returned_view,
                call_expr,
                "IR-INT-027: out of memory while lowering dynamic returned closure call arguments",
                "IR-LOWER-058: dynamic returned closure call is missing captured-value lowering metadata",
                out_value,
                &applied_dynamic_closure_dispatch)) {
            ir_clear_returned_callable_capture_slots(
                io_immediate_returned_closure_capture_local_ids);
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        if (applied_dynamic_closure_dispatch) {
            if (!ir_finish_returned_callable_capture_cleanup_applied(
                    io_immediate_returned_closure_capture_local_ids,
                    &applied_dynamic_closure_dispatch)) {
                return 0;
            }
            if (out_result) {
                out_result->handled = applied_dynamic_closure_dispatch;
                out_result->stop_outer_chain = applied_dynamic_closure_dispatch;
            }
            return 1;
        }
        return 1;
    }

    {
        IrLowerCallableObjectView returned_view;
        const AstExternal *returned_target_external_a = NULL;
        const AstExternal *returned_target_external_b = NULL;
        int returned_targets_need_specialization = 0;
        IrValueRef *call_args = NULL;
        const IrValueRef *call_args_view = NULL;
        size_t callable_arg_count = 0u;
        IrValueRef result_value;

        if (!have_returned_resolution ||
            returned_resolution.is_closure_family ||
            !ir_init_callable_object_view_from_function_object_descriptor(
                &returned_resolution.descriptor,
                &returned_view)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    call_expr->line,
                    call_expr->column,
                    "IR-INT-100: failed while collecting returned noncapturing target names");
            }
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-LOWER-071: failed to resolve static returned function-value parameter target");
            return 0;
        }
        if (!returned_resolution.is_dynamic_two_target) {
            int applied_static_returned_dispatch = 0;

            if (!ir_try_emit_static_callable_object_dispatch(
                    ctx,
                    &returned_view,
                    call_expr,
                    out_value,
                    &applied_static_returned_dispatch)) {
                return 0;
            }
            if (applied_static_returned_dispatch) {
                if (out_result) {
                    out_result->handled = 1;
                    out_result->stop_outer_chain = 1;
                }
                return 1;
            }
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-101: failed while materializing returned noncapturing callable view");
            return 0;
        }
        if (returned_resolution.descriptor.dynamic_target_names &&
            returned_resolution.descriptor.dynamic_target_names[0] &&
            returned_resolution.descriptor.dynamic_target_names[1]) {
            returned_target_external_a =
                ir_find_named_function_external_prefer_definition(
                    ctx->ast_program,
                    returned_resolution.descriptor.dynamic_target_names[0]);
            returned_target_external_b =
                ir_find_named_function_external_prefer_definition(
                    ctx->ast_program,
                    returned_resolution.descriptor.dynamic_target_names[1]);
            if (!returned_target_external_a) {
                returned_target_external_a =
                    ir_program_find_function_external_ref(
                        ctx->program,
                        returned_resolution.descriptor.dynamic_target_names[0]);
            }
            if (!returned_target_external_b) {
                returned_target_external_b =
                    ir_program_find_function_external_ref(
                        ctx->program,
                        returned_resolution.descriptor.dynamic_target_names[1]);
            }
            if (returned_target_external_a &&
                returned_target_external_b &&
                returned_target_external_a->parameter_count ==
                    call_expr->as.call.arg_count + returned_view.capture_count &&
                returned_target_external_b->parameter_count ==
                    call_expr->as.call.arg_count + returned_view.capture_count &&
                returned_target_external_a->parameter_value_kinds &&
                returned_target_external_b->parameter_value_kinds &&
                ir_external_body_needs_function_value_specialization(
                    ctx->ast_program,
                    returned_target_external_a,
                    returned_target_external_a->function_body) &&
                ir_external_body_needs_function_value_specialization(
                    ctx->ast_program,
                    returned_target_external_b,
                    returned_target_external_b->function_body)) {
                returned_targets_need_specialization = 1;
            }
        }
        if (returned_targets_need_specialization) {
            if (!ir_try_emit_returned_dynamic_function_value_specialized_call(
                    ctx,
                    call_expr,
                    returned_target_external_a,
                    returned_target_external_b,
                    &returned_view,
                    returned_resolution.descriptor.dynamic_target_names,
                    1,
                    out_value,
                    NULL)) {
                free((void *)returned_resolution.descriptor.dynamic_target_names);
                return 0;
            }
            if (out_result) {
                out_result->handled = 1;
                out_result->stop_outer_chain = 1;
            }
            return 1;
        }
        if (!ir_build_dynamic_visible_callable_args_view(
                ctx,
                call_expr,
                "IR-INT-126: out of memory while lowering callable-object arguments",
                &call_args,
                &call_args_view,
                &callable_arg_count)) {
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }

        if (!ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
                ctx,
                &returned_view,
                call_args_view,
                callable_arg_count,
                &result_value,
                NULL)) {
            free(call_args);
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }
        free(call_args);
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        if (!ir_finish_callable_dispatch_result(result_value, out_value, NULL)) {
            return 0;
        }
        if (out_result) {
            out_result->handled = 1;
            out_result->stop_outer_chain = 1;
        }
        return 1;
    }
}

static int ir_try_emit_direct_target_static_callee_dispatch(IrLowerContext *ctx,
    const IrLowerCallableObjectView *resolved_callee_view,
    const AstExpression *callee_base_expr,
    const char *callee_name,
    int callee_is_closure_local,
    int callee_from_static_returned_parameter_closure,
    const size_t *closure_capture_local_ids,
    size_t closure_prefix_arg_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_applied) {
        *out_applied = 0;
    }
    (void)callee_from_static_returned_parameter_closure;
    if (!ctx || !call_expr || !out_value || !callee_name || callee_name[0] == '\0') {
        return 1;
    }

    if (resolved_callee_view &&
        resolved_callee_view->direct_target_name &&
        resolved_callee_view->direct_target_name[0] != '\0' &&
        ctx->ast_program &&
        call_expr->as.call.args) {
        const AstExternal *resolved_target_external =
            ir_find_named_function_external_prefer_definition(
                ctx->ast_program,
                resolved_callee_view->direct_target_name);
        if (resolved_target_external &&
            resolved_target_external->parameter_value_kinds) {
            size_t function_param_count = 0u;

            for (size_t i = 0u; i < resolved_target_external->parameter_count; ++i) {
                if (resolved_target_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                    continue;
                }
                function_param_count += 1u;
            }

            if (!resolved_callee_view->is_closure_family &&
                function_param_count > 1u) {
                for (size_t i = 0u; i < resolved_target_external->parameter_count; ++i) {
                    IrLowerCallableObjectView dynamic_arg_view;
                    const char *const *owned_dynamic_target_names = NULL;
                    int requires_materialized_capture_forwarding = 0;
                    long long dynamic_then_tag_value = 1;

                    if (resolved_target_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION ||
                        i >= call_expr->as.call.arg_count) {
                        continue;
                    }

                    memset(&dynamic_arg_view, 0, sizeof(dynamic_arg_view));
                    dynamic_arg_view.dynamic_tag_local_id = (size_t)-1;
                    if (ir_try_resolve_dynamic_function_value_specialization_callable_object_view(
                            ctx,
                            call_expr->as.call.args[i],
                            resolved_target_external,
                            i,
                            &dynamic_arg_view,
                            &owned_dynamic_target_names,
                            &requires_materialized_capture_forwarding,
                            &dynamic_then_tag_value)) {
                        ir_free_callable_object_view_storage(&dynamic_arg_view);
                        free((void *)owned_dynamic_target_names);
                        return 1;
                    }
                }
            }
        }
    }

    if (resolved_callee_view &&
        resolved_callee_view->direct_target_name &&
        resolved_callee_view->direct_target_name[0] != '\0' &&
        (!resolved_callee_view->has_dynamic_family ||
            resolved_callee_view->dynamic_tag_local_id == (size_t)-1 ||
            resolved_callee_view->has_const_tag_value)) {
        return ir_try_emit_static_callable_object_dispatch(
            ctx,
            resolved_callee_view,
            call_expr,
            out_value,
            out_applied);
    }

    if (callee_is_closure_local) {
        return ir_try_emit_static_direct_target_callable_dispatch(
            ctx,
            callee_name,
            1,
            closure_capture_local_ids,
            closure_prefix_arg_count,
            call_expr,
            out_value,
            out_applied);
    }

    if (!callee_is_closure_local &&
        callee_base_expr &&
        callee_base_expr->kind == AST_EXPR_CALL &&
        ir_extract_direct_call_callee_name(callee_base_expr->as.call.callee)) {
        return ir_try_emit_static_direct_target_callable_dispatch(
            ctx,
            callee_name,
            0,
            NULL,
            0u,
            call_expr,
            out_value,
            out_applied);
    }

    return 1;
}

static int ir_aggregate_slot_count_for_kind(const AstProgram *program,
    AstDeclarationValueKind value_kind,
    const char *type_name,
    size_t *out_slot_count) {
    if (out_slot_count) {
        *out_slot_count = 0u;
    }
    if (!out_slot_count) {
        return 0;
    }

    switch (value_kind) {
    case AST_DECLARATION_VALUE_INT:
    case AST_DECLARATION_VALUE_FLOAT:
        *out_slot_count = 1u;
        return 1;
    case AST_DECLARATION_VALUE_PAIR:
        *out_slot_count = 2u;
        return 1;
    case AST_DECLARATION_VALUE_STRUCT:
        {
            const AstStructType *struct_type = ir_find_struct_type(program, type_name);
            size_t field_index;
            size_t slot_count = 0u;

            if (!struct_type || !struct_type->field_value_kinds) {
                return 0;
            }
            for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
                size_t field_slots = 0u;
                const char *field_type_name =
                    struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL;

                if (!ir_aggregate_slot_count_for_kind(program,
                        (AstDeclarationValueKind)struct_type->field_value_kinds[field_index],
                        field_type_name,
                        &field_slots)) {
                    return 0;
                }
                slot_count += field_slots;
            }
            *out_slot_count = slot_count;
            return slot_count > 0u;
        }
    default:
        return 0;
    }
}

static int ir_struct_field_slot_offset(const AstProgram *program,
    const AstStructType *struct_type,
    size_t field_index,
    size_t *out_offset) {
    size_t offset = 0u;
    size_t i;

    if (out_offset) {
        *out_offset = 0u;
    }
    if (!struct_type || !struct_type->field_value_kinds || field_index > struct_type->field_count ||
        !out_offset) {
        return 0;
    }

    for (i = 0u; i < field_index; ++i) {
        size_t field_slots = 0u;
        const char *field_type_name =
            struct_type->field_type_names ? struct_type->field_type_names[i] : NULL;

        if (!ir_aggregate_slot_count_for_kind(program,
                (AstDeclarationValueKind)struct_type->field_value_kinds[i],
                field_type_name,
                &field_slots)) {
            return 0;
        }
        offset += field_slots;
    }

    *out_offset = offset;
    return 1;
}

static int ir_resolve_local_aggregate_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerBindingInfo *out_info) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;
    const AstExpression *aggregate_base = NULL;

    if (out_info) {
        memset(out_info, 0, sizeof(*out_info));
    }
    if (!ctx || !base) {
        return 0;
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name &&
        ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info) &&
        (binding_info.kind == IR_LOWER_BINDING_PAIR_LOCAL ||
            binding_info.kind == IR_LOWER_BINDING_STRUCT_LOCAL)) {
        if (out_info) {
            *out_info = binding_info;
        }
        return 1;
    }

    if (base->kind != AST_EXPR_MEMBER || !base->as.member.field_name) {
        return 0;
    }

    aggregate_base = ir_unwrap_paren_expression(base->as.member.base);
    if (!aggregate_base ||
        !ir_resolve_local_aggregate_binding(ctx, aggregate_base, &binding_info) ||
        binding_info.kind != IR_LOWER_BINDING_STRUCT_LOCAL) {
        return 0;
    }

    {
        const AstStructType *struct_type = ir_find_struct_type(ctx->ast_program, binding_info.type_name);
        size_t field_index;

        if (!struct_type || !struct_type->field_names || !struct_type->field_value_kinds) {
            return 0;
        }

        for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
            AstDeclarationValueKind field_kind;
            const char *field_type_name;
            size_t slot_offset = 0u;
            size_t slot_count = 0u;

            if (!struct_type->field_names[field_index] ||
                strcmp(base->as.member.field_name, struct_type->field_names[field_index]) != 0) {
                continue;
            }

            field_kind = (AstDeclarationValueKind)struct_type->field_value_kinds[field_index];
            field_type_name =
                struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL;
            if (field_kind != AST_DECLARATION_VALUE_PAIR &&
                field_kind != AST_DECLARATION_VALUE_STRUCT) {
                return 0;
            }
            if (!ir_struct_field_slot_offset(ctx->ast_program, struct_type, field_index, &slot_offset) ||
                !ir_aggregate_slot_count_for_kind(ctx->ast_program,
                    field_kind,
                    field_type_name,
                    &slot_count)) {
                return 0;
            }

            if (out_info) {
                memset(out_info, 0, sizeof(*out_info));
                out_info->local_id = binding_info.local_id + slot_offset;
                out_info->kind = field_kind == AST_DECLARATION_VALUE_PAIR
                    ? IR_LOWER_BINDING_PAIR_LOCAL
                    : IR_LOWER_BINDING_STRUCT_LOCAL;
                out_info->pair_second_local_id =
                    slot_count > 1u ? (binding_info.local_id + slot_offset + 1u)
                                    : (binding_info.local_id + slot_offset);
                out_info->aggregate_slot_count = slot_count;
                out_info->type_name = field_kind == AST_DECLARATION_VALUE_STRUCT ? field_type_name : NULL;
            }
            return 1;
        }
    }

    return 0;
}

static int ir_resolve_aggregate_lvalue_ref(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_lvalue) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const AstExpression *aggregate_base = NULL;
    IrLowerBindingInfo binding_info;

    if (out_lvalue) {
        memset(out_lvalue, 0, sizeof(*out_lvalue));
    }
    if (!ctx || !base || !out_lvalue || base->kind != AST_EXPR_MEMBER || !base->as.member.field_name) {
        return 0;
    }

    aggregate_base = ir_unwrap_paren_expression(base->as.member.base);
    if (!aggregate_base ||
        !ir_resolve_local_aggregate_binding(ctx, aggregate_base, &binding_info)) {
        return 0;
    }

    if (binding_info.kind == IR_LOWER_BINDING_PAIR_LOCAL) {
        if (strcmp(base->as.member.field_name, "first") == 0) {
            *out_lvalue = ir_value_local(binding_info.local_id);
            return 1;
        }
        if (strcmp(base->as.member.field_name, "second") == 0) {
            *out_lvalue = ir_value_local(binding_info.pair_second_local_id);
            return 1;
        }
        return 0;
    }

    if (binding_info.kind == IR_LOWER_BINDING_STRUCT_LOCAL) {
        const AstStructType *struct_type = ir_find_struct_type(ctx->ast_program, binding_info.type_name);
        size_t field_index;

        if (!struct_type || !struct_type->field_names || !struct_type->field_value_kinds) {
            return 0;
        }
        for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
            AstDeclarationValueKind field_kind;
            size_t slot_offset = 0u;

            if (!struct_type->field_names[field_index] ||
                strcmp(base->as.member.field_name, struct_type->field_names[field_index]) != 0) {
                continue;
            }
            field_kind = (AstDeclarationValueKind)struct_type->field_value_kinds[field_index];
            if (field_kind != AST_DECLARATION_VALUE_INT &&
                field_kind != AST_DECLARATION_VALUE_FLOAT) {
                return 0;
            }
            if (!ir_struct_field_slot_offset(ctx->ast_program, struct_type, field_index, &slot_offset)) {
                return 0;
            }
            *out_lvalue = ir_value_local(binding_info.local_id + slot_offset);
            return 1;
        }
    }

    return 0;
}

static int ir_lower_initialize_aggregate_slots(IrLowerContext *ctx,
    IrLowerBindingKind kind,
    const char *type_name,
    size_t dest_first_local_id,
    size_t dest_slot_count,
    const AstExpression *initializer_expr,
    int line,
    int column) {
    const AstExpression *initializer_base = ir_unwrap_paren_expression(initializer_expr);
    IrLowerBindingInfo source_binding;
    size_t slot_index;

    if (!ctx || dest_slot_count == 0u) {
        return 0;
    }

    if (!initializer_base) {
        for (slot_index = 0u; slot_index < dest_slot_count; ++slot_index) {
            if (!ir_emit_mov(ctx, ir_value_local(dest_first_local_id + slot_index), ir_value_immediate(0))) {
                return 0;
            }
        }
        return 1;
    }

    if (initializer_base->kind == AST_EXPR_CALL) {
        if (ir_lower_call_into_aggregate_slots(ctx,
                initializer_base,
                kind,
                type_name,
                dest_first_local_id,
                dest_slot_count,
                0)) {
            return 1;
        }
    }

    if (ir_resolve_local_aggregate_binding(ctx, initializer_base, &source_binding)) {
        if (source_binding.kind != kind) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-LOWER-050: aggregate copy initialization requires matching aggregate kinds");
            return 0;
        }
        if (kind == IR_LOWER_BINDING_STRUCT_LOCAL &&
            ((!source_binding.type_name && type_name) ||
                (source_binding.type_name && !type_name) ||
                (source_binding.type_name && type_name &&
                    strcmp(source_binding.type_name, type_name) != 0))) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-LOWER-050: struct copy initialization requires matching struct types");
            return 0;
        }
        if (source_binding.aggregate_slot_count != dest_slot_count) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-LOWER-049: aggregate copy initialization requires matching lowering shape");
            return 0;
        }
        for (slot_index = 0u; slot_index < dest_slot_count; ++slot_index) {
            if (!ir_emit_mov(ctx,
                    ir_value_local(dest_first_local_id + slot_index),
                    ir_value_local(source_binding.local_id + slot_index))) {
                return 0;
            }
        }
        return 1;
    }

    if (initializer_base->kind != AST_EXPR_INIT_LIST) {
        ir_set_error(ctx->error,
            line,
            column,
            kind == IR_LOWER_BINDING_PAIR_LOCAL
                ? "IR-LOWER-045: pair initializers currently require an initializer list"
                : "IR-LOWER-051: struct initializers currently require an initializer list");
        return 0;
    }

    if (kind == IR_LOWER_BINDING_PAIR_LOCAL) {
        if (initializer_base->as.init_list.item_count > 2u) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-LOWER-045: pair initializers currently require an initializer list");
            return 0;
        }
        for (slot_index = 0u; slot_index < 2u; ++slot_index) {
            IrValueRef field_value = ir_value_immediate(0);

            if (initializer_base->as.init_list.items &&
                slot_index < initializer_base->as.init_list.item_count &&
                initializer_base->as.init_list.items[slot_index]) {
                if (!ir_lower_expression(ctx, initializer_base->as.init_list.items[slot_index], &field_value)) {
                    return 0;
                }
            }
            if (!ir_emit_mov(ctx, ir_value_local(dest_first_local_id + slot_index), field_value)) {
                return 0;
            }
        }
        return 1;
    }

    if (kind == IR_LOWER_BINDING_STRUCT_LOCAL) {
        const AstStructType *struct_type = ir_find_struct_type(ctx->ast_program, type_name);
        size_t field_index;

        if (!struct_type || !struct_type->field_names || !struct_type->field_value_kinds) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-LOWER-049: unsupported struct declaration lowering shape");
            return 0;
        }
        if (initializer_base->as.init_list.item_count > struct_type->field_count) {
            ir_set_error(ctx->error,
                line,
                column,
                "IR-LOWER-051: struct initializers currently require an initializer list");
            return 0;
        }

        for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
            const AstExpression *item = NULL;
            AstDeclarationValueKind field_kind;
            const char *field_type_name;
            size_t slot_offset = 0u;
            size_t field_slot_count = 0u;

            if (initializer_base->as.init_list.items &&
                field_index < initializer_base->as.init_list.item_count) {
                item = initializer_base->as.init_list.items[field_index];
            }
            field_kind = (AstDeclarationValueKind)struct_type->field_value_kinds[field_index];
            field_type_name =
                struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL;
            if (!ir_struct_field_slot_offset(ctx->ast_program, struct_type, field_index, &slot_offset) ||
                !ir_aggregate_slot_count_for_kind(ctx->ast_program,
                    field_kind,
                    field_type_name,
                    &field_slot_count)) {
                ir_set_error(ctx->error,
                    line,
                    column,
                    "IR-LOWER-049: unsupported struct declaration lowering shape");
                return 0;
            }

            if (field_kind == AST_DECLARATION_VALUE_INT || field_kind == AST_DECLARATION_VALUE_FLOAT) {
                IrValueRef field_value = ir_value_immediate(0);

                if (item && !ir_lower_expression(ctx, item, &field_value)) {
                    return 0;
                }
                if (!ir_emit_mov(ctx, ir_value_local(dest_first_local_id + slot_offset), field_value)) {
                    return 0;
                }
                continue;
            }

            if (!ir_lower_initialize_aggregate_slots(ctx,
                    field_kind == AST_DECLARATION_VALUE_PAIR
                        ? IR_LOWER_BINDING_PAIR_LOCAL
                        : IR_LOWER_BINDING_STRUCT_LOCAL,
                    field_kind == AST_DECLARATION_VALUE_STRUCT ? field_type_name : NULL,
                    dest_first_local_id + slot_offset,
                    field_slot_count,
                    item,
                    line,
                    column)) {
                return 0;
            }
        }
        return 1;
    }

    return 0;
}

static int ir_lower_append_call_argument(IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t param_index,
    const AstExpression *arg_expr,
    IrValueRef **io_args,
    size_t *io_arg_count) {
    IrValueRef *new_args;
    size_t next_count;
    AstDeclarationValueKind aggregate_kind = AST_DECLARATION_VALUE_INT;
    int is_aggregate_param = 0;

    if (!ctx || !io_args || !io_arg_count || !callee_external || param_index >= callee_external->parameter_count) {
        return 0;
    }
    switch (callee_external->parameter_value_kinds
                ? callee_external->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT) {
    case AST_PARAMETER_VALUE_PAIR:
        aggregate_kind = AST_DECLARATION_VALUE_PAIR;
        is_aggregate_param = 1;
        break;
    case AST_PARAMETER_VALUE_STRUCT:
        aggregate_kind = AST_DECLARATION_VALUE_STRUCT;
        is_aggregate_param = 1;
        break;
    default:
        is_aggregate_param = 0;
        break;
    }

    if (!is_aggregate_param) {
        IrValueRef lowered_arg;

        if (ctx->ast_program &&
            ir_call_argument_resolve_extension_function_value_target(
                ctx->ast_program,
                ctx,
                callee_external,
                param_index,
                arg_expr,
                NULL,
                NULL,
                NULL)) {
            lowered_arg = ir_value_immediate(0);
        } else if (!ir_lower_expression(ctx, arg_expr, &lowered_arg)) {
            return 0;
        }

        next_count = *io_arg_count + 1u;
        new_args = *io_args
            ? (IrValueRef *)realloc(*io_args, next_count * sizeof(IrValueRef))
            : (IrValueRef *)malloc(next_count * sizeof(IrValueRef));
        if (!new_args) {
            ir_set_error(ctx->error,
                arg_expr ? arg_expr->line : 0,
                arg_expr ? arg_expr->column : 0,
                "IR-INT-027: out of memory while lowering call arguments");
            return 0;
        }
        *io_args = new_args;
        (*io_args)[*io_arg_count] = lowered_arg;
        *io_arg_count = next_count;
        return 1;
    }

    {
        IrLowerBindingInfo binding_info;
        size_t slot_index;
        const char *expected_type_name =
            callee_external->parameter_type_names ? callee_external->parameter_type_names[param_index] : NULL;

        if (arg_expr && ir_resolve_local_aggregate_binding(ctx, arg_expr, &binding_info)) {
            if ((aggregate_kind == AST_DECLARATION_VALUE_PAIR &&
                    binding_info.kind != IR_LOWER_BINDING_PAIR_LOCAL) ||
                (aggregate_kind == AST_DECLARATION_VALUE_STRUCT &&
                    binding_info.kind != IR_LOWER_BINDING_STRUCT_LOCAL)) {
                ir_set_error(ctx->error,
                    arg_expr->line,
                    arg_expr->column,
                    "IR-LOWER-026: aggregate call arguments currently require matching aggregate kinds");
                return 0;
            }
            if (aggregate_kind == AST_DECLARATION_VALUE_STRUCT &&
                ((!expected_type_name && binding_info.type_name) ||
                    (expected_type_name && !binding_info.type_name) ||
                    (expected_type_name && binding_info.type_name &&
                        strcmp(expected_type_name, binding_info.type_name) != 0))) {
                ir_set_error(ctx->error,
                    arg_expr->line,
                    arg_expr->column,
                    "IR-LOWER-026: aggregate call arguments currently require matching struct types");
                return 0;
            }

            next_count = *io_arg_count + binding_info.aggregate_slot_count;
            new_args = *io_args
                ? (IrValueRef *)realloc(*io_args, next_count * sizeof(IrValueRef))
                : (IrValueRef *)malloc(next_count * sizeof(IrValueRef));
            if (!new_args) {
                ir_set_error(ctx->error,
                    arg_expr->line,
                    arg_expr->column,
                    "IR-INT-027: out of memory while lowering call arguments");
                return 0;
            }
            *io_args = new_args;
            for (slot_index = 0u; slot_index < binding_info.aggregate_slot_count; ++slot_index) {
                (*io_args)[*io_arg_count + slot_index] = ir_value_local(binding_info.local_id + slot_index);
            }
            *io_arg_count = next_count;
            return 1;
        }

        if (arg_expr) {
            size_t temp_first_local_id = ctx->function ? ctx->function->local_count : 0u;
            size_t aggregate_slot_count = 0u;

            if (!ir_aggregate_slot_count_for_kind(ctx->ast_program,
                    aggregate_kind,
                    aggregate_kind == AST_DECLARATION_VALUE_STRUCT ? expected_type_name : NULL,
                    &aggregate_slot_count) ||
                aggregate_slot_count == 0u) {
                ir_set_error(ctx->error,
                    arg_expr->line,
                    arg_expr->column,
                    "IR-LOWER-026: aggregate call arguments currently require a lowered aggregate local root");
                return 0;
            }
            for (slot_index = 0u; slot_index < aggregate_slot_count; ++slot_index) {
                char temp_name[64];
                size_t local_id = (size_t)-1;

                snprintf(temp_name, sizeof(temp_name), "__aggarg_tmp_%zu", temp_first_local_id + slot_index);
                if (!ir_function_append_local(ctx->function, temp_name, 0, &local_id, ctx->error) ||
                    local_id != temp_first_local_id + slot_index) {
                    return 0;
                }
            }
            if (!ir_lower_initialize_aggregate_slots(
                    ctx,
                    aggregate_kind == AST_DECLARATION_VALUE_PAIR
                        ? IR_LOWER_BINDING_PAIR_LOCAL
                        : IR_LOWER_BINDING_STRUCT_LOCAL,
                    aggregate_kind == AST_DECLARATION_VALUE_STRUCT ? expected_type_name : NULL,
                    temp_first_local_id,
                    aggregate_slot_count,
                    arg_expr,
                    arg_expr->line,
                    arg_expr->column)) {
                return 0;
            }

            next_count = *io_arg_count + aggregate_slot_count;
            new_args = *io_args
                ? (IrValueRef *)realloc(*io_args, next_count * sizeof(IrValueRef))
                : (IrValueRef *)malloc(next_count * sizeof(IrValueRef));
            if (!new_args) {
                ir_set_error(ctx->error,
                    arg_expr->line,
                    arg_expr->column,
                    "IR-INT-027: out of memory while lowering call arguments");
                return 0;
            }
            *io_args = new_args;
            for (slot_index = 0u; slot_index < aggregate_slot_count; ++slot_index) {
                (*io_args)[*io_arg_count + slot_index] = ir_value_local(temp_first_local_id + slot_index);
            }
            *io_arg_count = next_count;
            return 1;
        }

        ir_set_error(ctx->error,
            0,
            0,
            "IR-LOWER-026: aggregate call arguments currently require a lowered aggregate local root");
        return 0;
    }
}

static int ir_lower_call_into_aggregate_slots(IrLowerContext *ctx,
    const AstExpression *call_expr,
    IrLowerBindingKind expected_kind,
    const char *expected_type_name,
    size_t dest_first_local_id,
    size_t dest_slot_count,
    int dest_slots_are_addresses) {
    const AstExpression *base = ir_unwrap_paren_expression(call_expr);
    const char *callee_name = NULL;
    const AstExternal *callee_external = NULL;
    IrFunction *callee_function = NULL;
    IrValueRef *call_args = NULL;
    size_t arg_count = 0u;
    size_t source_arg_count = 0u;
    size_t hidden_ret_index;
    size_t source_arg_index;
    size_t dummy_temp_id;
    IrValueRef ignored_result;
    const char *current_function_name = NULL;

    if (!ctx || !base || base->kind != AST_EXPR_CALL || !ctx->program || !ctx->ast_program) {
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;
    callee_name = ir_extract_direct_call_callee_name(base->as.call.callee);
    if (!callee_name || callee_name[0] == '\0') {
        return 0;
    }

    callee_function = ir_program_find_function_mut(ctx->program, callee_name);
    if (!callee_function &&
        !ir_ensure_builtin_function_signature(ctx->program, callee_name, &callee_function, ctx->error)) {
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-LOWER-025: call target '%s' is not declared in IR lowering context",
            callee_name);
        return 0;
    }
    if (!ir_lower_refresh_current_function(ctx, current_function_name, base->line, base->column)) {
        return 0;
    }

    for (source_arg_index = 0u; source_arg_index < ctx->ast_program->count; ++source_arg_index) {
        const AstExternal *candidate = &ctx->ast_program->externals[source_arg_index];
        if (candidate->kind == AST_EXTERNAL_FUNCTION &&
            candidate->name &&
            strcmp(candidate->name, callee_name) == 0) {
            callee_external = candidate;
            break;
        }
    }
    if (!callee_external) {
        return 0;
    }
    if ((expected_kind == IR_LOWER_BINDING_PAIR_LOCAL &&
            callee_external->function_return_type != AST_FUNCTION_RETURN_PAIR) ||
        (expected_kind == IR_LOWER_BINDING_STRUCT_LOCAL &&
            callee_external->function_return_type != AST_FUNCTION_RETURN_STRUCT)) {
        return 0;
    }
    if (expected_kind == IR_LOWER_BINDING_STRUCT_LOCAL) {
        const char *callee_type_name = callee_external->function_return_type_name;
        if ((!expected_type_name && callee_type_name) ||
            (expected_type_name && !callee_type_name) ||
            (expected_type_name && callee_type_name &&
                strcmp(expected_type_name, callee_type_name) != 0)) {
            return 0;
        }
    }

    source_arg_count = base->as.call.arg_count;
    if (source_arg_count != callee_external->parameter_count) {
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-LOWER-026: call to '%s' has %zu args but callee source signature expects %zu",
            callee_name,
            source_arg_count,
            callee_external->parameter_count);
        return 0;
    }

    for (hidden_ret_index = 0u; hidden_ret_index < dest_slot_count; ++hidden_ret_index) {
        IrValueRef *new_args = (IrValueRef *)realloc(call_args, (arg_count + 1u) * sizeof(IrValueRef));
        IrValueRef addr_arg;
        if (!new_args) {
            free(call_args);
            ir_set_error(ctx->error,
                base->line,
                base->column,
                "IR-INT-027: out of memory while lowering call arguments");
            return 0;
        }
        call_args = new_args;
        if (dest_slots_are_addresses) {
            addr_arg = ir_value_local(dest_first_local_id + hidden_ret_index);
        } else {
            size_t temp_id = ir_allocate_temp(ctx->function);

            addr_arg = ir_value_temp(temp_id);
            if (!ir_emit_addr_local(ctx, addr_arg, dest_first_local_id + hidden_ret_index)) {
                free(call_args);
                return 0;
            }
        }
        call_args[arg_count++] = addr_arg;
    }

    for (source_arg_index = 0u; source_arg_index < source_arg_count; ++source_arg_index) {
        if (!ir_lower_append_call_argument(
                ctx,
                callee_external,
                source_arg_index,
                base->as.call.args[source_arg_index],
                &call_args,
                &arg_count)) {
            free(call_args);
            return 0;
        }
    }

    if (!callee_function || arg_count != callee_function->parameter_count) {
        free(call_args);
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-LOWER-026: call to '%s' lowers to %zu args but callee signature expects %zu",
            callee_name,
            arg_count,
            callee_function ? callee_function->parameter_count : 0u);
        return 0;
    }

    dummy_temp_id = ir_allocate_temp(ctx->function);
    ignored_result = ir_value_temp(dummy_temp_id);
    if (!ir_emit_call(ctx, ignored_result, callee_name, call_args, arg_count)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    return 1;
}

static int ir_emit_copy_local_slot_range(IrLowerContext *ctx,
    size_t dest_first_local_id,
    size_t source_first_local_id,
    size_t slot_count) {
    size_t slot_index;

    if (!ctx) {
        return 0;
    }

    for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
        if (!ir_emit_mov(ctx,
                ir_value_local(dest_first_local_id + slot_index),
                ir_value_local(source_first_local_id + slot_index))) {
            return 0;
        }
    }

    return 1;
}

static int ir_emit_store_slot_range_to_hidden_return_addresses(IrLowerContext *ctx,
    size_t source_first_local_id,
    size_t slot_count) {
    size_t slot_index;

    if (!ctx) {
        return 0;
    }

    for (slot_index = 0u; slot_index < slot_count; ++slot_index) {
        if (!ir_emit_store_indirect(
                ctx,
                ir_value_local(ctx->hidden_return_first_local_id + slot_index),
                ir_value_local(source_first_local_id + slot_index))) {
            return 0;
        }
    }

    return 1;
}

static int ir_lower_call_into_returned_closure_slots(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *expected_return_function_external,
    size_t dest_first_local_id,
    size_t dest_slot_count) {
    const AstExpression *base = ir_unwrap_paren_expression(call_expr);
    const char *callee_name = NULL;
    const AstExternal *callee_external = NULL;
    IrFunction *callee_function = NULL;
    IrValueRef *call_args = NULL;
    size_t arg_count = 0u;
    size_t source_arg_count = 0u;
    size_t hidden_ret_index;
    size_t source_arg_index;
    size_t dummy_temp_id;
    IrValueRef ignored_result;
    const char *current_function_name = NULL;
    int call_uses_function_value_specialization = 0;
    const AstExpression *returned_param_arg_expr = NULL;
    const AstExpression *returned_param_arg_base = NULL;
    const size_t *returned_param_capture_local_ids = NULL;
    size_t returned_param_capture_count = 0u;
    const char *returned_param_target_name = NULL;

    if (!ctx || !base || base->kind != AST_EXPR_CALL || !ctx->program || !ctx->ast_program ||
        !expected_return_function_external || !expected_return_function_external->has_function_return_signature) {
        if (ctx && ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                0,
                0,
                "IR-INT-102: invalid returned-call materialization contract");
        }
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;

    callee_name = ir_extract_direct_call_callee_name(base->as.call.callee);
    if (!callee_name || callee_name[0] == '\0') {
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-INT-103: returned-call materialization requires a direct identifier callee");
        return 0;
    }

    callee_external = ir_find_named_function_external_prefer_definition(ctx->ast_program, callee_name);
    if (!callee_external || !callee_external->has_function_return_signature ||
        strcmp(callee_external->name, expected_return_function_external->name) != 0) {
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-INT-104: returned-call materialization callee '%s' does not match expected function-value producer",
            callee_name);
        return 0;
    }

    callee_function = ir_program_find_function_mut(ctx->program, callee_name);
    if (!callee_function &&
        !ir_ensure_function_signature(ctx->ast_program, ctx->program, callee_external, &callee_function, ctx->error)) {
        return 0;
    }

    source_arg_count = base->as.call.arg_count;
    if (source_arg_count != callee_external->parameter_count) {
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-LOWER-026: call to '%s' has %zu args but callee source signature expects %zu",
            callee_name,
            source_arg_count,
            callee_external->parameter_count);
        return 0;
    }

    if (ir_try_resolve_returned_function_value_parameter_argument_info(
            ctx->ast_program,
            ctx,
            base,
            &returned_param_arg_expr,
            &returned_param_arg_base,
            &returned_param_capture_local_ids,
            &returned_param_capture_count,
            &returned_param_target_name,
            NULL) &&
        returned_param_arg_expr &&
        returned_param_target_name &&
        returned_param_target_name[0] != '\0') {
        if (returned_param_arg_base &&
            returned_param_arg_base->kind == AST_EXPR_IDENTIFIER) {
            IrLowerBindingInfo returned_param_binding_info;

            memset(&returned_param_binding_info, 0, sizeof(returned_param_binding_info));
            if (ir_lower_scope_lookup_binding(
                    &ctx->scopes,
                    returned_param_arg_base->as.identifier.name,
                    &returned_param_binding_info) &&
                (returned_param_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
                    returned_param_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) &&
                returned_param_binding_info.function_target_tag_local_id != (size_t)-1 &&
                returned_param_binding_info.closure_capture_count + 1u == dest_slot_count &&
                returned_param_binding_info.closure_capture_local_ids) {
                size_t payload_index;

                if (!ir_emit_mov(
                        ctx,
                        ir_value_local(dest_first_local_id),
                        returned_param_binding_info.has_const_value
                            ? ir_value_immediate(returned_param_binding_info.const_value)
                            : ir_value_local(returned_param_binding_info.function_target_tag_local_id))) {
                    return 0;
                }
                for (payload_index = 0u;
                        payload_index < returned_param_binding_info.closure_capture_count;
                        ++payload_index) {
                    if (!ir_emit_mov(
                            ctx,
                            ir_value_local(dest_first_local_id + 1u + payload_index),
                            ir_value_local(returned_param_binding_info.closure_capture_local_ids[payload_index]))) {
                        return 0;
                    }
                }
                return 1;
            }
        }
        if (returned_param_capture_count == dest_slot_count &&
            returned_param_capture_local_ids &&
            returned_param_capture_count > 0u) {
            size_t payload_index;

            for (payload_index = 0u; payload_index < returned_param_capture_count; ++payload_index) {
                if (!ir_emit_mov(
                        ctx,
                        ir_value_local(dest_first_local_id + payload_index),
                        ir_value_local(returned_param_capture_local_ids[payload_index]))) {
                    return 0;
                }
            }
            return 1;
        }
        if (returned_param_capture_count == 0u &&
            dest_slot_count == 1u &&
            returned_param_arg_base &&
            returned_param_arg_base->kind == AST_EXPR_IDENTIFIER) {
            IrLowerBindingInfo returned_param_binding_info;

            memset(&returned_param_binding_info, 0, sizeof(returned_param_binding_info));
            if (ir_lower_scope_lookup_binding(
                    &ctx->scopes,
                    returned_param_arg_base->as.identifier.name,
                    &returned_param_binding_info) &&
                returned_param_binding_info.function_target_tag_local_id != (size_t)-1) {
                return ir_emit_mov(
                    ctx,
                    ir_value_local(dest_first_local_id),
                    ir_value_local(returned_param_binding_info.function_target_tag_local_id));
            }
        }
        if (returned_param_arg_base &&
            returned_param_arg_base->kind == AST_EXPR_IDENTIFIER &&
            dest_slot_count > 1u) {
            IrLowerBindingInfo returned_param_binding_info;

            memset(&returned_param_binding_info, 0, sizeof(returned_param_binding_info));
            if (ir_lower_scope_lookup_binding(
                    &ctx->scopes,
                    returned_param_arg_base->as.identifier.name,
                    &returned_param_binding_info) &&
                (returned_param_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
                    returned_param_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) &&
                returned_param_binding_info.function_target_tag_local_id != (size_t)-1 &&
                returned_param_binding_info.closure_capture_count + 1u == dest_slot_count &&
                returned_param_binding_info.closure_capture_local_ids) {
                size_t payload_index;

                if (!ir_emit_mov(
                        ctx,
                        ir_value_local(dest_first_local_id),
                        returned_param_binding_info.has_const_value
                            ? ir_value_immediate(returned_param_binding_info.const_value)
                            : ir_value_local(returned_param_binding_info.function_target_tag_local_id))) {
                    return 0;
                }
                for (payload_index = 0u;
                        payload_index < returned_param_binding_info.closure_capture_count;
                        ++payload_index) {
                    if (!ir_emit_mov(
                            ctx,
                            ir_value_local(dest_first_local_id + 1u + payload_index),
                            ir_value_local(returned_param_binding_info.closure_capture_local_ids[payload_index]))) {
                        return 0;
                    }
                }
                return 1;
            }
        }
    }

    for (hidden_ret_index = 0u; hidden_ret_index < dest_slot_count; ++hidden_ret_index) {
        IrValueRef *new_args = (IrValueRef *)realloc(call_args, (arg_count + 1u) * sizeof(IrValueRef));
        IrValueRef addr_arg;
        if (!new_args) {
            free(call_args);
            ir_set_error(ctx->error,
                base->line,
                base->column,
                "IR-INT-027: out of memory while lowering returned-closure call arguments");
            return 0;
        }
        call_args = new_args;
        addr_arg = ir_value_temp(ir_allocate_temp(ctx->function));
        if (!ir_emit_addr_local(ctx, addr_arg, dest_first_local_id + hidden_ret_index)) {
            free(call_args);
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    base->line,
                    base->column,
                    "IR-INT-105: failed while materializing returned payload local address");
            }
            return 0;
        }
        call_args[arg_count++] = addr_arg;
    }

    if (callee_external &&
        callee_external->parameter_value_kinds &&
        ir_external_body_needs_function_value_specialization(
            ctx->ast_program,
            callee_external,
            callee_external->function_body)) {
        const char *specialized_name = NULL;
        int producer_specialization_needed = 1;

        if (expected_return_function_external->has_function_return_signature) {
            int expected_return_is_closure = 0;
            const char **returned_target_names = NULL;
            size_t returned_target_count = 0u;
            const char *returned_primary_target_name = NULL;

            if (ir_function_value_return_is_closure_family(
                    ctx->ast_program,
                    expected_return_function_external,
                    &expected_return_is_closure) &&
                expected_return_is_closure &&
                ir_resolve_returned_function_value_target_info(
                    ctx,
                    call_expr,
                    expected_return_function_external,
                    1,
                    &returned_target_names,
                    &returned_target_count,
                    &returned_primary_target_name) &&
                returned_target_count <= 1u &&
                returned_primary_target_name &&
                returned_primary_target_name[0] != '\0') {
                producer_specialization_needed = 0;
            }
            free((void *)returned_target_names);
        }

        if (!producer_specialization_needed) {
            goto returned_call_materialization_args;
        }

        if (!ir_build_function_value_specialization_name_from_call_args(
                ctx,
                callee_external,
                (const AstExpression *const *)base->as.call.args,
                source_arg_count,
                base->line,
                base->column,
                "IR-INT-029: out of memory while preparing returned function-value specialization",
                "IR-LOWER-028: failed to resolve returned function-value specialization target",
                "IR-INT-106: failed while building specialized returned-call helper",
                &specialized_name)) {
            free(call_args);
            return 0;
        }
        if (!ir_finish_function_value_specialization_callee_adoption(
                ctx,
                current_function_name,
                specialized_name,
                base->line,
                base->column,
                "IR-INT-107: failed while refreshing current function after returned-call specialization",
                &callee_name,
                &callee_function,
                &call_uses_function_value_specialization)) {
            free(call_args);
            return 0;
        }
    }

returned_call_materialization_args:
    for (source_arg_index = 0u; source_arg_index < source_arg_count; ++source_arg_index) {
        if (!ir_try_append_maybe_specialized_call_argument(
                ctx->ast_program,
                ctx,
                callee_external,
                source_arg_index,
                base->as.call.args[source_arg_index],
                call_uses_function_value_specialization,
                base->line,
                base->column,
                "IR-INT-108: failed while resolving specialized returned-call function-value argument",
                "IR-LOWER-058: specialized returned call is missing captured-value lowering metadata",
                "IR-INT-027: out of memory while lowering specialized returned call capture arguments",
                "IR-INT-109: failed while lowering returned-call ordinary argument %zu",
                NULL,
                &call_args,
                &arg_count)) {
            free(call_args);
            return 0;
        }
    }

    if (!callee_function || arg_count != callee_function->parameter_count) {
        free(call_args);
        ir_set_error(ctx->error,
            base->line,
            base->column,
            "IR-LOWER-026: call to '%s' lowers to %zu args but callee signature expects %zu",
            callee_name,
            arg_count,
            callee_function ? callee_function->parameter_count : 0u);
        return 0;
    }

    dummy_temp_id = ir_allocate_temp(ctx->function);
    ignored_result = ir_value_temp(dummy_temp_id);
    if (!ir_emit_call(ctx, ignored_result, callee_name, call_args, arg_count)) {
        free(call_args);
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                base->line,
                base->column,
                "IR-INT-110: failed while emitting returned payload materialization call '%s'",
                callee_name ? callee_name : "<null>");
        }
        return 0;
    }

    free(call_args);
    return 1;
}

static int ir_emit_function_value_return_payload_slot(IrLowerContext *ctx,
    int use_snapshot,
    size_t snapshot_first_local_id,
    IrValueRef payload_value) {
    if (!ctx) {
        return 0;
    }
    if (use_snapshot) {
        return ir_emit_mov(ctx, ir_value_local(snapshot_first_local_id), payload_value);
    }
    return ir_emit_store_indirect(
        ctx,
        ir_value_local(ctx->hidden_return_first_local_id),
        payload_value);
}

static int ir_materialize_returned_function_value_return_payload_slots(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    int use_snapshot,
    size_t snapshot_first_local_id,
    size_t payload_slot_count) {
    if (!ctx || !call_expr || !producer_external || !producer_external->has_function_return_signature ||
        payload_slot_count == 0u) {
        return 0;
    }

    return ir_lower_call_into_returned_closure_slots(
        ctx,
        call_expr,
        producer_external,
        use_snapshot ? snapshot_first_local_id : ctx->hidden_return_first_local_id,
        payload_slot_count);
}

static int ir_materialize_returned_function_value_payload_locals(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    const char *local_name_prefix,
    size_t payload_slot_count,
    size_t **out_payload_local_ids) {
    size_t *payload_local_ids = NULL;
    size_t payload_index;

    if (out_payload_local_ids) {
        *out_payload_local_ids = NULL;
    }
    if (!ctx || !call_expr || !producer_external || !producer_external->has_function_return_signature ||
        !local_name_prefix || local_name_prefix[0] == '\0' || payload_slot_count == 0u ||
        !out_payload_local_ids) {
        return 0;
    }

    payload_local_ids = (size_t *)malloc(payload_slot_count * sizeof(size_t));
    if (!payload_local_ids) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-029: out of memory while preparing returned function-value payload bridge");
        return 0;
    }
    for (payload_index = 0u; payload_index < payload_slot_count; ++payload_index) {
        char payload_local_name[256];

        snprintf(payload_local_name,
            sizeof(payload_local_name),
            "%s_%zu",
            local_name_prefix,
            payload_index);
        if (!ir_function_append_local(
                ctx->function,
                payload_local_name,
                0,
                &payload_local_ids[payload_index],
                ctx->error)) {
            free(payload_local_ids);
            return 0;
        }
    }
    if (!ir_lower_call_into_returned_closure_slots(
            ctx,
            call_expr,
            producer_external,
            payload_local_ids[0],
            payload_slot_count)) {
        free(payload_local_ids);
        return 0;
    }

    *out_payload_local_ids = payload_local_ids;
    return 1;
}

static int ir_append_named_capture_local_slots(IrLowerContext *ctx,
    const char *binding_name,
    size_t capture_count,
    size_t **out_capture_local_ids) {
    size_t *capture_local_ids = NULL;
    size_t capture_index;

    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (!ctx || !binding_name || binding_name[0] == '\0' || !out_capture_local_ids) {
        return 0;
    }
    if (capture_count == 0u) {
        *out_capture_local_ids = NULL;
        return 1;
    }

    capture_local_ids = (size_t *)malloc(capture_count * sizeof(size_t));
    if (!capture_local_ids) {
        return 0;
    }
    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        char capture_local_name[256];

        snprintf(capture_local_name,
            sizeof(capture_local_name),
            "%s$closurecap$%zu",
            binding_name,
            capture_index);
        if (!ir_function_append_local(
                ctx->function,
                capture_local_name,
                0,
                &capture_local_ids[capture_index],
                ctx->error)) {
            free(capture_local_ids);
            return 0;
        }
    }

    *out_capture_local_ids = capture_local_ids;
    return 1;
}

static int ir_copy_capture_local_slot_range(IrLowerContext *ctx,
    const size_t *dest_capture_local_ids,
    const size_t *source_capture_local_ids,
    size_t capture_count) {
    size_t capture_index;

    if (!ctx) {
        return 0;
    }
    if (capture_count == 0u) {
        return 1;
    }
    if (!dest_capture_local_ids || !source_capture_local_ids) {
        return 0;
    }

    for (capture_index = 0u; capture_index < capture_count; ++capture_index) {
        if (!ir_emit_mov(ctx,
                ir_value_local(dest_capture_local_ids[capture_index]),
                ir_value_local(source_capture_local_ids[capture_index]))) {
            return 0;
        }
    }
    return 1;
}

static int ir_materialize_returned_function_object_descriptor(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *producer_external,
    int is_closure_family,
    const char *direct_target_name,
    const char *const *dynamic_target_names,
    size_t dynamic_target_count,
    const char *payload_local_name_prefix,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    size_t payload_slot_count = 0u;
    size_t *payload_local_ids = NULL;

    ir_init_function_object_descriptor(out_descriptor);
    if (!ctx || !call_expr || !producer_external || !producer_external->has_function_return_signature ||
        !direct_target_name || direct_target_name[0] == '\0' || !out_descriptor) {
        return 0;
    }
    if (!ir_function_lowered_return_slot_count(
            ctx->ast_program,
            ctx->program,
            producer_external,
            &payload_slot_count)) {
        return 0;
    }
    if (payload_slot_count > 0u) {
        if (!payload_local_name_prefix || payload_local_name_prefix[0] == '\0') {
            return 0;
        }
        if (!ir_materialize_returned_function_value_payload_locals(
                ctx,
                call_expr,
                producer_external,
                payload_local_name_prefix,
                payload_slot_count,
                &payload_local_ids)) {
            return 0;
        }
    }
    if (!ir_init_function_object_descriptor_from_returned_lowered_payload(
            producer_external,
            is_closure_family,
            direct_target_name,
            dynamic_target_names,
            dynamic_target_count,
            payload_local_ids,
            payload_slot_count,
            out_descriptor)) {
        free(payload_local_ids);
        return 0;
    }
    return 1;
}

static int ir_try_get_returned_function_value_parameter_argument_expr(const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal **out_return_function_external,
    size_t *out_return_param_index,
    const AstExpression **out_arg_expr) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(call_expr);
    const AstExpression *callee_base = NULL;
    const AstExternal *return_function_external = NULL;
    size_t return_param_index = (size_t)-1;

    if (out_return_function_external) {
        *out_return_function_external = NULL;
    }
    if (out_return_param_index) {
        *out_return_param_index = (size_t)-1;
    }
    if (out_arg_expr) {
        *out_arg_expr = NULL;
    }
    if (!ctx || !ctx->ast_program || !base || base->kind != AST_EXPR_CALL) {
        return 0;
    }

    callee_base = ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
    if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
        return 0;
    }

    return_function_external =
        ir_find_named_function_external_prefer_definition(ctx->ast_program, callee_base->as.identifier.name);
    if (!return_function_external ||
        !return_function_external->has_function_return_signature ||
        !ir_function_value_return_parameter_index(
            ctx->ast_program,
            return_function_external,
            &return_param_index) ||
        return_param_index == (size_t)-1 ||
        !base->as.call.args ||
        return_param_index >= base->as.call.arg_count) {
        return 0;
    }

    if (out_return_function_external) {
        *out_return_function_external = return_function_external;
    }
    if (out_return_param_index) {
        *out_return_param_index = return_param_index;
    }
    if (out_arg_expr) {
        *out_arg_expr = base->as.call.args[return_param_index];
    }
    return 1;
}

static int ir_try_get_recursive_returned_function_value_parameter_argument_expr(
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal **out_return_function_external,
    size_t *out_return_param_index,
    const AstExpression **out_arg_expr) {
    const AstExpression *current_expr = call_expr;
    const AstExpression *resolved_arg_expr = NULL;
    const AstExternal *resolved_external = NULL;
    size_t resolved_param_index = (size_t)-1;
    size_t hop_count = 0u;

    if (out_return_function_external) {
        *out_return_function_external = NULL;
    }
    if (out_return_param_index) {
        *out_return_param_index = (size_t)-1;
    }
    if (out_arg_expr) {
        *out_arg_expr = NULL;
    }
    if (!ctx || !ctx->ast_program || !call_expr) {
        return 0;
    }

    while (current_expr &&
        ir_try_get_returned_function_value_parameter_argument_expr(
            ctx,
            current_expr,
            &resolved_external,
            &resolved_param_index,
            &resolved_arg_expr) &&
        resolved_arg_expr &&
        hop_count < 64u) {
        const AstExpression *resolved_arg_base =
            ir_unwrap_function_value_wrapper_expression(resolved_arg_expr);
        size_t nested_passthrough_index = (size_t)-1;

        if (out_return_function_external) {
            *out_return_function_external = resolved_external;
        }
        if (out_return_param_index) {
            *out_return_param_index = resolved_param_index;
        }
        if (out_arg_expr) {
            *out_arg_expr = resolved_arg_expr;
        }
        if (!resolved_arg_base || resolved_arg_base->kind != AST_EXPR_CALL ||
            !resolved_external ||
            !resolved_external->has_function_return_signature ||
            !ir_function_value_return_exact_parameter_passthrough_index(
                ctx->ast_program,
                resolved_external,
                &nested_passthrough_index) ||
            nested_passthrough_index == (size_t)-1) {
            return 1;
        }
        current_expr = resolved_arg_base;
        ++hop_count;
    }

    return out_arg_expr && *out_arg_expr != NULL;
}

static int ir_try_resolve_exact_returned_function_value_parameter_passthrough_base_expr(
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_arg_base) {
    const AstExpression *arg_expr = NULL;

    if (out_arg_base) {
        *out_arg_base = NULL;
    }
    if (!ctx || !ctx->ast_program || !out_arg_base ||
        !ir_try_get_recursive_returned_function_value_parameter_argument_expr(
            ctx,
            call_expr,
            NULL,
            NULL,
            &arg_expr) ||
        !arg_expr) {
        return 0;
    }

    *out_arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
    return *out_arg_base &&
        ((*out_arg_base)->kind == AST_EXPR_IDENTIFIER ||
            (*out_arg_base)->kind == AST_EXPR_CLOSURE ||
            (*out_arg_base)->kind == AST_EXPR_CALL ||
            (*out_arg_base)->kind == AST_EXPR_TERNARY);
}

static int ir_try_resolve_returned_function_value_parameter_argument_core(
    const IrLowerContext *ctx,
    const AstProgram *program,
    const AstExpression *call_expr,
    const AstExternal **out_return_function_external,
    size_t *out_return_param_index,
    const AstExpression **out_arg_expr,
    const AstExpression **out_arg_base,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name) {
    const AstExternal *return_function_external = NULL;
    const AstExpression *arg_expr = NULL;
    size_t return_param_index = (size_t)-1;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (out_return_function_external) {
        *out_return_function_external = NULL;
    }
    if (out_return_param_index) {
        *out_return_param_index = (size_t)-1;
    }
    if (out_arg_expr) {
        *out_arg_expr = NULL;
    }
    if (out_arg_base) {
        *out_arg_base = NULL;
    }
    if (!program || !ctx ||
        !ir_try_get_recursive_returned_function_value_parameter_argument_expr(
            ctx,
            call_expr,
            &return_function_external,
            &return_param_index,
            &arg_expr) ||
        !arg_expr ||
        !ir_call_argument_resolve_extension_function_value_target(
            program,
            ctx,
            return_function_external,
            return_param_index,
            arg_expr,
            out_capture_local_ids,
            out_capture_count,
            out_target_name)) {
        return 0;
    }

    if (out_return_function_external) {
        *out_return_function_external = return_function_external;
    }
    if (out_return_param_index) {
        *out_return_param_index = return_param_index;
    }
    if (out_arg_expr) {
        *out_arg_expr = arg_expr;
    }
    if (out_arg_base) {
        *out_arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
    }
    return 1;
}

static int ir_try_resolve_returned_function_value_parameter_argument_target(const AstProgram *program,
    const IrLowerContext *ctx,
    const AstExpression *call_expr,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name) {
    return ir_try_resolve_returned_function_value_parameter_argument_core(
        ctx,
        program,
        call_expr,
        NULL,
        NULL,
        NULL,
        NULL,
        out_capture_local_ids,
        out_capture_count,
        out_target_name);
}

static int ir_try_resolve_returned_function_value_parameter_argument_info(
    const AstProgram *program,
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExpression **out_arg_expr,
    const AstExpression **out_arg_base,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name,
    int *out_is_static_single_capture_closure_local) {
    const AstExpression *arg_expr = NULL;
    const AstExpression *arg_base = NULL;
    const size_t *capture_local_ids = NULL;
    size_t capture_count = 0u;
    const char *target_name = NULL;
    int is_static_single_capture_closure_local = 0;

    if (out_arg_expr) {
        *out_arg_expr = NULL;
    }
    if (out_arg_base) {
        *out_arg_base = NULL;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (out_is_static_single_capture_closure_local) {
        *out_is_static_single_capture_closure_local = 0;
    }
    if (!program || !ctx ||
        !ir_try_resolve_returned_function_value_parameter_argument_core(
            ctx,
            program,
            call_expr,
            NULL,
            NULL,
            &arg_expr,
            &arg_base,
            &capture_local_ids,
            &capture_count,
            &target_name) ||
        !target_name || target_name[0] == '\0') {
        return 0;
    }

    if (capture_count == 1u &&
        arg_base &&
        arg_base->kind == AST_EXPR_IDENTIFIER &&
        arg_base->as.identifier.name) {
        IrLowerBindingInfo binding_info;

        memset(&binding_info, 0, sizeof(binding_info));
        if (ir_lower_scope_lookup_binding(&ctx->scopes, arg_base->as.identifier.name, &binding_info) &&
            binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL &&
            (binding_info.function_target_tag_local_id == (size_t)-1 ||
                binding_info.has_const_value)) {
            is_static_single_capture_closure_local = 1;
        }
    }

    if (out_arg_expr) {
        *out_arg_expr = arg_expr;
    }
    if (out_arg_base) {
        *out_arg_base = arg_base;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = capture_local_ids;
    }
    if (out_capture_count) {
        *out_capture_count = capture_count;
    }
    if (out_target_name) {
        *out_target_name = target_name;
    }
    if (out_is_static_single_capture_closure_local) {
        *out_is_static_single_capture_closure_local = is_static_single_capture_closure_local;
    }
    return 1;
}

static int ir_build_single_target_name_array(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *target_name,
    const char ***out_target_names,
    size_t *out_target_count) {
    const char **target_names = NULL;

    if (out_target_names) {
        *out_target_names = NULL;
    }
    if (out_target_count) {
        *out_target_count = 0u;
    }
    if (!ctx || !call_expr || !target_name || target_name[0] == '\0') {
        return 0;
    }

    target_names = (const char **)malloc(sizeof(const char *));
    if (!target_names) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-089: out of memory while lowering returned function-value parameter target set");
        return 0;
    }
    target_names[0] = target_name;

    if (out_target_names) {
        *out_target_names = target_names;
    } else {
        free(target_names);
    }
    if (out_target_count) {
        *out_target_count = 1u;
    }
    return 1;
}

static int ir_try_resolve_returned_function_value_parameter_fallback_target_name(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char **out_target_name) {
    const char *returned_arg_target_name = NULL;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (!ctx || !ctx->ast_program || !call_expr) {
        return 0;
    }
    if (!ir_try_resolve_returned_function_value_parameter_argument_target(
            ctx->ast_program,
            ctx,
            call_expr,
            NULL,
            NULL,
            &returned_arg_target_name) ||
        !returned_arg_target_name ||
        returned_arg_target_name[0] == '\0') {
        return 0;
    }

    if (out_target_name) {
        *out_target_name = returned_arg_target_name;
    }
    return 1;
}

static int ir_collect_returned_function_value_target_names_with_parameter_fallback(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *return_function_external,
    const char ***out_target_names,
    size_t *out_target_count) {
    const char **target_names = NULL;
    size_t target_count = 0u;
    const char *fallback_target_name = NULL;

    if (out_target_names) {
        *out_target_names = NULL;
    }
    if (out_target_count) {
        *out_target_count = 0u;
    }
    if (!ctx || !ctx->ast_program || !ctx->program || !call_expr ||
        !return_function_external || !return_function_external->has_function_return_signature) {
        return 0;
    }

    if (!ir_collect_function_value_return_target_names(
            ctx->ast_program,
            ctx->program,
            return_function_external,
            &target_names,
            &target_count,
            ctx->error) ||
        target_count == 0u) {
        free(target_names);
        target_names = NULL;
        target_count = 0u;
        if (ir_try_resolve_returned_function_value_parameter_fallback_target_name(
                ctx,
                call_expr,
                &fallback_target_name) &&
            ir_build_single_target_name_array(
                ctx,
                call_expr,
                fallback_target_name,
                &target_names,
                &target_count)) {
            return 1;
        }
    }

    if (!target_names || target_count == 0u) {
        free(target_names);
        return 0;
    }
    if (out_target_names) {
        *out_target_names = target_names;
    } else {
        free(target_names);
    }
    if (out_target_count) {
        *out_target_count = target_count;
    }
    return 1;
}

static int ir_resolve_returned_closure_target_info(
    const AstProgram *program,
    IrLowerContext *ctx,
    const AstExternal *return_function_external,
    const char **out_helper_name,
    const char ***out_target_names,
    size_t *out_target_count,
    const char **out_primary_target_name) {
    const char *helper_name = NULL;
    const char **target_names = NULL;
    size_t target_count = 0u;
    const char *primary_target_name = NULL;

    if (out_helper_name) {
        *out_helper_name = NULL;
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
    if (!program || !ctx || !return_function_external || !return_function_external->has_function_return_signature) {
        return 0;
    }

    if (!ir_resolve_function_value_return_target_name(
            program,
            ctx->program,
            return_function_external,
            &helper_name,
            ctx->error)) {
        return 0;
    }
    if (!ir_collect_function_value_return_target_names(
            program,
            ctx->program,
            return_function_external,
            &target_names,
            &target_count,
            ctx->error)) {
        free(target_names);
        target_names = NULL;
        target_count = 0u;
    }

    primary_target_name =
        (target_count > 0u && target_names && target_names[0] && target_names[0][0] != '\0')
            ? target_names[0]
            : helper_name;

    if (out_helper_name) {
        *out_helper_name = helper_name;
    }
    if (out_target_names) {
        *out_target_names = target_names;
    } else {
        free(target_names);
    }
    if (out_target_count) {
        *out_target_count = target_count;
    }
    if (out_primary_target_name) {
        *out_primary_target_name = primary_target_name;
    }
    return 1;
}

static int ir_resolve_returned_function_value_target_info(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *return_function_external,
    int return_family_is_closure,
    const char ***out_target_names,
    size_t *out_target_count,
    const char **out_primary_target_name) {
    const char *returned_helper_name = NULL;
    const char **returned_target_names = NULL;
    size_t returned_target_count = 0u;
    const char *returned_primary_target_name = NULL;

    if (out_target_names) {
        *out_target_names = NULL;
    }
    if (out_target_count) {
        *out_target_count = 0u;
    }
    if (out_primary_target_name) {
        *out_primary_target_name = NULL;
    }
    if (!ctx || !ctx->ast_program || !ctx->program || !call_expr ||
        !return_function_external || !return_function_external->has_function_return_signature) {
        return 0;
    }

    if (return_family_is_closure) {
        if (!ir_resolve_returned_closure_target_info(
                ctx->ast_program,
                ctx,
                return_function_external,
                &returned_helper_name,
                &returned_target_names,
                &returned_target_count,
                &returned_primary_target_name)) {
            return 0;
        }
    } else {
        if (!ir_collect_returned_function_value_target_names_with_parameter_fallback(
                ctx,
                call_expr,
                return_function_external,
                &returned_target_names,
                &returned_target_count) ||
            returned_target_count == 0u ||
            !returned_target_names ||
            !returned_target_names[0] ||
            returned_target_names[0][0] == '\0') {
            free(returned_target_names);
            return 0;
        }
        returned_primary_target_name = returned_target_names[0];
    }

    if (out_target_names) {
        *out_target_names = returned_target_names;
    } else {
        free(returned_target_names);
    }
    if (out_target_count) {
        *out_target_count = returned_target_count;
    }
    if (out_primary_target_name) {
        *out_primary_target_name = returned_primary_target_name;
    }
    return 1;
}

static int ir_try_resolve_static_noncapturing_returned_target_descriptor(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *return_function_external,
    IrLowerFunctionObjectDescriptor *out_descriptor) {
    const char **returned_target_names = NULL;
    size_t returned_target_count = 0u;
    const char *returned_primary_target_name = NULL;

    ir_init_function_object_descriptor(out_descriptor);
    if (!ctx || !call_expr || !return_function_external || !out_descriptor) {
        return 0;
    }
    if (!ir_resolve_returned_function_value_target_info(
            ctx,
            call_expr,
            return_function_external,
            0,
            &returned_target_names,
            &returned_target_count,
            &returned_primary_target_name)) {
        return 0;
    }
    if (!(returned_target_count == 1u &&
            returned_primary_target_name &&
            returned_primary_target_name[0] != '\0' &&
            ir_init_function_object_descriptor_from_static_target(
                returned_primary_target_name,
                0,
                1,
                1,
                0,
                out_descriptor))) {
        free((void *)returned_target_names);
        return 0;
    }
    free((void *)returned_target_names);
    return 1;
}

static int ir_try_preserve_returned_function_value_parameter_closure_captures(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *binding_name,
    size_t *io_closure_capture_count,
    size_t **out_closure_capture_local_ids,
    int *out_preserved) {
    const size_t *return_arg_capture_local_ids = NULL;
    size_t return_arg_capture_count = 0u;
    size_t *closure_capture_local_ids = NULL;

    if (out_closure_capture_local_ids) {
        *out_closure_capture_local_ids = NULL;
    }
    if (out_preserved) {
        *out_preserved = 0;
    }
    if (!ctx || !call_expr || !binding_name || binding_name[0] == '\0') {
        return 0;
    }

    if (!ir_try_resolve_returned_function_value_parameter_argument_info(
            ctx->ast_program,
            ctx,
            call_expr,
            NULL,
            NULL,
            &return_arg_capture_local_ids,
            &return_arg_capture_count,
            NULL,
            NULL) ||
        return_arg_capture_count == 0u) {
        return 1;
    }

    if (!io_closure_capture_count || !out_closure_capture_local_ids) {
        return 0;
    }

    *io_closure_capture_count = return_arg_capture_count;
    if (!ir_append_named_capture_local_slots(
            ctx,
            binding_name,
            *io_closure_capture_count,
            &closure_capture_local_ids)) {
        return 0;
    }
    if (!ir_copy_capture_local_slot_range(
            ctx,
            closure_capture_local_ids,
            return_arg_capture_local_ids,
            *io_closure_capture_count)) {
        free(closure_capture_local_ids);
        return 0;
    }

    *out_closure_capture_local_ids = closure_capture_local_ids;
    if (out_preserved) {
        *out_preserved = 1;
    }
    return 1;
}

static int ir_try_build_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    IrLowerFunctionValueReturnResolution *out_resolution) {
    size_t payload_slot_count = 0u;
    int is_closure_return = 0;
    int have_explicit_payload_slot_count = 0;

    if (out_resolution) {
        memset(out_resolution, 0, sizeof(*out_resolution));
        ir_init_function_object_descriptor(&out_resolution->descriptor);
    }
    if (!ctx || !stmt || !base_expr || !out_resolution) {
        return 0;
    }
    is_closure_return = ctx->function_value_return_is_closure_family;

    if (base_expr->kind == AST_EXPR_CALL) {
        if (!ir_try_build_direct_call_function_value_return_resolution(
                ctx,
                stmt,
                base_expr,
                &is_closure_return,
                &payload_slot_count,
                out_resolution)) {
            return 0;
        }
        have_explicit_payload_slot_count = 1;
    } else if (base_expr->kind == AST_EXPR_TERNARY) {
        if (!ir_try_build_ternary_function_value_return_resolution(
                ctx,
                stmt,
                base_expr,
                &is_closure_return,
                &payload_slot_count,
                out_resolution)) {
            return 0;
        }
        have_explicit_payload_slot_count = 1;
    }

    if (base_expr->kind == AST_EXPR_IDENTIFIER &&
        base_expr->as.identifier.name) {
        if (!ir_try_build_identifier_function_value_return_resolution(
                ctx,
                stmt,
                base_expr,
                &is_closure_return,
                &payload_slot_count,
                out_resolution)) {
            return 0;
        }
        have_explicit_payload_slot_count = 1;
    } else if (base_expr->kind != AST_EXPR_CALL &&
            base_expr->kind != AST_EXPR_TERNARY) {
        payload_slot_count = is_closure_return
            ? (base_expr->kind == AST_EXPR_CLOSURE ? base_expr->as.closure.capture_name_count : 0u)
            : 1u;
        have_explicit_payload_slot_count = 1;
    }

    out_resolution->descriptor.is_closure_family = is_closure_return ? 1 : 0;
    if (have_explicit_payload_slot_count) {
        out_resolution->descriptor.payload_slot_count = payload_slot_count;
        out_resolution->payload_slot_count = payload_slot_count;
    } else {
        out_resolution->payload_slot_count = out_resolution->descriptor.payload_slot_count;
    }
    return 1;
}

static int ir_try_build_identifier_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    int *io_is_closure_return,
    size_t *out_payload_slot_count,
    IrLowerFunctionValueReturnResolution *out_resolution) {
    IrLowerBindingInfo identifier_binding_info;
    IrLowerFunctionObjectDescriptor descriptor;
    const char *static_target_name = NULL;

    if (out_payload_slot_count) {
        *out_payload_slot_count = 0u;
    }
    if (!ctx || !stmt || !base_expr || !base_expr->as.identifier.name ||
        !io_is_closure_return || !out_payload_slot_count || !out_resolution) {
        return 0;
    }

    memset(&identifier_binding_info, 0, sizeof(identifier_binding_info));
    if (ir_lower_scope_lookup_binding(
            &ctx->scopes,
            base_expr->as.identifier.name,
            &identifier_binding_info) &&
        (identifier_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL ||
            identifier_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL)) {
        if (!ir_init_function_object_descriptor_from_binding_info(
                &identifier_binding_info,
                ir_lower_find_function_value_target_set(
                    ctx,
                    base_expr->as.identifier.name),
                &descriptor)) {
            return 0;
        }
        if (descriptor.from_returned_function_value_call &&
            !descriptor.is_closure_family) {
            descriptor.payload_slot_count = 1u;
        }
        out_resolution->descriptor = descriptor;
        out_resolution->direct_return_call_external = descriptor.producer_external;
        out_resolution->has_identifier_binding_payload = 1;
        *io_is_closure_return = descriptor.is_closure_family;
        *out_payload_slot_count = descriptor.payload_slot_count;
        return 1;
    }

    if (!(*io_is_closure_return) &&
        ctx->program &&
        ir_resolve_noncapturing_function_target_name(
            ctx->program,
            base_expr->as.identifier.name,
            NULL,
            NULL,
            0u,
            &static_target_name,
            ctx->error)) {
        if (!ir_init_function_object_descriptor_from_static_target(
                static_target_name ? static_target_name : base_expr->as.identifier.name,
                0,
                1,
                1,
                0,
                &descriptor)) {
            return 0;
        }
        descriptor.payload_slot_count = 1u;
        out_resolution->descriptor = descriptor;
        out_resolution->payload_slot_count = 1u;
        out_resolution->has_identifier_binding_payload = 1;
        *out_payload_slot_count = 1u;
        return 1;
    }

    *out_payload_slot_count = *io_is_closure_return ? 0u : 1u;
    return 1;
}

static int ir_try_build_direct_call_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    int *io_is_closure_return,
    size_t *out_payload_slot_count,
    IrLowerFunctionValueReturnResolution *out_resolution) {
    const AstExpression *callee_base = NULL;
    const AstExpression *returned_arg_base = NULL;
    const AstExternal *producer_external = NULL;
    IrLowerFunctionObjectDescriptor descriptor;

    if (out_payload_slot_count) {
        *out_payload_slot_count = 0u;
    }
    if (!ctx || !stmt || !base_expr || base_expr->kind != AST_EXPR_CALL ||
        !io_is_closure_return || !out_payload_slot_count || !out_resolution) {
        return 0;
    }

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
        returned_arg_base) {
        if (returned_arg_base->kind == AST_EXPR_IDENTIFIER &&
            returned_arg_base->as.identifier.name &&
            ir_try_build_identifier_function_value_return_resolution(
                ctx,
                stmt,
                returned_arg_base,
                io_is_closure_return,
                out_payload_slot_count,
                out_resolution)) {
            out_resolution->descriptor.from_returned_function_value_call = 1;
            out_resolution->has_identifier_binding_payload = 1;
            return 1;
        }
        if (returned_arg_base->kind == AST_EXPR_CALL &&
            ir_try_build_direct_call_function_value_return_resolution(
                ctx,
                stmt,
                returned_arg_base,
                io_is_closure_return,
                out_payload_slot_count,
                out_resolution)) {
            out_resolution->descriptor.from_returned_function_value_call = 1;
            return 1;
        }
    }

    callee_base = ir_unwrap_function_value_wrapper_expression(base_expr->as.call.callee);
    if (!callee_base || callee_base->kind != AST_EXPR_IDENTIFIER || !callee_base->as.identifier.name) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-060: function-valued return currently requires returning one closure literal or one compatible non-capturing function value");
        return 0;
    }
    out_resolution->direct_return_call_external =
        ir_find_named_function_external_prefer_definition(
            ctx->ast_program,
            callee_base->as.identifier.name);
    if (!out_resolution->direct_return_call_external ||
        !out_resolution->direct_return_call_external->has_function_return_signature) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-060: function-valued return currently requires returning one closure literal or one compatible non-capturing function value");
        return 0;
    }
    if (!ir_function_value_return_is_closure_family(
            ctx->ast_program,
            out_resolution->direct_return_call_external,
            io_is_closure_return)) {
        if (ctx->error && ctx->error->message[0] == '\0') {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-INT-111: failed while classifying direct returned call function-value family");
        }
        return 0;
    }
    if (*io_is_closure_return) {
        if (!ir_function_returned_closure_payload_slot_count(
                ctx->ast_program,
                ctx->program,
                out_resolution->direct_return_call_external,
                out_payload_slot_count)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-112: failed while resolving direct returned call payload slot count");
            }
            return 0;
        }
        out_resolution->descriptor.producer_external =
            out_resolution->direct_return_call_external;
    } else {
        producer_external = out_resolution->direct_return_call_external;
        *out_payload_slot_count = 1u;
        if (ir_try_resolve_static_noncapturing_returned_target_descriptor(
                ctx,
                base_expr,
                out_resolution->direct_return_call_external,
                &descriptor)) {
            descriptor.payload_slot_count = 1u;
            out_resolution->descriptor = descriptor;
            out_resolution->payload_slot_count = 1u;
            out_resolution->has_identifier_binding_payload = 1;
        }
        out_resolution->direct_return_call_external = producer_external;
    }
    return 1;
}

static int ir_try_build_ternary_function_value_return_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    int *io_is_closure_return,
    size_t *out_payload_slot_count,
    IrLowerFunctionValueReturnResolution *out_resolution) {
    IrLowerFunctionValueTernaryInfo ternary_info;

    if (out_payload_slot_count) {
        *out_payload_slot_count = 0u;
    }
    if (!ctx || !stmt || !base_expr || base_expr->kind != AST_EXPR_TERNARY ||
        !io_is_closure_return || !out_payload_slot_count || !out_resolution) {
        return 0;
    }

    if (!ir_resolve_identifier_ternary_function_value_info(ctx, base_expr, &ternary_info) ||
        ternary_info.then_is_closure_local != ternary_info.else_is_closure_local) {
        const AstExpression *then_base =
            ir_unwrap_function_value_wrapper_expression(base_expr->as.ternary.then_expr);
        const AstExpression *else_base =
            ir_unwrap_function_value_wrapper_expression(base_expr->as.ternary.else_expr);
        IrLowerFunctionValueReturnResolution then_resolution;
        IrLowerFunctionValueReturnResolution else_resolution;

        memset(&then_resolution, 0, sizeof(then_resolution));
        memset(&else_resolution, 0, sizeof(else_resolution));
        ir_init_function_object_descriptor(&then_resolution.descriptor);
        ir_init_function_object_descriptor(&else_resolution.descriptor);
        if (then_base && else_base &&
            ir_try_build_direct_call_function_value_return_resolution(
                ctx,
                stmt,
                then_base,
                io_is_closure_return,
                out_payload_slot_count,
                &then_resolution) &&
            ir_try_build_direct_call_function_value_return_resolution(
                ctx,
                stmt,
                else_base,
                io_is_closure_return,
                out_payload_slot_count,
                &else_resolution) &&
            then_resolution.payload_slot_count == else_resolution.payload_slot_count &&
            then_resolution.descriptor.is_closure_family == else_resolution.descriptor.is_closure_family) {
            *io_is_closure_return = then_resolution.descriptor.is_closure_family ? 1 : 0;
            *out_payload_slot_count = then_resolution.payload_slot_count;
            out_resolution->descriptor.is_closure_family = *io_is_closure_return;
            out_resolution->descriptor.payload_slot_count = *out_payload_slot_count;
            out_resolution->payload_slot_count = *out_payload_slot_count;
            return 1;
        }
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-060: function-valued return currently requires returning one closure literal or one compatible non-capturing function value");
        return 0;
    }
    *io_is_closure_return = ternary_info.then_is_closure_local;

    if (ternary_info.then_capture_count != ternary_info.else_capture_count) {
        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-061: returned closure payload slot count does not match function return lowering shape");
        return 0;
    }
    if (*io_is_closure_return) {
        *out_payload_slot_count = ternary_info.then_capture_count;
        if (ctx->hidden_return_slot_count == ternary_info.then_capture_count + 1u) {
            *out_payload_slot_count += 1u;
        }
    } else {
        *out_payload_slot_count = 1u;
    }
    out_resolution->descriptor.is_closure_family = *io_is_closure_return ? 1 : 0;
    out_resolution->descriptor.payload_slot_count = *out_payload_slot_count;
    out_resolution->payload_slot_count = *out_payload_slot_count;
    return 1;
}

static int ir_emit_function_value_return_payload_from_resolution(
    IrLowerContext *ctx,
    const AstStatement *stmt,
    const AstExpression *base_expr,
    const IrLowerFunctionValueReturnResolution *return_resolution,
    int use_snapshot,
    size_t snapshot_first_local_id,
    size_t capture_count,
    int *out_payload_already_lowered) {
    if (!ctx || !stmt || !base_expr || !return_resolution) {
        return 0;
    }
    if (out_payload_already_lowered) {
        *out_payload_already_lowered = 0;
    }

    if (base_expr->kind == AST_EXPR_TERNARY) {
        IrLowerFunctionValueTernaryInfo ternary_info;
        size_t slot_write_base_local_id;

        if (!ir_resolve_identifier_ternary_function_value_info(ctx, base_expr, &ternary_info) ||
            ternary_info.then_is_closure_local != ternary_info.else_is_closure_local ||
            ternary_info.then_capture_count != ternary_info.else_capture_count ||
            !ternary_info.then_target_name || !ternary_info.else_target_name) {
            if (base_expr->as.ternary.then_expr &&
                base_expr->as.ternary.else_expr &&
                strstr(ctx->function ? ctx->function->name : "", "pick") != NULL) {
                const AstExpression *dbg_then =
                    ir_unwrap_function_value_wrapper_expression(base_expr->as.ternary.then_expr);
                const AstExpression *dbg_else =
                    ir_unwrap_function_value_wrapper_expression(base_expr->as.ternary.else_expr);
                fprintf(stderr,
                    "debug ternary return info failed in %s: then_kind=%d else_kind=%d\n",
                    ctx->function ? ctx->function->name : "<null>",
                    dbg_then ? (int)dbg_then->kind : -1,
                    dbg_else ? (int)dbg_else->kind : -1);
            }
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-060: function-valued return currently requires returning one closure literal or one compatible non-capturing function value");
            return 0;
        }

        slot_write_base_local_id =
            use_snapshot ? snapshot_first_local_id : ctx->hidden_return_first_local_id;
        if (!return_resolution->descriptor.is_closure_family) {
            if (!ir_lower_register_function_value_target(
                    ctx,
                    ctx->function && ctx->function->name ? ctx->function->name : "__retfn",
                    ternary_info.then_target_name,
                    &ternary_info.then_const_value) ||
                !ir_lower_register_function_value_target(
                    ctx,
                    ctx->function && ctx->function->name ? ctx->function->name : "__retfn",
                    ternary_info.else_target_name,
                    &ternary_info.else_const_value)) {
                return 0;
            }
        } else if (capture_count == ternary_info.then_capture_count + 1u) {
            if (!ir_lower_register_function_value_target(
                    ctx,
                    ctx->function && ctx->function->name ? ctx->function->name : "__retclosure",
                    ternary_info.then_target_name,
                    &ternary_info.then_const_value) ||
                !ir_lower_register_function_value_target(
                    ctx,
                    ctx->function && ctx->function->name ? ctx->function->name : "__retclosure",
                    ternary_info.else_target_name,
                    &ternary_info.else_const_value)) {
                return 0;
            }
        }
        if (!ir_emit_materialized_ternary_function_value_return_payload(
                ctx,
                base_expr->as.ternary.condition,
                &ternary_info,
                return_resolution->descriptor.is_closure_family,
                slot_write_base_local_id,
                capture_count,
                use_snapshot ? 0 : 1)) {
            return 0;
        }
        if (out_payload_already_lowered) {
            *out_payload_already_lowered = 1;
        }
        return 1;
    }

    if (!return_resolution->descriptor.is_closure_family) {
        if (return_resolution->has_identifier_binding_payload) {
            if (return_resolution->descriptor.dynamic_tag_local_id != (size_t)-1) {
                return ir_emit_function_value_return_payload_slot(
                    ctx,
                    use_snapshot,
                    snapshot_first_local_id,
                    ir_value_local(return_resolution->descriptor.dynamic_tag_local_id));
            }
            if (return_resolution->descriptor.has_const_tag_value) {
                return ir_emit_function_value_return_payload_slot(
                    ctx,
                    use_snapshot,
                    snapshot_first_local_id,
                    ir_value_immediate(return_resolution->descriptor.const_tag_value));
            }
        }

        if (base_expr->kind == AST_EXPR_CALL && return_resolution->direct_return_call_external) {
            return ir_materialize_returned_function_value_return_payload_slots(
                ctx,
                base_expr,
                return_resolution->direct_return_call_external,
                use_snapshot,
                snapshot_first_local_id,
                1u);
        }

        if (base_expr->kind != AST_EXPR_IDENTIFIER || !base_expr->as.identifier.name) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-066: returned noncapturing function value requires a runtime-tagged local binding or a resolvable static target");
            return 0;
        }

        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-066: returned noncapturing function value requires transport-resolved tag payload");
        return 0;
    }

    if (return_resolution->has_identifier_binding_payload) {
        size_t slot_index;

        for (slot_index = 0u; slot_index < capture_count; ++slot_index) {
            IrValueRef capture_value;
            size_t capture_index = slot_index;

            if (return_resolution->descriptor.dynamic_tag_local_id != (size_t)-1 &&
                !return_resolution->descriptor.has_const_tag_value) {
                if (slot_index == 0u) {
                    capture_value = ir_value_local(
                        return_resolution->descriptor.dynamic_tag_local_id);
                    goto have_resolution_closure_capture_value;
                }
                capture_index -= 1u;
            }
            if (!return_resolution->descriptor.capture_local_ids ||
                capture_index >= return_resolution->descriptor.capture_count ||
                return_resolution->descriptor.capture_local_ids[capture_index] == (size_t)-1) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-062: returned closure capture binding is not available during return lowering");
                return 0;
            }
            capture_value = ir_value_local(
                return_resolution->descriptor.capture_local_ids[capture_index]);
have_resolution_closure_capture_value:
            if (use_snapshot) {
                if (!ir_emit_mov(ctx,
                        ir_value_local(snapshot_first_local_id + slot_index),
                        capture_value)) {
                    return 0;
                }
            } else if (!ir_emit_store_indirect(
                    ctx,
                    ir_value_local(ctx->hidden_return_first_local_id + slot_index),
                    capture_value)) {
                return 0;
            }
        }
        return 1;
    }

    if (base_expr->kind == AST_EXPR_CALL && return_resolution->direct_return_call_external) {
        return ir_materialize_returned_function_value_return_payload_slots(
            ctx,
            base_expr,
            return_resolution->direct_return_call_external,
            use_snapshot,
            snapshot_first_local_id,
            capture_count);
    }

    {
        size_t slot_index;

        for (slot_index = 0u; slot_index < capture_count; ++slot_index) {
            const char *capture_name =
                base_expr->as.closure.capture_names ? base_expr->as.closure.capture_names[slot_index] : NULL;
            AstExpression capture_expr;
            IrValueRef capture_value;

            if (!capture_name || capture_name[0] == '\0') {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-062: returned closure capture binding is not available during return lowering");
                return 0;
            }
            memset(&capture_expr, 0, sizeof(capture_expr));
            capture_expr.kind = AST_EXPR_IDENTIFIER;
            capture_expr.line = base_expr->line;
            capture_expr.column = base_expr->column;
            capture_expr.as.identifier.name = (char *)capture_name;
            capture_expr.as.identifier.name_length = strlen(capture_name);
            if (!ir_lower_expression(ctx, &capture_expr, &capture_value)) {
                return 0;
            }
            if (use_snapshot) {
                if (!ir_emit_mov(ctx,
                        ir_value_local(snapshot_first_local_id + slot_index),
                        capture_value)) {
                    return 0;
                }
            } else if (!ir_emit_store_indirect(
                    ctx,
                    ir_value_local(ctx->hidden_return_first_local_id + slot_index),
                    capture_value)) {
                return 0;
            }
        }
    }

    return 1;
}

static int ir_try_materialize_two_target_returned_callable_object_view_with_family_prefixes(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const char *nonclosure_payload_local_name_prefix,
    const char *closure_payload_local_name_prefix,
    IrLowerCallableObjectView *out_view) {
    IrLowerFunctionObjectDescriptor descriptor;
    int return_family_is_closure = 0;

    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->dynamic_tag_local_id = (size_t)-1;
    }
    if (!ctx || !call_expr || !out_view ||
        !nonclosure_payload_local_name_prefix ||
        nonclosure_payload_local_name_prefix[0] == '\0' ||
        !closure_payload_local_name_prefix ||
        closure_payload_local_name_prefix[0] == '\0' ||
        !ctx->ast_program) {
        return 0;
    }
    if (!ir_try_resolve_returned_function_object_descriptor_with_family_prefixes(
            ctx,
            call_expr,
            2u,
            nonclosure_payload_local_name_prefix,
            closure_payload_local_name_prefix,
            &descriptor,
            &return_family_is_closure) ||
        descriptor.dynamic_target_count != 2u ||
        !descriptor.dynamic_target_names ||
        !descriptor.dynamic_target_names[0] ||
        !descriptor.dynamic_target_names[1] ||
        descriptor.is_closure_family != (return_family_is_closure ? 1 : 0) ||
        !ir_init_callable_object_view_from_function_object_descriptor(
            &descriptor,
            out_view)) {
        return 0;
    }
    return 1;
}

static int ir_lower_refresh_current_function(IrLowerContext *ctx,
    const char *current_function_name,
    int line,
    int column) {
    if (!ctx || !current_function_name || current_function_name[0] == '\0') {
        return 1;
    }
    ctx->function = ir_program_find_function_mut(ctx->program, current_function_name);
    if (!ctx->function) {
        ir_set_error(ctx->error,
            line,
            column,
            "IR-INT-084: current lowering function '%s' disappeared after builtin insertion",
            current_function_name);
        return 0;
    }
    return 1;
}

static int ir_lower_boolean_branch(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t then_target,
    size_t else_target) {
    IrValueRef condition_value;
    IrValueRef masked_value;
    IrValueRef compare_value;
    IrBasicBlock *block;
    size_t rhs_block_id;

    if (!ctx || !expr || !ctx->function || !ctx->has_block) {
        return 0;
    }

    if (expr->kind == AST_EXPR_UNARY && expr->as.unary.op == TOKEN_BANG) {
        return ir_lower_boolean_branch(ctx, expr->as.unary.operand, else_target, then_target);
    }

    if (expr->kind == AST_EXPR_BINARY && expr->as.binary.op == TOKEN_AND_AND) {
        if (!ir_function_append_block(ctx->function, &rhs_block_id, NULL, ctx->error)) {
            return 0;
        }
        if (!ir_lower_boolean_branch(ctx, expr->as.binary.left, rhs_block_id, else_target)) {
            return 0;
        }
        ctx->block_id = rhs_block_id;
        ctx->has_block = 1;
        return ir_lower_boolean_branch(ctx, expr->as.binary.right, then_target, else_target);
    }

    if (expr->kind == AST_EXPR_BINARY && expr->as.binary.op == TOKEN_OR_OR) {
        if (!ir_function_append_block(ctx->function, &rhs_block_id, NULL, ctx->error)) {
            return 0;
        }
        if (!ir_lower_boolean_branch(ctx, expr->as.binary.left, then_target, rhs_block_id)) {
            return 0;
        }
        ctx->block_id = rhs_block_id;
        ctx->has_block = 1;
        return ir_lower_boolean_branch(ctx, expr->as.binary.right, then_target, else_target);
    }

    if (expr->kind == AST_EXPR_TERNARY) {
        size_t then_expr_block_id;
        size_t else_expr_block_id;

        if (!ir_function_append_block(ctx->function, &then_expr_block_id, NULL, ctx->error) ||
            !ir_function_append_block(ctx->function, &else_expr_block_id, NULL, ctx->error) ||
            !ir_lower_boolean_branch(ctx,
                expr->as.ternary.condition,
                then_expr_block_id,
                else_expr_block_id)) {
            return 0;
        }

        ctx->block_id = then_expr_block_id;
        ctx->has_block = 1;
        if (!ir_lower_boolean_branch(ctx, expr->as.ternary.then_expr, then_target, else_target)) {
            return 0;
        }

        ctx->block_id = else_expr_block_id;
        ctx->has_block = 1;
        return ir_lower_boolean_branch(ctx, expr->as.ternary.else_expr, then_target, else_target);
    }

    if (!ir_lower_expression(ctx, expr, &condition_value)) {
        return 0;
    }

    if (ir_expression_is_direct_float_condition(ctx, expr)) {
        size_t temp_id = ir_allocate_temp(ctx->function);
        masked_value = ir_value_temp(temp_id);
        temp_id = ir_allocate_temp(ctx->function);
        compare_value = ir_value_temp(temp_id);

        if (!ir_emit_binary(ctx,
                masked_value,
                IR_BINARY_BIT_AND,
                condition_value,
                ir_value_immediate(0x7fffffffLL)) ||
            !ir_emit_binary(ctx,
                compare_value,
                IR_BINARY_NE,
                masked_value,
                ir_value_immediate(0))) {
            return 0;
        }
        condition_value = compare_value;
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }

    return ir_block_set_branch(block, condition_value, then_target, else_target, ctx->error);
}

static int ir_lower_logical_value_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    size_t result_temp_id;
    IrValueRef result_value;
    size_t true_block_id;
    size_t false_block_id;
    size_t join_block_id;

    if (!ctx || !expr || !out_value || !ctx->function || !ctx->has_block) {
        return 0;
    }

    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);

    if (!ir_function_append_block(ctx->function, &true_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &false_block_id, NULL, ctx->error) ||
        !ir_lower_boolean_branch(ctx, expr, true_block_id, false_block_id) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = true_block_id;
    ctx->has_block = 1;
    if (!ir_emit_mov(ctx, result_value, ir_value_immediate(1)) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = false_block_id;
    ctx->has_block = 1;
    if (!ir_emit_mov(ctx, result_value, ir_value_immediate(0)) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(&ctx->scopes, expr)) {
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-INT-077: failed to clear logical-expression mutated local facts during IR lowering");
        return 0;
    }
    *out_value = result_value;
    return 1;
}

static int ir_lower_ternary_value_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    size_t result_temp_id;
    IrValueRef result_value;
    IrValueRef then_value;
    IrValueRef else_value;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;

    if (!ctx || !expr || !out_value || expr->kind != AST_EXPR_TERNARY || !ctx->function ||
        !ctx->has_block) {
        return 0;
    }

    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);

    if (!ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_lower_boolean_branch(ctx,
            expr->as.ternary.condition,
            then_block_id,
            else_block_id) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_lower_expression(ctx, expr->as.ternary.then_expr, &then_value) ||
        !ir_emit_mov(ctx, result_value, then_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_lower_expression(ctx, expr->as.ternary.else_expr, &else_value) ||
        !ir_emit_mov(ctx, result_value, else_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    if (!ir_lower_scope_clear_mutated_scalar_locals_in_expr(&ctx->scopes, expr)) {
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-INT-078: failed to clear ternary-expression mutated local facts during IR lowering");
        return 0;
    }
    *out_value = result_value;
    return 1;
}

static int ir_lower_return_statement(IrLowerContext *ctx, const AstStatement *stmt) {
    const AstExpression *expr = NULL;
    IrValueRef value = ir_value_immediate(0);
    IrBasicBlock *block;
    int has_return_expr = 0;
    int has_runtime_counted_function_defer = 0;
    int has_any_defer = 0;
    size_t fndefer_index;

    if (!ctx || !stmt) {
        return 0;
    }

    if (stmt->has_primary_expression && stmt->primary_expression_index < stmt->expression_count) {
        expr = stmt->expressions[stmt->primary_expression_index];
    } else if (stmt->expression_count == 1) {
        expr = stmt->expressions[0];
    }
    has_return_expr = expr != NULL;

    has_any_defer = ctx->defer_entries.count > 0 || ctx->function_defer_entries.count > 0;
    if (ctx->has_function_value_return_payload) {
        const AstExpression *base_expr = ir_unwrap_function_value_wrapper_expression(expr);
        IrLowerFunctionValueReturnResolution return_resolution;
        size_t snapshot_first_local_id = (size_t)-1;
        size_t slot_index;
        size_t capture_count = 0u;
        int use_snapshot = has_any_defer ? 1 : 0;
        int return_payload_already_lowered = 0;

        if (!has_return_expr || !base_expr ||
            (base_expr->kind != AST_EXPR_CLOSURE &&
                base_expr->kind != AST_EXPR_CALL &&
                base_expr->kind != AST_EXPR_TERNARY &&
                (base_expr->kind != AST_EXPR_IDENTIFIER || !base_expr->as.identifier.name))) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-060: function-valued return currently requires returning one closure literal or one compatible non-capturing function value");
            return 0;
        }
        if (!ir_lower_emit_function_value_wrapper_side_effects(ctx, expr)) {
            return 0;
        }
        if (!ir_try_build_function_value_return_resolution(
                ctx,
                stmt,
                base_expr,
                &return_resolution)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-145: function-valued return resolution failed without a specific diagnostic (kind=%d)",
                    (int)base_expr->kind);
            }
            return 0;
        }
        capture_count = return_resolution.payload_slot_count;
        if (capture_count != ctx->hidden_return_slot_count) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-061: returned closure payload slot count does not match function return lowering shape");
            return 0;
        }

        if (use_snapshot && capture_count > 0u) {
            snapshot_first_local_id = ctx->function->local_count;
            for (slot_index = 0u; slot_index < capture_count; ++slot_index) {
                char hidden_name[48];
                size_t local_id = (size_t)-1;

                snprintf(hidden_name, sizeof(hidden_name), "__retclosure_tmp_%zu", slot_index);
                if (!ir_function_append_local(ctx->function, hidden_name, 0, &local_id, ctx->error) ||
                    local_id != snapshot_first_local_id + slot_index) {
                    if (ctx->error && ctx->error->message[0] == '\0') {
                        ir_set_error(ctx->error,
                            stmt->line,
                            stmt->column,
                            "IR-INT-113: failed while creating function-valued return snapshot slot");
                    }
                    return 0;
                }
            }
        }

        if (!ir_emit_function_value_return_payload_from_resolution(
                ctx,
                stmt,
                base_expr,
                &return_resolution,
                use_snapshot,
                snapshot_first_local_id,
                capture_count,
                &return_payload_already_lowered)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-INT-146: function-valued return payload emission failed without a specific diagnostic (kind=%d, payload_slots=%zu)",
                    (int)base_expr->kind,
                    capture_count);
            }
            return 0;
        }

        if (!ir_lower_emit_defer_entries(ctx, 0u) ||
            !ir_lower_emit_function_defer_entries(ctx)) {
            return 0;
        }
        block = ir_lower_current_block(ctx);
        if (!block) {
            return 0;
        }
        if (block->has_terminator) {
            return 1;
        }
        if (use_snapshot &&
            !ir_emit_store_slot_range_to_hidden_return_addresses(
                ctx,
                snapshot_first_local_id,
                capture_count)) {
            return 0;
        }
        return ir_block_set_void_return(block, ctx->error);
    }

    if (ctx->hidden_return_slot_count > 0u) {
        size_t snapshot_first_local_id = (size_t)-1;
        IrLowerBindingInfo source_binding;
        int use_snapshot = 0;
        size_t slot_index;

        if (!has_return_expr) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-001: return statement without expression is not supported in IR v1");
            return 0;
        }

        for (fndefer_index = 0u; fndefer_index < ctx->function_defer_entries.count; ++fndefer_index) {
            if (ctx->function_defer_entries.items[fndefer_index].has_runtime_repeat_count) {
                has_runtime_counted_function_defer = 1;
                break;
            }
        }
        use_snapshot = has_any_defer ? 1 : 0;

        if (use_snapshot) {
            snapshot_first_local_id = ctx->function->local_count;
            for (slot_index = 0u; slot_index < ctx->hidden_return_slot_count; ++slot_index) {
                char hidden_name[48];
                size_t local_id = (size_t)-1;

                snprintf(hidden_name, sizeof(hidden_name), "__aggret_tmp_%zu", slot_index);
                if (!ir_function_append_local(ctx->function, hidden_name, 0, &local_id, ctx->error) ||
                    local_id != snapshot_first_local_id + slot_index) {
                    return 0;
                }
            }
        }

        if (ir_resolve_local_aggregate_binding(ctx, expr, &source_binding)) {
            if ((ctx->hidden_return_aggregate_kind == AST_DECLARATION_VALUE_PAIR &&
                    source_binding.kind != IR_LOWER_BINDING_PAIR_LOCAL) ||
                (ctx->hidden_return_aggregate_kind == AST_DECLARATION_VALUE_STRUCT &&
                    source_binding.kind != IR_LOWER_BINDING_STRUCT_LOCAL)) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-055: aggregate return requires matching aggregate kinds");
                return 0;
            }
            if (ctx->hidden_return_aggregate_kind == AST_DECLARATION_VALUE_STRUCT) {
                const char *source_type_name = source_binding.type_name;
                if ((!ctx->hidden_return_type_name && source_type_name) ||
                    (ctx->hidden_return_type_name && !source_type_name) ||
                    (ctx->hidden_return_type_name && source_type_name &&
                        strcmp(ctx->hidden_return_type_name, source_type_name) != 0)) {
                    ir_set_error(ctx->error,
                        stmt->line,
                        stmt->column,
                        "IR-LOWER-056: struct return requires matching struct types");
                    return 0;
                }
            }
            if (source_binding.aggregate_slot_count != ctx->hidden_return_slot_count) {
                ir_set_error(ctx->error,
                    stmt->line,
                    stmt->column,
                    "IR-LOWER-049: aggregate return requires matching lowering shape");
                return 0;
            }
            if (use_snapshot) {
                if (!ir_emit_copy_local_slot_range(
                        ctx,
                        snapshot_first_local_id,
                        source_binding.local_id,
                        ctx->hidden_return_slot_count)) {
                    return 0;
                }
            } else if (!ir_emit_store_slot_range_to_hidden_return_addresses(
                    ctx,
                    source_binding.local_id,
                    ctx->hidden_return_slot_count)) {
                return 0;
            }
        } else if (!ir_lower_call_into_aggregate_slots(
                ctx,
                expr,
                ctx->hidden_return_aggregate_kind == AST_DECLARATION_VALUE_PAIR
                    ? IR_LOWER_BINDING_PAIR_LOCAL
                    : IR_LOWER_BINDING_STRUCT_LOCAL,
                ctx->hidden_return_type_name,
                use_snapshot ? snapshot_first_local_id : ctx->hidden_return_first_local_id,
                ctx->hidden_return_slot_count,
                use_snapshot ? 0 : 1)) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-057: aggregate return currently requires a matching local aggregate or direct aggregate-returning call");
            return 0;
        }

        if (!ir_lower_emit_defer_entries(ctx, 0u) ||
            !ir_lower_emit_function_defer_entries(ctx)) {
            return 0;
        }
        block = ir_lower_current_block(ctx);
        if (!block) {
            return 0;
        }
        if (block->has_terminator) {
            return 1;
        }
        if (use_snapshot &&
            !ir_emit_store_slot_range_to_hidden_return_addresses(
                ctx,
                snapshot_first_local_id,
                ctx->hidden_return_slot_count)) {
            return 0;
        }
        return ir_block_set_void_return(block, ctx->error);
    }

    if (!has_return_expr) {
        if (ctx->function_return_type != AST_FUNCTION_RETURN_VOID) {
            ir_set_error(ctx->error,
                stmt->line,
                stmt->column,
                "IR-LOWER-001: return statement without expression is not supported in IR v1");
            return 0;
        }
    } else {
        if (!ir_lower_expression(ctx, expr, &value)) {
            if (ctx && ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    expr ? expr->line : stmt->line,
                    expr ? expr->column : stmt->column,
                    "IR-INT-130: return expression kind '%s' failed to lower without a diagnostic",
                    expr ? ast_expression_kind_name(expr->kind) : "<null>");
            }
            return 0;
        }
    }

    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }

    for (fndefer_index = 0u; fndefer_index < ctx->function_defer_entries.count; ++fndefer_index) {
        if (ctx->function_defer_entries.items[fndefer_index].has_runtime_repeat_count) {
            has_runtime_counted_function_defer = 1;
            break;
        }
    }

    if (has_any_defer && has_return_expr) {
        if (has_runtime_counted_function_defer) {
            size_t local_id = (size_t)-1;

            if (!ir_function_append_local(ctx->function, "__fndefer_ret", 0, &local_id, ctx->error) ||
                !ir_emit_mov(ctx, ir_value_local(local_id), value)) {
                return 0;
            }
            value = ir_value_local(local_id);
        } else if (value.kind == IR_VALUE_LOCAL || value.kind == IR_VALUE_GLOBAL) {
            size_t temp_id = ir_allocate_temp(ctx->function);
            IrValueRef temp_ref = ir_value_temp(temp_id);

            if (!ir_emit_mov(ctx, temp_ref, value)) {
                return 0;
            }
            value = temp_ref;
        }
    }

    if (!ir_lower_emit_defer_entries(ctx, 0u)) {
        return 0;
    }
    if (!ir_lower_emit_function_defer_entries(ctx)) {
        return 0;
    }
    block = ir_lower_current_block(ctx);
    if (!block) {
        return 0;
    }
    if (block->has_terminator) {
        return 1;
    }

    if (!has_return_expr) {
        return ir_block_set_void_return(block, ctx->error);
    }

    return ir_block_set_return(block, value, ctx->error);
}

static int ir_try_resolve_returned_function_value_argument_target(const AstProgram *program,
    const IrLowerContext *ctx,
    const AstExpression *arg_base,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name) {
    const AstExpression *base = NULL;
    const AstExternal *return_function_external = NULL;
    int return_family_is_closure = 0;
    IrLowerReturnedFunctionObjectResolution returned_resolution;
    int have_returned_resolution = 0;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (!program || !ctx || !arg_base || arg_base->kind != AST_EXPR_CALL) {
        return 0;
    }
    if (!ir_resolve_returned_function_value_call_producer_info(
            ctx,
            arg_base,
            &base,
            &return_function_external,
            &return_family_is_closure) ||
        !base) {
        return 0;
    }

    if (!ir_try_resolve_returned_function_object_resolution(
            (IrLowerContext *)ctx,
            arg_base,
            "__retfn_argcap",
            "__retclosure_argcap",
            &returned_resolution)) {
        return 0;
    }
    have_returned_resolution = 1;

    if (!return_family_is_closure) {
        int target_discovery_only =
            out_target_name && out_capture_local_ids == NULL && out_capture_count == NULL;

        if (target_discovery_only) {
            *out_target_name = returned_resolution.descriptor.direct_target_name;
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 1;
        }
        if (!returned_resolution.is_dynamic_two_target) {
            IrLowerCallableObjectView returned_view;

            if (!ir_init_callable_object_view_from_function_object_descriptor(
                    &returned_resolution.descriptor,
                    &returned_view)) {
                free((void *)returned_resolution.descriptor.dynamic_target_names);
                return 0;
            }
            (void)ir_project_static_callable_object_view_target_binding(
                &returned_view,
                out_capture_local_ids,
                out_capture_count,
                out_target_name);
            return 1;
        }
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        return 0;
    }

    {
        if (!have_returned_resolution ||
            !returned_resolution.helper_name ||
            returned_resolution.helper_name[0] == '\0') {
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }
        if (returned_resolution.is_dynamic_two_target) {
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }

        if (out_target_name && out_capture_local_ids == NULL && out_capture_count == NULL) {
            *out_target_name = returned_resolution.helper_name;
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 1;
        }
        if (out_capture_local_ids) {
            *out_capture_local_ids = returned_resolution.descriptor.capture_local_ids;
        }
        if (out_capture_count) {
            *out_capture_count = returned_resolution.descriptor.capture_count;
        }
        if (out_target_name) {
            *out_target_name = returned_resolution.helper_name;
        }
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        return 1;
    }
}

static int ir_call_argument_resolve_extension_function_value_target(const AstProgram *program,
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t arg_index,
    const AstExpression *arg_expr,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count,
    const char **out_target_name) {
    const AstExternal *arg_function = NULL;
    const char *arg_name = NULL;
    size_t external_index;
    int resolved_from_runtime_binding = 0;

    if (out_target_name) {
        *out_target_name = NULL;
    }
    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (!program || !callee_external || !arg_expr) {
        return 0;
    }

    if (!callee_external->parameter_value_kinds ||
        arg_index >= callee_external->parameter_count ||
        callee_external->parameter_value_kinds[arg_index] != AST_PARAMETER_VALUE_FUNCTION) {
        return 0;
    }

    {
        const AstExpression *arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
        if (ctx && arg_base && arg_base->kind == AST_EXPR_TERNARY) {
            const AstExpression *then_base =
                ir_unwrap_paren_expression(arg_base->as.ternary.then_expr);
            const AstExpression *else_base =
                ir_unwrap_paren_expression(arg_base->as.ternary.else_expr);
            const AstExpression *then_passthrough_base = NULL;
            const AstExpression *else_passthrough_base = NULL;
            const char *then_target_name = NULL;
            const char *else_target_name = NULL;
            const size_t *then_capture_local_ids = NULL;
            const size_t *else_capture_local_ids = NULL;
            size_t then_capture_count = 0u;
            size_t else_capture_count = 0u;
            int target_discovery_only = out_target_name != NULL && !out_capture_local_ids && !out_capture_count;

            if (then_base && then_base->kind == AST_EXPR_CALL &&
                ir_try_resolve_exact_returned_function_value_parameter_passthrough_base_expr(
                    ctx,
                    then_base,
                    &then_passthrough_base) &&
                then_passthrough_base) {
                then_base = then_passthrough_base;
            }
            if (else_base && else_base->kind == AST_EXPR_CALL &&
                ir_try_resolve_exact_returned_function_value_parameter_passthrough_base_expr(
                    ctx,
                    else_base,
                    &else_passthrough_base) &&
                else_passthrough_base) {
                else_base = else_passthrough_base;
            }

            if (!then_base || !else_base ||
                (then_base->kind != AST_EXPR_IDENTIFIER &&
                    then_base->kind != AST_EXPR_CALL &&
                    then_base->kind != AST_EXPR_CLOSURE &&
                    then_base->kind != AST_EXPR_TERNARY) ||
                (else_base->kind != AST_EXPR_IDENTIFIER &&
                    else_base->kind != AST_EXPR_CALL &&
                    else_base->kind != AST_EXPR_CLOSURE &&
                    else_base->kind != AST_EXPR_TERNARY) ||
                !ir_call_argument_resolve_extension_function_value_target(
                    program,
                    ctx,
                    callee_external,
                    arg_index,
                    then_base,
                    &then_capture_local_ids,
                    &then_capture_count,
                    &then_target_name) ||
                !ir_call_argument_resolve_extension_function_value_target(
                    program,
                    ctx,
                    callee_external,
                    arg_index,
                    else_base,
                    &else_capture_local_ids,
                    &else_capture_count,
                    &else_target_name) ||
                !then_target_name || !else_target_name) {
                return 0;
            }
            if (then_capture_count != else_capture_count) {
                return 0;
            }
            if (target_discovery_only) {
                if (out_target_name) {
                    *out_target_name = then_target_name;
                }
                if (out_capture_count) {
                    *out_capture_count = then_capture_count;
                }
                return 1;
            }
            if (then_target_name && else_target_name &&
                strcmp(then_target_name, else_target_name) == 0) {
                if (out_target_name) {
                    *out_target_name = then_target_name;
                }
                if (out_capture_local_ids) {
                    *out_capture_local_ids = then_capture_local_ids;
                }
                if (out_capture_count) {
                    *out_capture_count = then_capture_count;
                }
                return 1;
            }
            if (out_target_name) {
                *out_target_name = then_target_name;
            }
            if (out_capture_local_ids) {
                *out_capture_local_ids = then_capture_local_ids;
            }
            if (out_capture_count) {
                *out_capture_count = then_capture_count;
            }
            return 1;
        }
        if (ctx && arg_base && arg_base->kind == AST_EXPR_CLOSURE) {
            char *closure_helper_name = NULL;
            size_t closure_capture_count = arg_base->as.closure.capture_name_count;
            size_t closure_capture_index;
            size_t *closure_capture_local_ids = NULL;

            if (closure_capture_count > 0u) {
                closure_capture_local_ids =
                    (size_t *)malloc(closure_capture_count * sizeof(size_t));
                if (!closure_capture_local_ids) {
                    return 0;
                }
            }
            for (closure_capture_index = 0u; closure_capture_index < closure_capture_count; ++closure_capture_index) {
                const char *capture_name =
                    arg_base->as.closure.capture_names
                        ? arg_base->as.closure.capture_names[closure_capture_index]
                        : NULL;
                AstExpression capture_expr;
                IrValueRef capture_value;
                char capture_local_name[256];

                if (!capture_name || capture_name[0] == '\0') {
                    free(closure_capture_local_ids);
                    return 0;
                }
                memset(&capture_expr, 0, sizeof(capture_expr));
                capture_expr.kind = AST_EXPR_IDENTIFIER;
                capture_expr.line = arg_base->line;
                capture_expr.column = arg_base->column;
                capture_expr.as.identifier.name = (char *)capture_name;
                capture_expr.as.identifier.name_length = strlen(capture_name);
                if (!ir_lower_expression((IrLowerContext *)ctx, &capture_expr, &capture_value)) {
                    free(closure_capture_local_ids);
                    return 0;
                }
                snprintf(capture_local_name,
                    sizeof(capture_local_name),
                    "__argclosure_cap_%zu",
                    closure_capture_index);
                if (!ir_function_append_local(
                        ctx->function,
                        capture_local_name,
                        0,
                        &closure_capture_local_ids[closure_capture_index],
                        ctx->error) ||
                    !ir_emit_mov((IrLowerContext *)ctx,
                        ir_value_local(closure_capture_local_ids[closure_capture_index]),
                        capture_value)) {
                    free(closure_capture_local_ids);
                    return 0;
                }
            }
            if (!ir_build_closure_helper_name(
                    ctx->function && ctx->function->name ? ctx->function->name : "fn",
                    "__argclosure",
                    ((size_t)(arg_base->line > 0 ? arg_base->line : 0) << 16) |
                        (size_t)(arg_base->column > 0 ? arg_base->column : 0),
                    &closure_helper_name,
                    ctx->error) ||
                !ir_lower_closure_helper_function_from_expr(
                    (IrLowerContext *)ctx,
                    arg_base,
                    closure_helper_name,
                    (const char *const *)arg_base->as.closure.capture_names,
                    closure_capture_local_ids,
                    closure_capture_count,
                    ctx->error)) {
                free(closure_capture_local_ids);
                free(closure_helper_name);
                return 0;
            }
            if (out_capture_local_ids) {
                *out_capture_local_ids = closure_capture_local_ids;
            } else {
                free(closure_capture_local_ids);
            }
            if (out_capture_count) {
                *out_capture_count = closure_capture_count;
            }
            if (out_target_name) {
                IrFunction *helper_function =
                    ctx->program ? ir_program_find_function_mut(ctx->program, closure_helper_name) : NULL;
                *out_target_name = helper_function ? helper_function->name : closure_helper_name;
            }
            free(closure_helper_name);
            return 1;
        }
        if (ctx && arg_base && arg_base->kind == AST_EXPR_CALL &&
            ir_try_resolve_returned_function_value_parameter_argument_target(
                program,
                ctx,
                arg_base,
                out_capture_local_ids,
                out_capture_count,
                out_target_name)) {
            return 1;
        }
        if (ctx && arg_base && arg_base->kind == AST_EXPR_CALL &&
            ir_try_resolve_returned_function_value_argument_target(
                program,
                ctx,
                arg_base,
                out_capture_local_ids,
                out_capture_count,
                out_target_name)) {
            return 1;
        }
    }

    if (ctx) {
        if (!ir_resolve_function_value_wrapper_direct_identifier(ctx, arg_expr, &arg_name) ||
            !arg_name || arg_name[0] == '\0') {
            return 0;
        }
    } else if (!ir_classify_direct_identifier_expr(arg_expr, &arg_name) ||
        !arg_name || arg_name[0] == '\0') {
        return 0;
    }

    if (ctx) {
        IrLowerCallableObjectView arg_view;
        int have_arg_view = 0;
        const char *source_arg_name = arg_name;
        memset(&arg_view, 0, sizeof(arg_view));
        arg_view.dynamic_tag_local_id = (size_t)-1;
        if (ir_resolve_callable_object_view((IrLowerContext *)ctx, arg_expr, &arg_view)) {
            have_arg_view = 1;
        }
        if (have_arg_view &&
            ir_project_static_callable_object_view_target_binding(
                &arg_view,
                out_capture_local_ids,
                out_capture_count,
                out_target_name)) {
            return 1;
        }
        if (have_arg_view &&
            arg_view.direct_target_name &&
            arg_view.direct_target_name[0] != '\0') {
            arg_name = arg_view.direct_target_name;
            resolved_from_runtime_binding = 1;
            if (arg_view.is_closure_family && arg_view.capture_count > 0u) {
                if (out_capture_local_ids) {
                    *out_capture_local_ids = arg_view.capture_local_ids;
                }
                if (out_capture_count) {
                    *out_capture_count = arg_view.capture_count;
                }
            } else {
                (void)ir_project_static_callable_object_view_target_binding(
                    &arg_view,
                    out_capture_local_ids,
                    out_capture_count,
                    NULL);
            }
        } else {
            const char *bound_target_name = ir_lower_lookup_function_value_target(ctx, arg_name);

            if (bound_target_name && bound_target_name[0] != '\0') {
                resolved_from_runtime_binding = 1;
                arg_name = bound_target_name;
                (void)ir_lower_lookup_function_value_capture_binding(
                    ctx,
                    source_arg_name,
                    out_capture_local_ids,
                    out_capture_count);
                for (external_index = 0u; external_index < program->count; ++external_index) {
                    const AstExternal *candidate = &program->externals[external_index];

                    if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                        candidate->name &&
                        strcmp(candidate->name, bound_target_name) == 0) {
                        arg_function = candidate;
                        break;
                    }
                }
            }
        }
    }

    if (!arg_function) {
        for (external_index = 0u; external_index < program->count; ++external_index) {
            const AstExternal *candidate = &program->externals[external_index];

            if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                candidate->name &&
                strcmp(candidate->name, arg_name) == 0) {
                arg_function = candidate;
                break;
            }
        }
    }

    if (!arg_function) {
        size_t expected_param_count = callee_external->parameter_function_parameter_counts
            ? callee_external->parameter_function_parameter_counts[arg_index]
            : 0u;
        AstFunctionReturnType expected_return_type = callee_external->parameter_function_return_types
            ? callee_external->parameter_function_return_types[arg_index]
            : AST_FUNCTION_RETURN_INT;

        if (!ir_builtin_function_matches_signature(arg_name, expected_return_type, expected_param_count)) {
            if (resolved_from_runtime_binding &&
                ctx &&
                ctx->program &&
                ir_program_find_function_mut(ctx->program, arg_name)) {
                if (out_target_name) {
                    *out_target_name = arg_name;
                }
                return 1;
            }
            return 0;
        }

        if (out_target_name) {
            *out_target_name = arg_name;
        }
        return 1;
    }

    if (callee_external->parameter_function_return_types &&
        callee_external->parameter_function_return_types[arg_index] != arg_function->function_return_type) {
        return 0;
    }

    if (callee_external->parameter_function_parameter_counts &&
        callee_external->parameter_function_parameter_counts[arg_index] != arg_function->parameter_count) {
        return 0;
    }

    if (out_target_name) {
        *out_target_name = arg_function->name;
    }
    return 1;
}

static int ir_classify_direct_identifier_expr(const AstExpression *expr, const char **out_name) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);

    if (out_name) {
        *out_name = NULL;
    }
    if (!base || base->kind != AST_EXPR_IDENTIFIER) {
        return 0;
    }
    if (out_name) {
        *out_name = base->as.identifier.name;
    }
    return 1;
}

static const char *ir_lower_lookup_function_value_target(const IrLowerContext *ctx, const char *name) {
    size_t index;

    if (!ctx || !name || name[0] == '\0' ||
        !ctx->function_value_parameter_names ||
        !ctx->function_value_parameter_targets) {
        return NULL;
    }

    for (index = 0u; index < ctx->function_value_parameter_count; ++index) {
        if (ctx->function_value_parameter_names[index] &&
            ctx->function_value_parameter_targets[index] &&
            strcmp(ctx->function_value_parameter_names[index], name) == 0) {
            return ctx->function_value_parameter_targets[index];
        }
    }

    return NULL;
}

static int ir_lower_lookup_function_value_capture_binding(const IrLowerContext *ctx,
    const char *name,
    const size_t **out_capture_local_ids,
    size_t *out_capture_count) {
    size_t index;

    if (out_capture_local_ids) {
        *out_capture_local_ids = NULL;
    }
    if (out_capture_count) {
        *out_capture_count = 0u;
    }
    if (!ctx || !name || name[0] == '\0' ||
        !ctx->function_value_parameter_names ||
        !ctx->function_value_parameter_targets ||
        !ctx->function_value_parameter_capture_counts ||
        !ctx->function_value_parameter_capture_local_ids) {
        return 0;
    }

    for (index = 0u; index < ctx->function_value_parameter_count; ++index) {
        if (ctx->function_value_parameter_names[index] &&
            ctx->function_value_parameter_targets[index] &&
            strcmp(ctx->function_value_parameter_names[index], name) == 0) {
            if (out_capture_local_ids) {
                *out_capture_local_ids = ctx->function_value_parameter_capture_local_ids[index];
            }
            if (out_capture_count) {
                *out_capture_count = ctx->function_value_parameter_capture_counts[index];
            }
            return 1;
        }
    }

    return 0;
}

static int ir_find_external_parameter_index_by_name(const AstExternal *external,
    const char *name,
    int require_function_kind,
    size_t *out_index) {
    size_t index;

    if (out_index) {
        *out_index = (size_t)-1;
    }
    if (!external || !name || name[0] == '\0' ||
        !external->parameter_names || !external->parameter_value_kinds) {
        return 0;
    }

    for (index = 0u; index < external->parameter_count; ++index) {
        if (!external->parameter_names[index] ||
            strcmp(external->parameter_names[index], name) != 0) {
            continue;
        }
        if (require_function_kind &&
            external->parameter_value_kinds[index] != AST_PARAMETER_VALUE_FUNCTION) {
            return 0;
        }
        if (out_index) {
            *out_index = index;
        }
        return 1;
    }

    return 0;
}

static int ir_try_resolve_function_parameter_passthrough_alias_name(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const AstExpression *expr,
    const char **out_parameter_name) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);
    size_t alias_index;

    if (out_parameter_name) {
        *out_parameter_name = NULL;
    }
    if (!ctx || !callee_external || !expr || !out_parameter_name || !base) {
        return 0;
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
        if (ir_find_external_parameter_index_by_name(
                callee_external,
                base->as.identifier.name,
                1,
                NULL)) {
            *out_parameter_name = base->as.identifier.name;
            return 1;
        }
        for (alias_index = 0u; alias_index < alias_count; ++alias_index) {
            if (aliases[alias_index].local_name &&
                aliases[alias_index].source_parameter_name &&
                strcmp(aliases[alias_index].local_name, base->as.identifier.name) == 0) {
                *out_parameter_name = aliases[alias_index].source_parameter_name;
                return 1;
            }
        }
        return 0;
    }

    if (base->kind == AST_EXPR_CALL && ctx->ast_program) {
        const char *called_name = ir_extract_direct_call_callee_name(base->as.call.callee);
        const AstExternal *nested_external = NULL;
        size_t passthrough_index = (size_t)-1;

        if ((!called_name || called_name[0] == '\0') &&
            !ir_classify_direct_identifier_expr(base->as.call.callee, &called_name)) {
            return 0;
        }
        if (!called_name || called_name[0] == '\0') {
            return 0;
        }
        nested_external =
            ir_find_named_function_external_prefer_definition(ctx->ast_program, called_name);
        if (!nested_external ||
            !nested_external->has_function_return_signature ||
            !ir_function_value_return_exact_parameter_passthrough_index(
                ctx->ast_program,
                nested_external,
                &passthrough_index) ||
            passthrough_index == (size_t)-1 ||
            !base->as.call.args ||
            passthrough_index >= base->as.call.arg_count) {
            return 0;
        }
        return ir_try_resolve_function_parameter_passthrough_alias_name(
            ctx,
            callee_external,
            aliases,
            alias_count,
            base->as.call.args[passthrough_index],
            out_parameter_name);
    }

    return 0;
}

static int ir_statement_is_zero_return(const AstStatement *stmt) {
    const AstExpression *base = NULL;

    if (!stmt || stmt->kind != AST_STMT_RETURN ||
        stmt->expression_count != 1u || !stmt->expressions || !stmt->expressions[0]) {
        return 0;
    }
    base = ir_unwrap_paren_expression(stmt->expressions[0]);
    return base && base->kind == AST_EXPR_NUMBER && base->as.number_value == 0;
}

static int ir_statement_is_void_return(const AstStatement *stmt) {
    return stmt &&
        stmt->kind == AST_STMT_RETURN &&
        stmt->expression_count == 0u;
}

static size_t ir_find_function_parameter_passthrough_alias_index(
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const char *name) {
    size_t alias_index;

    if (!aliases || !name || name[0] == '\0') {
        return (size_t)-1;
    }
    for (alias_index = 0u; alias_index < alias_count; ++alias_index) {
        if (aliases[alias_index].local_name &&
            strcmp(aliases[alias_index].local_name, name) == 0) {
            return alias_index;
        }
    }

    return (size_t)-1;
}

static size_t ir_find_scalar_parameter_passthrough_alias_index(
    const IrScalarParameterPassthroughAlias *aliases,
    size_t alias_count,
    const char *name) {
    size_t alias_index;

    if (!aliases || !name || name[0] == '\0') {
        return (size_t)-1;
    }
    for (alias_index = 0u; alias_index < alias_count; ++alias_index) {
        if (aliases[alias_index].local_name &&
            strcmp(aliases[alias_index].local_name, name) == 0) {
            return alias_index;
        }
    }

    return (size_t)-1;
}

static int ir_try_resolve_scalar_parameter_passthrough_alias_name(
    const AstExternal *callee_external,
    const IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_count,
    const AstExpression *expr,
    const char **out_parameter_name) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    size_t param_index;
    size_t alias_index;

    if (out_parameter_name) {
        *out_parameter_name = NULL;
    }
    if (!callee_external || !out_parameter_name || !base ||
        base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }

    if (callee_external->parameter_names && callee_external->parameter_value_kinds) {
        for (param_index = 0u; param_index < callee_external->parameter_count; ++param_index) {
            if (callee_external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION ||
                !callee_external->parameter_names[param_index]) {
                continue;
            }
            if (strcmp(callee_external->parameter_names[param_index], base->as.identifier.name) == 0) {
                *out_parameter_name = callee_external->parameter_names[param_index];
                return 1;
            }
        }
    }

    for (alias_index = 0u; alias_index < scalar_alias_count; ++alias_index) {
        if (scalar_aliases[alias_index].local_name &&
            scalar_aliases[alias_index].source_parameter_name &&
            strcmp(scalar_aliases[alias_index].local_name, base->as.identifier.name) == 0) {
            *out_parameter_name = scalar_aliases[alias_index].source_parameter_name;
            return 1;
        }
    }

    return 0;
}

static int ir_passthrough_stmt_uses_only_function_arguments(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const AstStatement *passthrough_stmt) {
    const AstExpression *callee_expr = NULL;
    const AstExpression *callee_base = NULL;
    size_t arg_index;

    if (!ctx || !callee_external || !passthrough_stmt) {
        return 0;
    }

    if (passthrough_stmt->kind == AST_STMT_RETURN &&
        passthrough_stmt->expression_count == 1u &&
        passthrough_stmt->expressions) {
        callee_expr = passthrough_stmt->expressions[0];
    } else if (passthrough_stmt->kind == AST_STMT_EXPRESSION &&
        passthrough_stmt->expression_count == 1u &&
        passthrough_stmt->expressions) {
        callee_expr = passthrough_stmt->expressions[0];
    } else {
        return 0;
    }

    callee_base = ir_unwrap_function_value_wrapper_expression(callee_expr);
    if (!callee_base || callee_base->kind != AST_EXPR_CALL) {
        return 0;
    }

    for (arg_index = 0u; arg_index < callee_base->as.call.arg_count; ++arg_index) {
        const AstExpression *arg_expr = callee_base->as.call.args
            ? callee_base->as.call.args[arg_index]
            : NULL;
        const char *parameter_name = NULL;

        if (!arg_expr ||
            !ir_try_resolve_function_parameter_passthrough_alias_name(
                ctx,
                callee_external,
                aliases,
                alias_count,
                arg_expr,
                &parameter_name) ||
            !parameter_name ||
            parameter_name[0] == '\0') {
            return 0;
        }
    }

    return 1;
}

static int ir_try_collect_function_parameter_passthrough_aliases(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    const AstStatement *body,
    IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_capacity,
    IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_capacity,
    size_t *out_alias_count,
    size_t *out_scalar_alias_count,
    const AstStatement **out_passthrough_stmt,
    int *out_returns_zero_after_void_call) {
    size_t child_index;
    size_t alias_count = 0u;
    size_t scalar_alias_count = 0u;

    if (out_alias_count) {
        *out_alias_count = 0u;
    }
    if (out_scalar_alias_count) {
        *out_scalar_alias_count = 0u;
    }
    if (out_passthrough_stmt) {
        *out_passthrough_stmt = NULL;
    }
    if (out_returns_zero_after_void_call) {
        *out_returns_zero_after_void_call = 0;
    }
    if (!ctx || !callee_external || !body || body->kind != AST_STMT_COMPOUND ||
        !body->children || body->child_count == 0u || !aliases || !scalar_aliases ||
        !out_alias_count || !out_scalar_alias_count || !out_passthrough_stmt) {
        return 0;
    }

    if (body->child_count >= 2u &&
        (ir_statement_is_zero_return(body->children[body->child_count - 1u]) ||
            ir_statement_is_void_return(body->children[body->child_count - 1u]))) {
        if (out_returns_zero_after_void_call) {
            *out_returns_zero_after_void_call = 1;
        }
        *out_passthrough_stmt = body->children[body->child_count - 2u];
        if (!*out_passthrough_stmt) {
            return 0;
        }
    } else {
        *out_passthrough_stmt = body->children[body->child_count - 1u];
        if (!*out_passthrough_stmt) {
            return 0;
        }
    }

    if (!*out_passthrough_stmt) {
        return 0;
    }

    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children[child_index];
        const AstExpression *init_expr = NULL;
        const AstExpression *expr = NULL;
        const AstExpression *base = NULL;
        const AstExpression *lhs = NULL;
        const AstExpression *rhs = NULL;
        const char *source_parameter_name = NULL;
        size_t alias_index = (size_t)-1;
        size_t scalar_alias_index = (size_t)-1;

        if (child == *out_passthrough_stmt ||
            (out_returns_zero_after_void_call &&
                *out_returns_zero_after_void_call &&
                (ir_statement_is_zero_return(child) ||
                    ir_statement_is_void_return(child)))) {
            continue;
        }

        if (!child) {
            return 0;
        }

        if (child->kind == AST_STMT_DECLARATION) {
            if (!child->declaration_names ||
                !child->declaration_value_kinds ||
                child->declaration_name_count != 1u ||
                !child->declaration_names[0] ||
                child->declaration_names[0][0] == '\0' ||
                !child->expressions ||
                child->expression_count != 1u ||
                !child->expressions[0]) {
                return 0;
            }

            init_expr = child->expressions[0];
            if (child->declaration_value_kinds[0] == AST_DECLARATION_VALUE_FUNCTION) {
                if (alias_count >= alias_capacity ||
                    !ir_try_resolve_function_parameter_passthrough_alias_name(
                        ctx,
                        callee_external,
                        aliases,
                        alias_count,
                        init_expr,
                        &source_parameter_name) ||
                    !source_parameter_name ||
                    source_parameter_name[0] == '\0' ||
                    ir_find_function_parameter_passthrough_alias_index(
                        aliases,
                        alias_count,
                        child->declaration_names[0]) != (size_t)-1) {
                    return 0;
                }

                aliases[alias_count].local_name = child->declaration_names[0];
                aliases[alias_count].source_parameter_name = source_parameter_name;
                alias_count += 1u;
                continue;
            }

            if (child->declaration_value_kinds[0] == AST_DECLARATION_VALUE_INT) {
                if (ir_try_resolve_scalar_parameter_passthrough_alias_name(
                        callee_external,
                        scalar_aliases,
                        scalar_alias_count,
                        init_expr,
                        &source_parameter_name) &&
                    source_parameter_name &&
                    source_parameter_name[0] != '\0' &&
                    ir_find_scalar_parameter_passthrough_alias_index(
                        scalar_aliases,
                        scalar_alias_count,
                        child->declaration_names[0]) == (size_t)-1) {
                    if (scalar_alias_count >= scalar_alias_capacity) {
                        return 0;
                    }
                    scalar_aliases[scalar_alias_count].local_name = child->declaration_names[0];
                    scalar_aliases[scalar_alias_count].source_parameter_name = source_parameter_name;
                    scalar_alias_count += 1u;
                }
                continue;
            }

            return 0;
            continue;
        }

        if (child->kind != AST_STMT_EXPRESSION ||
            !child->expressions ||
            child->expression_count != 1u ||
            !child->expressions[0]) {
            return 0;
        }

        expr = child->expressions[0];
        base = ir_unwrap_paren_expression(expr);
        if (!base || base->kind != AST_EXPR_BINARY || base->as.binary.op != TOKEN_ASSIGN) {
            return 0;
        }
        lhs = ir_unwrap_paren_expression(base->as.binary.left);
        rhs = base->as.binary.right;
        if (!lhs || lhs->kind != AST_EXPR_IDENTIFIER || !lhs->as.identifier.name || !rhs) {
            return 0;
        }
        alias_index = ir_find_function_parameter_passthrough_alias_index(
            aliases,
            alias_count,
            lhs->as.identifier.name);
        if (alias_index != (size_t)-1) {
            if (!ir_try_resolve_function_parameter_passthrough_alias_name(
                    ctx,
                    callee_external,
                    aliases,
                    alias_count,
                    rhs,
                    &source_parameter_name) ||
                !source_parameter_name ||
                source_parameter_name[0] == '\0') {
                return 0;
            }

            aliases[alias_index].source_parameter_name = source_parameter_name;
            continue;
        }

        scalar_alias_index = ir_find_scalar_parameter_passthrough_alias_index(
            scalar_aliases,
            scalar_alias_count,
            lhs->as.identifier.name);
        if (scalar_alias_index == (size_t)-1) {
            if (ir_passthrough_stmt_uses_only_function_arguments(
                    ctx,
                    callee_external,
                    aliases,
                    alias_count,
                    *out_passthrough_stmt)) {
                continue;
            }
            return 0;
        }
        if (ir_try_resolve_scalar_parameter_passthrough_alias_name(
                callee_external,
                scalar_aliases,
                scalar_alias_count,
                rhs,
                &source_parameter_name) &&
            source_parameter_name &&
            source_parameter_name[0] != '\0') {
            scalar_aliases[scalar_alias_index].source_parameter_name = source_parameter_name;
        } else {
            scalar_aliases[scalar_alias_index].source_parameter_name = NULL;
        }
    }

    *out_alias_count = alias_count;
    *out_scalar_alias_count = scalar_alias_count;
    return 1;
}

static int ir_try_lower_passthrough_bound_value_expr(IrLowerContext *ctx,
    const AstExternal *callee_external,
    const IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_count,
    const AstExpression *call_expr,
    const AstExpression *expr,
    IrValueRef *out_value) {
    const AstExpression *base = ir_unwrap_function_value_wrapper_expression(expr);
    const char *parameter_name = NULL;
    size_t source_arg_index = (size_t)-1;

    if (!ctx || !callee_external || !call_expr || !expr || !out_value || !base) {
        return 0;
    }
    if (!ir_try_resolve_scalar_parameter_passthrough_alias_name(
            callee_external,
            scalar_aliases,
            scalar_alias_count,
            base,
            &parameter_name) ||
        !parameter_name ||
        parameter_name[0] == '\0' ||
        !ir_find_external_parameter_index_by_name(
            callee_external,
            parameter_name,
            0,
            &source_arg_index) ||
        source_arg_index >= call_expr->as.call.arg_count) {
        return 0;
    }
    return ir_lower_expression(ctx, call_expr->as.call.args[source_arg_index], out_value);
}

static void ir_function_body_eval_binding_clear(IrFunctionBodyEvalBinding *binding) {
    if (!binding) {
        return;
    }
    if (binding->has_callable_view) {
        ir_free_callable_object_view_storage(&binding->callable_view);
    }
    memset(binding, 0, sizeof(*binding));
    binding->callable_view.dynamic_tag_local_id = (size_t)-1;
}

static void ir_function_body_eval_binding_array_clear(
    IrFunctionBodyEvalBinding *bindings,
    size_t binding_count) {
    size_t index;

    if (!bindings) {
        return;
    }
    for (index = 0u; index < binding_count; ++index) {
        ir_function_body_eval_binding_clear(&bindings[index]);
    }
}

static const IrFunctionBodyEvalBinding *ir_find_function_body_eval_binding(
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    const char *name) {
    size_t index;

    if (!bindings || !name || name[0] == '\0') {
        return NULL;
    }
    for (index = 0u; index < binding_count; ++index) {
        if (bindings[index].name && strcmp(bindings[index].name, name) == 0) {
            return &bindings[index];
        }
    }
    return NULL;
}

static int ir_shallow_copy_callable_view_for_eval_binding(
    const IrLowerCallableObjectView *source,
    IrLowerCallableObjectView *dest) {
    if (!source || !dest) {
        return 0;
    }
    *dest = *source;
    dest->owned_dynamic_target_names = NULL;
    dest->owned_payload_local_ids = NULL;
    return 1;
}

static int ir_try_eval_simple_function_body_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    IrValueRef *out_value,
    int *out_applied);

static int ir_try_eval_narrow_static_helper_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied);

static int ir_simple_helper_body_expression_is_evaluable(
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count);

static int ir_narrow_helper_body_expression_is_evaluable(
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    size_t recursion_depth);

static int ir_try_eval_simple_helper_body_unary_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrValueRef operand = ir_value_immediate(0);
    int operand_applied = 0;
    IrBinaryOp op = IR_BINARY_ADD;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !base || !bindings || !out_value || !out_applied ||
        base->kind != AST_EXPR_UNARY) {
        return 1;
    }

    if (!ir_try_eval_simple_function_body_expression(
            ctx,
            base->as.unary.operand,
            bindings,
            binding_count,
            &operand,
            &operand_applied)) {
        return 0;
    }
    if (!operand_applied) {
        return 1;
    }

    switch (base->as.unary.op) {
    case TOKEN_PLUS:
        *out_value = operand;
        *out_applied = 1;
        return 1;
    case TOKEN_MINUS:
        op = IR_BINARY_SUB;
        break;
    case TOKEN_TILDE:
        op = IR_BINARY_BIT_XOR;
        break;
    case TOKEN_BANG:
        op = IR_BINARY_EQ;
        break;
    default:
        return 1;
    }

    *out_value = ir_value_temp(ir_allocate_temp(ctx->function));
    if (base->as.unary.op == TOKEN_MINUS) {
        return ir_emit_binary(
                   ctx,
                   *out_value,
                   op,
                   ir_value_immediate(0),
                   operand) &&
            ((*out_applied = 1), 1);
    }
    if (base->as.unary.op == TOKEN_TILDE) {
        return ir_emit_binary(
                   ctx,
                   *out_value,
                   op,
                   operand,
                   ir_value_immediate(-1)) &&
            ((*out_applied = 1), 1);
    }
    return ir_emit_binary(
               ctx,
               *out_value,
               op,
               operand,
               ir_value_immediate(0)) &&
        ((*out_applied = 1), 1);
}

static int ir_try_eval_narrow_helper_body_unary_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrValueRef operand = ir_value_immediate(0);
    int operand_applied = 0;
    IrBinaryOp op = IR_BINARY_ADD;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !base || !bindings || !out_value || !out_applied ||
        base->kind != AST_EXPR_UNARY) {
        return 1;
    }

    if (!ir_try_eval_narrow_static_helper_expression(
            ctx,
            base->as.unary.operand,
            bindings,
            binding_count,
            recursion_depth,
            &operand,
            &operand_applied)) {
        return 0;
    }
    if (!operand_applied) {
        return 1;
    }

    switch (base->as.unary.op) {
    case TOKEN_PLUS:
        *out_value = operand;
        *out_applied = 1;
        return 1;
    case TOKEN_MINUS:
        op = IR_BINARY_SUB;
        break;
    case TOKEN_TILDE:
        op = IR_BINARY_BIT_XOR;
        break;
    case TOKEN_BANG:
        op = IR_BINARY_EQ;
        break;
    default:
        return 1;
    }

    *out_value = ir_value_temp(ir_allocate_temp(ctx->function));
    if (base->as.unary.op == TOKEN_MINUS) {
        return ir_emit_binary(
                   ctx,
                   *out_value,
                   op,
                   ir_value_immediate(0),
                   operand) &&
            ((*out_applied = 1), 1);
    }
    if (base->as.unary.op == TOKEN_TILDE) {
        return ir_emit_binary(
                   ctx,
                   *out_value,
                   op,
                   operand,
                   ir_value_immediate(-1)) &&
            ((*out_applied = 1), 1);
    }
    return ir_emit_binary(
               ctx,
               *out_value,
               op,
               operand,
               ir_value_immediate(0)) &&
        ((*out_applied = 1), 1);
}

static int ir_simple_helper_body_expression_is_evaluable(
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    size_t arg_index;

    if (!base || !bindings) {
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_NUMBER:
        return 1;
    case AST_EXPR_IDENTIFIER: {
        const IrFunctionBodyEvalBinding *binding =
            base->as.identifier.name
                ? ir_find_function_body_eval_binding(
                    bindings,
                    binding_count,
                    base->as.identifier.name)
                : NULL;
        return binding && binding->has_scalar_value;
    }
    case AST_EXPR_UNARY:
        return base->as.unary.op == TOKEN_PLUS ||
            base->as.unary.op == TOKEN_MINUS ||
            base->as.unary.op == TOKEN_TILDE ||
            base->as.unary.op == TOKEN_BANG
            ? ir_simple_helper_body_expression_is_evaluable(
                base->as.unary.operand,
                bindings,
                binding_count)
            : 0;
    case AST_EXPR_BINARY: {
        int op_supported = 0;

        if (base->as.binary.op == TOKEN_COMMA) {
            return ir_simple_helper_body_expression_is_evaluable(
                       base->as.binary.left,
                       bindings,
                       binding_count) &&
                ir_simple_helper_body_expression_is_evaluable(
                    base->as.binary.right,
                    bindings,
                    binding_count);
        }
        (void)ir_binary_op_from_token(base->as.binary.op, &op_supported);
        return op_supported &&
            ir_simple_helper_body_expression_is_evaluable(
                base->as.binary.left,
                bindings,
                binding_count) &&
            ir_simple_helper_body_expression_is_evaluable(
                base->as.binary.right,
                bindings,
                binding_count);
    }
    case AST_EXPR_TERNARY:
        return ir_simple_helper_body_expression_is_evaluable(
                   base->as.ternary.condition,
                   bindings,
                   binding_count) &&
            ir_simple_helper_body_expression_is_evaluable(
                base->as.ternary.then_expr,
                bindings,
                binding_count) &&
            ir_simple_helper_body_expression_is_evaluable(
                base->as.ternary.else_expr,
                bindings,
                binding_count);
    case AST_EXPR_CALL: {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const IrFunctionBodyEvalBinding *callee_binding = NULL;

        if (!callee_base ||
            callee_base->kind != AST_EXPR_IDENTIFIER ||
            !callee_base->as.identifier.name) {
            return 0;
        }
        callee_binding = ir_find_function_body_eval_binding(
            bindings,
            binding_count,
            callee_base->as.identifier.name);
        if (!callee_binding || !callee_binding->has_callable_view) {
            return 0;
        }
        for (arg_index = 0u; arg_index < base->as.call.arg_count; ++arg_index) {
            if (!ir_simple_helper_body_expression_is_evaluable(
                    base->as.call.args[arg_index],
                    bindings,
                    binding_count)) {
                return 0;
            }
        }
        return 1;
    }
    default:
        return 0;
    }
}

static int ir_narrow_helper_body_expression_is_evaluable(
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    size_t recursion_depth) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    size_t arg_index;

    if (!base || !bindings) {
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_NUMBER:
        return 1;
    case AST_EXPR_IDENTIFIER: {
        const IrFunctionBodyEvalBinding *binding =
            base->as.identifier.name
                ? ir_find_function_body_eval_binding(
                    bindings,
                    binding_count,
                    base->as.identifier.name)
                : NULL;
        return binding && binding->has_scalar_value;
    }
    case AST_EXPR_UNARY:
        return base->as.unary.op == TOKEN_PLUS ||
            base->as.unary.op == TOKEN_MINUS ||
            base->as.unary.op == TOKEN_TILDE ||
            base->as.unary.op == TOKEN_BANG
            ? ir_narrow_helper_body_expression_is_evaluable(
                base->as.unary.operand,
                bindings,
                binding_count,
                recursion_depth)
            : 0;
    case AST_EXPR_BINARY: {
        int op_supported = 0;

        if (base->as.binary.op == TOKEN_COMMA) {
            return ir_narrow_helper_body_expression_is_evaluable(
                       base->as.binary.left,
                       bindings,
                       binding_count,
                       recursion_depth) &&
                ir_narrow_helper_body_expression_is_evaluable(
                    base->as.binary.right,
                    bindings,
                    binding_count,
                    recursion_depth);
        }
        (void)ir_binary_op_from_token(base->as.binary.op, &op_supported);
        return op_supported &&
            ir_narrow_helper_body_expression_is_evaluable(
                base->as.binary.left,
                bindings,
                binding_count,
                recursion_depth) &&
            ir_narrow_helper_body_expression_is_evaluable(
                base->as.binary.right,
                bindings,
                binding_count,
                recursion_depth);
    }
    case AST_EXPR_TERNARY:
        return ir_narrow_helper_body_expression_is_evaluable(
                   base->as.ternary.condition,
                   bindings,
                   binding_count,
                   recursion_depth) &&
            ir_narrow_helper_body_expression_is_evaluable(
                base->as.ternary.then_expr,
                bindings,
                binding_count,
                recursion_depth) &&
            ir_narrow_helper_body_expression_is_evaluable(
                base->as.ternary.else_expr,
                bindings,
                binding_count,
                recursion_depth);
    case AST_EXPR_CALL: {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const IrFunctionBodyEvalBinding *callee_binding = NULL;

        if (!callee_base ||
            callee_base->kind != AST_EXPR_IDENTIFIER ||
            !callee_base->as.identifier.name) {
            return 0;
        }
        callee_binding = ir_find_function_body_eval_binding(
            bindings,
            binding_count,
            callee_base->as.identifier.name);
        if (!callee_binding || !callee_binding->has_callable_view) {
            return 0;
        }
        for (arg_index = 0u; arg_index < base->as.call.arg_count; ++arg_index) {
            if (!ir_narrow_helper_body_expression_is_evaluable(
                    base->as.call.args[arg_index],
                    bindings,
                    binding_count,
                    recursion_depth)) {
                return 0;
            }
        }
        return 1;
    }
    default:
        return 0;
    }
}

static int ir_try_apply_simple_function_body_statement(IrLowerContext *ctx,
    const AstStatement *stmt,
    IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    int *out_applied) {
    const AstExpression *expr = NULL;
    const AstExpression *base = NULL;
    const AstExpression *lhs = NULL;
    const AstExpression *rhs = NULL;
    const AstExpression *operand = NULL;
    size_t binding_index;
    IrFunctionBodyEvalBinding *dest_binding = NULL;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !stmt || !bindings || !out_applied) {
        return 1;
    }
    if (stmt->kind != AST_STMT_EXPRESSION ||
        stmt->expression_count != 1u ||
        !stmt->expressions ||
        !stmt->expressions[0]) {
        return 1;
    }

    expr = stmt->expressions[0];
    base = ir_unwrap_paren_expression(expr);
    if (!base) {
        return 1;
    }
    if (base->kind == AST_EXPR_BINARY) {
        lhs = ir_unwrap_paren_expression(base->as.binary.left);
        rhs = base->as.binary.right;
    } else if (base->kind == AST_EXPR_UNARY &&
               (base->as.unary.op == TOKEN_PLUS_PLUS ||
                   base->as.unary.op == TOKEN_MINUS_MINUS)) {
        lhs = ir_unwrap_paren_expression(base->as.unary.operand);
    } else if (base->kind == AST_EXPR_POSTFIX &&
               (base->as.postfix.op == TOKEN_PLUS_PLUS ||
                   base->as.postfix.op == TOKEN_MINUS_MINUS)) {
        lhs = ir_unwrap_paren_expression(base->as.postfix.operand);
    } else {
        return 1;
    }
    if (!lhs || lhs->kind != AST_EXPR_IDENTIFIER || !lhs->as.identifier.name) {
        return 1;
    }

    for (binding_index = 0u; binding_index < binding_count; ++binding_index) {
        if (bindings[binding_index].name &&
            strcmp(bindings[binding_index].name, lhs->as.identifier.name) == 0) {
            dest_binding = &bindings[binding_index];
            break;
        }
    }
    if (!dest_binding) {
        return 1;
    }

    if (dest_binding->has_scalar_value) {
        if (base->kind == AST_EXPR_BINARY) {
            IrValueRef rhs_value = ir_value_immediate(0);
            int rhs_applied = 0;
            int op_supported = 0;
            IrBinaryOp op = ir_binary_op_from_assignment_token(base->as.binary.op, &op_supported);

            if (base->as.binary.op != TOKEN_ASSIGN &&
                !op_supported) {
                return 1;
            }
            if (!ir_try_eval_simple_function_body_expression(
                    ctx,
                    rhs,
                    bindings,
                    binding_count,
                    &rhs_value,
                    &rhs_applied)) {
                return 0;
            }
            if (!rhs_applied) {
                const AstExpression *rhs_base = ir_unwrap_paren_expression(rhs);

                if (!rhs_base || rhs_base->kind != AST_EXPR_CALL) {
                    return 1;
                }
                if (!ir_try_eval_simple_function_body_expression(
                        ctx,
                        rhs,
                        bindings,
                        binding_count,
                        &rhs_value,
                        &rhs_applied)) {
                    return 0;
                }
                if (!rhs_applied) {
                    return 1;
                }
            }

            if (base->as.binary.op == TOKEN_ASSIGN) {
                dest_binding->scalar_value = rhs_value;
            } else {
                IrValueRef updated_value = ir_value_temp(ir_allocate_temp(ctx->function));

                if (!ir_emit_binary(
                        ctx,
                        updated_value,
                        op,
                        dest_binding->scalar_value,
                        rhs_value)) {
                    return 0;
                }
                dest_binding->scalar_value = updated_value;
            }
            *out_applied = 1;
            return 1;
        }

        if (base->kind == AST_EXPR_UNARY ||
            base->kind == AST_EXPR_POSTFIX) {
            IrBinaryOp update_op =
                ((base->kind == AST_EXPR_UNARY && base->as.unary.op == TOKEN_PLUS_PLUS) ||
                    (base->kind == AST_EXPR_POSTFIX && base->as.postfix.op == TOKEN_PLUS_PLUS))
                    ? IR_BINARY_ADD
                    : IR_BINARY_SUB;
            IrValueRef updated_value = ir_value_temp(ir_allocate_temp(ctx->function));

            operand = lhs;
            if (!operand) {
                return 1;
            }
            if (!ir_emit_binary(
                    ctx,
                    updated_value,
                    update_op,
                    dest_binding->scalar_value,
                    ir_value_immediate(1))) {
                return 0;
            }
            dest_binding->scalar_value = updated_value;
            *out_applied = 1;
            return 1;
        }
    }

    if (dest_binding->has_callable_view) {
        const AstExpression *rhs_base =
            ir_unwrap_function_value_wrapper_expression(rhs);
        const IrFunctionBodyEvalBinding *source_binding = NULL;
        size_t source_index;

        if (!rhs_base ||
            rhs_base->kind != AST_EXPR_IDENTIFIER ||
            !rhs_base->as.identifier.name) {
            return 1;
        }
        for (source_index = 0u; source_index < binding_count; ++source_index) {
            if (bindings[source_index].name &&
                strcmp(bindings[source_index].name, rhs_base->as.identifier.name) == 0) {
                source_binding = &bindings[source_index];
                break;
            }
        }
        if (!source_binding || !source_binding->has_callable_view) {
            return 1;
        }
        ir_function_body_eval_binding_clear(dest_binding);
        dest_binding->name = lhs->as.identifier.name;
        if (!ir_shallow_copy_callable_view_for_eval_binding(
                &source_binding->callable_view,
                &dest_binding->callable_view)) {
            return 0;
        }
        dest_binding->has_callable_view = 1;
        *out_applied = 1;
        return 1;
    }

    return 1;
}

static int ir_try_eval_simple_function_body_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !base || !bindings || !out_value || !out_applied) {
        return 1;
    }

    if (base->kind == AST_EXPR_NUMBER) {
        *out_value = ir_value_immediate(base->as.number_value);
        *out_applied = 1;
        return 1;
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
        const IrFunctionBodyEvalBinding *binding =
            ir_find_function_body_eval_binding(
                bindings,
                binding_count,
                base->as.identifier.name);
        if (!binding || !binding->has_scalar_value) {
            return 1;
        }
        *out_value = binding->scalar_value;
        *out_applied = 1;
        return 1;
    }

    if (base->kind == AST_EXPR_UNARY) {
        return ir_try_eval_simple_helper_body_unary_expression(
            ctx,
            expr,
            bindings,
            binding_count,
            out_value,
            out_applied);
    }

    if (base->kind == AST_EXPR_TERNARY) {
        IrValueRef condition_value = ir_value_immediate(0);
        int condition_applied = 0;
        IrValueRef compare_value;
        IrValueRef result_value;
        IrValueRef then_value = ir_value_immediate(0);
        IrValueRef else_value = ir_value_immediate(0);
        int then_applied = 0;
        int else_applied = 0;
        size_t then_block_id;
        size_t else_block_id;
        size_t join_block_id;

        if (!ir_simple_helper_body_expression_is_evaluable(
                base->as.ternary.condition,
                bindings,
                binding_count) ||
            !ir_simple_helper_body_expression_is_evaluable(
                base->as.ternary.then_expr,
                bindings,
                binding_count) ||
            !ir_simple_helper_body_expression_is_evaluable(
                base->as.ternary.else_expr,
                bindings,
                binding_count)) {
            return 1;
        }
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                base->as.ternary.condition,
                bindings,
                binding_count,
                &condition_value,
                &condition_applied)) {
            return 0;
        }
        if (!condition_applied) {
            return 1;
        }

        compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
        result_value = ir_value_temp(ir_allocate_temp(ctx->function));
        if (!ir_emit_binary(
                ctx,
                compare_value,
                IR_BINARY_NE,
                condition_value,
                ir_value_immediate(0)) ||
            !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
            !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
            !ir_block_set_branch(
                ir_lower_current_block(ctx),
                compare_value,
                then_block_id,
                else_block_id,
                ctx->error) ||
            !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
            return 0;
        }

        ctx->block_id = then_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                base->as.ternary.then_expr,
                bindings,
                binding_count,
                &then_value,
                &then_applied) ||
            !then_applied ||
            !ir_emit_mov(ctx, result_value, then_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return then_applied;
        }

        ctx->block_id = else_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                base->as.ternary.else_expr,
                bindings,
                binding_count,
                &else_value,
                &else_applied) ||
            !else_applied ||
            !ir_emit_mov(ctx, result_value, else_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return else_applied;
        }

        ctx->block_id = join_block_id;
        ctx->has_block = 1;
        *out_value = result_value;
        *out_applied = 1;
        return 1;
    }

    if (base->kind == AST_EXPR_BINARY) {
        if (base->as.binary.op == TOKEN_COMMA) {
            IrValueRef lhs = ir_value_immediate(0);
            IrValueRef rhs = ir_value_immediate(0);
            int lhs_applied = 0;
            int rhs_applied = 0;

            if (!ir_try_eval_simple_function_body_expression(
                    ctx,
                    base->as.binary.left,
                    bindings,
                    binding_count,
                    &lhs,
                    &lhs_applied) ||
                !lhs_applied ||
                !ir_try_eval_simple_function_body_expression(
                    ctx,
                    base->as.binary.right,
                    bindings,
                    binding_count,
                    &rhs,
                    &rhs_applied)) {
                return 0;
            }
            if (!rhs_applied) {
                return 1;
            }
            *out_value = rhs;
            *out_applied = 1;
            return 1;
        }
        IrValueRef lhs = ir_value_immediate(0);
        IrValueRef rhs = ir_value_immediate(0);
        int lhs_applied = 0;
        int rhs_applied = 0;
        int op_supported = 0;
        IrBinaryOp op = ir_binary_op_from_token(base->as.binary.op, &op_supported);

        if (!op_supported) {
            return 1;
        }
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                base->as.binary.left,
                bindings,
                binding_count,
                &lhs,
                &lhs_applied) ||
            !lhs_applied ||
            !ir_try_eval_simple_function_body_expression(
                ctx,
                base->as.binary.right,
                bindings,
                binding_count,
                &rhs,
                &rhs_applied) ||
            !rhs_applied) {
            return lhs_applied && rhs_applied;
        }

        {
            IrValueRef result = ir_value_temp(ir_allocate_temp(ctx->function));
            if (!ir_emit_binary(ctx, result, op, lhs, rhs)) {
                return 0;
            }
            *out_value = result;
            *out_applied = 1;
            return 1;
        }
    }

    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const IrFunctionBodyEvalBinding *callee_binding = NULL;
        IrValueRef *call_args = NULL;
        const IrValueRef *call_args_view = NULL;
        size_t arg_count = 0u;
        size_t arg_index;
        IrValueRef result = ir_value_immediate(0);
        int applied_dispatch = 0;

        if (!callee_base ||
            callee_base->kind != AST_EXPR_IDENTIFIER ||
            !callee_base->as.identifier.name) {
            return 1;
        }
        callee_binding = ir_find_function_body_eval_binding(
            bindings,
            binding_count,
            callee_base->as.identifier.name);
        if (!callee_binding || !callee_binding->has_callable_view) {
            return 1;
        }

        if (base->as.call.arg_count > 0u) {
            call_args = (IrValueRef *)malloc(base->as.call.arg_count * sizeof(IrValueRef));
            if (!call_args) {
                ir_set_error(ctx->error,
                    base->line,
                    base->column,
                    "IR-INT-126: out of memory while evaluating simple helper body call arguments");
                return 0;
            }
        }
        for (arg_index = 0u; arg_index < base->as.call.arg_count; ++arg_index) {
            int arg_applied = 0;

            if (!ir_try_eval_simple_function_body_expression(
                    ctx,
                    base->as.call.args[arg_index],
                    bindings,
                    binding_count,
                    &call_args[arg_count],
                    &arg_applied)) {
                free(call_args);
                return 0;
            }
            if (!arg_applied) {
                free(call_args);
                return 1;
            }
            arg_count += 1u;
        }
        call_args_view = arg_count > 0u ? call_args : NULL;
        if (!ir_try_emit_static_callable_view_dispatch_with_args(
                ctx,
                &callee_binding->callable_view,
                call_args_view,
                arg_count,
                &result,
                &applied_dispatch)) {
            free(call_args);
            return 0;
        }
        free(call_args);
        if (!applied_dispatch) {
            return 1;
        }
        *out_value = result;
        *out_applied = 1;
        return 1;
    }

    return 1;
}

static int ir_try_emit_simple_function_body_eval_dispatch(IrLowerContext *ctx,
    const AstExternal *callee_external,
    int is_closure_family,
    const size_t *capture_local_ids,
    size_t capture_count,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    const AstStatement *body = NULL;
    const AstStatement *return_stmt = NULL;
    IrFunctionBodyEvalBinding *bindings = NULL;
    size_t binding_count = 0u;
    size_t binding_capacity = 0u;
    size_t param_index;
    size_t visible_arg_index = 0u;
    size_t child_index;
    IrValueRef result = ir_value_immediate(0);
    int applied_eval = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !callee_external || !call_expr || call_expr->kind != AST_EXPR_CALL ||
        !out_value || !out_applied || !callee_external->function_body ||
        callee_external->function_body->kind != AST_STMT_COMPOUND ||
        !callee_external->parameter_names ||
        !callee_external->parameter_value_kinds) {
        return 1;
    }

    body = callee_external->function_body;
    if (!body->children || body->child_count == 0u) {
        return 1;
    }
    return_stmt = body->children[body->child_count - 1u];
    if (!return_stmt ||
        return_stmt->kind != AST_STMT_RETURN ||
        return_stmt->expression_count != 1u ||
        !return_stmt->expressions ||
        !return_stmt->expressions[0]) {
        return 1;
    }
    if (is_closure_family && capture_count > 0u && !capture_local_ids) {
        return 1;
    }
    if (!call_expr->as.call.args ||
        call_expr->as.call.arg_count + (is_closure_family ? capture_count : 0u) !=
            callee_external->parameter_count) {
        return 1;
    }

    binding_capacity = callee_external->parameter_count + (body->child_count - 1u);
    binding_count = callee_external->parameter_count;
    bindings = (IrFunctionBodyEvalBinding *)calloc(binding_capacity, sizeof(IrFunctionBodyEvalBinding));
    if (!bindings) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-126: out of memory while preparing simple helper body evaluation");
        return 0;
    }
    for (param_index = 0u; param_index < binding_count; ++param_index) {
        bindings[param_index].callable_view.dynamic_tag_local_id = (size_t)-1;
        bindings[param_index].name =
            callee_external->parameter_names ? callee_external->parameter_names[param_index] : NULL;
        if (!bindings[param_index].name || bindings[param_index].name[0] == '\0') {
            goto cleanup;
        }

        if (is_closure_family && param_index < capture_count) {
            bindings[param_index].has_scalar_value = 1;
            bindings[param_index].scalar_value =
                ir_value_local(capture_local_ids[param_index]);
            continue;
        }

        if (visible_arg_index >= call_expr->as.call.arg_count) {
            goto cleanup;
        }
        if (callee_external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION) {
            if (!ir_try_resolve_static_actual_argument_callable_object_view(
                    ctx,
                    call_expr->as.call.args[visible_arg_index],
                    &bindings[param_index].callable_view) ||
                !bindings[param_index].callable_view.direct_target_name ||
                bindings[param_index].callable_view.direct_target_name[0] == '\0' ||
                bindings[param_index].callable_view.has_dynamic_family) {
                goto cleanup;
            }
            bindings[param_index].has_callable_view = 1;
        } else {
            if (!ir_lower_expression(
                    ctx,
                    call_expr->as.call.args[visible_arg_index],
                    &bindings[param_index].scalar_value)) {
                ir_function_body_eval_binding_array_clear(bindings, binding_count);
                free(bindings);
                return 0;
            }
            bindings[param_index].has_scalar_value = 1;
        }
        visible_arg_index += 1u;
    }

    if (body->child_count == 2u &&
        body->children &&
        body->children[0] &&
        body->children[0]->kind == AST_STMT_IF &&
        body->children[1] == return_stmt &&
        body->children[0]->child_count >= 1u &&
        body->children[0]->children &&
        body->children[0]->children[0] &&
        body->children[0]->children[0]->kind == AST_STMT_RETURN &&
        body->children[0]->children[0]->expression_count == 1u &&
        body->children[0]->children[0]->expressions &&
        body->children[0]->children[0]->expressions[0] &&
        body->children[0]->child_count == 1u) {
        const AstExpression *cond_expr =
            statement_get_condition_expression(body->children[0]);
        const AstExpression *then_expr =
            body->children[0]->children[0]->expressions[0];
        const AstExpression *else_expr = return_stmt->expressions[0];
        IrValueRef condition_value = ir_value_immediate(0);
        int condition_applied = 0;
        IrValueRef compare_value;
        IrValueRef branch_result;
        IrValueRef then_value = ir_value_immediate(0);
        IrValueRef else_value = ir_value_immediate(0);
        int then_applied = 0;
        int else_applied = 0;
        size_t then_block_id;
        size_t else_block_id;
        size_t join_block_id;

        if (ir_simple_helper_body_expression_is_evaluable(
                cond_expr,
                bindings,
                binding_count) &&
            ir_simple_helper_body_expression_is_evaluable(
                then_expr,
                bindings,
                binding_count) &&
            ir_simple_helper_body_expression_is_evaluable(
                else_expr,
                bindings,
                binding_count)) {
            if (!ir_try_eval_simple_function_body_expression(
                    ctx,
                    cond_expr,
                    bindings,
                    binding_count,
                    &condition_value,
                    &condition_applied)) {
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 0;
            }
            if (condition_applied) {
                compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
                branch_result = ir_value_temp(ir_allocate_temp(ctx->function));
                if (!ir_emit_binary(
                        ctx,
                        compare_value,
                        IR_BINARY_NE,
                        condition_value,
                        ir_value_immediate(0)) ||
                    !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
                    !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
                    !ir_block_set_branch(
                        ir_lower_current_block(ctx),
                        compare_value,
                        then_block_id,
                        else_block_id,
                        ctx->error) ||
                    !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return 0;
                }

                ctx->block_id = then_block_id;
                ctx->has_block = 1;
                if (!ir_try_eval_simple_function_body_expression(
                        ctx,
                        then_expr,
                        bindings,
                        binding_count,
                        &then_value,
                        &then_applied) ||
                    !then_applied ||
                    !ir_emit_mov(ctx, branch_result, then_value) ||
                    !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return then_applied ? 0 : 1;
                }

                ctx->block_id = else_block_id;
                ctx->has_block = 1;
                if (!ir_try_eval_simple_function_body_expression(
                        ctx,
                        else_expr,
                        bindings,
                        binding_count,
                        &else_value,
                        &else_applied) ||
                    !else_applied ||
                    !ir_emit_mov(ctx, branch_result, else_value) ||
                    !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return else_applied ? 0 : 1;
                }

                ctx->block_id = join_block_id;
                ctx->has_block = 1;
                *out_value = branch_result;
                *out_applied = 1;
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 1;
            }
        }
    }

    for (child_index = 0u; child_index + 1u < body->child_count; ++child_index) {
        const AstStatement *child = body->children[child_index];
        IrFunctionBodyEvalBinding *dest_binding = NULL;
        int applied_stmt = 0;

        if (child && child->kind == AST_STMT_EXPRESSION) {
            if (!ir_try_apply_simple_function_body_statement(
                    ctx,
                    child,
                    bindings,
                    binding_count,
                    &applied_stmt)) {
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 0;
            }
            if (applied_stmt) {
                continue;
            }
            if (child->expression_count == 1u &&
                child->expressions &&
                child->expressions[0]) {
                IrValueRef ignored_value = ir_value_immediate(0);
                int ignored_applied = 0;

                if (!ir_try_eval_simple_function_body_expression(
                        ctx,
                        child->expressions[0],
                        bindings,
                        binding_count,
                        &ignored_value,
                        &ignored_applied)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return 0;
                }
                if (ignored_applied) {
                    continue;
                }
            }
            goto cleanup;
        }

        if (!child ||
            child->kind != AST_STMT_DECLARATION ||
            !child->declaration_names ||
            !child->declaration_value_kinds ||
            child->declaration_name_count != 1u ||
            !child->declaration_names[0] ||
            child->declaration_names[0][0] == '\0' ||
            !child->expressions ||
            child->expression_count != 1u ||
            !child->expressions[0] ||
            binding_count >= binding_capacity) {
            goto cleanup;
        }

        dest_binding = &bindings[binding_count];
        dest_binding->callable_view.dynamic_tag_local_id = (size_t)-1;
        dest_binding->name = child->declaration_names[0];

        if (child->declaration_value_kinds[0] == AST_DECLARATION_VALUE_FUNCTION) {
            const AstExpression *init_base =
                ir_unwrap_function_value_wrapper_expression(child->expressions[0]);
            const IrFunctionBodyEvalBinding *source_binding = NULL;

            if (!init_base ||
                init_base->kind != AST_EXPR_IDENTIFIER ||
                !init_base->as.identifier.name) {
                goto cleanup;
            }
            source_binding = ir_find_function_body_eval_binding(
                bindings,
                binding_count,
                init_base->as.identifier.name);
            if (!source_binding || !source_binding->has_callable_view ||
                !ir_shallow_copy_callable_view_for_eval_binding(
                    &source_binding->callable_view,
                    &dest_binding->callable_view)) {
                goto cleanup;
            }
            dest_binding->has_callable_view = 1;
            binding_count += 1u;
            continue;
        }

        if (child->declaration_value_kinds[0] != AST_DECLARATION_VALUE_INT) {
            goto cleanup;
        }

        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                child->expressions[0],
                bindings,
                binding_count,
                &dest_binding->scalar_value,
                &dest_binding->has_scalar_value)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        if (!dest_binding->has_scalar_value) {
            goto cleanup;
        }
        binding_count += 1u;
    }

    if (body->child_count == 2u &&
        body->children &&
        body->children[0] &&
        body->children[0]->kind == AST_STMT_IF &&
        body->children[1] == return_stmt &&
        body->children[0]->child_count >= 1u &&
        body->children[0]->children &&
        body->children[0]->children[0] &&
        body->children[0]->children[0]->kind == AST_STMT_RETURN &&
        body->children[0]->children[0]->expression_count == 1u &&
        body->children[0]->children[0]->expressions &&
        body->children[0]->children[0]->expressions[0] &&
        body->children[0]->child_count == 1u) {
        const AstExpression *cond_expr =
            statement_get_condition_expression(body->children[0]);
        const AstExpression *then_expr =
            body->children[0]->children[0]->expressions[0];
        const AstExpression *else_expr = return_stmt->expressions[0];
        IrValueRef condition_value = ir_value_immediate(0);
        int condition_applied = 0;
        IrValueRef compare_value;
        IrValueRef branch_result;
        IrValueRef then_value = ir_value_immediate(0);
        IrValueRef else_value = ir_value_immediate(0);
        int then_applied = 0;
        int else_applied = 0;
        size_t then_block_id;
        size_t else_block_id;
        size_t join_block_id;

        if (!ir_simple_helper_body_expression_is_evaluable(
                cond_expr,
                bindings,
                binding_count) ||
            !ir_simple_helper_body_expression_is_evaluable(
                then_expr,
                bindings,
                binding_count) ||
            !ir_simple_helper_body_expression_is_evaluable(
                else_expr,
                bindings,
                binding_count)) {
            goto cleanup;
        }
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                cond_expr,
                bindings,
                binding_count,
                &condition_value,
                &condition_applied)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        if (!condition_applied) {
            goto cleanup;
        }

        compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
        branch_result = ir_value_temp(ir_allocate_temp(ctx->function));
        if (!ir_emit_binary(
                ctx,
                compare_value,
                IR_BINARY_NE,
                condition_value,
                ir_value_immediate(0)) ||
            !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
            !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
            !ir_block_set_branch(
                ir_lower_current_block(ctx),
                compare_value,
                then_block_id,
                else_block_id,
                ctx->error) ||
            !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }

        ctx->block_id = then_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                then_expr,
                bindings,
                binding_count,
                &then_value,
                &then_applied) ||
            !then_applied ||
            !ir_emit_mov(ctx, branch_result, then_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return then_applied ? 0 : 1;
        }

        ctx->block_id = else_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_simple_function_body_expression(
                ctx,
                else_expr,
                bindings,
                binding_count,
                &else_value,
                &else_applied) ||
            !else_applied ||
            !ir_emit_mov(ctx, branch_result, else_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return else_applied ? 0 : 1;
        }

        ctx->block_id = join_block_id;
        ctx->has_block = 1;
        *out_value = branch_result;
        *out_applied = 1;
        ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
        free(bindings);
        return 1;
    }

    if (!ir_try_eval_simple_function_body_expression(
            ctx,
            return_stmt->expressions[0],
            bindings,
            binding_count,
            &result,
            &applied_eval)) {
        ir_function_body_eval_binding_array_clear(bindings, binding_count);
        free(bindings);
        return 0;
    }
    if (applied_eval) {
        *out_value = result;
        *out_applied = 1;
    }

cleanup:
    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
    free(bindings);
    return 1;
}

static int ir_try_build_wrapper_function_body_eval_bindings(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    const AstExpression *call_expr,
    const AstStatement *stop_before_stmt,
    IrFunctionBodyEvalBinding **out_bindings,
    size_t *out_binding_count) {
    const AstStatement *body = NULL;
    IrFunctionBodyEvalBinding *bindings = NULL;
    size_t binding_capacity = 0u;
    size_t binding_count = 0u;
    size_t param_index;
    size_t child_index;

    if (out_bindings) {
        *out_bindings = NULL;
    }
    if (out_binding_count) {
        *out_binding_count = 0u;
    }
    if (!ctx || !callee_external || !call_expr || call_expr->kind != AST_EXPR_CALL ||
        !out_bindings || !out_binding_count ||
        !callee_external->function_body ||
        callee_external->function_body->kind != AST_STMT_COMPOUND ||
        !callee_external->parameter_names ||
        !callee_external->parameter_value_kinds ||
        !call_expr->as.call.args ||
        call_expr->as.call.arg_count != callee_external->parameter_count) {
        return 1;
    }

    body = callee_external->function_body;
    binding_capacity = callee_external->parameter_count + body->child_count;
    binding_count = callee_external->parameter_count;
    bindings = (IrFunctionBodyEvalBinding *)calloc(binding_capacity, sizeof(IrFunctionBodyEvalBinding));
    if (!bindings) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-126: out of memory while preparing wrapper helper-body bindings");
        return 0;
    }

    for (param_index = 0u; param_index < binding_count; ++param_index) {
        bindings[param_index].callable_view.dynamic_tag_local_id = (size_t)-1;
        bindings[param_index].name = callee_external->parameter_names[param_index];
        if (!bindings[param_index].name || bindings[param_index].name[0] == '\0') {
            goto cleanup;
        }

        if (callee_external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION) {
            if (!ir_resolve_callable_object_view(
                    ctx,
                    call_expr->as.call.args[param_index],
                    &bindings[param_index].callable_view) ||
                (!bindings[param_index].callable_view.direct_target_name &&
                    !bindings[param_index].callable_view.has_dynamic_family)) {
                goto cleanup;
            }
            bindings[param_index].has_callable_view = 1;
            continue;
        }

        if (!ir_lower_expression(
                ctx,
                call_expr->as.call.args[param_index],
                &bindings[param_index].scalar_value)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        bindings[param_index].has_scalar_value = 1;
    }

    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children[child_index];
        IrFunctionBodyEvalBinding *dest_binding = NULL;
        int applied_stmt = 0;

        if (!child || child == stop_before_stmt) {
            break;
        }

        if (child->kind == AST_STMT_EXPRESSION) {
            if (!ir_try_apply_simple_function_body_statement(
                    ctx,
                    child,
                    bindings,
                    binding_count,
                    &applied_stmt)) {
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 0;
            }
            if (applied_stmt) {
                continue;
            }
            goto cleanup;
        }

        if (child->kind != AST_STMT_DECLARATION ||
            !child->declaration_names ||
            !child->declaration_value_kinds ||
            child->declaration_name_count != 1u ||
            !child->declaration_names[0] ||
            child->declaration_names[0][0] == '\0' ||
            !child->expressions ||
            child->expression_count != 1u ||
            !child->expressions[0] ||
            binding_count >= binding_capacity) {
            goto cleanup;
        }

        dest_binding = &bindings[binding_count];
        dest_binding->callable_view.dynamic_tag_local_id = (size_t)-1;
        dest_binding->name = child->declaration_names[0];

        if (child->declaration_value_kinds[0] == AST_DECLARATION_VALUE_FUNCTION) {
            const AstExpression *init_base =
                ir_unwrap_function_value_wrapper_expression(child->expressions[0]);
            const IrFunctionBodyEvalBinding *source_binding = NULL;

            if (!init_base ||
                init_base->kind != AST_EXPR_IDENTIFIER ||
                !init_base->as.identifier.name) {
                goto cleanup;
            }
            source_binding = ir_find_function_body_eval_binding(
                bindings,
                binding_count,
                init_base->as.identifier.name);
            if (!source_binding || !source_binding->has_callable_view ||
                !ir_shallow_copy_callable_view_for_eval_binding(
                    &source_binding->callable_view,
                    &dest_binding->callable_view)) {
                goto cleanup;
            }
            dest_binding->has_callable_view = 1;
            binding_count += 1u;
            continue;
        }

        if (child->declaration_value_kinds[0] != AST_DECLARATION_VALUE_INT ||
            !ir_try_eval_simple_function_body_expression(
                ctx,
                child->expressions[0],
                bindings,
                binding_count,
                &dest_binding->scalar_value,
                &dest_binding->has_scalar_value)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        if (!dest_binding->has_scalar_value) {
            goto cleanup;
        }
        binding_count += 1u;
    }

    *out_bindings = bindings;
    *out_binding_count = binding_count;
    return 1;

cleanup:
    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
    free(bindings);
    return 1;
}

static int ir_try_eval_narrow_static_helper_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied);

static int ir_try_emit_narrow_static_noncapturing_helper_eval_from_bindings(
    IrLowerContext *ctx,
    const AstExternal *callee_external,
    const AstExpression *const *actual_args,
    size_t actual_arg_count,
    const IrFunctionBodyEvalBinding *caller_bindings,
    size_t caller_binding_count,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied) {
    const AstStatement *body = NULL;
    const AstStatement *return_stmt = NULL;
    IrFunctionBodyEvalBinding *bindings = NULL;
    size_t binding_capacity = 0u;
    size_t binding_count = 0u;
    size_t param_index;
    size_t child_index;
    IrValueRef result = ir_value_immediate(0);
    int applied_eval = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !callee_external || !caller_bindings ||
        !out_value || !out_applied ||
        !callee_external->function_body ||
        callee_external->function_body->kind != AST_STMT_COMPOUND ||
        (callee_external->parameter_count > 0u &&
            (!callee_external->parameter_names ||
                !callee_external->parameter_value_kinds)) ||
        (actual_arg_count > 0u && !actual_args) ||
        actual_arg_count != callee_external->parameter_count ||
        recursion_depth >= 3u) {
        return 1;
    }

    body = callee_external->function_body;
    if (!body->children || body->child_count == 0u) {
        return 1;
    }
    return_stmt = body->children[body->child_count - 1u];
    if (!return_stmt ||
        return_stmt->kind != AST_STMT_RETURN ||
        return_stmt->expression_count != 1u ||
        !return_stmt->expressions ||
        !return_stmt->expressions[0]) {
        return 1;
    }

    binding_capacity = callee_external->parameter_count + body->child_count;
    binding_count = callee_external->parameter_count;
    bindings = (IrFunctionBodyEvalBinding *)calloc(binding_capacity, sizeof(IrFunctionBodyEvalBinding));
    if (!bindings) {
        ir_set_error(ctx->error,
            body->line,
            body->column,
            "IR-INT-126: out of memory while preparing narrow helper-body bindings");
        return 0;
    }

    for (param_index = 0u; param_index < binding_count; ++param_index) {
        const AstExpression *arg_expr = actual_args[param_index];
        const AstExpression *arg_base = NULL;
        const IrFunctionBodyEvalBinding *source_binding = NULL;

        bindings[param_index].callable_view.dynamic_tag_local_id = (size_t)-1;
        bindings[param_index].name = callee_external->parameter_names[param_index];
        if (!bindings[param_index].name || bindings[param_index].name[0] == '\0' || !arg_expr) {
            goto cleanup;
        }

        if (callee_external->parameter_value_kinds[param_index] == AST_PARAMETER_VALUE_FUNCTION) {
            arg_base = ir_unwrap_function_value_wrapper_expression(arg_expr);
            if (!arg_base ||
                arg_base->kind != AST_EXPR_IDENTIFIER ||
                !arg_base->as.identifier.name) {
                goto cleanup;
            }
            source_binding = ir_find_function_body_eval_binding(
                caller_bindings,
                caller_binding_count,
                arg_base->as.identifier.name);
            if (!source_binding ||
                !source_binding->has_callable_view ||
                source_binding->callable_view.is_closure_family ||
                source_binding->callable_view.has_dynamic_family ||
                !source_binding->callable_view.direct_target_name ||
                source_binding->callable_view.direct_target_name[0] == '\0' ||
                !ir_shallow_copy_callable_view_for_eval_binding(
                    &source_binding->callable_view,
                    &bindings[param_index].callable_view)) {
                goto cleanup;
            }
            bindings[param_index].has_callable_view = 1;
            continue;
        }

        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                arg_expr,
                caller_bindings,
                caller_binding_count,
                recursion_depth,
                &bindings[param_index].scalar_value,
                &bindings[param_index].has_scalar_value)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        if (!bindings[param_index].has_scalar_value) {
            goto cleanup;
        }
    }

    if (body->child_count == 2u &&
        body->children &&
        body->children[0] &&
        body->children[0]->kind == AST_STMT_IF &&
        body->children[1] == return_stmt &&
        body->children[0]->child_count >= 1u &&
        body->children[0]->children &&
        body->children[0]->children[0] &&
        body->children[0]->children[0]->kind == AST_STMT_RETURN &&
        body->children[0]->children[0]->expression_count == 1u &&
        body->children[0]->children[0]->expressions &&
        body->children[0]->children[0]->expressions[0] &&
        body->children[0]->child_count == 1u) {
        const AstExpression *cond_expr =
            statement_get_condition_expression(body->children[0]);
        const AstExpression *then_expr =
            body->children[0]->children[0]->expressions[0];
        const AstExpression *else_expr = return_stmt->expressions[0];
        IrValueRef condition_value = ir_value_immediate(0);
        int condition_applied = 0;
        IrValueRef compare_value;
        IrValueRef branch_result;
        IrValueRef then_value = ir_value_immediate(0);
        IrValueRef else_value = ir_value_immediate(0);
        int then_applied = 0;
        int else_applied = 0;
        size_t then_block_id;
        size_t else_block_id;
        size_t join_block_id;

        if (ir_narrow_helper_body_expression_is_evaluable(
                cond_expr,
                bindings,
                binding_count,
                recursion_depth) &&
            ir_narrow_helper_body_expression_is_evaluable(
                then_expr,
                bindings,
                binding_count,
                recursion_depth) &&
            ir_narrow_helper_body_expression_is_evaluable(
                else_expr,
                bindings,
                binding_count,
                recursion_depth)) {
            if (!ir_try_eval_narrow_static_helper_expression(
                    ctx,
                    cond_expr,
                    bindings,
                    binding_count,
                    recursion_depth,
                    &condition_value,
                    &condition_applied)) {
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 0;
            }
            if (condition_applied) {
                compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
                branch_result = ir_value_temp(ir_allocate_temp(ctx->function));
                if (!ir_emit_binary(
                        ctx,
                        compare_value,
                        IR_BINARY_NE,
                        condition_value,
                        ir_value_immediate(0)) ||
                    !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
                    !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
                    !ir_block_set_branch(
                        ir_lower_current_block(ctx),
                        compare_value,
                        then_block_id,
                        else_block_id,
                        ctx->error) ||
                    !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return 0;
                }

                ctx->block_id = then_block_id;
                ctx->has_block = 1;
                if (!ir_try_eval_narrow_static_helper_expression(
                        ctx,
                        then_expr,
                        bindings,
                        binding_count,
                        recursion_depth,
                        &then_value,
                        &then_applied) ||
                    !then_applied ||
                    !ir_emit_mov(ctx, branch_result, then_value) ||
                    !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return then_applied ? 0 : 1;
                }

                ctx->block_id = else_block_id;
                ctx->has_block = 1;
                if (!ir_try_eval_narrow_static_helper_expression(
                        ctx,
                        else_expr,
                        bindings,
                        binding_count,
                        recursion_depth,
                        &else_value,
                        &else_applied) ||
                    !else_applied ||
                    !ir_emit_mov(ctx, branch_result, else_value) ||
                    !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return else_applied ? 0 : 1;
                }

                ctx->block_id = join_block_id;
                ctx->has_block = 1;
                *out_value = branch_result;
                *out_applied = 1;
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 1;
            }
        }
    }

    for (child_index = 0u; child_index + 1u < body->child_count; ++child_index) {
        const AstStatement *child = body->children[child_index];
        IrFunctionBodyEvalBinding *dest_binding = NULL;
        int applied_stmt = 0;

        if (child && child->kind == AST_STMT_EXPRESSION) {
            if (!ir_try_apply_simple_function_body_statement(
                    ctx,
                    child,
                    bindings,
                    binding_count,
                    &applied_stmt)) {
                ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                free(bindings);
                return 0;
            }
            if (applied_stmt) {
                continue;
            }
            if (child->expression_count == 1u &&
                child->expressions &&
                child->expressions[0]) {
                IrValueRef ignored_value = ir_value_immediate(0);
                int ignored_applied = 0;

                if (!ir_try_eval_narrow_static_helper_expression(
                        ctx,
                        child->expressions[0],
                        bindings,
                        binding_count,
                        recursion_depth,
                        &ignored_value,
                        &ignored_applied)) {
                    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
                    free(bindings);
                    return 0;
                }
                if (ignored_applied) {
                    continue;
                }
            }
            goto cleanup;
        }

        if (!child ||
            child->kind != AST_STMT_DECLARATION ||
            !child->declaration_names ||
            !child->declaration_value_kinds ||
            child->declaration_name_count != 1u ||
            !child->declaration_names[0] ||
            child->declaration_names[0][0] == '\0' ||
            !child->expressions ||
            child->expression_count != 1u ||
            !child->expressions[0] ||
            binding_count >= binding_capacity) {
            goto cleanup;
        }

        dest_binding = &bindings[binding_count];
        dest_binding->callable_view.dynamic_tag_local_id = (size_t)-1;
        dest_binding->name = child->declaration_names[0];

        if (child->declaration_value_kinds[0] == AST_DECLARATION_VALUE_FUNCTION) {
            const AstExpression *init_base =
                ir_unwrap_function_value_wrapper_expression(child->expressions[0]);
            const IrFunctionBodyEvalBinding *source_binding = NULL;

            if (!init_base ||
                init_base->kind != AST_EXPR_IDENTIFIER ||
                !init_base->as.identifier.name) {
                goto cleanup;
            }
            source_binding = ir_find_function_body_eval_binding(
                bindings,
                binding_count,
                init_base->as.identifier.name);
            if (!source_binding || !source_binding->has_callable_view ||
                !ir_shallow_copy_callable_view_for_eval_binding(
                    &source_binding->callable_view,
                    &dest_binding->callable_view)) {
                goto cleanup;
            }
            dest_binding->has_callable_view = 1;
            binding_count += 1u;
            continue;
        }

        if (child->declaration_value_kinds[0] != AST_DECLARATION_VALUE_INT ||
            !ir_try_eval_narrow_static_helper_expression(
                ctx,
                child->expressions[0],
                bindings,
                binding_count,
                recursion_depth,
                &dest_binding->scalar_value,
                &dest_binding->has_scalar_value)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        if (!dest_binding->has_scalar_value) {
            goto cleanup;
        }
        binding_count += 1u;
    }

    if (body->child_count == 2u &&
        body->children &&
        body->children[0] &&
        body->children[0]->kind == AST_STMT_IF &&
        body->children[1] == return_stmt &&
        body->children[0]->child_count >= 1u &&
        body->children[0]->children &&
        body->children[0]->children[0] &&
        body->children[0]->children[0]->kind == AST_STMT_RETURN &&
        body->children[0]->children[0]->expression_count == 1u &&
        body->children[0]->children[0]->expressions &&
        body->children[0]->children[0]->expressions[0] &&
        body->children[0]->child_count == 1u) {
        const AstExpression *cond_expr =
            statement_get_condition_expression(body->children[0]);
        const AstExpression *then_expr =
            body->children[0]->children[0]->expressions[0];
        const AstExpression *else_expr = return_stmt->expressions[0];
        IrValueRef condition_value = ir_value_immediate(0);
        int condition_applied = 0;
        IrValueRef compare_value;
        IrValueRef branch_result;
        IrValueRef then_value = ir_value_immediate(0);
        IrValueRef else_value = ir_value_immediate(0);
        int then_applied = 0;
        int else_applied = 0;
        size_t then_block_id;
        size_t else_block_id;
        size_t join_block_id;

        if (!ir_narrow_helper_body_expression_is_evaluable(
                cond_expr,
                bindings,
                binding_count,
                recursion_depth) ||
            !ir_narrow_helper_body_expression_is_evaluable(
                then_expr,
                bindings,
                binding_count,
                recursion_depth) ||
            !ir_narrow_helper_body_expression_is_evaluable(
                else_expr,
                bindings,
                binding_count,
                recursion_depth)) {
            goto cleanup;
        }
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                cond_expr,
                bindings,
                binding_count,
                recursion_depth,
                &condition_value,
                &condition_applied)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }
        if (!condition_applied) {
            goto cleanup;
        }

        compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
        branch_result = ir_value_temp(ir_allocate_temp(ctx->function));
        if (!ir_emit_binary(
                ctx,
                compare_value,
                IR_BINARY_NE,
                condition_value,
                ir_value_immediate(0)) ||
            !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
            !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
            !ir_block_set_branch(
                ir_lower_current_block(ctx),
                compare_value,
                then_block_id,
                else_block_id,
                ctx->error) ||
            !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return 0;
        }

        ctx->block_id = then_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                then_expr,
                bindings,
                binding_count,
                recursion_depth,
                &then_value,
                &then_applied) ||
            !then_applied ||
            !ir_emit_mov(ctx, branch_result, then_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return then_applied ? 0 : 1;
        }

        ctx->block_id = else_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                else_expr,
                bindings,
                binding_count,
                recursion_depth,
                &else_value,
                &else_applied) ||
            !else_applied ||
            !ir_emit_mov(ctx, branch_result, else_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
            free(bindings);
            return else_applied ? 0 : 1;
        }

        ctx->block_id = join_block_id;
        ctx->has_block = 1;
        *out_value = branch_result;
        *out_applied = 1;
        ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
        free(bindings);
        return 1;
    }

    if (!ir_try_eval_narrow_static_helper_expression(
            ctx,
            return_stmt->expressions[0],
            bindings,
            binding_count,
            recursion_depth,
            &result,
            &applied_eval)) {
        ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
        free(bindings);
        return 0;
    }
    if (applied_eval) {
        *out_value = result;
        *out_applied = 1;
    }

cleanup:
    ir_function_body_eval_binding_array_clear(bindings, binding_capacity);
    free(bindings);
    return 1;
}

static int ir_try_eval_narrow_static_helper_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    const IrFunctionBodyEvalBinding *bindings,
    size_t binding_count,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !base || !bindings || !out_value || !out_applied) {
        return 1;
    }

    if (base->kind == AST_EXPR_NUMBER) {
        *out_value = ir_value_immediate(base->as.number_value);
        *out_applied = 1;
        return 1;
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
        const IrFunctionBodyEvalBinding *binding =
            ir_find_function_body_eval_binding(bindings, binding_count, base->as.identifier.name);
        if (!binding || !binding->has_scalar_value) {
            return 1;
        }
        *out_value = binding->scalar_value;
        *out_applied = 1;
        return 1;
    }

    if (base->kind == AST_EXPR_UNARY) {
        return ir_try_eval_narrow_helper_body_unary_expression(
            ctx,
            expr,
            bindings,
            binding_count,
            recursion_depth,
            out_value,
            out_applied);
    }

    if (base->kind == AST_EXPR_TERNARY) {
        IrValueRef condition_value = ir_value_immediate(0);
        int condition_applied = 0;
        IrValueRef compare_value;
        IrValueRef result_value;
        IrValueRef then_value = ir_value_immediate(0);
        IrValueRef else_value = ir_value_immediate(0);
        int then_applied = 0;
        int else_applied = 0;
        size_t then_block_id;
        size_t else_block_id;
        size_t join_block_id;

        if (!ir_narrow_helper_body_expression_is_evaluable(
                base->as.ternary.condition,
                bindings,
                binding_count,
                recursion_depth) ||
            !ir_narrow_helper_body_expression_is_evaluable(
                base->as.ternary.then_expr,
                bindings,
                binding_count,
                recursion_depth) ||
            !ir_narrow_helper_body_expression_is_evaluable(
                base->as.ternary.else_expr,
                bindings,
                binding_count,
                recursion_depth)) {
            return 1;
        }
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                base->as.ternary.condition,
                bindings,
                binding_count,
                recursion_depth,
                &condition_value,
                &condition_applied)) {
            return 0;
        }
        if (!condition_applied) {
            return 1;
        }

        compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
        result_value = ir_value_temp(ir_allocate_temp(ctx->function));
        if (!ir_emit_binary(
                ctx,
                compare_value,
                IR_BINARY_NE,
                condition_value,
                ir_value_immediate(0)) ||
            !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
            !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
            !ir_block_set_branch(
                ir_lower_current_block(ctx),
                compare_value,
                then_block_id,
                else_block_id,
                ctx->error) ||
            !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
            return 0;
        }

        ctx->block_id = then_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                base->as.ternary.then_expr,
                bindings,
                binding_count,
                recursion_depth,
                &then_value,
                &then_applied) ||
            !then_applied ||
            !ir_emit_mov(ctx, result_value, then_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return then_applied;
        }

        ctx->block_id = else_block_id;
        ctx->has_block = 1;
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                base->as.ternary.else_expr,
                bindings,
                binding_count,
                recursion_depth,
                &else_value,
                &else_applied) ||
            !else_applied ||
            !ir_emit_mov(ctx, result_value, else_value) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
            return else_applied;
        }

        ctx->block_id = join_block_id;
        ctx->has_block = 1;
        *out_value = result_value;
        *out_applied = 1;
        return 1;
    }

    if (base->kind == AST_EXPR_BINARY) {
        if (base->as.binary.op == TOKEN_COMMA) {
            IrValueRef lhs = ir_value_immediate(0);
            IrValueRef rhs = ir_value_immediate(0);
            int lhs_applied = 0;
            int rhs_applied = 0;

            if (!ir_try_eval_narrow_static_helper_expression(
                    ctx,
                    base->as.binary.left,
                    bindings,
                    binding_count,
                    recursion_depth,
                    &lhs,
                    &lhs_applied) ||
                !lhs_applied ||
                !ir_try_eval_narrow_static_helper_expression(
                    ctx,
                    base->as.binary.right,
                    bindings,
                    binding_count,
                    recursion_depth,
                    &rhs,
                    &rhs_applied)) {
                return 0;
            }
            if (!rhs_applied) {
                return 1;
            }
            *out_value = rhs;
            *out_applied = 1;
            return 1;
        }
        IrValueRef lhs = ir_value_immediate(0);
        IrValueRef rhs = ir_value_immediate(0);
        int lhs_applied = 0;
        int rhs_applied = 0;
        int op_supported = 0;
        IrBinaryOp op = ir_binary_op_from_token(base->as.binary.op, &op_supported);

        if (!op_supported) {
            return 1;
        }
        if (!ir_try_eval_narrow_static_helper_expression(
                ctx,
                base->as.binary.left,
                bindings,
                binding_count,
                recursion_depth,
                &lhs,
                &lhs_applied) ||
            !lhs_applied ||
            !ir_try_eval_narrow_static_helper_expression(
                ctx,
                base->as.binary.right,
                bindings,
                binding_count,
                recursion_depth,
                &rhs,
                &rhs_applied) ||
            !rhs_applied) {
            return lhs_applied && rhs_applied;
        }

        {
            IrValueRef result = ir_value_temp(ir_allocate_temp(ctx->function));
            if (!ir_emit_binary(ctx, result, op, lhs, rhs)) {
                return 0;
            }
            *out_value = result;
            *out_applied = 1;
            return 1;
        }
    }

    if (base->kind == AST_EXPR_CALL) {
        const AstExpression *callee_base =
            ir_unwrap_function_value_wrapper_expression(base->as.call.callee);
        const IrFunctionBodyEvalBinding *callee_binding = NULL;
        const AstExternal *target_external = NULL;
        IrValueRef *call_args = NULL;
        const IrValueRef *call_args_view = NULL;
        size_t arg_count = 0u;
        size_t arg_index;
        IrValueRef result = ir_value_immediate(0);
        int applied_dispatch = 0;

        if (!callee_base ||
            callee_base->kind != AST_EXPR_IDENTIFIER ||
            !callee_base->as.identifier.name) {
            return 1;
        }
        callee_binding = ir_find_function_body_eval_binding(
            bindings,
            binding_count,
            callee_base->as.identifier.name);
        if (!callee_binding || !callee_binding->has_callable_view) {
            return 1;
        }

        if (!callee_binding->callable_view.is_closure_family &&
            !callee_binding->callable_view.has_dynamic_family &&
            callee_binding->callable_view.direct_target_name &&
            callee_binding->callable_view.direct_target_name[0] != '\0' &&
            ctx->ast_program &&
            recursion_depth + 1u < 3u) {
            target_external = ir_find_named_function_external_prefer_definition(
                ctx->ast_program,
                callee_binding->callable_view.direct_target_name);
            if (target_external) {
                if (!ir_try_emit_narrow_static_noncapturing_helper_eval_from_bindings(
                        ctx,
                        target_external,
                        (const AstExpression *const *)base->as.call.args,
                        base->as.call.arg_count,
                        bindings,
                        binding_count,
                        recursion_depth + 1u,
                        out_value,
                        out_applied)) {
                    return 0;
                }
                if (*out_applied) {
                    return 1;
                }
            }
        }

        if (base->as.call.arg_count > 0u) {
            call_args = (IrValueRef *)malloc(base->as.call.arg_count * sizeof(IrValueRef));
            if (!call_args) {
                ir_set_error(ctx->error,
                    base->line,
                    base->column,
                    "IR-INT-126: out of memory while evaluating narrow helper call arguments");
                return 0;
            }
        }
        for (arg_index = 0u; arg_index < base->as.call.arg_count; ++arg_index) {
            int arg_applied = 0;

            if (!ir_try_eval_narrow_static_helper_expression(
                    ctx,
                    base->as.call.args[arg_index],
                    bindings,
                    binding_count,
                    recursion_depth,
                    &call_args[arg_count],
                    &arg_applied)) {
                free(call_args);
                return 0;
            }
            if (!arg_applied) {
                free(call_args);
                return 1;
            }
            arg_count += 1u;
        }
        call_args_view = arg_count > 0u ? call_args : NULL;
        if (!ir_try_emit_static_callable_view_dispatch_with_args(
                ctx,
                &callee_binding->callable_view,
                call_args_view,
                arg_count,
                &result,
                &applied_dispatch)) {
            free(call_args);
            return 0;
        }
        free(call_args);
        if (!applied_dispatch) {
            return 1;
        }
        *out_value = result;
        *out_applied = 1;
        return 1;
    }

    return 1;
}

static int ir_try_emit_two_target_dynamic_closure_body_eval_dispatch(
    IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external_a,
    const AstExternal *callee_external_b,
    const IrLowerCallableObjectView *dynamic_callable_view,
    long long dynamic_then_tag_value,
    IrValueRef *out_value,
    int *out_applied) {
    IrValueRef compare_value;
    IrValueRef result_value;
    IrValueRef then_value = ir_value_immediate(0);
    IrValueRef else_value = ir_value_immediate(0);
    size_t result_temp_id;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;
    int then_applied = 0;
    int else_applied = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !call_expr || !callee_external_a || !callee_external_b ||
        !dynamic_callable_view || !out_value || !out_applied ||
        !dynamic_callable_view->is_closure_family ||
        dynamic_callable_view->dynamic_tag_local_id == (size_t)-1 ||
        !dynamic_callable_view->capture_local_ids ||
        dynamic_callable_view->capture_count == 0u) {
        return 1;
    }

    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_binary(
            ctx,
            compare_value,
            IR_BINARY_EQ,
            ir_value_local(dynamic_callable_view->dynamic_tag_local_id),
            ir_value_immediate(dynamic_then_tag_value)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(
            ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        return 0;
    }

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_try_emit_simple_function_body_eval_dispatch(
            ctx,
            callee_external_a,
            1,
            dynamic_callable_view->capture_local_ids,
            dynamic_callable_view->capture_count,
            call_expr,
            &then_value,
            &then_applied)) {
        return 0;
    }
    if (!then_applied) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-148: dynamic closure body-eval dispatch emitted branch scaffolding but target '%s' did not apply",
            callee_external_a->name ? callee_external_a->name : "<unnamed>");
        return 0;
    }
    if (!ir_emit_mov(ctx, result_value, then_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_try_emit_simple_function_body_eval_dispatch(
            ctx,
            callee_external_b,
            1,
            dynamic_callable_view->capture_local_ids,
            dynamic_callable_view->capture_count,
            call_expr,
            &else_value,
            &else_applied)) {
        return 0;
    }
    if (!else_applied) {
        ir_set_error(ctx->error,
            call_expr->line,
            call_expr->column,
            "IR-INT-149: dynamic closure body-eval dispatch emitted branch scaffolding but target '%s' did not apply",
            callee_external_b->name ? callee_external_b->name : "<unnamed>");
        return 0;
    }
    if (!ir_emit_mov(ctx, result_value, else_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    *out_value = result_value;
    *out_applied = 1;
    return 1;
}

static void ir_function_parameter_recursive_target_plan_free(
    IrFunctionParameterRecursiveTargetPlan *plan) {
    if (!plan) {
        return;
    }
    ir_free_callable_object_view_storage(&plan->function_callable_view);
    free(plan->mapped_args);
    plan->mapped_args = NULL;
    plan->target_external = NULL;
    plan->function_arg_index = (size_t)-1;
}

static int ir_try_build_recursive_passthrough_target_plan(IrLowerContext *ctx,
    const AstExpression *outer_call_expr,
    const AstExternal *outer_external,
    const AstExpression *inner_call_base,
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_count,
    const char *target_name,
    IrFunctionParameterRecursiveTargetPlan *out_plan) {
    const AstExternal *target_external = NULL;
    AstExpression **mapped_args = NULL;
    size_t function_arg_index = (size_t)-1;
    IrLowerCallableObjectView function_callable_view;
    size_t nested_arg_index;

    if (out_plan) {
        memset(out_plan, 0, sizeof(*out_plan));
        out_plan->function_arg_index = (size_t)-1;
        out_plan->function_callable_view.dynamic_tag_local_id = (size_t)-1;
    }
    memset(&function_callable_view, 0, sizeof(function_callable_view));
    function_callable_view.dynamic_tag_local_id = (size_t)-1;
    if (!ctx || !ctx->ast_program || !outer_call_expr || !outer_external ||
        !inner_call_base || inner_call_base->kind != AST_EXPR_CALL ||
        !target_name || target_name[0] == '\0' || !out_plan) {
        return 0;
    }

    target_external =
        ir_find_named_function_external_prefer_definition(ctx->ast_program, target_name);
    if (!target_external || !target_external->parameter_names ||
        !target_external->parameter_value_kinds ||
        target_external->parameter_count != inner_call_base->as.call.arg_count) {
        return 0;
    }

    mapped_args = (AstExpression **)malloc(
        target_external->parameter_count * sizeof(AstExpression *));
    if (!mapped_args) {
        ir_set_error(ctx->error,
            outer_call_expr->line,
            outer_call_expr->column,
            "IR-INT-126: out of memory while building recursive passthrough target plan");
        return 0;
    }

    for (nested_arg_index = 0u;
         nested_arg_index < target_external->parameter_count;
         ++nested_arg_index) {
        const AstExpression *inner_arg =
            ir_unwrap_function_value_wrapper_expression(
                inner_call_base->as.call.args[nested_arg_index]);
        const char *inner_arg_name = NULL;
        const char *resolved_scalar_name = NULL;
        size_t outer_index = 0u;
        int found_outer = 0;

        if (!inner_arg ||
            inner_arg->kind != AST_EXPR_IDENTIFIER ||
            !inner_arg->as.identifier.name) {
            free(mapped_args);
            return 0;
        }
        inner_arg_name = inner_arg->as.identifier.name;
        for (outer_index = 0u; outer_index < outer_external->parameter_count; ++outer_index) {
            if (outer_external->parameter_names[outer_index] &&
                strcmp(outer_external->parameter_names[outer_index], inner_arg_name) == 0) {
                found_outer = 1;
                break;
            }
        }
        if (!found_outer &&
            ir_try_resolve_function_parameter_passthrough_alias_name(
                ctx,
                outer_external,
                aliases,
                alias_count,
                inner_arg,
                &inner_arg_name) &&
            inner_arg_name &&
            inner_arg_name[0] != '\0') {
            for (outer_index = 0u; outer_index < outer_external->parameter_count; ++outer_index) {
                if (outer_external->parameter_names[outer_index] &&
                    strcmp(outer_external->parameter_names[outer_index], inner_arg_name) == 0) {
                    found_outer = 1;
                    break;
                }
            }
        }
        if (!found_outer &&
            target_external->parameter_value_kinds[nested_arg_index] != AST_PARAMETER_VALUE_FUNCTION &&
            ir_try_resolve_scalar_parameter_passthrough_alias_name(
                outer_external,
                scalar_aliases,
                scalar_alias_count,
                inner_arg,
                &resolved_scalar_name) &&
            resolved_scalar_name &&
            resolved_scalar_name[0] != '\0') {
            for (outer_index = 0u; outer_index < outer_external->parameter_count; ++outer_index) {
                if (outer_external->parameter_names[outer_index] &&
                    strcmp(outer_external->parameter_names[outer_index], resolved_scalar_name) == 0) {
                    found_outer = 1;
                    break;
                }
            }
        }
        if (!found_outer || outer_index >= outer_call_expr->as.call.arg_count) {
            free(mapped_args);
            return 0;
        }

        mapped_args[nested_arg_index] = outer_call_expr->as.call.args[outer_index];
        if (target_external->parameter_value_kinds[nested_arg_index] == AST_PARAMETER_VALUE_FUNCTION) {
            IrLowerCallableObjectView candidate_view;

            memset(&candidate_view, 0, sizeof(candidate_view));
            candidate_view.dynamic_tag_local_id = (size_t)-1;
            if (!ir_try_resolve_static_actual_argument_callable_object_view(
                    ctx,
                    mapped_args[nested_arg_index],
                    &candidate_view)) {
                ir_free_callable_object_view_storage(&candidate_view);
                continue;
            }
            if (function_arg_index != (size_t)-1) {
                ir_free_callable_object_view_storage(&candidate_view);
                free(mapped_args);
                return 0;
            }
            function_arg_index = nested_arg_index;
            function_callable_view = candidate_view;
        }
    }

    if (function_arg_index == (size_t)-1 ||
        !target_external->function_body ||
        target_external->function_body->kind != AST_STMT_COMPOUND) {
        ir_free_callable_object_view_storage(&function_callable_view);
        free(mapped_args);
        return 0;
    }

    out_plan->target_external = target_external;
    out_plan->mapped_args = mapped_args;
    out_plan->function_arg_index = function_arg_index;
    out_plan->function_callable_view = function_callable_view;
    return 1;
}

static int ir_try_emit_recursive_passthrough_dynamic_function_dispatch(
    IrLowerContext *ctx,
    const AstExpression *outer_call_expr,
    const AstExternal *outer_external,
    const AstExpression *inner_call_base,
    const IrFunctionParameterPassthroughAlias *aliases,
    size_t alias_count,
    const IrScalarParameterPassthroughAlias *scalar_aliases,
    size_t scalar_alias_count,
    const IrLowerCallableObjectView *callable_view,
    size_t recursion_depth,
    int returns_zero_after_void_call,
    IrValueRef *out_value,
    int *out_applied) {
    IrFunctionParameterRecursiveTargetPlan then_plan;
    IrFunctionParameterRecursiveTargetPlan else_plan;
    IrValueRef compare_value;
    IrValueRef result_value;
    size_t result_temp_id;
    size_t then_block_id;
    size_t else_block_id;
    size_t join_block_id;
    AstExpression synthetic_then_callee_expr;
    AstExpression synthetic_else_callee_expr;
    AstExpression synthetic_then_call_expr;
    AstExpression synthetic_else_call_expr;
    IrValueRef then_value = ir_value_immediate(0);
    IrValueRef else_value = ir_value_immediate(0);
    int then_applied = 0;
    int else_applied = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    memset(&then_plan, 0, sizeof(then_plan));
    memset(&else_plan, 0, sizeof(else_plan));
    then_plan.function_arg_index = (size_t)-1;
    else_plan.function_arg_index = (size_t)-1;
    then_plan.function_callable_view.dynamic_tag_local_id = (size_t)-1;
    else_plan.function_callable_view.dynamic_tag_local_id = (size_t)-1;
    if (!ctx || !outer_call_expr || !outer_external || !inner_call_base ||
        !callable_view || !out_value || !out_applied ||
        recursion_depth >= 2u ||
        callable_view->is_closure_family ||
        !ir_callable_object_view_is_two_target_dynamic_family(callable_view) ||
        callable_view->dynamic_tag_local_id == (size_t)-1 ||
        !callable_view->dynamic_target_names ||
        !callable_view->dynamic_target_names[0] ||
        !callable_view->dynamic_target_names[1]) {
        return 1;
    }

    if (!ir_try_build_recursive_passthrough_target_plan(
            ctx,
            outer_call_expr,
            outer_external,
            inner_call_base,
            aliases,
            alias_count,
            scalar_aliases,
            scalar_alias_count,
            callable_view->dynamic_target_names[0],
            &then_plan) ||
        !ir_try_build_recursive_passthrough_target_plan(
            ctx,
            outer_call_expr,
            outer_external,
            inner_call_base,
            aliases,
            alias_count,
            scalar_aliases,
            scalar_alias_count,
            callable_view->dynamic_target_names[1],
            &else_plan)) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        return 1;
    }

    compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
    result_temp_id = ir_allocate_temp(ctx->function);
    result_value = ir_value_temp(result_temp_id);
    if (!ir_emit_binary(
            ctx,
            compare_value,
            IR_BINARY_EQ,
            ir_value_local(callable_view->dynamic_tag_local_id),
            ir_value_immediate(1)) ||
        !ir_function_append_block(ctx->function, &then_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &else_block_id, NULL, ctx->error) ||
        !ir_block_set_branch(
            ir_lower_current_block(ctx),
            compare_value,
            then_block_id,
            else_block_id,
            ctx->error) ||
        !ir_function_append_block(ctx->function, &join_block_id, NULL, ctx->error)) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        return 0;
    }

    memset(&synthetic_then_callee_expr, 0, sizeof(synthetic_then_callee_expr));
    synthetic_then_callee_expr.kind = AST_EXPR_IDENTIFIER;
    synthetic_then_callee_expr.line = outer_call_expr->line;
    synthetic_then_callee_expr.column = outer_call_expr->column;
    synthetic_then_callee_expr.as.identifier.name =
        (char *)callable_view->dynamic_target_names[0];
    synthetic_then_callee_expr.as.identifier.name_length =
        strlen(callable_view->dynamic_target_names[0]);
    memset(&synthetic_then_call_expr, 0, sizeof(synthetic_then_call_expr));
    synthetic_then_call_expr.kind = AST_EXPR_CALL;
    synthetic_then_call_expr.line = outer_call_expr->line;
    synthetic_then_call_expr.column = outer_call_expr->column;
    synthetic_then_call_expr.as.call.callee = &synthetic_then_callee_expr;
    synthetic_then_call_expr.as.call.args = then_plan.mapped_args;
    synthetic_then_call_expr.as.call.arg_count = then_plan.target_external->parameter_count;

    ctx->block_id = then_block_id;
    ctx->has_block = 1;
    if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
            ctx,
            &synthetic_then_call_expr,
            then_plan.target_external,
            then_plan.function_arg_index,
            &then_plan.function_callable_view,
            recursion_depth + 1u,
            &then_value,
            &then_applied)) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        return 0;
    }
    if (!then_applied) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        ir_set_error(ctx->error,
            outer_call_expr->line,
            outer_call_expr->column,
            "IR-INT-146: recursive passthrough dispatch emitted branch scaffolding but target '%s' did not apply",
            callable_view->dynamic_target_names[0]);
        return 0;
    }
    if (!ir_emit_mov(ctx, result_value, then_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        return 0;
    }

    memset(&synthetic_else_callee_expr, 0, sizeof(synthetic_else_callee_expr));
    synthetic_else_callee_expr.kind = AST_EXPR_IDENTIFIER;
    synthetic_else_callee_expr.line = outer_call_expr->line;
    synthetic_else_callee_expr.column = outer_call_expr->column;
    synthetic_else_callee_expr.as.identifier.name =
        (char *)callable_view->dynamic_target_names[1];
    synthetic_else_callee_expr.as.identifier.name_length =
        strlen(callable_view->dynamic_target_names[1]);
    memset(&synthetic_else_call_expr, 0, sizeof(synthetic_else_call_expr));
    synthetic_else_call_expr.kind = AST_EXPR_CALL;
    synthetic_else_call_expr.line = outer_call_expr->line;
    synthetic_else_call_expr.column = outer_call_expr->column;
    synthetic_else_call_expr.as.call.callee = &synthetic_else_callee_expr;
    synthetic_else_call_expr.as.call.args = else_plan.mapped_args;
    synthetic_else_call_expr.as.call.arg_count = else_plan.target_external->parameter_count;

    ctx->block_id = else_block_id;
    ctx->has_block = 1;
    if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
            ctx,
            &synthetic_else_call_expr,
            else_plan.target_external,
            else_plan.function_arg_index,
            &else_plan.function_callable_view,
            recursion_depth + 1u,
            &else_value,
            &else_applied)) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        return 0;
    }
    if (!else_applied) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        ir_set_error(ctx->error,
            outer_call_expr->line,
            outer_call_expr->column,
            "IR-INT-147: recursive passthrough dispatch emitted branch scaffolding but target '%s' did not apply",
            callable_view->dynamic_target_names[1]);
        return 0;
    }
    if (!ir_emit_mov(ctx, result_value, else_value) ||
        !ir_block_set_jump(ir_lower_current_block(ctx), join_block_id, ctx->error)) {
        ir_function_parameter_recursive_target_plan_free(&then_plan);
        ir_function_parameter_recursive_target_plan_free(&else_plan);
        return 0;
    }

    ctx->block_id = join_block_id;
    ctx->has_block = 1;
    *out_value = returns_zero_after_void_call ? ir_value_immediate(0) : result_value;
    *out_applied = 1;
    ir_function_parameter_recursive_target_plan_free(&then_plan);
    ir_function_parameter_recursive_target_plan_free(&else_plan);
    return 1;
}

static int ir_try_emit_static_callable_view_dispatch_with_args(IrLowerContext *ctx,
    const IrLowerCallableObjectView *callable_view,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value,
    int *out_applied) {
    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !callable_view || !out_value || !callable_view->direct_target_name ||
        callable_view->direct_target_name[0] == '\0' ||
        (arg_count > 0u && !args)) {
        return 1;
    }

    if (callable_view->dynamic_target_count > 1u) {
        return ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
            ctx,
            callable_view,
            args,
            arg_count,
            out_value,
            out_applied);
    }

    if (callable_view->is_closure_family) {
        return ir_try_emit_static_closure_callable_indirect_call_with_args(
            ctx,
            callable_view->direct_target_name,
            callable_view->capture_local_ids,
            callable_view->capture_count,
            args,
            arg_count,
            out_value,
            out_applied);
    }

    return ir_try_emit_static_noncapturing_target_callable_indirect_call_with_args(
        ctx,
        callable_view->direct_target_name,
        args,
        arg_count,
        out_value,
        out_applied);
}

static int ir_try_emit_function_parameter_passthrough_callable_dispatch(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    size_t function_arg_index,
    const IrLowerCallableObjectView *callable_view,
    size_t recursion_depth,
    IrValueRef *out_value,
    int *out_applied) {
    const AstStatement *body = NULL;
    const AstStatement *passthrough_stmt = NULL;
    const AstExpression *callee_expr = NULL;
    const AstExpression *callee_base = NULL;
    IrFunctionParameterPassthroughAlias aliases[8];
    IrScalarParameterPassthroughAlias scalar_aliases[8];
    size_t alias_count = 0u;
    size_t scalar_alias_count = 0u;
    int returns_zero_after_void_call = 0;
    const char *called_name = NULL;
    IrValueRef *call_args = NULL;
    const IrValueRef *call_args_view = NULL;
    IrValueRef nested_call_args[8];
    const IrValueRef *nested_call_args_view = NULL;
    size_t callable_arg_count = 0u;

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !call_expr || !callee_external || !callable_view || !out_value) {
        return 1;
    }
    if (!callee_external->function_body ||
        callee_external->function_body->kind != AST_STMT_COMPOUND ||
        !callee_external->function_body->children ||
        callee_external->function_body->child_count == 0u ||
        !callee_external->parameter_names ||
        !callee_external->parameter_value_kinds ||
        function_arg_index >= callee_external->parameter_count ||
        callee_external->parameter_value_kinds[function_arg_index] != AST_PARAMETER_VALUE_FUNCTION) {
        return 1;
    }

    body = callee_external->function_body;
    if (!ir_try_collect_function_parameter_passthrough_aliases(
            ctx,
            callee_external,
            body,
            aliases,
            sizeof(aliases) / sizeof(aliases[0]),
            scalar_aliases,
            sizeof(scalar_aliases) / sizeof(scalar_aliases[0]),
            &alias_count,
            &scalar_alias_count,
            &passthrough_stmt,
            &returns_zero_after_void_call)) {
        return 1;
    }
    if (passthrough_stmt->kind == AST_STMT_RETURN &&
        passthrough_stmt->expression_count == 1u &&
        !returns_zero_after_void_call) {
        callee_expr = passthrough_stmt->expressions ? passthrough_stmt->expressions[0] : NULL;
    } else if ((callee_external->function_return_type == AST_FUNCTION_RETURN_VOID ||
                   returns_zero_after_void_call) &&
               passthrough_stmt->kind == AST_STMT_EXPRESSION &&
               passthrough_stmt->expression_count == 1u) {
        callee_expr = passthrough_stmt->expressions ? passthrough_stmt->expressions[0] : NULL;
    } else {
        return 1;
    }
    callee_base = ir_unwrap_function_value_wrapper_expression(callee_expr);
    if (!callee_base || callee_base->kind != AST_EXPR_CALL) {
        return 1;
    }
    called_name = ir_extract_direct_call_callee_name(callee_base->as.call.callee);
    if ((!called_name || called_name[0] == '\0') &&
        !ir_classify_direct_identifier_expr(callee_base->as.call.callee, &called_name)) {
        return 1;
    }
    if (called_name && called_name[0] != '\0' &&
        !ir_find_external_parameter_index_by_name(
            callee_external,
            called_name,
            1,
            NULL)) {
        const char *resolved_alias_name = NULL;

        if (ir_try_resolve_function_parameter_passthrough_alias_name(
                ctx,
                callee_external,
                aliases,
                alias_count,
                callee_base->as.call.callee,
                &resolved_alias_name) &&
            resolved_alias_name &&
            resolved_alias_name[0] != '\0') {
            called_name = resolved_alias_name;
        }
    }
    if (!called_name || called_name[0] == '\0') {
        return 1;
    }
    if (strcmp(called_name, callee_external->parameter_names[function_arg_index]) != 0) {
        const AstExternal *nested_external = NULL;
        AstExpression synthetic_call_expr;
        AstExpression **mapped_args = NULL;
        size_t nested_function_arg_index = (size_t)-1;
        int recurse_result = 1;
        int recurse_applied = 0;

        if (recursion_depth >= 2u || !ctx->ast_program ||
            !callee_base->as.call.args ||
            callee_base->as.call.arg_count == 0u) {
            return 1;
        }
        nested_external =
            ir_find_named_function_external_prefer_definition(ctx->ast_program, called_name);
        if (!nested_external ||
            !nested_external->parameter_names ||
            !nested_external->parameter_value_kinds ||
            nested_external->parameter_count != callee_base->as.call.arg_count) {
            return 1;
        }
        mapped_args = (AstExpression **)malloc(nested_external->parameter_count * sizeof(AstExpression *));
        if (!mapped_args) {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-126: out of memory while remapping passthrough wrapper arguments");
            return 0;
        }
        for (size_t nested_arg_index = 0u; nested_arg_index < nested_external->parameter_count; ++nested_arg_index) {
            const AstExpression *inner_arg =
                ir_unwrap_function_value_wrapper_expression(callee_base->as.call.args[nested_arg_index]);
            const char *inner_arg_name = NULL;
            const char *resolved_scalar_name = NULL;
            size_t outer_index = 0u;
            int found_outer = 0;

            if (!inner_arg || inner_arg->kind != AST_EXPR_IDENTIFIER || !inner_arg->as.identifier.name) {
                free(mapped_args);
                return 1;
            }
            inner_arg_name = inner_arg->as.identifier.name;
            for (outer_index = 0u; outer_index < callee_external->parameter_count; ++outer_index) {
                if (callee_external->parameter_names[outer_index] &&
                    strcmp(callee_external->parameter_names[outer_index], inner_arg_name) == 0) {
                    found_outer = 1;
                    break;
                }
            }
            if (!found_outer &&
                nested_external->parameter_value_kinds[nested_arg_index] == AST_PARAMETER_VALUE_FUNCTION &&
                ir_try_resolve_function_parameter_passthrough_alias_name(
                    ctx,
                    callee_external,
                    aliases,
                    alias_count,
                    inner_arg,
                    &inner_arg_name) &&
                inner_arg_name &&
                inner_arg_name[0] != '\0') {
                for (outer_index = 0u; outer_index < callee_external->parameter_count; ++outer_index) {
                    if (callee_external->parameter_names[outer_index] &&
                        strcmp(callee_external->parameter_names[outer_index], inner_arg_name) == 0) {
                        found_outer = 1;
                        break;
                    }
                }
            }
            if (!found_outer &&
                nested_external->parameter_value_kinds[nested_arg_index] != AST_PARAMETER_VALUE_FUNCTION &&
                ir_try_resolve_scalar_parameter_passthrough_alias_name(
                    callee_external,
                    scalar_aliases,
                    scalar_alias_count,
                    inner_arg,
                    &resolved_scalar_name) &&
                resolved_scalar_name &&
                resolved_scalar_name[0] != '\0') {
                for (outer_index = 0u; outer_index < callee_external->parameter_count; ++outer_index) {
                    if (callee_external->parameter_names[outer_index] &&
                        strcmp(callee_external->parameter_names[outer_index], resolved_scalar_name) == 0) {
                        found_outer = 1;
                        break;
                    }
                }
            }
            if (!found_outer || outer_index >= call_expr->as.call.arg_count) {
                free(mapped_args);
                return 1;
            }
            mapped_args[nested_arg_index] = call_expr->as.call.args[outer_index];
            if (outer_index == function_arg_index) {
                if (nested_external->parameter_value_kinds[nested_arg_index] != AST_PARAMETER_VALUE_FUNCTION ||
                    nested_function_arg_index != (size_t)-1) {
                    free(mapped_args);
                    return 1;
                }
                nested_function_arg_index = nested_arg_index;
            }
        }
        if (nested_function_arg_index == (size_t)-1) {
            free(mapped_args);
            return 1;
        }
        memset(&synthetic_call_expr, 0, sizeof(synthetic_call_expr));
        synthetic_call_expr.kind = AST_EXPR_CALL;
        synthetic_call_expr.line = call_expr->line;
        synthetic_call_expr.column = call_expr->column;
        synthetic_call_expr.as.call.callee = callee_base->as.call.callee;
        synthetic_call_expr.as.call.args = mapped_args;
        synthetic_call_expr.as.call.arg_count = nested_external->parameter_count;
        recurse_result = ir_try_emit_function_parameter_passthrough_callable_dispatch(
            ctx,
            &synthetic_call_expr,
            nested_external,
            nested_function_arg_index,
            callable_view,
            recursion_depth + 1u,
            out_value,
            &recurse_applied);
        free(mapped_args);
        if (!recurse_result) {
            return 0;
        }
        if (recurse_applied) {
            if (returns_zero_after_void_call) {
                *out_value = ir_value_immediate(0);
            }
            return ir_mark_callable_dispatch_applied(out_applied);
        }
        return 1;
    }
    if (recursion_depth < 2u &&
        ctx->ast_program &&
        callable_view->dynamic_target_count <= 1u &&
        callable_view->direct_target_name &&
        callable_view->direct_target_name[0] != '\0') {
        const AstExternal *nested_external = NULL;
        IrFunctionParameterPassthroughAlias nested_aliases[8];
        IrScalarParameterPassthroughAlias nested_scalar_aliases[8];
        size_t nested_alias_count = 0u;
        size_t nested_scalar_alias_count = 0u;
        const AstStatement *nested_passthrough_stmt = NULL;
        int nested_returns_zero_after_void_call = 0;
        const AstExpression *nested_callee_expr = NULL;
        const AstExpression *nested_callee_base = NULL;
        const char *nested_called_name = NULL;
        AstExpression synthetic_call_expr;
        AstExpression synthetic_callee_expr;
        AstExpression **mapped_args = NULL;
        IrLowerCallableObjectView nested_callable_view;
        IrFunctionBodyEvalBinding *wrapper_bindings = NULL;
        size_t wrapper_binding_count = 0u;
        size_t nested_function_arg_index = (size_t)-1;
        int recurse_result = 1;
        int recurse_applied = 0;
        int narrow_applied = 0;

        nested_external =
            ir_find_named_function_external_prefer_definition(
                ctx->ast_program,
                callable_view->direct_target_name);
        if (nested_external &&
            nested_external->function_body &&
            nested_external->function_body->kind == AST_STMT_COMPOUND &&
            nested_external->parameter_names &&
            nested_external->parameter_value_kinds &&
            nested_external->parameter_count == callee_base->as.call.arg_count &&
            ir_try_collect_function_parameter_passthrough_aliases(
                ctx,
                nested_external,
                nested_external->function_body,
                nested_aliases,
                sizeof(nested_aliases) / sizeof(nested_aliases[0]),
                nested_scalar_aliases,
                sizeof(nested_scalar_aliases) / sizeof(nested_scalar_aliases[0]),
                &nested_alias_count,
                &nested_scalar_alias_count,
                &nested_passthrough_stmt,
                &nested_returns_zero_after_void_call)) {
            if (nested_passthrough_stmt->kind == AST_STMT_RETURN &&
                nested_passthrough_stmt->expression_count == 1u &&
                !nested_returns_zero_after_void_call) {
                nested_callee_expr =
                    nested_passthrough_stmt->expressions
                        ? nested_passthrough_stmt->expressions[0]
                        : NULL;
            } else if ((nested_external->function_return_type == AST_FUNCTION_RETURN_VOID ||
                           nested_returns_zero_after_void_call) &&
                       nested_passthrough_stmt->kind == AST_STMT_EXPRESSION &&
                       nested_passthrough_stmt->expression_count == 1u) {
                nested_callee_expr =
                    nested_passthrough_stmt->expressions
                        ? nested_passthrough_stmt->expressions[0]
                        : NULL;
            }
            nested_callee_base =
                nested_callee_expr
                    ? ir_unwrap_function_value_wrapper_expression(nested_callee_expr)
                    : NULL;
            if (nested_callee_base && nested_callee_base->kind == AST_EXPR_CALL) {
                nested_called_name =
                    ir_extract_direct_call_callee_name(nested_callee_base->as.call.callee);
                if ((!nested_called_name || nested_called_name[0] == '\0') &&
                    !ir_classify_direct_identifier_expr(
                        nested_callee_base->as.call.callee,
                        &nested_called_name)) {
                    nested_called_name = NULL;
                }
                if (nested_called_name &&
                    nested_called_name[0] != '\0' &&
                    !ir_find_external_parameter_index_by_name(
                        nested_external,
                        nested_called_name,
                        1,
                        NULL)) {
                    const char *resolved_nested_name = NULL;

                    if (ir_try_resolve_function_parameter_passthrough_alias_name(
                            ctx,
                            nested_external,
                            nested_aliases,
                            nested_alias_count,
                            nested_callee_base->as.call.callee,
                            &resolved_nested_name) &&
                        resolved_nested_name &&
                        resolved_nested_name[0] != '\0') {
                        nested_called_name = resolved_nested_name;
                    }
                }
                if (nested_called_name &&
                    nested_called_name[0] != '\0' &&
                    ir_find_external_parameter_index_by_name(
                        nested_external,
                        nested_called_name,
                        1,
                        &nested_function_arg_index)) {
                    if (!callable_view->is_closure_family &&
                        ir_try_build_wrapper_function_body_eval_bindings(
                            ctx,
                            callee_external,
                            call_expr,
                            passthrough_stmt,
                            &wrapper_bindings,
                            &wrapper_binding_count)) {
                        if (wrapper_bindings &&
                            !ir_try_emit_narrow_static_noncapturing_helper_eval_from_bindings(
                                ctx,
                                nested_external,
                                (const AstExpression *const *)callee_base->as.call.args,
                                callee_base->as.call.arg_count,
                                wrapper_bindings,
                                wrapper_binding_count,
                                0u,
                                out_value,
                                &narrow_applied)) {
                            ir_function_body_eval_binding_array_clear(
                                wrapper_bindings,
                                wrapper_binding_count);
                            free(wrapper_bindings);
                            return 0;
                        }
                        ir_function_body_eval_binding_array_clear(
                            wrapper_bindings,
                            wrapper_binding_count);
                        free(wrapper_bindings);
                        wrapper_bindings = NULL;
                        wrapper_binding_count = 0u;
                        if (narrow_applied) {
                            if (returns_zero_after_void_call) {
                                *out_value = ir_value_immediate(0);
                            }
                            return ir_mark_callable_dispatch_applied(out_applied);
                        }
                    } else if (ctx->error && ctx->error->message[0] != '\0') {
                        return 0;
                    }

                    mapped_args = (AstExpression **)malloc(
                        nested_external->parameter_count * sizeof(AstExpression *));
                    if (!mapped_args) {
                        ir_set_error(ctx->error,
                            call_expr->line,
                            call_expr->column,
                            "IR-INT-126: out of memory while remapping passthrough callable arguments");
                        return 0;
                    }
                    for (size_t nested_arg_index = 0u;
                         nested_arg_index < nested_external->parameter_count;
                         ++nested_arg_index) {
                        const AstExpression *inner_arg =
                            ir_unwrap_function_value_wrapper_expression(
                                callee_base->as.call.args[nested_arg_index]);
                        const char *inner_arg_name = NULL;
                        const char *resolved_scalar_name = NULL;
                        size_t outer_index = 0u;
                        int found_outer = 0;

                        if (!inner_arg ||
                            inner_arg->kind != AST_EXPR_IDENTIFIER ||
                            !inner_arg->as.identifier.name) {
                            free(mapped_args);
                            mapped_args = NULL;
                            break;
                        }
                        inner_arg_name = inner_arg->as.identifier.name;
                        for (outer_index = 0u;
                             outer_index < callee_external->parameter_count;
                             ++outer_index) {
                            if (callee_external->parameter_names[outer_index] &&
                                strcmp(
                                    callee_external->parameter_names[outer_index],
                                    inner_arg_name) == 0) {
                                found_outer = 1;
                                break;
                            }
                        }
                        if (!found_outer &&
                            nested_external->parameter_value_kinds[nested_arg_index] == AST_PARAMETER_VALUE_FUNCTION &&
                            ir_try_resolve_function_parameter_passthrough_alias_name(
                                ctx,
                                callee_external,
                                aliases,
                                alias_count,
                                inner_arg,
                                &inner_arg_name) &&
                            inner_arg_name &&
                            inner_arg_name[0] != '\0') {
                            for (outer_index = 0u;
                                 outer_index < callee_external->parameter_count;
                                 ++outer_index) {
                                if (callee_external->parameter_names[outer_index] &&
                                    strcmp(
                                        callee_external->parameter_names[outer_index],
                                        inner_arg_name) == 0) {
                                    found_outer = 1;
                                    break;
                                }
                            }
                        }
                        if (!found_outer &&
                            nested_external->parameter_value_kinds[nested_arg_index] != AST_PARAMETER_VALUE_FUNCTION &&
                            ir_try_resolve_scalar_parameter_passthrough_alias_name(
                                callee_external,
                                scalar_aliases,
                                scalar_alias_count,
                                inner_arg,
                                &resolved_scalar_name) &&
                            resolved_scalar_name &&
                            resolved_scalar_name[0] != '\0') {
                            for (outer_index = 0u;
                                 outer_index < callee_external->parameter_count;
                                 ++outer_index) {
                                if (callee_external->parameter_names[outer_index] &&
                                    strcmp(
                                        callee_external->parameter_names[outer_index],
                                        resolved_scalar_name) == 0) {
                                    found_outer = 1;
                                    break;
                                }
                            }
                        }
                        if (!found_outer || outer_index >= call_expr->as.call.arg_count) {
                            free(mapped_args);
                            mapped_args = NULL;
                            break;
                        }
                        mapped_args[nested_arg_index] =
                            call_expr->as.call.args[outer_index];
                    }
                    if (mapped_args) {
                        memset(&nested_callable_view, 0, sizeof(nested_callable_view));
                        nested_callable_view.dynamic_tag_local_id = (size_t)-1;
                        if (ir_try_resolve_static_actual_argument_callable_object_view(
                                ctx,
                                mapped_args[nested_function_arg_index],
                                &nested_callable_view)) {
                            memset(&synthetic_callee_expr, 0, sizeof(synthetic_callee_expr));
                            synthetic_callee_expr.kind = AST_EXPR_IDENTIFIER;
                            synthetic_callee_expr.line = call_expr->line;
                            synthetic_callee_expr.column = call_expr->column;
                            synthetic_callee_expr.as.identifier.name =
                                (char *)callable_view->direct_target_name;
                            synthetic_callee_expr.as.identifier.name_length =
                                strlen(callable_view->direct_target_name);

                            memset(&synthetic_call_expr, 0, sizeof(synthetic_call_expr));
                            synthetic_call_expr.kind = AST_EXPR_CALL;
                            synthetic_call_expr.line = call_expr->line;
                            synthetic_call_expr.column = call_expr->column;
                            synthetic_call_expr.as.call.callee = &synthetic_callee_expr;
                            synthetic_call_expr.as.call.args = mapped_args;
                            synthetic_call_expr.as.call.arg_count =
                                nested_external->parameter_count;

                            recurse_result =
                                ir_try_emit_function_parameter_passthrough_callable_dispatch(
                                    ctx,
                                    &synthetic_call_expr,
                                    nested_external,
                                    nested_function_arg_index,
                                    &nested_callable_view,
                                    recursion_depth + 1u,
                                    out_value,
                                    &recurse_applied);
                        }
                        ir_free_callable_object_view_storage(&nested_callable_view);
                        free(mapped_args);
                        if (!recurse_result) {
                            return 0;
                        }
                        if (recurse_applied) {
                            if (returns_zero_after_void_call) {
                                *out_value = ir_value_immediate(0);
                            }
                            return ir_mark_callable_dispatch_applied(out_applied);
                        }
                    }
                }
            }
        }
    }
    if (!ir_try_emit_recursive_passthrough_dynamic_function_dispatch(
            ctx,
            call_expr,
            callee_external,
            callee_base,
            aliases,
            alias_count,
            scalar_aliases,
            scalar_alias_count,
            callable_view,
            recursion_depth,
            returns_zero_after_void_call,
            out_value,
            out_applied)) {
        free(call_args);
        return 0;
    }
    if (out_applied && *out_applied) {
        free(call_args);
        return 1;
    }
    if (!callee_base->as.call.args &&
        callee_base->as.call.arg_count > 0u) {
        return 1;
    }
    if (!call_expr->as.call.args ||
        call_expr->as.call.arg_count != callee_external->parameter_count) {
        return 1;
    }

    if (callee_base->as.call.arg_count > 0u) {
        call_args = (IrValueRef *)malloc(callee_base->as.call.arg_count * sizeof(IrValueRef));
        if (!call_args) {
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-126: out of memory while lowering passthrough callable arguments");
            return 0;
        }
    }

    for (size_t callee_arg_index = 0u; callee_arg_index < callee_base->as.call.arg_count; ++callee_arg_index) {
        const AstExpression *inner_arg = callee_base->as.call.args[callee_arg_index];
        const AstExpression *inner_arg_base = ir_unwrap_function_value_wrapper_expression(inner_arg);
        const char *inner_arg_name = NULL;
        const char *nested_called_name = NULL;
        size_t source_arg_index = (size_t)-1;

        if (!inner_arg_base) {
            free(call_args);
            return 1;
        }
        if (inner_arg_base->kind == AST_EXPR_CALL) {
            size_t nested_function_parameter_index = (size_t)-1;
            IrLowerCallableObjectView nested_callable_view;
            IrValueRef nested_result = ir_value_immediate(0);
            int nested_applied = 0;
            size_t nested_arg_count = 0u;

            if (inner_arg_base->as.call.arg_count > (sizeof(nested_call_args) / sizeof(nested_call_args[0]))) {
                free(call_args);
                return 1;
            }
            nested_called_name = ir_extract_direct_call_callee_name(inner_arg_base->as.call.callee);
            if ((!nested_called_name || nested_called_name[0] == '\0') &&
                !ir_classify_direct_identifier_expr(inner_arg_base->as.call.callee, &nested_called_name)) {
                free(call_args);
                return 1;
            }
            if (nested_called_name && nested_called_name[0] != '\0' &&
                !ir_find_external_parameter_index_by_name(
                    callee_external,
                    nested_called_name,
                    1,
                    NULL)) {
                const char *resolved_nested_name = NULL;

                if (ir_try_resolve_function_parameter_passthrough_alias_name(
                        ctx,
                        callee_external,
                        aliases,
                        alias_count,
                        inner_arg_base->as.call.callee,
                        &resolved_nested_name) &&
                    resolved_nested_name &&
                    resolved_nested_name[0] != '\0') {
                    nested_called_name = resolved_nested_name;
                }
            }
            if (!nested_called_name || nested_called_name[0] == '\0' ||
                !ir_find_external_parameter_index_by_name(
                    callee_external,
                    nested_called_name,
                    1,
                    &nested_function_parameter_index) ||
                nested_function_parameter_index >= call_expr->as.call.arg_count) {
                free(call_args);
                return 1;
            }
            memset(&nested_callable_view, 0, sizeof(nested_callable_view));
            nested_callable_view.dynamic_tag_local_id = (size_t)-1;
            if (!ir_try_resolve_static_actual_argument_callable_object_view(
                    ctx,
                    call_expr->as.call.args[nested_function_parameter_index],
                    &nested_callable_view)) {
                free(call_args);
                return 1;
            }
            for (size_t nested_arg_index = 0u; nested_arg_index < inner_arg_base->as.call.arg_count; ++nested_arg_index) {
                const AstExpression *nested_inner_arg =
                    ir_unwrap_function_value_wrapper_expression(
                        inner_arg_base->as.call.args[nested_arg_index]);
                size_t nested_outer_index = (size_t)-1;

                if (nested_inner_arg &&
                    nested_inner_arg->kind == AST_EXPR_IDENTIFIER &&
                    nested_inner_arg->as.identifier.name &&
                    ir_find_external_parameter_index_by_name(
                        callee_external,
                        nested_inner_arg->as.identifier.name,
                        1,
                        &nested_outer_index) &&
                    nested_outer_index < call_expr->as.call.arg_count) {
                    IrLowerCallableObjectView nested_arg_callable_view;
                    IrValueRef nested_function_arg_result = ir_value_immediate(0);
                    int nested_function_arg_applied = 0;

                    memset(&nested_arg_callable_view, 0, sizeof(nested_arg_callable_view));
                    nested_arg_callable_view.dynamic_tag_local_id = (size_t)-1;
                    if (!ir_try_resolve_static_actual_argument_callable_object_view(
                            ctx,
                            call_expr->as.call.args[nested_outer_index],
                            &nested_arg_callable_view)) {
                        free(call_args);
                        return 1;
                    }
                    if (!ir_try_emit_static_callable_view_dispatch_with_args(
                            ctx,
                            &nested_arg_callable_view,
                            NULL,
                            0u,
                            &nested_function_arg_result,
                            &nested_function_arg_applied)) {
                        ir_free_callable_object_view_storage(&nested_arg_callable_view);
                        ir_free_callable_object_view_storage(&nested_callable_view);
                        free(call_args);
                        return 0;
                    }
                    ir_free_callable_object_view_storage(&nested_arg_callable_view);
                    if (!nested_function_arg_applied) {
                        ir_free_callable_object_view_storage(&nested_callable_view);
                        free(call_args);
                        return 1;
                    }
                    nested_call_args[nested_arg_count] = nested_function_arg_result;
                    nested_arg_count += 1u;
                    continue;
                }

                if (!ir_try_lower_passthrough_bound_value_expr(
                        ctx,
                        callee_external,
                        scalar_aliases,
                        scalar_alias_count,
                        call_expr,
                        inner_arg_base->as.call.args[nested_arg_index],
                        &nested_call_args[nested_arg_count])) {
                    ir_free_callable_object_view_storage(&nested_callable_view);
                    free(call_args);
                    return 1;
                }
                nested_arg_count += 1u;
            }
            nested_call_args_view = nested_arg_count > 0u ? nested_call_args : NULL;
            if (!ir_try_emit_static_callable_view_dispatch_with_args(
                    ctx,
                    &nested_callable_view,
                    nested_call_args_view,
                    nested_arg_count,
                    &nested_result,
                    &nested_applied)) {
                ir_free_callable_object_view_storage(&nested_callable_view);
                free(call_args);
                return 0;
            }
            ir_free_callable_object_view_storage(&nested_callable_view);
            if (!nested_applied) {
                free(call_args);
                return 1;
            }
            call_args[callable_arg_count] = nested_result;
            callable_arg_count += 1u;
            continue;
        }
        if (inner_arg_base->kind != AST_EXPR_IDENTIFIER || !inner_arg_base->as.identifier.name) {
            free(call_args);
            return 1;
        }
        if (!ir_try_resolve_scalar_parameter_passthrough_alias_name(
                callee_external,
                scalar_aliases,
                scalar_alias_count,
                inner_arg_base,
                &inner_arg_name) ||
            !inner_arg_name ||
            inner_arg_name[0] == '\0') {
            free(call_args);
            return 1;
        }
        if (!ir_find_external_parameter_index_by_name(
                callee_external,
                inner_arg_name,
                0,
                &source_arg_index) ||
            source_arg_index >= call_expr->as.call.arg_count ||
            !ir_lower_expression(
                ctx,
                call_expr->as.call.args[source_arg_index],
                &call_args[callable_arg_count])) {
            free(call_args);
            return 0;
        }
        callable_arg_count += 1u;
    }

    call_args_view = callable_arg_count > 0u ? call_args : NULL;
    if (!ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
            ctx,
            callable_view,
            call_args_view,
            callable_arg_count,
            out_value,
            out_applied)) {
        free(call_args);
        return 0;
    }
    if (!(out_applied && *out_applied) && callable_view->dynamic_target_count <= 1u) {
        if (callable_view->is_closure_family) {
            if (!ir_try_emit_static_closure_callable_indirect_call_with_args(
                    ctx,
                    callable_view->direct_target_name,
                    callable_view->capture_local_ids,
                    callable_view->capture_count,
                    call_args_view,
                    callable_arg_count,
                    out_value,
                    out_applied)) {
                free(call_args);
                return 0;
            }
        } else {
            if (!ir_try_emit_static_noncapturing_target_callable_indirect_call_with_args(
                    ctx,
                    callable_view->direct_target_name,
                    call_args_view,
                    callable_arg_count,
                    out_value,
                    out_applied)) {
                free(call_args);
                return 0;
            }
        }
    }

    if (returns_zero_after_void_call && out_applied && *out_applied) {
        *out_value = ir_value_immediate(0);
    }

    free(call_args);
    return 1;
}

static int ir_can_use_direct_function_parameter_passthrough_dispatch(
    const IrLowerContext *ctx,
    const AstExternal *callee_external,
    size_t function_arg_index) {
    const AstStatement *body = NULL;
    const AstStatement *passthrough_stmt = NULL;
    const AstExpression *callee_expr = NULL;
    const AstExpression *callee_base = NULL;
    IrFunctionParameterPassthroughAlias aliases[8];
    IrScalarParameterPassthroughAlias scalar_aliases[8];
    size_t alias_count = 0u;
    size_t scalar_alias_count = 0u;
    int returns_zero_after_void_call = 0;
    const char *called_name = NULL;

    if (!ctx || !callee_external ||
        !callee_external->function_body ||
        callee_external->function_body->kind != AST_STMT_COMPOUND ||
        !callee_external->function_body->children ||
        callee_external->function_body->child_count == 0u ||
        !callee_external->parameter_names ||
        !callee_external->parameter_value_kinds ||
        function_arg_index >= callee_external->parameter_count ||
        callee_external->parameter_value_kinds[function_arg_index] != AST_PARAMETER_VALUE_FUNCTION) {
        return 0;
    }

    body = callee_external->function_body;
    if (!ir_try_collect_function_parameter_passthrough_aliases(
            ctx,
            callee_external,
            body,
            aliases,
            sizeof(aliases) / sizeof(aliases[0]),
            scalar_aliases,
            sizeof(scalar_aliases) / sizeof(scalar_aliases[0]),
            &alias_count,
            &scalar_alias_count,
            &passthrough_stmt,
            &returns_zero_after_void_call)) {
        return 0;
    }

    if (passthrough_stmt->kind == AST_STMT_RETURN &&
        passthrough_stmt->expression_count == 1u &&
        !returns_zero_after_void_call) {
        callee_expr = passthrough_stmt->expressions ? passthrough_stmt->expressions[0] : NULL;
    } else if ((callee_external->function_return_type == AST_FUNCTION_RETURN_VOID ||
                   returns_zero_after_void_call) &&
               passthrough_stmt->kind == AST_STMT_EXPRESSION &&
               passthrough_stmt->expression_count == 1u) {
        callee_expr = passthrough_stmt->expressions ? passthrough_stmt->expressions[0] : NULL;
    } else {
        return 0;
    }

    callee_base = ir_unwrap_function_value_wrapper_expression(callee_expr);
    if (callee_base && callee_base->kind == AST_EXPR_CALL) {
        called_name = ir_extract_direct_call_callee_name(callee_base->as.call.callee);
        if ((!called_name || called_name[0] == '\0') &&
            !ir_classify_direct_identifier_expr(callee_base->as.call.callee, &called_name)) {
            called_name = NULL;
        }
        if (called_name && called_name[0] != '\0' &&
            !ir_find_external_parameter_index_by_name(
                callee_external,
                called_name,
                1,
                NULL)) {
            const char *resolved_alias_name = NULL;

            if (ir_try_resolve_function_parameter_passthrough_alias_name(
                    ctx,
                    callee_external,
                    aliases,
                    alias_count,
                    callee_base->as.call.callee,
                    &resolved_alias_name) &&
                resolved_alias_name &&
                resolved_alias_name[0] != '\0') {
                called_name = resolved_alias_name;
            }
        }
        if (called_name &&
            called_name[0] != '\0' &&
            strcmp(called_name, callee_external->parameter_names[function_arg_index]) == 0) {
            return 1;
        }
    }

    return ir_expression_contains_function_parameter_callee_call(
        callee_external,
        callee_expr);
}

static int ir_lower_try_emit_dynamic_function_value_specialized_call(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    const char *callee_name,
    int function_arg_wrapper_side_effects_emitted,
    IrValueRef *out_value,
    int *out_applied) {
    size_t function_arg_index = (size_t)-1;
    size_t function_value_param_count = 0u;
    size_t non_function_arg_index = (size_t)-1;
    size_t non_function_arg_count = 0u;
    IrLowerCallableObjectView dynamic_callable_view;
    const char *const *owned_dynamic_target_names = NULL;
    long long dynamic_then_tag_value = 1;
    int requires_materialized_capture_forwarding = 0;
    int body_has_function_parameter_callee_calls = 0;

    if (out_applied) {
        *out_applied = 0;
    }
    memset(&dynamic_callable_view, 0, sizeof(dynamic_callable_view));
    dynamic_callable_view.dynamic_tag_local_id = (size_t)-1;
    (void)callee_name;
    if (!ctx || !call_expr || !callee_external || !callee_name || !out_value ||
        !callee_external->parameter_value_kinds || !ctx->ast_program) {
        return 1;
    }

    size_t dynamic_function_arg_count = 0u;
    for (size_t i = 0u; i < callee_external->parameter_count; ++i) {
        if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
            function_value_param_count += 1u;
            const AstExpression *arg_expr =
                call_expr->as.call.args && i < call_expr->as.call.arg_count
                    ? call_expr->as.call.args[i]
                    : NULL;
            const AstExpression *arg_base =
                arg_expr ? ir_unwrap_function_value_wrapper_expression(arg_expr) : NULL;
            int arg_is_dynamic_function_value = 0;

        if (arg_base && arg_base->kind == AST_EXPR_TERNARY) {
            arg_is_dynamic_function_value = 1;
        } else if (arg_expr) {
            const AstExpression *returned_base_call_expr = NULL;
            const AstExternal *returned_producer_external = NULL;
            int returned_family_is_closure = 0;
            const char **returned_target_names = NULL;
            size_t returned_target_count = 0u;
            IrLowerCallableObjectView candidate_view;

            if (arg_base && arg_base->kind == AST_EXPR_CALL &&
                ir_resolve_returned_function_value_call_producer_info(
                    ctx,
                    arg_base,
                    &returned_base_call_expr,
                    &returned_producer_external,
                    &returned_family_is_closure) &&
                returned_base_call_expr &&
                returned_producer_external &&
                ir_resolve_returned_function_value_target_info(
                    ctx,
                    returned_base_call_expr,
                    returned_producer_external,
                    returned_family_is_closure,
                    &returned_target_names,
                    &returned_target_count,
                    NULL)) {
                free((void *)returned_target_names);
                if (returned_target_count == 2u) {
                    arg_is_dynamic_function_value = 1;
                }
            } else {
                memset(&candidate_view, 0, sizeof(candidate_view));
                candidate_view.dynamic_tag_local_id = (size_t)-1;
                if (ir_resolve_callable_object_view(ctx, arg_expr, &candidate_view) &&
                    ir_callable_object_view_is_two_target_dynamic_family(&candidate_view)) {
                    arg_is_dynamic_function_value = 1;
                }
                ir_free_callable_object_view_storage(&candidate_view);
            }
        }

        if (arg_is_dynamic_function_value) {
            if (function_arg_index != (size_t)-1) {
                return 1;
                }
                function_arg_index = i;
                dynamic_function_arg_count += 1u;
                continue;
            }
            dynamic_function_arg_count += 0u;
        } else {
            if (non_function_arg_count == 0u) {
                non_function_arg_index = i;
            }
            non_function_arg_count += 1u;
        }
    }

    if (function_arg_index == (size_t)-1 ||
        call_expr->as.call.arg_count != callee_external->parameter_count ||
        function_arg_index >= call_expr->as.call.arg_count) {
        return 1;
    }

    body_has_function_parameter_callee_calls =
        callee_external->function_body &&
        ir_external_body_has_function_parameter_callee_calls(
            callee_external,
            callee_external->function_body);

    if (function_value_param_count <= 1u) {
        if (!ir_try_emit_ternary_function_value_argument_dispatch(
                ctx,
                call_expr,
                callee_external,
                function_arg_index,
                non_function_arg_index,
                non_function_arg_count,
                out_value,
                out_applied)) {
            return 0;
        }
        if (out_applied && *out_applied) {
            return 1;
        }
    }

    if (!body_has_function_parameter_callee_calls) {
        return 1;
    }

    if (body_has_function_parameter_callee_calls) {
        if (ir_try_emit_static_returned_single_capture_closure_callable_indirect_call(
                ctx,
                call_expr->as.call.args[function_arg_index],
                non_function_arg_count,
                non_function_arg_index,
                callee_external->function_return_type,
                call_expr,
                out_value,
                out_applied)) {
            if (out_applied && *out_applied) {
                return 1;
            }
        } else {
            return 0;
        }
    }

    if (!function_arg_wrapper_side_effects_emitted) {
        if (!ir_lower_emit_function_value_wrapper_side_effects(
                ctx,
                call_expr->as.call.args[function_arg_index])) {
            free((void *)owned_dynamic_target_names);
            return 0;
        }
    }

    if (!ir_try_resolve_dynamic_function_value_specialization_callable_object_view(
            ctx,
            call_expr->as.call.args[function_arg_index],
            callee_external,
            function_arg_index,
            &dynamic_callable_view,
            &owned_dynamic_target_names,
            &requires_materialized_capture_forwarding,
            &dynamic_then_tag_value)) {
        free((void *)owned_dynamic_target_names);
        return 1;
    }

    if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
            ctx,
            call_expr,
            callee_external,
            function_arg_index,
            &dynamic_callable_view,
            0u,
            out_value,
            out_applied)) {
        free((void *)owned_dynamic_target_names);
        return 0;
    }
    if (out_applied && *out_applied) {
        free((void *)owned_dynamic_target_names);
        return 1;
    }

    return ir_try_emit_dynamic_function_value_specialized_call_from_callable_object_view(
        ctx,
        call_expr,
        callee_external,
        function_arg_index,
        &dynamic_callable_view,
        owned_dynamic_target_names,
        requires_materialized_capture_forwarding,
        dynamic_then_tag_value,
        out_value,
        out_applied);
}

static IrBinaryOp ir_binary_op_from_token(TokenType token_type, int *out_supported) {
    if (out_supported) {
        *out_supported = 1;
    }

    switch (token_type) {
    case TOKEN_PLUS:
        return IR_BINARY_ADD;
    case TOKEN_MINUS:
        return IR_BINARY_SUB;
    case TOKEN_STAR:
        return IR_BINARY_MUL;
    case TOKEN_SLASH:
        return IR_BINARY_DIV;
    case TOKEN_PERCENT:
        return IR_BINARY_MOD;
    case TOKEN_AMP:
        return IR_BINARY_BIT_AND;
    case TOKEN_CARET:
        return IR_BINARY_BIT_XOR;
    case TOKEN_PIPE:
        return IR_BINARY_BIT_OR;
    case TOKEN_SHIFT_LEFT:
        return IR_BINARY_SHIFT_LEFT;
    case TOKEN_SHIFT_RIGHT:
        return IR_BINARY_SHIFT_RIGHT;
    case TOKEN_EQ:
        return IR_BINARY_EQ;
    case TOKEN_NE:
        return IR_BINARY_NE;
    case TOKEN_LT:
        return IR_BINARY_LT;
    case TOKEN_LE:
        return IR_BINARY_LE;
    case TOKEN_GT:
        return IR_BINARY_GT;
    case TOKEN_GE:
        return IR_BINARY_GE;
    default:
        if (out_supported) {
            *out_supported = 0;
        }
        return IR_BINARY_ADD;
    }
}

static IrBinaryOp ir_binary_op_from_assignment_token(TokenType token_type, int *out_supported) {
    if (out_supported) {
        *out_supported = 1;
    }

    switch (token_type) {
    case TOKEN_PLUS_ASSIGN:
        return IR_BINARY_ADD;
    case TOKEN_MINUS_ASSIGN:
        return IR_BINARY_SUB;
    case TOKEN_STAR_ASSIGN:
        return IR_BINARY_MUL;
    case TOKEN_SLASH_ASSIGN:
        return IR_BINARY_DIV;
    case TOKEN_PERCENT_ASSIGN:
        return IR_BINARY_MOD;
    case TOKEN_AMP_ASSIGN:
        return IR_BINARY_BIT_AND;
    case TOKEN_CARET_ASSIGN:
        return IR_BINARY_BIT_XOR;
    case TOKEN_PIPE_ASSIGN:
        return IR_BINARY_BIT_OR;
    case TOKEN_SHIFT_LEFT_ASSIGN:
        return IR_BINARY_SHIFT_LEFT;
    case TOKEN_SHIFT_RIGHT_ASSIGN:
        return IR_BINARY_SHIFT_RIGHT;
    default:
        if (out_supported) {
            *out_supported = 0;
        }
        return IR_BINARY_ADD;
    }
}

static const AstExpression *ir_unwrap_paren_expression(const AstExpression *expr) {
    const AstExpression *current = expr;

    while (current && current->kind == AST_EXPR_PAREN) {
        current = current->as.inner;
    }

    return current;
}

static const char *ir_extract_direct_call_callee_name(const AstExpression *callee) {
    const AstExpression *base = ir_unwrap_paren_expression(callee);

    if (!base || base->kind != AST_EXPR_IDENTIFIER) {
        return NULL;
    }

    return base->as.identifier.name;
}

static int ir_expression_is_direct_float_condition(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (ir_expression_is_direct_float_transport_root(ctx, expr) ||
        ir_expression_is_float_arithmetic(ctx, expr)) {
        return 1;
    }
    return base &&
        base->kind == AST_EXPR_CONVERSION &&
        base->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT;
}

static int ir_expression_is_signed_float_literal(const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const AstExpression *operand = NULL;

    if (!base) {
        return 0;
    }
    if (base->kind == AST_EXPR_FLOAT_LITERAL) {
        return 1;
    }
    if (base->kind != AST_EXPR_UNARY ||
        (base->as.unary.op != TOKEN_PLUS && base->as.unary.op != TOKEN_MINUS)) {
        return 0;
    }
    operand = ir_unwrap_paren_expression(base->as.unary.operand);
    return operand && operand->kind == AST_EXPR_FLOAT_LITERAL;
}

static int ir_expression_is_direct_float_transport_root(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!ctx || !base) {
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_UNARY:
        if (base->as.unary.op == TOKEN_PLUS || base->as.unary.op == TOKEN_MINUS) {
            if (ir_expression_is_signed_float_literal(base)) {
                return 1;
            }
            return ir_expression_is_direct_float_transport_root(ctx, base->as.unary.operand);
        }
        return 0;
    case AST_EXPR_IDENTIFIER: {
        IrLowerBindingInfo binding_info;
        IrGlobal *global = NULL;

        if (base->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info)) {
            return binding_info.array_rank == 0u &&
                binding_info.kind == IR_LOWER_BINDING_SCALAR_LOCAL &&
                ctx->function &&
                binding_info.local_id < ctx->function->local_count &&
                ctx->function->locals[binding_info.local_id].value_type == AST_FUNCTION_RETURN_FLOAT;
        }
        if (base->as.identifier.name && ctx->program) {
            global = ir_program_find_global_mut(ctx->program, base->as.identifier.name);
            return global &&
                global->array_rank == 0u &&
                global->value_type == AST_FUNCTION_RETURN_FLOAT;
        }
        return 0;
    }
    case AST_EXPR_CALL: {
        const char *callee_name = ir_extract_direct_call_callee_name(base->as.call.callee);
        IrFunction *callee_function = NULL;

        if (!callee_name || !ctx->program) {
            return 0;
        }
        callee_function = ir_program_find_function_mut(ctx->program, callee_name);
        return callee_function &&
            callee_function->return_type == AST_FUNCTION_RETURN_FLOAT;
    }
    default:
        return 0;
    }
}

static int ir_lower_signed_float_literal_expression(const AstExpression *expr,
    IrValueRef *out_value) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    const AstExpression *literal_expr = NULL;
    union {
        float f;
        uint32_t u;
    } bits;

    if (!out_value || !ir_expression_is_signed_float_literal(base)) {
        return 0;
    }

    literal_expr = base;
    if (base->kind == AST_EXPR_UNARY) {
        literal_expr = ir_unwrap_paren_expression(base->as.unary.operand);
    }
    if (!literal_expr || literal_expr->kind != AST_EXPR_FLOAT_LITERAL) {
        return 0;
    }

    bits.f = (float)literal_expr->as.float_number_value;
    if (base->kind == AST_EXPR_UNARY && base->as.unary.op == TOKEN_MINUS) {
        bits.u ^= 0x80000000u;
    }
    *out_value = ir_value_immediate((long long)(uint64_t)bits.u);
    return 1;
}

static int ir_expression_is_float_direct_compare(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);

    if (!ctx || !base || base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_EQ &&
            base->as.binary.op != TOKEN_NE &&
            base->as.binary.op != TOKEN_LT &&
            base->as.binary.op != TOKEN_LE &&
            base->as.binary.op != TOKEN_GT &&
            base->as.binary.op != TOKEN_GE)) {
        return 0;
    }

    return ir_expression_is_direct_float_condition(ctx, base->as.binary.left) &&
        ir_expression_is_direct_float_condition(ctx, base->as.binary.right);
}

static int ir_expression_is_float_arithmetic(const IrLowerContext *ctx,
    const AstExpression *expr) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;
    IrGlobal *global = NULL;
    IrFunction *callee_function = NULL;

    if (!ctx || !base || base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_PLUS &&
            base->as.binary.op != TOKEN_MINUS &&
            base->as.binary.op != TOKEN_STAR &&
            base->as.binary.op != TOKEN_SLASH)) {
        return 0;
    }

    switch (base->kind) {
    default:
        break;
    }

    if (base->as.binary.left) {
        const AstExpression *lhs = ir_unwrap_paren_expression(base->as.binary.left);
        if (lhs && lhs->kind == AST_EXPR_BINARY &&
            ir_expression_is_float_arithmetic(ctx, lhs)) {
            /* allowed */
        } else if (!ir_expression_is_direct_float_transport_root(ctx, base->as.binary.left)) {
            return 0;
        }
    }
    if (base->as.binary.right) {
        const AstExpression *rhs = ir_unwrap_paren_expression(base->as.binary.right);
        if (rhs && rhs->kind == AST_EXPR_BINARY &&
            ir_expression_is_float_arithmetic(ctx, rhs)) {
            /* allowed */
        } else if (!ir_expression_is_direct_float_transport_root(ctx, base->as.binary.right)) {
            return 0;
        }
    }
    (void)binding_info;
    (void)global;
    (void)callee_function;
    return 1;
}

static int ir_lower_float_arithmetic_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    IrValueRef lhs;
    IrValueRef rhs;
    IrValueRef call_args_storage[2];
    const char *helper_name = NULL;
    const char *current_function_name = NULL;
    IrFunction *callee_function = NULL;

    if (!ctx || !expr || !out_value || !ir_expression_is_float_arithmetic(ctx, expr)) {
        return 0;
    }

    switch (expr->as.binary.op) {
    case TOKEN_PLUS:
        helper_name = "__builtin_fadd32";
        break;
    case TOKEN_MINUS:
        helper_name = "__builtin_fsub32";
        break;
    case TOKEN_STAR:
        helper_name = "__builtin_fmul32";
        break;
    case TOKEN_SLASH:
        helper_name = "__builtin_fdiv32";
        break;
    default:
        return 0;
    }
    current_function_name = ctx->function ? ctx->function->name : NULL;
    if (!ir_ensure_builtin_function_signature(ctx->program, helper_name, &callee_function, ctx->error) ||
        !ir_lower_refresh_current_function(ctx, current_function_name, expr->line, expr->column) ||
        !ir_lower_expression(ctx, expr->as.binary.left, &lhs) ||
        !ir_lower_expression(ctx, expr->as.binary.right, &rhs) ||
        !callee_function) {
        return 0;
    }

    call_args_storage[0] = lhs;
    call_args_storage[1] = rhs;
    *out_value = ir_value_temp(ir_allocate_temp(ctx->function));
    return ir_emit_call(ctx, *out_value, helper_name, call_args_storage, 2u);
}

static int ir_lower_float_compare_normalize_zero(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_normalized) {
    IrValueRef masked;
    IrValueRef nonzero;
    size_t temp_id;

    if (!ctx || !out_normalized) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    masked = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    nonzero = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    *out_normalized = ir_value_temp(temp_id);

    return ir_emit_binary(ctx, masked, IR_BINARY_BIT_AND, input, ir_value_immediate(0x7fffffffLL)) &&
        ir_emit_binary(ctx, nonzero, IR_BINARY_NE, masked, ir_value_immediate(0)) &&
        ir_emit_binary(ctx, *out_normalized, IR_BINARY_MUL, input, nonzero);
}

static int ir_lower_float_compare_order_key(IrLowerContext *ctx,
    IrValueRef input,
    IrValueRef *out_key) {
    IrValueRef normalized;
    IrValueRef sign_mask;
    IrValueRef order_mask;
    size_t temp_id;

    if (!ctx || !out_key) {
        return 0;
    }
    if (!ir_lower_float_compare_normalize_zero(ctx, input, &normalized)) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    sign_mask = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    order_mask = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    *out_key = ir_value_temp(temp_id);

    return ir_emit_binary(ctx, sign_mask, IR_BINARY_SHIFT_RIGHT, normalized, ir_value_immediate(31)) &&
        ir_emit_binary(ctx, order_mask, IR_BINARY_BIT_OR, sign_mask, ir_value_immediate(0x80000000LL)) &&
        ir_emit_binary(ctx, *out_key, IR_BINARY_BIT_XOR, normalized, order_mask);
}

static int ir_lower_float_compare_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    IrValueRef lhs;
    IrValueRef rhs;
    IrValueRef lhs_key;
    IrValueRef rhs_key;
    IrValueRef lhs_normalized;
    IrValueRef rhs_normalized;
    IrBinaryOp compare_op;
    size_t temp_id;

    if (!ctx || !expr || !out_value || !ir_expression_is_float_direct_compare(ctx, expr)) {
        return 0;
    }
    if (!ir_lower_expression(ctx, expr->as.binary.left, &lhs) ||
        !ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    *out_value = ir_value_temp(temp_id);

    switch (expr->as.binary.op) {
    case TOKEN_EQ:
        compare_op = IR_BINARY_EQ;
        break;
    case TOKEN_NE:
        compare_op = IR_BINARY_NE;
        break;
    case TOKEN_LT:
        compare_op = IR_BINARY_LT;
        break;
    case TOKEN_LE:
        compare_op = IR_BINARY_LE;
        break;
    case TOKEN_GT:
        compare_op = IR_BINARY_GT;
        break;
    case TOKEN_GE:
        compare_op = IR_BINARY_GE;
        break;
    default:
        return 0;
    }

    if (compare_op == IR_BINARY_EQ || compare_op == IR_BINARY_NE) {
        return ir_lower_float_compare_normalize_zero(ctx, lhs, &lhs_normalized) &&
            ir_lower_float_compare_normalize_zero(ctx, rhs, &rhs_normalized) &&
            ir_emit_binary(ctx, *out_value, compare_op, lhs_normalized, rhs_normalized);
    }

    return ir_lower_float_compare_order_key(ctx, lhs, &lhs_key) &&
        ir_lower_float_compare_order_key(ctx, rhs, &rhs_key) &&
        ir_emit_binary(ctx, *out_value, compare_op, lhs_key, rhs_key);
}

typedef struct {
    IrValueRef address;
    size_t array_rank;
    const size_t *array_extents;
} IrLowerArrayAddress;

typedef enum {
    IR_LOWER_SCALAR_LVALUE_SLOT = 0,
    IR_LOWER_SCALAR_LVALUE_INDIRECT,
} IrLowerScalarLValueKind;

typedef struct {
    IrLowerScalarLValueKind kind;
    IrValueRef slot;
    IrValueRef address;
} IrLowerScalarLValue;

static int ir_emit_scaled_offset_add(IrLowerContext *ctx,
    IrValueRef *io_address,
    IrValueRef index_value,
    size_t byte_stride) {
    IrValueRef scaled_index;
    IrValueRef new_address;
    size_t temp_id;

    if (!ctx || !io_address) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    scaled_index = ir_value_temp(temp_id);
    if (!ir_emit_binary(ctx,
            scaled_index,
            IR_BINARY_MUL,
            index_value,
            ir_value_immediate((long long)byte_stride))) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    new_address = ir_value_temp(temp_id);
    if (!ir_emit_binary(ctx,
            new_address,
            IR_BINARY_ADD,
            *io_address,
            scaled_index)) {
        return 0;
    }

    *io_address = new_address;
    return 1;
}

static int ir_lower_array_base_address(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerArrayAddress *out_array) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerBindingInfo binding_info;
    IrGlobal *global;
    size_t temp_id;

    if (out_array) {
        memset(out_array, 0, sizeof(*out_array));
    }
    if (!ctx || !base || !out_array || base->kind != AST_EXPR_IDENTIFIER ||
        !base->as.identifier.name) {
        return 0;
    }

    if (ir_lower_scope_lookup_binding(&ctx->scopes, base->as.identifier.name, &binding_info)) {
        if (binding_info.array_rank == 0u) {
            return 0;
        }
        if (binding_info.kind == IR_LOWER_BINDING_ARRAY_PARAMETER) {
            out_array->address = ir_value_local(binding_info.local_id);
        } else {
            temp_id = ir_allocate_temp(ctx->function);
            out_array->address = ir_value_temp(temp_id);
            if (!ir_emit_addr_local(ctx, out_array->address, binding_info.local_id)) {
                return 0;
            }
        }
        out_array->array_rank = binding_info.array_rank;
        out_array->array_extents = binding_info.array_extents;
        return 1;
    }

    if (!ctx->program) {
        return 0;
    }
    global = ir_program_find_global_mut(ctx->program, base->as.identifier.name);
    if (!global || global->array_rank == 0u) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    out_array->address = ir_value_temp(temp_id);
    if (!ir_emit_addr_global(ctx, out_array->address, global->id)) {
        return 0;
    }
    out_array->array_rank = global->array_rank;
    out_array->array_extents = global->array_extents;
    return 1;
}

static int ir_lower_subscript_address(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerArrayAddress *out_array) {
    const AstExpression *base_expr;
    IrValueRef index_value;
    IrLowerArrayAddress base_array;
    size_t stride_elements = 1u;
    size_t stride_bytes;
    size_t i;

    if (!ctx || !expr || !out_array || expr->kind != AST_EXPR_SUBSCRIPT) {
        return 0;
    }

    base_expr = ir_unwrap_paren_expression(expr->as.subscript.base);
    if (!base_expr) {
        return 0;
    }

    if (base_expr->kind == AST_EXPR_SUBSCRIPT) {
        if (!ir_lower_subscript_address(ctx, base_expr, &base_array)) {
            return 0;
        }
    } else {
        if (!ir_lower_array_base_address(ctx, base_expr, &base_array)) {
            return 0;
        }
    }

    if (base_array.array_rank == 0u) {
        return 0;
    }

    for (i = 1u; i < base_array.array_rank; ++i) {
        if (!base_array.array_extents || base_array.array_extents[i] == 0u) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-032: multidimensional array subscript needs known trailing extents");
            return 0;
        }
        stride_elements *= base_array.array_extents[i];
    }
    stride_bytes = stride_elements * 4u;

    if (!ir_lower_expression(ctx, expr->as.subscript.index, &index_value) ||
        !ir_emit_scaled_offset_add(ctx, &base_array.address, index_value, stride_bytes)) {
        return 0;
    }

    out_array->address = base_array.address;
    out_array->array_rank = base_array.array_rank - 1u;
    out_array->array_extents = base_array.array_extents
        ? base_array.array_extents + 1u
        : NULL;
    return 1;
}

static int ir_extract_lvalue_ref(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_lvalue) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    size_t local_id;
    IrGlobal *global;

    if (!ctx || !base || !out_lvalue) {
        return 0;
    }

    if (base->kind == AST_EXPR_MEMBER && base->as.member.field_name) {
        if (ir_resolve_aggregate_lvalue_ref(ctx, base, out_lvalue)) {
            return 1;
        }
        return 0;
    }

    if (base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }

    if (ir_lower_scope_lookup(&ctx->scopes, base->as.identifier.name, &local_id)) {
        *out_lvalue = ir_value_local(local_id);
        return 1;
    }

    if (ctx->program) {
        global = ir_program_find_global_mut(ctx->program, base->as.identifier.name);
        if (global) {
            *out_lvalue = ir_value_global(global->id);
            return 1;
        }
    }

    return 0;
}

static int ir_lookup_pair_local_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t *out_first_local_id,
    size_t *out_second_local_id) {
    IrLowerBindingInfo binding_info;

    if (out_first_local_id) {
        *out_first_local_id = (size_t)-1;
    }
    if (out_second_local_id) {
        *out_second_local_id = (size_t)-1;
    }
    if (!ctx || !ir_resolve_local_aggregate_binding(ctx, expr, &binding_info) ||
        binding_info.kind != IR_LOWER_BINDING_PAIR_LOCAL) {
        return 0;
    }

    if (out_first_local_id) {
        *out_first_local_id = binding_info.local_id;
    }
    if (out_second_local_id) {
        *out_second_local_id = binding_info.pair_second_local_id;
    }
    return 1;
}

static const AstStructType *ir_find_struct_type(const AstProgram *program, const char *name) {
    size_t i;

    if (!program || !name || name[0] == '\0' || !program->struct_types) {
        return NULL;
    }
    for (i = 0u; i < program->struct_type_count; ++i) {
        if (program->struct_types[i].name &&
            strcmp(program->struct_types[i].name, name) == 0) {
            return &program->struct_types[i];
        }
    }
    return NULL;
}

static int ir_lookup_struct_local_binding(IrLowerContext *ctx,
    const AstExpression *expr,
    size_t *out_first_local_id,
    size_t *out_second_local_id,
    const char **out_type_name) {
    IrLowerBindingInfo binding_info;

    if (out_first_local_id) {
        *out_first_local_id = (size_t)-1;
    }
    if (out_second_local_id) {
        *out_second_local_id = (size_t)-1;
    }
    if (out_type_name) {
        *out_type_name = NULL;
    }
    if (!ctx || !ir_resolve_local_aggregate_binding(ctx, expr, &binding_info) ||
        binding_info.kind != IR_LOWER_BINDING_STRUCT_LOCAL) {
        return 0;
    }

    if (out_first_local_id) {
        *out_first_local_id = binding_info.local_id;
    }
    if (out_second_local_id) {
        *out_second_local_id = binding_info.pair_second_local_id;
    }
    if (out_type_name) {
        *out_type_name = binding_info.type_name;
    }
    return 1;
}

static int ir_lower_scalar_lvalue(IrLowerContext *ctx,
    const AstExpression *expr,
    IrLowerScalarLValue *out_lvalue) {
    const AstExpression *base = ir_unwrap_paren_expression(expr);
    IrLowerArrayAddress array_address;

    if (out_lvalue) {
        memset(out_lvalue, 0, sizeof(*out_lvalue));
    }
    if (!ctx || !base || !out_lvalue) {
        return 0;
    }

    if (base->kind == AST_EXPR_SUBSCRIPT) {
        if (!ir_lower_subscript_address(ctx, base, &array_address) ||
            array_address.array_rank > 0u) {
            return 0;
        }
        out_lvalue->kind = IR_LOWER_SCALAR_LVALUE_INDIRECT;
        out_lvalue->address = array_address.address;
        return 1;
    }

    if (base->kind == AST_EXPR_MEMBER) {
        if (!ir_extract_lvalue_ref(ctx, base, &out_lvalue->slot)) {
            return 0;
        }
        out_lvalue->kind = IR_LOWER_SCALAR_LVALUE_SLOT;
        return 1;
    }

    if (!ir_extract_lvalue_ref(ctx, base, &out_lvalue->slot)) {
        return 0;
    }

    out_lvalue->kind = IR_LOWER_SCALAR_LVALUE_SLOT;
    return 1;
}

static int ir_emit_scalar_lvalue_load(IrLowerContext *ctx,
    const IrLowerScalarLValue *lvalue,
    IrValueRef *out_value) {
    size_t temp_id;

    if (!ctx || !lvalue || !out_value || !ctx->function) {
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    *out_value = ir_value_temp(temp_id);

    if (lvalue->kind == IR_LOWER_SCALAR_LVALUE_INDIRECT) {
        return ir_emit_load_indirect(ctx, *out_value, lvalue->address);
    }

    return ir_emit_mov(ctx, *out_value, lvalue->slot);
}

static int ir_emit_scalar_lvalue_store(IrLowerContext *ctx,
    const IrLowerScalarLValue *lvalue,
    IrValueRef value) {
    long long known_value = 0;
    int has_known_value = 0;

    if (!ctx || !lvalue) {
        return 0;
    }

    if (lvalue->kind == IR_LOWER_SCALAR_LVALUE_INDIRECT) {
        return ir_emit_store_indirect(ctx, lvalue->address, value);
    }

    if (value.kind == IR_VALUE_IMMEDIATE) {
        has_known_value = 1;
        known_value = value.immediate;
    } else if (value.kind == IR_VALUE_LOCAL && ctx->scopes.count > 0) {
        IrLowerBindingInfo binding_info;

        if (ir_function_get_local(ctx->function, value.id) &&
            ir_lower_scope_lookup_binding(&ctx->scopes,
                ir_function_get_local(ctx->function, value.id)->source_name,
                &binding_info) &&
            binding_info.has_const_value) {
            has_known_value = 1;
            known_value = binding_info.const_value;
        }
    }

    if (!ir_lower_scope_update_binding_const_value_by_local_id(&ctx->scopes,
            lvalue->slot.id,
            has_known_value,
            known_value)) {
        /* Best-effort metadata update only; lowering still succeeds without it. */
    }

    return ir_emit_mov(ctx, lvalue->slot, value);
}

static int ir_lower_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value) {
    IrValueRef operand;
    IrValueRef lhs;
    IrValueRef rhs;
    IrValueRef *call_args = NULL;
    const char *callee_name;
    const AstExternal *callee_external = NULL;
    IrFunction *callee_function;
    size_t arg_count;
    size_t i;
    int op_supported;
    IrBinaryOp op;
    size_t temp_id;
    IrLowerScalarLValue scalar_lvalue;

    if (!ctx || !expr || !out_value) {
        ir_set_error(ctx ? ctx->error : NULL,
            expr ? expr->line : 0,
            expr ? expr->column : 0,
            "IR-INT-015: invalid expression lowering contract");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_NUMBER:
        *out_value = ir_value_immediate(expr->as.number_value);
        return 1;
    case AST_EXPR_FLOAT_LITERAL:
        return ir_lower_signed_float_literal_expression(expr, out_value);
    case AST_EXPR_CONVERSION:
        if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_INT) {
            IrValueRef call_args_storage[1];
            const char *current_function_name = ctx->function ? ctx->function->name : NULL;

            if (!ir_ensure_builtin_function_signature(
                    ctx->program,
                    "__builtin_f2i32",
                    &callee_function,
                    ctx->error) ||
                !ir_lower_refresh_current_function(
                    ctx,
                    current_function_name,
                    expr->line,
                    expr->column) ||
                !ir_lower_expression(ctx, expr->as.conversion.operand, &operand) ||
                !callee_function) {
                return 0;
            }

            call_args_storage[0] = operand;
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_call(ctx, *out_value, "__builtin_f2i32", call_args_storage, 1u);
        } else if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT) {
            IrValueRef call_args_storage[1];
            const char *current_function_name = ctx->function ? ctx->function->name : NULL;

            if (!ir_ensure_builtin_function_signature(
                    ctx->program,
                    "__builtin_i2f32",
                    &callee_function,
                    ctx->error) ||
                !ir_lower_refresh_current_function(
                    ctx,
                    current_function_name,
                    expr->line,
                    expr->column) ||
                !ir_lower_expression(ctx, expr->as.conversion.operand, &operand) ||
                !callee_function) {
                return 0;
            }

            call_args_storage[0] = operand;
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_call(ctx, *out_value, "__builtin_i2f32", call_args_storage, 1u);
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-054: explicit scalar conversion is not lowered for this target type");
        return 0;
    case AST_EXPR_IDENTIFIER: {
        IrLowerBindingInfo binding_info;
        IrGlobal *global = NULL;

        if (expr->as.identifier.name &&
            ir_lower_scope_lookup_binding(&ctx->scopes, expr->as.identifier.name, &binding_info)) {
            if (binding_info.kind == IR_LOWER_BINDING_PAIR_LOCAL) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-046: plain pair values are not supported in IR lowering");
                return 0;
            }
            if (binding_info.kind == IR_LOWER_BINDING_STRUCT_LOCAL) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-048: plain struct values are not supported in IR lowering");
                return 0;
            }
            if (binding_info.array_rank > 0u) {
                if (binding_info.kind == IR_LOWER_BINDING_ARRAY_PARAMETER) {
                    *out_value = ir_value_local(binding_info.local_id);
                    return 1;
                }
                {
                    IrLowerArrayAddress array_value;

                    if (!ir_lower_array_base_address(ctx, expr, &array_value)) {
                        return 0;
                    }
                    *out_value = array_value.address;
                    return 1;
                }
            }
            if (binding_info.is_const && binding_info.has_const_value) {
                *out_value = ir_value_immediate(binding_info.const_value);
                return 1;
            }
            *out_value = ir_value_local(binding_info.local_id);
            return 1;
        }
        if (expr->as.identifier.name && ctx->program) {
            global = ir_program_find_global_mut(ctx->program, expr->as.identifier.name);
            if (global && global->array_rank > 0u) {
                IrLowerArrayAddress array_value;

                if (!ir_lower_array_base_address(ctx, expr, &array_value)) {
                    return 0;
                }
                *out_value = array_value.address;
                return 1;
            }
        }
        if (expr->as.identifier.name &&
            ir_extract_lvalue_ref(ctx, expr, out_value)) {
            return 1;
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-002: unresolved identifier '%s' during IR lowering",
            expr->as.identifier.name ? expr->as.identifier.name : "<unnamed>");
        return 0;
    }
    case AST_EXPR_MEMBER:
        if (!ir_lower_scalar_lvalue(ctx, expr, &scalar_lvalue) ||
            !ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, out_value)) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-044: unsupported pair member access during IR lowering");
            return 0;
        }
        return 1;
    case AST_EXPR_PAREN:
        return ir_lower_expression(ctx, expr->as.inner, out_value);
    case AST_EXPR_CALL:
        {
            int applied_plain_direct_call = 0;
            if (!ir_try_lower_plain_direct_call(ctx, expr, out_value, &applied_plain_direct_call)) {
                return 0;
            }
            if (applied_plain_direct_call) {
                return 1;
            }
        }
        if (!ir_lower_emit_function_value_wrapper_side_effects(ctx, expr->as.call.callee)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-INT-131: call wrapper side-effect lowering failed without a diagnostic");
            }
            return 0;
        }
        callee_name = NULL;
        {
            size_t closure_prefix_arg_count = 0u;
            const size_t *closure_capture_local_ids = NULL;
            size_t *immediate_returned_closure_capture_local_ids = NULL;
            int callee_is_closure_local = 0;
            int call_uses_function_value_specialization = 0;
            int callee_from_static_returned_parameter_closure = 0;
            const AstExpression *callee_base_expr = ir_unwrap_paren_expression(expr->as.call.callee);
            IrLowerCallableObjectView direct_callee_view;
            int have_direct_callee_view = 0;

            memset(&direct_callee_view, 0, sizeof(direct_callee_view));
            direct_callee_view.dynamic_tag_local_id = (size_t)-1;

        if (callee_base_expr && callee_base_expr->kind == AST_EXPR_TERNARY) {
            IrLowerFunctionValueTernaryInfo ternary_info;
            size_t tag_local_id = (size_t)-1;
            size_t *payload_local_ids = NULL;
            IrValueRef *dynamic_args = NULL;
            const IrValueRef *dynamic_args_view = NULL;
            size_t dynamic_arg_count = 0u;
            size_t capture_index;
            IrLowerCallableObjectView ternary_callee_view;
            int applied_ternary_dispatch = 0;

            if (ir_resolve_identifier_ternary_function_value_info(ctx, callee_base_expr, &ternary_info) &&
                ternary_info.then_target_name && ternary_info.else_target_name &&
                ternary_info.then_capture_count == ternary_info.else_capture_count) {
                callee_is_closure_local = ternary_info.then_is_closure_local;
                if (!ir_function_append_local(
                        ctx->function,
                        "__ternary_callee_tag",
                        0,
                        &tag_local_id,
                        ctx->error) ||
                    !ir_lower_register_function_value_target(
                        ctx,
                        "__ternary_callee",
                        ternary_info.then_target_name,
                        &ternary_info.then_const_value) ||
                    !ir_lower_register_function_value_target(
                        ctx,
                        "__ternary_callee",
                        ternary_info.else_target_name,
                        &ternary_info.else_const_value)) {
                    return 0;
                }
                if (ternary_info.then_capture_count > 0u) {
                    payload_local_ids = (size_t *)malloc(ternary_info.then_capture_count * sizeof(size_t));
                    if (!payload_local_ids) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-INT-029: out of memory while lowering ternary callee closure payload");
                        return 0;
                    }
                    for (capture_index = 0u; capture_index < ternary_info.then_capture_count; ++capture_index) {
                        char payload_local_name[256];

                        snprintf(payload_local_name,
                            sizeof(payload_local_name),
                            "__ternary_callee_cap_%zu",
                            capture_index);
                        if (!ir_function_append_local(
                                ctx->function,
                                payload_local_name,
                                0,
                        &payload_local_ids[capture_index],
                        ctx->error)) {
                            free(payload_local_ids);
                            return 0;
                        }
                    }
                }
                if (!ir_emit_materialized_ternary_function_value_assignment(
                        ctx,
                        callee_base_expr->as.ternary.condition,
                        &ternary_info,
                        tag_local_id,
                        payload_local_ids,
                        ternary_info.then_capture_count)) {
                    free(payload_local_ids);
                    return 0;
                }
                dynamic_arg_count = expr->as.call.arg_count;
                if (dynamic_arg_count > 0u) {
                    dynamic_args = (IrValueRef *)malloc(dynamic_arg_count * sizeof(IrValueRef));
                    if (!dynamic_args) {
                        free(payload_local_ids);
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-INT-027: out of memory while lowering ternary callee call arguments");
                        return 0;
                    }
                }
                for (i = 0u; i < expr->as.call.arg_count; ++i) {
                    if (!ir_lower_expression(
                            ctx,
                            expr->as.call.args[i],
                            &dynamic_args[i])) {
                        free(dynamic_args);
                        free(payload_local_ids);
                        return 0;
                    }
                }
                memset(&ternary_callee_view, 0, sizeof(ternary_callee_view));
                ternary_callee_view.direct_target_name = ternary_info.then_target_name;
                ternary_callee_view.dynamic_target_names =
                    (const char *const[2]){ternary_info.then_target_name, ternary_info.else_target_name};
                ternary_callee_view.dynamic_target_count = 2u;
                ternary_callee_view.capture_local_ids = payload_local_ids;
                ternary_callee_view.capture_count = ternary_info.then_capture_count;
                ternary_callee_view.dynamic_tag_local_id = tag_local_id;
                ternary_callee_view.is_closure_family = ternary_info.then_is_closure_local ? 1 : 0;
                ternary_callee_view.has_dynamic_family = 1;
                dynamic_args_view = dynamic_arg_count > 0u ? dynamic_args : NULL;
                if (!ir_try_emit_two_target_callable_object_indirect_dispatch_with_args(
                        ctx,
                        &ternary_callee_view,
                        dynamic_args_view,
                        dynamic_arg_count,
                        out_value,
                        &applied_ternary_dispatch)) {
                    free(dynamic_args);
                    free(payload_local_ids);
                    return 0;
                }
                free(dynamic_args);
                free(payload_local_ids);
                if (applied_ternary_dispatch) {
                    return 1;
                }
            }
        }

        {
            IrReturnedCallCalleeResult returned_call_result;

            returned_call_result.handled = 0;
            returned_call_result.stop_outer_chain = 0;

            if (!ir_try_handle_returned_call_callee(
                    ctx,
                    expr,
                    callee_base_expr,
                    &callee_name,
                    &callee_is_closure_local,
                    &callee_from_static_returned_parameter_closure,
                    &closure_capture_local_ids,
                    &closure_prefix_arg_count,
                    &immediate_returned_closure_capture_local_ids,
                    out_value,
                    &returned_call_result)) {
                return 0;
            }
            if (returned_call_result.handled) {
                return 1;
            }
            if (returned_call_result.stop_outer_chain) {
                free(immediate_returned_closure_capture_local_ids);
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-INT-143: returned call-callee probe requested outer-chain stop without handling");
                return 0;
            }
        }
        if (!callee_name || callee_name[0] == '\0') {
            callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
        }

        if (!callee_is_closure_local) {
            int handled_dynamic_dispatch = 0;

            if (!(callee_base_expr && callee_base_expr->kind == AST_EXPR_CALL &&
                    closure_prefix_arg_count > 0u &&
                    closure_capture_local_ids)) {
                if (ir_resolve_any_callable_object_view(
                        ctx,
                        expr->as.call.callee,
                        &direct_callee_view) &&
                    direct_callee_view.direct_target_name &&
                    direct_callee_view.direct_target_name[0] != '\0') {
                    have_direct_callee_view = 1;
                }
            }
            if (!ir_try_handle_direct_callee_callable_object_dynamic_dispatch(
                    ctx,
                    have_direct_callee_view ? &direct_callee_view : NULL,
                    expr,
                    &callee_is_closure_local,
                    &closure_capture_local_ids,
                    &closure_prefix_arg_count,
                    immediate_returned_closure_capture_local_ids,
                    &callee_name,
                    out_value,
                    &handled_dynamic_dispatch)) {
                free(immediate_returned_closure_capture_local_ids);
                return ir_finish_failed_callable_probe_with_default_diagnostic(
                    ctx,
                    expr->line,
                    expr->column,
                    "IR-INT-125: direct callee callable-object dynamic dispatch probe failed without a diagnostic");
            }
            if (handled_dynamic_dispatch) {
                return 1;
            }
        }
        if (!callee_name || callee_name[0] == '\0') {
            const char *direct_identifier_name = NULL;

            callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
            if (!callee_name || callee_name[0] == '\0') {
                if (!ir_callee_is_supported_direct_identifier_shape(ctx, expr->as.call.callee)) {
                    free(immediate_returned_closure_capture_local_ids);
                    ir_set_error(ctx->error,
                        expr->line,
                        expr->column,
                        "IR-LOWER-013: call callee is not a supported direct identifier in IR v1");
                    return 0;
                }
                (void)ir_resolve_function_value_wrapper_direct_identifier(
                    ctx,
                    expr->as.call.callee,
                    &direct_identifier_name);
                callee_name = direct_identifier_name;
            }
        }
        if (!callee_is_closure_local && callee_name && callee_name[0] != '\0') {
            const char *bound_target_name = ir_lower_lookup_function_value_target(ctx, callee_name);
            const size_t *bound_capture_local_ids = NULL;
            size_t bound_capture_count = 0u;

            if (bound_target_name && bound_target_name[0] != '\0' &&
                ir_lower_lookup_function_value_capture_binding(
                    ctx,
                    callee_name,
                    &bound_capture_local_ids,
                    &bound_capture_count) &&
                bound_capture_count > 0u) {
                callee_name = bound_target_name;
                callee_is_closure_local = 1;
                closure_capture_local_ids = bound_capture_local_ids;
                closure_prefix_arg_count = bound_capture_count;
            }
        }
        if (!callee_name || callee_name[0] == '\0') {
            free(immediate_returned_closure_capture_local_ids);
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-025: call target '%s' is not declared in IR lowering context",
                "<unknown>");
            return 0;
        }

        if (!ctx->program) {
            free(immediate_returned_closure_capture_local_ids);
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-INT-028: call lowering requires program symbol table context");
            return 0;
        }

        ir_clear_call_lowering_probe_error(ctx);

        {
            int applied_callable_dispatch = 0;
            int handled_callable_dispatch = 0;
            int should_try_static_direct_target_dispatch = 0;
            int has_static_direct_callee_view = 0;
            const char *callable_callee_name = NULL;
            IrLowerBindingInfo callable_binding_info;

            memset(&callable_binding_info, 0, sizeof(callable_binding_info));
            if ((callee_base_expr && callee_base_expr->kind == AST_EXPR_CALL) ||
                callee_is_closure_local ||
                callee_from_static_returned_parameter_closure) {
                should_try_static_direct_target_dispatch = 1;
            } else if (have_direct_callee_view &&
                direct_callee_view.direct_target_name &&
                direct_callee_view.direct_target_name[0] != '\0' &&
                (!direct_callee_view.has_dynamic_family ||
                    direct_callee_view.dynamic_tag_local_id == (size_t)-1 ||
                    direct_callee_view.has_const_tag_value)) {
                should_try_static_direct_target_dispatch = 1;
                has_static_direct_callee_view = 1;
            } else if (ir_resolve_function_value_wrapper_direct_identifier(
                           ctx,
                           expr->as.call.callee,
                           &callable_callee_name) &&
                callable_callee_name &&
                callable_callee_name[0] != '\0') {
                if (ir_lower_lookup_function_value_target(ctx, callable_callee_name)) {
                    should_try_static_direct_target_dispatch = 1;
                } else if (ir_lower_scope_lookup_binding(
                               &ctx->scopes,
                               callable_callee_name,
                               &callable_binding_info) &&
                    callable_binding_info.type_name &&
                    callable_binding_info.type_name[0] != '\0' &&
                    (callable_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
                        callable_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL)) {
                    should_try_static_direct_target_dispatch = 1;
                }
            }

            if (should_try_static_direct_target_dispatch &&
                (callee_is_closure_local ||
                    callee_from_static_returned_parameter_closure ||
                    (callee_base_expr && callee_base_expr->kind == AST_EXPR_CALL) ||
                    has_static_direct_callee_view)) {
                if (!ir_try_emit_direct_target_static_callee_dispatch(
                        ctx,
                        have_direct_callee_view ? &direct_callee_view : NULL,
                        callee_base_expr,
                        callee_name,
                        callee_is_closure_local,
                        callee_from_static_returned_parameter_closure,
                        closure_capture_local_ids,
                        closure_prefix_arg_count,
                        expr,
                        out_value,
                        &applied_callable_dispatch)) {
                    free(immediate_returned_closure_capture_local_ids);
                    return ir_finish_failed_callable_probe_with_default_diagnostic(
                        ctx,
                        expr->line,
                        expr->column,
                        "IR-INT-126: static direct-target callable dispatch probe failed without a diagnostic");
                }
                if (!ir_finish_optional_returned_callable_dispatch(
                        applied_callable_dispatch,
                        immediate_returned_closure_capture_local_ids,
                        &handled_callable_dispatch)) {
                    return 0;
                }
                if (handled_callable_dispatch) {
                    return 1;
                }
            }
        }

        if (!ir_finish_call_lowering_probe_error_if_present(
                ctx,
                immediate_returned_closure_capture_local_ids)) {
            return 0;
        }

        {
            int handled_callable_dispatch = 0;

            if (!ir_try_handle_returned_binding_probe(
                    ctx,
                    have_direct_callee_view ? &direct_callee_view : NULL,
                    expr->as.call.callee,
                    expr,
                    callee_base_expr,
                    immediate_returned_closure_capture_local_ids,
                    out_value,
                    &handled_callable_dispatch)) {
                free(immediate_returned_closure_capture_local_ids);
                return 0;
            }
            if (handled_callable_dispatch) {
                return 1;
            }
        }

        if (!ir_finish_call_lowering_probe_error_if_present(
                ctx,
                immediate_returned_closure_capture_local_ids)) {
            return 0;
        }

        callee_function = ir_program_find_function_mut(ctx->program, callee_name);
        if (!callee_function &&
            !ir_ensure_builtin_function_signature(ctx->program, callee_name, &callee_function, ctx->error)) {
            free(immediate_returned_closure_capture_local_ids);
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-025: call target '%s' is not declared in IR lowering context",
                callee_name);
            return 0;
        }
        if (ctx->ast_program) {
            const char *specialized_name = NULL;
            const char *current_function_name = ctx->function ? ctx->function->name : NULL;
            int used_dynamic_function_value_specialization = 0;
            int used_static_actual_argument_callable_dispatch = 0;
            int function_arg_wrapper_side_effects_emitted = 0;
            int callee_has_function_value_parameters = 0;
            int body_needs_function_value_specialization = 0;
            size_t function_parameter_index = (size_t)-1;
            size_t function_parameter_count = 0u;
            IrLowerCallableObjectView static_actual_argument_view;

            for (i = 0u; i < ctx->ast_program->count; ++i) {
                const AstExternal *candidate = &ctx->ast_program->externals[i];
                if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                    candidate->name &&
                    strcmp(candidate->name, callee_name) == 0) {
                    callee_external = candidate;
                    break;
                }
            }

            if (callee_external && callee_external->parameter_value_kinds) {
                for (i = 0u; i < callee_external->parameter_count; ++i) {
                    if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
                        callee_has_function_value_parameters = 1;
                        function_parameter_count += 1u;
                        if (function_parameter_index == (size_t)-1) {
                            function_parameter_index = i;
                        } else {
                            function_parameter_index = (size_t)-1;
                        }
                    }
                }
                body_needs_function_value_specialization =
                    ir_external_body_needs_function_value_specialization(
                        ctx->ast_program,
                        callee_external,
                        callee_external->function_body);
            }

            if (body_needs_function_value_specialization &&
                !callee_has_function_value_parameters) {
                free(immediate_returned_closure_capture_local_ids);
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-INT-133: call target '%s' was marked as needing function-value specialization despite having no function-valued parameters",
                    callee_name ? callee_name : "<unknown>");
                return 0;
            }

            if (callee_has_function_value_parameters &&
                function_parameter_count > 1u &&
                expr->as.call.arg_count == callee_external->parameter_count) {
                int multi_static_passthrough_safe = 1;

                for (i = 0u; i < callee_external->parameter_count; ++i) {
                    const AstExpression *function_arg_expr = NULL;
                    const AstExpression *function_arg_base = NULL;

                    if (callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                        continue;
                    }
                    function_arg_expr = expr->as.call.args[i];
                    function_arg_base =
                        ir_unwrap_function_value_wrapper_expression(function_arg_expr);
                    if (ir_function_value_wrapper_expression_has_side_effects(function_arg_expr) ||
                        !function_arg_base ||
                        function_arg_base->kind != AST_EXPR_IDENTIFIER) {
                        multi_static_passthrough_safe = 0;
                        break;
                    }
                }

                if (multi_static_passthrough_safe) {
                    for (i = 0u; i < callee_external->parameter_count; ++i) {
                        if (callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                            continue;
                        }
                        memset(&static_actual_argument_view, 0, sizeof(static_actual_argument_view));
                        static_actual_argument_view.dynamic_tag_local_id = (size_t)-1;
                        if (!ir_try_resolve_static_actual_argument_callable_object_view(
                                ctx,
                                expr->as.call.args[i],
                                &static_actual_argument_view)) {
                            ir_free_callable_object_view_storage(&static_actual_argument_view);
                            continue;
                        }
                        if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                                ctx,
                                expr,
                                callee_external,
                                i,
                                &static_actual_argument_view,
                                0u,
                                out_value,
                                &used_static_actual_argument_callable_dispatch)) {
                            ir_free_callable_object_view_storage(&static_actual_argument_view);
                            free(immediate_returned_closure_capture_local_ids);
                            return 0;
                        }
                        ir_free_callable_object_view_storage(&static_actual_argument_view);
                        if (used_static_actual_argument_callable_dispatch) {
                            return ir_finish_returned_callable_capture_cleanup(
                                immediate_returned_closure_capture_local_ids);
                        }
                    }
                }
            }

            if (body_needs_function_value_specialization &&
                callee_has_function_value_parameters &&
                expr->as.call.arg_count == callee_external->parameter_count) {
                if (function_parameter_count == 1u &&
                    function_parameter_index != (size_t)-1 &&
                    function_parameter_index < expr->as.call.arg_count) {
                    const AstExpression *static_function_arg_expr =
                        expr->as.call.args[function_parameter_index];
                    const AstExpression *static_function_arg_base =
                        ir_unwrap_function_value_wrapper_expression(static_function_arg_expr);

                    if (ir_lower_emit_function_value_wrapper_side_effects(
                            ctx,
                            static_function_arg_expr)) {
                        function_arg_wrapper_side_effects_emitted = 1;
                        memset(&static_actual_argument_view, 0, sizeof(static_actual_argument_view));
                        static_actual_argument_view.dynamic_tag_local_id = (size_t)-1;
                        if (ir_try_resolve_static_actual_argument_callable_object_view(
                                ctx,
                                static_function_arg_expr,
                                &static_actual_argument_view)) {
                            if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                                    ctx,
                                    expr,
                                    callee_external,
                                    function_parameter_index,
                                    &static_actual_argument_view,
                                    0u,
                                    out_value,
                                    &used_static_actual_argument_callable_dispatch)) {
                                ir_free_callable_object_view_storage(&static_actual_argument_view);
                                free(immediate_returned_closure_capture_local_ids);
                                return 0;
                            }
                        }
                        ir_free_callable_object_view_storage(&static_actual_argument_view);
                        if (used_static_actual_argument_callable_dispatch) {
                            return ir_finish_returned_callable_capture_cleanup(
                                immediate_returned_closure_capture_local_ids);
                        }

                        if (static_function_arg_base &&
                            static_function_arg_base->kind == AST_EXPR_IDENTIFIER) {
                            IrLowerBindingInfo static_function_arg_binding_info;
                            const size_t *resolved_capture_local_ids = NULL;
                            size_t resolved_capture_count = 0u;
                            const char *resolved_target_name = NULL;
                            IrLowerCallableObjectView resolved_target_view;

                            memset(&static_function_arg_binding_info, 0, sizeof(static_function_arg_binding_info));
                            if (ir_lower_scope_lookup_binding(
                                    &ctx->scopes,
                                    static_function_arg_base->as.identifier.name,
                                    &static_function_arg_binding_info) &&
                                (static_function_arg_binding_info.kind == IR_LOWER_BINDING_FUNCTION_LOCAL ||
                                    static_function_arg_binding_info.kind == IR_LOWER_BINDING_CLOSURE_LOCAL) &&
                                (static_function_arg_binding_info.function_target_tag_local_id == (size_t)-1 ||
                                    static_function_arg_binding_info.has_const_value ||
                                    (ir_lower_find_function_value_target_set(
                                            ctx,
                                            static_function_arg_base->as.identifier.name) &&
                                        ir_lower_find_function_value_target_set(
                                            ctx,
                                            static_function_arg_base->as.identifier.name)->target_count > 1u)) &&
                                ir_call_argument_resolve_extension_function_value_target(
                                    ctx->ast_program,
                                    ctx,
                                    callee_external,
                                    function_parameter_index,
                                    static_function_arg_expr,
                                    &resolved_capture_local_ids,
                                    &resolved_capture_count,
                                    &resolved_target_name) &&
                                resolved_target_name &&
                                resolved_target_name[0] != '\0') {
                                memset(&resolved_target_view, 0, sizeof(resolved_target_view));
                                resolved_target_view.dynamic_tag_local_id = (size_t)-1;
                                resolved_target_view.direct_target_name = resolved_target_name;
                                resolved_target_view.capture_local_ids = resolved_capture_local_ids;
                                resolved_target_view.capture_count = resolved_capture_count;
                                resolved_target_view.is_closure_family =
                                    resolved_capture_count > 0u ? 1 : 0;
                                if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                                        ctx,
                                        expr,
                                        callee_external,
                                        function_parameter_index,
                                        &resolved_target_view,
                                        0u,
                                        out_value,
                                        &used_static_actual_argument_callable_dispatch)) {
                                    free(immediate_returned_closure_capture_local_ids);
                                    return 0;
                                }
                                if (used_static_actual_argument_callable_dispatch) {
                                    return ir_finish_returned_callable_capture_cleanup(
                                        immediate_returned_closure_capture_local_ids);
                                }
                            }
                        }
                    }
                } else if (function_parameter_count > 1u) {
                    int multi_static_passthrough_safe = 1;

                    for (i = 0u; i < callee_external->parameter_count; ++i) {
                        const AstExpression *function_arg_expr = NULL;
                        const AstExpression *function_arg_base = NULL;

                        if (callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                            continue;
                        }
                        function_arg_expr = expr->as.call.args[i];
                        function_arg_base = ir_unwrap_paren_expression(function_arg_expr);
                        if (ir_function_value_wrapper_expression_has_side_effects(function_arg_expr) ||
                            !function_arg_base ||
                            function_arg_base->kind != AST_EXPR_IDENTIFIER) {
                            multi_static_passthrough_safe = 0;
                            break;
                        }
                    }

                    if (multi_static_passthrough_safe) {
                        for (i = 0u; i < callee_external->parameter_count; ++i) {
                            if (callee_external->parameter_value_kinds[i] != AST_PARAMETER_VALUE_FUNCTION) {
                                continue;
                            }
                            memset(&static_actual_argument_view, 0, sizeof(static_actual_argument_view));
                            static_actual_argument_view.dynamic_tag_local_id = (size_t)-1;
                            if (!ir_try_resolve_static_actual_argument_callable_object_view(
                                    ctx,
                                    expr->as.call.args[i],
                                    &static_actual_argument_view)) {
                                ir_free_callable_object_view_storage(&static_actual_argument_view);
                                continue;
                            }
                            if (!ir_try_emit_function_parameter_passthrough_callable_dispatch(
                                    ctx,
                                    expr,
                                    callee_external,
                                    i,
                                    &static_actual_argument_view,
                                    0u,
                                    out_value,
                                    &used_static_actual_argument_callable_dispatch)) {
                                ir_free_callable_object_view_storage(&static_actual_argument_view);
                                free(immediate_returned_closure_capture_local_ids);
                                return 0;
                            }
                            ir_free_callable_object_view_storage(&static_actual_argument_view);
                            if (used_static_actual_argument_callable_dispatch) {
                                return ir_finish_returned_callable_capture_cleanup(
                                    immediate_returned_closure_capture_local_ids);
                            }
                        }
                    }
                }
            }

            if (callee_external &&
                callee_external->parameter_value_kinds) {
                if (!ir_lower_try_emit_dynamic_function_value_specialized_call(
                        ctx,
                        expr,
                        callee_external,
                        callee_name,
                        function_arg_wrapper_side_effects_emitted,
                        out_value,
                        &used_dynamic_function_value_specialization)) {
                    if (ctx->error && ctx->error->message[0] == '\0') {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-INT-127: dynamic function-value specialization probe failed without a diagnostic");
                    }
                    free(immediate_returned_closure_capture_local_ids);
                    return 0;
                }
                if (used_dynamic_function_value_specialization) {
                    return ir_finish_returned_callable_capture_cleanup(
                        immediate_returned_closure_capture_local_ids);
                }
            }

            if (body_needs_function_value_specialization) {
                if (!ir_build_function_value_specialization_name_from_call_args(
                        ctx,
                        callee_external,
                        (const AstExpression *const *)expr->as.call.args,
                        callee_external->parameter_count,
                        expr->line,
                        expr->column,
                        "IR-INT-029: out of memory while preparing function-value specialization",
                        "IR-LOWER-028: failed to resolve function-value call specialization target",
                        "IR-INT-134: static function-value specialization materialization failed without a diagnostic",
                        &specialized_name)) {
                    free(immediate_returned_closure_capture_local_ids);
                    return 0;
                }
                if (!ir_finish_function_value_specialization_callee_adoption(
                        ctx,
                        current_function_name,
                        specialized_name,
                        expr->line,
                        expr->column,
                        "IR-INT-083: current lowering function disappeared after specialization",
                        &callee_name,
                        &callee_function,
                        &call_uses_function_value_specialization)) {
                    free(immediate_returned_closure_capture_local_ids);
                    return 0;
                }
            }
        }

        arg_count = expr->as.call.arg_count;
        if (callee_external && callee_external->kind == AST_EXTERNAL_FUNCTION) {
            size_t source_arg_count = expr->as.call.arg_count;
            size_t lowered_arg_count = 0u;

            if (source_arg_count != callee_external->parameter_count) {
                free(immediate_returned_closure_capture_local_ids);
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-026: call to '%s' has %zu args but callee source signature expects %zu",
                    callee_name,
                    source_arg_count,
                    callee_external->parameter_count);
                return 0;
            }
            for (i = 0u; i < source_arg_count; ++i) {
                if (!ir_try_append_maybe_specialized_call_argument(
                        ctx->ast_program,
                        ctx,
                        callee_external,
                        i,
                        expr->as.call.args[i],
                        call_uses_function_value_specialization,
                        expr->line,
                        expr->column,
                        "IR-LOWER-028: failed to resolve function-value call specialization target",
                        "IR-LOWER-058: closure forwarding call is missing captured-value lowering metadata",
                        "IR-INT-027: out of memory while lowering specialized closure capture arguments",
                        "IR-INT-128: call argument %zu for '%s' failed to lower without a diagnostic",
                        callee_name,
                        &call_args,
                        &lowered_arg_count)) {
                    free(call_args);
                    free(immediate_returned_closure_capture_local_ids);
                    return 0;
                }
            }
            arg_count = lowered_arg_count;
        } else {
            size_t expected_arg_count =
                callee_is_closure_local
                    ? closure_prefix_arg_count + arg_count
                    : arg_count;

            if (expected_arg_count != callee_function->parameter_count) {
                free(immediate_returned_closure_capture_local_ids);
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-026: call to '%s' has %zu args but callee signature expects %zu",
                    callee_name,
                    expected_arg_count,
                    callee_function->parameter_count);
                return 0;
            }
            if (expected_arg_count > 0) {
                call_args = (IrValueRef *)malloc(expected_arg_count * sizeof(IrValueRef));
                if (!call_args) {
                    free(immediate_returned_closure_capture_local_ids);
                    ir_set_error(ctx->error,
                        expr->line,
                        expr->column,
                        "IR-INT-027: out of memory while lowering call arguments");
                    return 0;
                }
            }
            for (i = 0u; i < closure_prefix_arg_count; ++i) {
                if (!closure_capture_local_ids) {
                    free(call_args);
                    free(immediate_returned_closure_capture_local_ids);
                    ir_set_error(ctx->error,
                        expr->line,
                        expr->column,
                        "IR-LOWER-058: closure call is missing captured-value lowering metadata");
                    return 0;
                }
                call_args[i] = ir_value_local(closure_capture_local_ids[i]);
            }
            for (i = 0u; i < arg_count; ++i) {
                if (!ir_lower_expression(
                        ctx,
                        expr->as.call.args[i],
                        &call_args[closure_prefix_arg_count + i])) {
                    free(call_args);
                    free(immediate_returned_closure_capture_local_ids);
                    return 0;
                }
            }
            arg_count = expected_arg_count;
        }

        if (arg_count != callee_function->parameter_count) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-026: call to '%s' lowers to %zu args but callee signature expects %zu",
                callee_name,
                arg_count,
                callee_function->parameter_count);
            free(call_args);
            free(immediate_returned_closure_capture_local_ids);
            return 0;
        }

        if (callee_function->return_type == AST_FUNCTION_RETURN_VOID) {
            IrValueRef ignored_result = ir_value_temp(ir_allocate_temp(ctx->function));

            *out_value = ir_value_immediate(0);
            if (!ir_emit_call(ctx, ignored_result, callee_name, call_args, arg_count)) {
                if (ctx->error && ctx->error->message[0] == '\0') {
                    ir_set_error(ctx->error,
                        expr->line,
                        expr->column,
                        "IR-INT-129: direct call emission for '%s' failed without a diagnostic",
                        callee_name ? callee_name : "<unknown>");
                }
                free(call_args);
                free(immediate_returned_closure_capture_local_ids);
                return 0;
            }
            free(call_args);
            return ir_finish_returned_callable_capture_cleanup(
                immediate_returned_closure_capture_local_ids);
        }

        temp_id = ir_allocate_temp(ctx->function);
        *out_value = ir_value_temp(temp_id);
        if (!ir_emit_call(ctx, *out_value, callee_name, call_args, arg_count)) {
            if (ctx->error && ctx->error->message[0] == '\0') {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-INT-129: direct call emission for '%s' failed without a diagnostic",
                    callee_name ? callee_name : "<unknown>");
            }
            free(call_args);
            free(immediate_returned_closure_capture_local_ids);
            return 0;
        }
        free(call_args);
        return ir_finish_returned_callable_capture_cleanup(
            immediate_returned_closure_capture_local_ids);
        }
    case AST_EXPR_TERNARY:
        return ir_lower_ternary_value_expression(ctx, expr, out_value);
    case AST_EXPR_UNARY:
        if ((expr->as.unary.op == TOKEN_PLUS || expr->as.unary.op == TOKEN_MINUS) &&
            ir_expression_is_direct_float_transport_root(ctx, expr)) {
            if (expr->as.unary.op == TOKEN_PLUS) {
                return ir_lower_expression(ctx, expr->as.unary.operand, out_value);
            }
            if (ir_expression_is_signed_float_literal(expr)) {
                return ir_lower_signed_float_literal_expression(expr, out_value);
            }
            if (!ir_lower_expression(ctx, expr->as.unary.operand, &operand)) {
                return 0;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_binary(ctx,
                *out_value,
                IR_BINARY_BIT_XOR,
                operand,
                ir_value_immediate(0x80000000LL));
        }
        if (ir_expression_is_signed_float_literal(expr)) {
            return ir_lower_signed_float_literal_expression(expr, out_value);
        }
        if (expr->as.unary.op == TOKEN_PLUS_PLUS || expr->as.unary.op == TOKEN_MINUS_MINUS) {
            IrBinaryOp update_op = expr->as.unary.op == TOKEN_PLUS_PLUS
                ? IR_BINARY_ADD
                : IR_BINARY_SUB;
            IrValueRef current_value;

            if (!ir_lower_scalar_lvalue(ctx, expr->as.unary.operand, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-014: prefix update target is not a supported scalar lvalue");
                return 0;
            }

            if (!ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, &current_value)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            if (!ir_emit_binary(ctx,
                    *out_value,
                    update_op,
                    current_value,
                    ir_value_immediate(1)) ||
                !ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, *out_value)) {
                return 0;
            }
            return 1;
        }
        if (expr->as.unary.op == TOKEN_PLUS) {
            return ir_lower_expression(ctx, expr->as.unary.operand, out_value);
        }
        if (expr->as.unary.op == TOKEN_MINUS) {
            if (!ir_lower_expression(ctx, expr->as.unary.operand, &operand)) {
                return 0;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_binary(ctx,
                *out_value,
                IR_BINARY_SUB,
                ir_value_immediate(0),
                operand);
        }
        if (expr->as.unary.op == TOKEN_TILDE) {
            if (!ir_lower_expression(ctx, expr->as.unary.operand, &operand)) {
                return 0;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_binary(ctx,
                *out_value,
                IR_BINARY_BIT_XOR,
                operand,
                ir_value_immediate(-1));
        }
        if (expr->as.unary.op == TOKEN_BANG) {
            return ir_lower_logical_value_expression(ctx, expr, out_value);
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-003: unary operator '%s' is not supported in IR v1",
            lexer_token_type_name(expr->as.unary.op));
        return 0;
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_COMMA) {
            if (!ir_lower_expression(ctx, expr->as.binary.left, &lhs)) {
                return 0;
            }
            return ir_lower_expression(ctx, expr->as.binary.right, out_value);
        }

        if (expr->as.binary.op == TOKEN_ASSIGN) {
            const AstExpression *lhs_expr = expr->as.binary.left;
            size_t pair_lhs_first_local_id = (size_t)-1;
            size_t pair_lhs_second_local_id = (size_t)-1;
            size_t struct_lhs_first_local_id = (size_t)-1;
            size_t struct_lhs_second_local_id = (size_t)-1;
            const char *struct_lhs_type_name = NULL;
            IrLowerBindingInfo lhs_binding_info;

            if (ir_lookup_pair_local_binding(ctx,
                    lhs_expr,
                    &pair_lhs_first_local_id,
                    &pair_lhs_second_local_id)) {
                if (!ir_resolve_local_aggregate_binding(ctx, lhs_expr, &lhs_binding_info) ||
                    !ir_lower_initialize_aggregate_slots(ctx,
                        IR_LOWER_BINDING_PAIR_LOCAL,
                        NULL,
                        lhs_binding_info.local_id,
                        2u,
                        expr->as.binary.right,
                        expr->line,
                        expr->column)) {
                    if (!ctx->error->message[0]) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-LOWER-047: pair assignment requires a local pair or initializer list");
                    }
                    return 0;
                }
                *out_value = ir_value_immediate(0);
                return 1;
            }
            if (ir_lookup_struct_local_binding(ctx,
                    lhs_expr,
                    &struct_lhs_first_local_id,
                    &struct_lhs_second_local_id,
                    &struct_lhs_type_name)) {
                if (!ir_resolve_local_aggregate_binding(ctx, lhs_expr, &lhs_binding_info) ||
                    !ir_lower_initialize_aggregate_slots(ctx,
                        IR_LOWER_BINDING_STRUCT_LOCAL,
                        struct_lhs_type_name,
                        lhs_binding_info.local_id,
                        lhs_binding_info.aggregate_slot_count,
                        expr->as.binary.right,
                        expr->line,
                        expr->column)) {
                    if (!ctx->error->message[0]) {
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-LOWER-053: struct assignment requires a local struct or initializer list");
                    }
                    return 0;
                }
                *out_value = ir_value_immediate(0);
                return 1;
            }

            if (!ir_lower_scalar_lvalue(ctx, lhs_expr, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-004: assignment target is not a supported scalar lvalue");
                return 0;
            }
            if (!ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
                return 0;
            }
            *out_value = rhs;
            return ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, rhs);
        }

        op = ir_binary_op_from_assignment_token(expr->as.binary.op, &op_supported);
        if (op_supported) {
            IrValueRef current_value;

            if (!ir_lower_scalar_lvalue(ctx, expr->as.binary.left, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-015: compound assignment target is not a supported scalar lvalue");
                return 0;
            }
            if (!ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, &current_value)) {
                return 0;
            }
            if (!ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            lhs = current_value;
            *out_value = ir_value_temp(temp_id);
            if (!ir_emit_binary(ctx, *out_value, op, lhs, rhs) ||
                !ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, *out_value)) {
                return 0;
            }
            return 1;
        }

        if (expr->as.binary.op == TOKEN_AND_AND || expr->as.binary.op == TOKEN_OR_OR) {
            return ir_lower_logical_value_expression(ctx, expr, out_value);
        }
        if (ir_expression_is_float_arithmetic(ctx, expr)) {
            return ir_lower_float_arithmetic_expression(ctx, expr, out_value);
        }
        if (ir_expression_is_float_direct_compare(ctx, expr)) {
            return ir_lower_float_compare_expression(ctx, expr, out_value);
        }

        op = ir_binary_op_from_token(expr->as.binary.op, &op_supported);
        if (!op_supported) {
            ir_set_error(ctx->error,
                expr->line,
                expr->column,
                "IR-LOWER-005: binary operator '%s' is not supported in IR v1",
                lexer_token_type_name(expr->as.binary.op));
            return 0;
        }

        if (!ir_lower_expression(ctx, expr->as.binary.left, &lhs) ||
            !ir_lower_expression(ctx, expr->as.binary.right, &rhs)) {
            return 0;
        }

        temp_id = ir_allocate_temp(ctx->function);
        *out_value = ir_value_temp(temp_id);
        return ir_emit_binary(ctx, *out_value, op, lhs, rhs);
    case AST_EXPR_POSTFIX:
        if (expr->as.postfix.op == TOKEN_PLUS_PLUS || expr->as.postfix.op == TOKEN_MINUS_MINUS) {
            IrBinaryOp update_op = expr->as.postfix.op == TOKEN_PLUS_PLUS
                ? IR_BINARY_ADD
                : IR_BINARY_SUB;
            IrValueRef original_value;
            IrValueRef updated_value;
            IrValueRef current_value;

            if (!ir_lower_scalar_lvalue(ctx, expr->as.postfix.operand, &scalar_lvalue)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-016: postfix update target is not a supported scalar lvalue");
                return 0;
            }

            if (!ir_emit_scalar_lvalue_load(ctx, &scalar_lvalue, &current_value)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            original_value = ir_value_temp(temp_id);
            if (!ir_emit_mov(ctx, original_value, current_value)) {
                return 0;
            }

            temp_id = ir_allocate_temp(ctx->function);
            updated_value = ir_value_temp(temp_id);
            if (!ir_emit_binary(ctx,
                    updated_value,
                    update_op,
                    current_value,
                    ir_value_immediate(1)) ||
                !ir_emit_scalar_lvalue_store(ctx, &scalar_lvalue, updated_value)) {
                return 0;
            }

            *out_value = original_value;
            return 1;
        }
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-017: postfix operator '%s' is not supported in IR v1",
            lexer_token_type_name(expr->as.postfix.op));
        return 0;
    case AST_EXPR_SUBSCRIPT:
        {
            IrLowerArrayAddress array_address;

            if (!ir_lower_subscript_address(ctx, expr, &array_address)) {
                ir_set_error(ctx->error,
                    expr->line,
                    expr->column,
                    "IR-LOWER-018: array subscript expressions are not supported in IR v1 yet");
                return 0;
            }
            if (array_address.array_rank > 0u) {
                *out_value = array_address.address;
                return 1;
            }
            temp_id = ir_allocate_temp(ctx->function);
            *out_value = ir_value_temp(temp_id);
            return ir_emit_load_indirect(ctx, *out_value, array_address.address);
        }
    default:
        ir_set_error(ctx->error,
            expr->line,
            expr->column,
            "IR-LOWER-006: expression kind '%s' is not supported in IR v1",
            ast_expression_kind_name(expr->kind));
        return 0;
    }
}

#endif
