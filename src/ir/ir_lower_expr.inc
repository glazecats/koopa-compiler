#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static int ir_lower_scope_clear_mutated_scalar_locals_in_expr(IrLowerScopeStack *stack,
    const AstExpression *expr);
static int ir_expression_is_direct_float_condition(const IrLowerContext *ctx,
    const AstExpression *expr);
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
static int ir_try_emit_static_direct_target_callable_dispatch(IrLowerContext *ctx,
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
    if (out_owned_dynamic_target_names) {
        *out_owned_dynamic_target_names = out_view->dynamic_target_names;
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

    if (out_info) {
        memset(out_info, 0, sizeof(*out_info));
        out_info->then_tag_local_id = (size_t)-1;
        out_info->else_tag_local_id = (size_t)-1;
    }
    if (!ctx || !base || base->kind != AST_EXPR_TERNARY || !out_info) {
        return 0;
    }

    out_info->then_base = ir_unwrap_paren_expression(base->as.ternary.then_expr);
    out_info->else_base = ir_unwrap_paren_expression(base->as.ternary.else_expr);
    if (!out_info->then_base || !out_info->else_base ||
        out_info->then_base->kind != AST_EXPR_IDENTIFIER ||
        out_info->else_base->kind != AST_EXPR_IDENTIFIER) {
        return 0;
    }

    return ir_resolve_function_value_initializer_binding(
               ctx,
               out_info->then_base,
               &out_info->then_target_name,
               &out_info->then_capture_local_ids,
               &out_info->then_capture_count,
               &out_info->then_tag_local_id,
               &out_info->then_has_const_value,
               &out_info->then_const_value,
               &out_info->then_is_closure_local) &&
        ir_resolve_function_value_initializer_binding(
               ctx,
               out_info->else_base,
               &out_info->else_target_name,
               &out_info->else_capture_local_ids,
               &out_info->else_capture_count,
               &out_info->else_tag_local_id,
               &out_info->else_has_const_value,
               &out_info->else_const_value,
               &out_info->else_is_closure_local);
}

static int ir_emit_materialized_ternary_function_value_selection(IrLowerContext *ctx,
    const AstExpression *condition_expr,
    long long then_tag_value,
    long long else_tag_value,
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
            ir_value_immediate(then_tag_value))) {
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
            ir_value_immediate(else_tag_value))) {
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
                    ir_value_immediate(ternary_info->then_const_value),
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
                    ir_value_immediate(ternary_info->else_const_value),
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
    if (!ctx || !call_expr || !out_args || !out_args_view || !out_arg_count ||
        call_expr->kind != AST_EXPR_CALL ||
        function_arg_index >= call_expr->as.call.arg_count) {
        return 0;
    }

    if (call_expr->as.call.arg_count > 0u) {
        visible_arg_count = call_expr->as.call.arg_count - 1u;
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
        if (source_arg_index == function_arg_index) {
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

static int ir_build_dynamic_function_value_specialization_forward_args(
    IrLowerContext *ctx,
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
        !out_dynamic_forward_arg_count) {
        return 0;
    }
    if (dynamic_callable_view->capture_count == 0u) {
        return 1;
    }

    dynamic_forward_arg_count = dynamic_callable_view->capture_count + callable_arg_count;
    dynamic_forward_args = (IrValueRef *)malloc(dynamic_forward_arg_count * sizeof(IrValueRef));
    if (!dynamic_forward_args) {
        ir_set_error(ctx->error,
            line,
            column,
            "IR-INT-029: out of memory while preparing dynamic returned closure forwarding args");
        return 0;
    }
    for (size_t i = 0u; i < dynamic_callable_view->capture_count; ++i) {
        dynamic_forward_args[i] = ir_value_local(dynamic_callable_view->capture_local_ids[i]);
    }
    for (size_t i = 0u; i < callable_arg_count; ++i) {
        dynamic_forward_args[dynamic_callable_view->capture_count + i] = call_args[i];
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
            if (!ir_call_argument_resolve_extension_function_value_target(
                    ctx->ast_program,
                    ctx,
                    callee_external,
                    i,
                    call_args[i],
                    &bound_capture_local_ids[i],
                    &bound_capture_counts[i],
                    &bound_targets[i])) {
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

    if (!ir_call_argument_resolve_extension_function_value_target(
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

static int ir_emit_two_target_single_capture_closure_callable_indirect_call(IrLowerContext *ctx,
    IrValueRef tag_value,
    long long then_tag_value,
    const char *then_target_name,
    const char *else_target_name,
    IrValueRef env_value,
    const IrValueRef *args,
    size_t arg_count,
    IrValueRef *out_value) {
    size_t shape_id;

    if (out_value) {
        *out_value = ir_value_immediate(0);
    }
    if (!ctx || !ctx->program || !then_target_name || !else_target_name || !out_value ||
        (arg_count > 0u && !args)) {
        return 0;
    }

    if (!ir_build_two_target_callable_shape_bridge_from_names(
            ctx,
            then_target_name,
            else_target_name,
            1u,
            "IR-LOWER-071: returned callable-wrapper dispatch bridge shape mismatch",
            &shape_id)) {
        return 0;
    }

    if (!ir_emit_two_target_callable_object_indirect_call_bridged(
            ctx,
            tag_value,
            then_tag_value,
            then_target_name,
            else_target_name,
            env_value,
            env_value,
            shape_id,
            args,
            arg_count,
            out_value)) {
        ir_set_error(ctx->error, 0, 0, "IR-INT-094: failed while lowering callable dispatch bridge");
        return 0;
    }
    return 1;
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

static int ir_try_emit_static_returned_single_capture_closure_callable_indirect_call(IrLowerContext *ctx,
    const AstExpression *function_arg_expr,
    size_t non_function_arg_count,
    size_t non_function_arg_index,
    AstFunctionReturnType expected_return_type,
    const AstExpression *call_expr,
    IrValueRef *out_value,
    int *out_applied) {
    IrLowerFunctionObjectDescriptor arg_descriptor;
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
    if (!ir_resolve_function_value_descriptor(ctx, function_arg_expr, &arg_descriptor) ||
        !arg_descriptor.is_closure_family ||
        arg_descriptor.has_dynamic_family ||
        !arg_descriptor.direct_target_name ||
        arg_descriptor.direct_target_name[0] == '\0' ||
        !strstr(arg_descriptor.direct_target_name, "__retclosure_") ||
        arg_descriptor.capture_count != 1u ||
        !arg_descriptor.capture_local_ids) {
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
        return 0;
    }
    if (!ir_try_emit_static_single_capture_closure_callable_indirect_call_with_args(
            ctx,
            arg_descriptor.direct_target_name,
            ir_value_local(arg_descriptor.capture_local_ids[0]),
            call_args_view,
            callable_arg_count,
            &result_value,
            &applied)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    if (!applied) {
        return 1;
    }
    return ir_finish_void_or_value_callable_dispatch_result(
        expected_return_type == AST_FUNCTION_RETURN_VOID,
        result_value,
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
    if (!ir_emit_two_target_single_capture_closure_callable_indirect_call(
            ctx,
            tag_value,
            then_tag_value,
            then_target_name,
            else_target_name,
            ir_value_local(capture_local_ids[0]),
            call_args_view,
            callable_arg_count,
            &result_value)) {
        free(call_args);
        return 0;
    }
    free(call_args);
    return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
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

    if (!ir_try_emit_dynamic_single_capture_closure_callable_indirect_call(
            ctx,
            ir_value_local(callee_view->dynamic_tag_local_id),
            1,
            callee_view->dynamic_target_names[0],
            callee_view->dynamic_target_names[1],
            callee_view->capture_local_ids,
            callee_view->capture_count,
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
        callee_view->capture_count == 1u &&
        callee_view->capture_local_ids) {
        if (!ir_emit_two_target_single_capture_closure_callable_indirect_call(
                ctx,
                ir_value_local(callee_view->dynamic_tag_local_id),
                1,
                callee_view->dynamic_target_names[0],
                callee_view->dynamic_target_names[1],
                ir_value_local(callee_view->capture_local_ids[0]),
                args,
                arg_count,
                out_value)) {
            return 0;
        }
        return ir_mark_callable_dispatch_applied(out_applied);
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

    if (out_applied) {
        *out_applied = 0;
    }
    if (!ctx || !ctx->program || !call_expr || !out_value || call_expr->kind != AST_EXPR_CALL) {
        return 1;
    }

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
    if (ir_lower_lookup_function_value_target(ctx, callee_name)) {
        return 1;
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

    if (callee_external && callee_external->parameter_value_kinds) {
        for (i = 0u; i < callee_external->parameter_count; ++i) {
            if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
                return 1;
            }
        }
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
            payload_local_ids &&
            callee_external->function_return_type != AST_FUNCTION_RETURN_VOID) {
            if (!ir_emit_two_target_single_capture_closure_callable_indirect_call(
                    ctx,
                    ir_value_local(tag_local_id),
                    then_tag_value,
                    ternary_info.then_target_name,
                    ternary_info.else_target_name,
                    ir_value_local(payload_local_ids[0]),
                    forwarded_scalar_args_view,
                    forwarded_scalar_arg_count,
                    out_value)) {
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
    if (!ir_try_emit_static_single_capture_closure_callable_indirect_call_with_args(
            ctx,
            target_name,
            ir_value_local(capture_local_ids[0]),
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

    if (callee_view->is_closure_family) {
        if (callee_view->capture_count > 1u && callee_view->capture_local_ids) {
            const char *wrapper_name = NULL;
            IrValueRef *call_args = NULL;
            const IrValueRef *call_args_view = NULL;
            size_t callable_arg_count = 0u;
            size_t shape_id = (size_t)-1;
            size_t env_first_local_id = (size_t)-1;
            IrValueRef result_value;
            IrValueRef env_addr = ir_value_immediate(0);
            size_t capture_index;

            if (!ir_ensure_closure_callable_wrapper_function(
                    ctx,
                    callee_view->direct_target_name,
                    callee_view->capture_count,
                    &wrapper_name)) {
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
            for (capture_index = 0u; capture_index < callee_view->capture_count; ++capture_index) {
                char env_local_name[64];
                size_t local_id = (size_t)-1;

                snprintf(env_local_name, sizeof(env_local_name), "__closure_env_%zu", capture_index);
                if (!ir_function_append_local(
                        ctx->function,
                        env_local_name,
                        0,
                        &local_id,
                        ctx->error)) {
                    free(call_args);
                    return 0;
                }
                if (capture_index == 0u) {
                    env_first_local_id = local_id;
                }
            }
            for (capture_index = 0u; capture_index < callee_view->capture_count; ++capture_index) {
                if (!ir_emit_mov(
                        ctx,
                        ir_value_local(env_first_local_id + capture_index),
                        ir_value_local(callee_view->capture_local_ids[capture_index]))) {
                    free(call_args);
                    return 0;
                }
            }
            env_addr = ir_value_temp(ir_allocate_temp(ctx->function));
            if (!ir_emit_addr_local(ctx, env_addr, env_first_local_id)) {
                free(call_args);
                return 0;
            }
            if (!ir_build_single_target_callable_shape_bridge_from_name(
                    ctx,
                    wrapper_name,
                    1u,
                    callable_arg_count,
                    &shape_id,
                    NULL)) {
                free(call_args);
                return 1;
            }
            if (!ir_emit_callable_object_indirect_call(
                    ctx,
                    wrapper_name,
                    env_addr,
                    shape_id,
                    call_args_view,
                    callable_arg_count,
                    &result_value)) {
                free(call_args);
                return 0;
            }
            free(call_args);
            return ir_finish_callable_dispatch_result(result_value, out_value, out_applied);
        }
        return ir_try_emit_static_single_capture_closure_callable_indirect_call(
            ctx,
            callee_view->direct_target_name,
            callee_view->capture_local_ids,
            callee_view->capture_count,
            call_expr,
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
    if (ir_prepare_dynamic_function_value_specialization_dispatch(
            dynamic_callable_view,
            owned_dynamic_target_names,
            requires_materialized_capture_forwarding)) {
        return 1;
    }

    if (!ir_build_dynamic_non_function_callable_args_view(
            ctx,
            call_expr,
            function_arg_index,
            "IR-INT-126: out of memory while lowering callable-object arguments",
            &call_args,
            &call_args_view,
            &callable_arg_count)) {
        free((void *)owned_dynamic_target_names);
        return 0;
    }
    forwarded_visible_arg_count = callable_arg_count;

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

    if (requires_materialized_capture_forwarding &&
        dynamic_callable_view->capture_count > 0u &&
        !ir_build_dynamic_function_value_specialization_forward_args(
            ctx,
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
    int *out_handled) {
    const char *returned_callee_name = NULL;
    const AstExternal *return_function_external = NULL;
    IrLowerReturnedFunctionObjectResolution returned_resolution;
    int have_returned_resolution = 0;
    const size_t *returned_capture_local_ids = NULL;
    size_t returned_capture_count = 0u;
    const char *returned_arg_target_name = NULL;
    int returned_is_static_single_capture_closure_local = 0;

    if (out_handled) {
        *out_handled = 0;
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
        return 1;
    }

    if (!ir_try_resolve_returned_function_object_resolution(
            ctx,
            callee_base_expr,
            "__retfn_callcap",
            "__retclosure_callcap",
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
            return ir_finish_returned_callable_capture_cleanup_applied(
                io_immediate_returned_closure_capture_local_ids,
                out_handled);
        }
        return 1;
    }

    {
        IrLowerCallableObjectView returned_view;
        IrValueRef call_args[1];
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
                return ir_mark_callable_dispatch_applied(out_handled);
            }
            ir_set_error(ctx->error,
                call_expr->line,
                call_expr->column,
                "IR-INT-101: failed while materializing returned noncapturing callable view");
            return 0;
        }
        if (!ir_build_required_single_visible_callable_args_view(
                ctx,
                call_expr,
                "IR-LOWER-067: first returned dynamic noncapturing function-value slice supports one-argument calls only",
                call_args,
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
            free((void *)returned_resolution.descriptor.dynamic_target_names);
            return 0;
        }
        free((void *)returned_resolution.descriptor.dynamic_target_names);
        return ir_finish_callable_dispatch_result(result_value, out_value, out_handled);
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
        (!resolved_callee_view->has_dynamic_family ||
            resolved_callee_view->dynamic_tag_local_id == (size_t)-1)) {
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

    if (!ctx || !base || base->kind != AST_EXPR_CALL || !ctx->program || !ctx->ast_program) {
        return 0;
    }
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
    if (!ir_function_returned_closure_payload_slot_count(
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
        !ir_try_get_returned_function_value_parameter_argument_expr(
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
    } else if (base_expr->kind != AST_EXPR_CALL &&
            base_expr->kind != AST_EXPR_TERNARY) {
        payload_slot_count = is_closure_return
            ? (base_expr->kind == AST_EXPR_CLOSURE ? base_expr->as.closure.capture_name_count : 0u)
            : 1u;
    }

    out_resolution->descriptor.is_closure_family = is_closure_return ? 1 : 0;
    out_resolution->descriptor.payload_slot_count = payload_slot_count;
    out_resolution->payload_slot_count = payload_slot_count;
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
    const AstExternal *producer_external = NULL;
    IrLowerFunctionObjectDescriptor descriptor;

    if (out_payload_slot_count) {
        *out_payload_slot_count = 0u;
    }
    if (!ctx || !stmt || !base_expr || base_expr->kind != AST_EXPR_CALL ||
        !io_is_closure_return || !out_payload_slot_count || !out_resolution) {
        return 0;
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

        ir_set_error(ctx->error,
            stmt->line,
            stmt->column,
            "IR-LOWER-066: returned noncapturing function value requires transport-resolved tag payload");
        return 0;
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
    if (base_expr->kind == AST_EXPR_IDENTIFIER &&
        base_expr->as.identifier.name &&
        return_resolution->has_identifier_binding_payload) {
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
            const char *then_target_name = NULL;
            const char *else_target_name = NULL;
            const size_t *then_capture_local_ids = NULL;
            const size_t *else_capture_local_ids = NULL;
            size_t then_capture_count = 0u;
            size_t else_capture_count = 0u;
            int target_discovery_only = out_target_name != NULL && !out_capture_local_ids && !out_capture_count;

            if (!then_base || !else_base ||
                then_base->kind != AST_EXPR_IDENTIFIER ||
                else_base->kind != AST_EXPR_IDENTIFIER ||
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
        } else if (have_arg_view &&
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

static int ir_lower_try_emit_dynamic_function_value_specialized_call(IrLowerContext *ctx,
    const AstExpression *call_expr,
    const AstExternal *callee_external,
    const char *callee_name,
    IrValueRef *out_value,
    int *out_applied) {
    size_t function_arg_index = (size_t)-1;
    size_t non_function_arg_index = (size_t)-1;
    size_t non_function_arg_count = 0u;
    IrLowerCallableObjectView dynamic_callable_view;
    const char *const *owned_dynamic_target_names = NULL;
    long long dynamic_then_tag_value = 1;
    int requires_materialized_capture_forwarding = 0;

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

    for (size_t i = 0u; i < callee_external->parameter_count; ++i) {
        if (callee_external->parameter_value_kinds[i] == AST_PARAMETER_VALUE_FUNCTION) {
            if (function_arg_index != (size_t)-1) {
                return 1;
            }
            function_arg_index = i;
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

    if (!ir_lower_emit_function_value_wrapper_side_effects(
            ctx,
            call_expr->as.call.args[function_arg_index])) {
        free((void *)owned_dynamic_target_names);
        return 0;
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
            IrValueRef compare_value;
            IrValueRef result_value;
            size_t result_then_block_id;
            size_t result_else_block_id;
            size_t result_join_block_id;
            size_t result_temp_id;
            IrValueRef *dynamic_args = NULL;
            size_t expected_arg_count = 0u;
            size_t capture_index;

            if (ir_resolve_identifier_ternary_function_value_info(ctx, callee_base_expr, &ternary_info) &&
                ternary_info.then_target_name && ternary_info.else_target_name &&
                ternary_info.then_capture_count == ternary_info.else_capture_count) {
                callee_is_closure_local = ternary_info.then_is_closure_local;
                expected_arg_count = ternary_info.then_capture_count + expr->as.call.arg_count;
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
                if (expected_arg_count > 0u) {
                    dynamic_args = (IrValueRef *)malloc(expected_arg_count * sizeof(IrValueRef));
                    if (!dynamic_args) {
                        free(payload_local_ids);
                        ir_set_error(ctx->error,
                            expr->line,
                            expr->column,
                            "IR-INT-027: out of memory while lowering ternary callee call arguments");
                        return 0;
                    }
                }
                for (capture_index = 0u; capture_index < ternary_info.then_capture_count; ++capture_index) {
                    dynamic_args[capture_index] = ir_value_local(payload_local_ids[capture_index]);
                }
                for (i = 0u; i < expr->as.call.arg_count; ++i) {
                    if (!ir_lower_expression(
                            ctx,
                            expr->as.call.args[i],
                            &dynamic_args[ternary_info.then_capture_count + i])) {
                        free(dynamic_args);
                        free(payload_local_ids);
                        return 0;
                    }
                }

                if (ternary_info.then_is_closure_local &&
                    ternary_info.then_capture_count == 1u &&
                    payload_local_ids) {
                    if (!ir_emit_two_target_single_capture_closure_callable_indirect_call(
                            ctx,
                            ir_value_local(tag_local_id),
                            ternary_info.then_const_value,
                            ternary_info.then_target_name,
                            ternary_info.else_target_name,
                            ir_value_local(payload_local_ids[0]),
                            expr->as.call.arg_count > 0u ? &dynamic_args[1] : NULL,
                            expr->as.call.arg_count,
                            out_value)) {
                        free(dynamic_args);
                        free(payload_local_ids);
                        return 0;
                    }
                    free(dynamic_args);
                    free(payload_local_ids);
                    return 1;
                }

                compare_value = ir_value_temp(ir_allocate_temp(ctx->function));
                result_temp_id = ir_allocate_temp(ctx->function);
                result_value = ir_value_temp(result_temp_id);
                if (!ir_emit_binary(ctx,
                        compare_value,
                        IR_BINARY_EQ,
                        ir_value_local(tag_local_id),
                        ir_value_immediate(ternary_info.then_const_value)) ||
                    !ir_function_append_block(ctx->function, &result_then_block_id, NULL, ctx->error) ||
                    !ir_function_append_block(ctx->function, &result_else_block_id, NULL, ctx->error) ||
                    !ir_block_set_branch(ir_lower_current_block(ctx),
                        compare_value,
                        result_then_block_id,
                        result_else_block_id,
                        ctx->error) ||
                    !ir_function_append_block(ctx->function, &result_join_block_id, NULL, ctx->error)) {
                    free(dynamic_args);
                    free(payload_local_ids);
                    return 0;
                }

                ctx->block_id = result_then_block_id;
                ctx->has_block = 1;
                if (!ir_emit_call(ctx,
                        result_value,
                        ternary_info.then_target_name,
                        dynamic_args,
                        expected_arg_count) ||
                    !ir_block_set_jump(ir_lower_current_block(ctx), result_join_block_id, ctx->error)) {
                    free(dynamic_args);
                    free(payload_local_ids);
                    return 0;
                }

                ctx->block_id = result_else_block_id;
                ctx->has_block = 1;
                if (!ir_emit_call(ctx,
                        result_value,
                        ternary_info.else_target_name,
                        dynamic_args,
                        expected_arg_count) ||
                    !ir_block_set_jump(ir_lower_current_block(ctx), result_join_block_id, ctx->error)) {
                    free(dynamic_args);
                    free(payload_local_ids);
                    return 0;
                }

                ctx->block_id = result_join_block_id;
                ctx->has_block = 1;
                free(dynamic_args);
                free(payload_local_ids);
                return ir_finish_callable_bridge_result_value(result_value, out_value);
            }
        }

        {
            int handled_returned_call_callee = 0;

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
                    &handled_returned_call_callee)) {
                return 0;
            }
            if (handled_returned_call_callee) {
                return 1;
            }
        }
        if (!callee_name || callee_name[0] == '\0') {
            callee_name = ir_extract_direct_call_callee_name(expr->as.call.callee);
        }

        if (!callee_is_closure_local) {
            int handled_dynamic_dispatch = 0;

            if (ir_try_resolve_direct_callee_callable_object_view(
                    ctx,
                    expr->as.call.callee,
                    &direct_callee_view)) {
                have_direct_callee_view = 1;
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
            const char *callable_callee_name = NULL;
            IrLowerBindingInfo callable_binding_info;

            memset(&callable_binding_info, 0, sizeof(callable_binding_info));
            if ((callee_base_expr && callee_base_expr->kind == AST_EXPR_CALL) ||
                callee_is_closure_local ||
                callee_from_static_returned_parameter_closure) {
                should_try_static_direct_target_dispatch = 1;
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
                    (callee_base_expr && callee_base_expr->kind == AST_EXPR_CALL))) {
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
            int callee_has_function_value_parameters = 0;
            int body_needs_function_value_specialization = 0;

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
                        break;
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

            if (callee_external &&
                callee_external->parameter_value_kinds) {
                if (!ir_lower_try_emit_dynamic_function_value_specialized_call(
                        ctx,
                        expr,
                        callee_external,
                        callee_name,
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
