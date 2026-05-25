#include "ir.h"

#include <stdint.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    IR_LOWER_BINDING_SCALAR_LOCAL = 0,
    IR_LOWER_BINDING_ARRAY_LOCAL,
    IR_LOWER_BINDING_ARRAY_PARAMETER,
    IR_LOWER_BINDING_PAIR_LOCAL,
    IR_LOWER_BINDING_STRUCT_LOCAL,
} IrLowerBindingKind;

typedef struct {
    const char *name;
    size_t local_id;
    IrLowerBindingKind kind;
    size_t array_rank;
    size_t *array_extents;
    size_t pair_second_local_id;
    const char *type_name;
    int is_const;
    int has_const_value;
    long long const_value;
} IrLowerBinding;

typedef struct {
    IrLowerBinding *bindings;
    size_t count;
    size_t capacity;
} IrLowerScope;

typedef struct {
    size_t local_id;
    IrLowerBindingKind kind;
    size_t array_rank;
    const size_t *array_extents;
    size_t pair_second_local_id;
    const char *type_name;
    int is_const;
    int has_const_value;
    long long const_value;
} IrLowerBindingInfo;

typedef struct {
    IrLowerScope *frames;
    size_t count;
    size_t capacity;
} IrLowerScopeStack;

typedef struct {
    size_t break_target;
    size_t continue_target;
    size_t defer_scope_depth;
} IrLoopTarget;

typedef struct {
    IrLoopTarget *items;
    size_t count;
    size_t capacity;
} IrLoopTargetStack;

typedef struct {
    const AstStatement *payload_stmt;
    const char *const *capture_names;
    const size_t *capture_local_ids;
    size_t capture_count;
    int has_runtime_repeat_count;
    size_t runtime_repeat_count_local_id;
} IrDeferEntry;

typedef struct {
    IrDeferEntry *items;
    size_t count;
    size_t capacity;
} IrDeferEntryStack;

typedef struct {
    size_t defer_base;
} IrDeferScopeEntry;

typedef struct {
    IrDeferScopeEntry *items;
    size_t count;
    size_t capacity;
} IrDeferScopeStack;

typedef struct {
    IrDeferEntry *items;
    size_t count;
    size_t capacity;
} IrFunctionDeferEntryStack;

typedef struct {
    IrProgram *program;
    const AstProgram *ast_program;
    IrFunction *function;
    AstFunctionReturnType function_return_type;
    int allow_implicit_fallthrough_return;
    size_t block_id;
    int has_block;
    IrLowerScopeStack scopes;
    IrLoopTargetStack loop_targets;
    IrDeferEntryStack defer_entries;
    IrFunctionDeferEntryStack function_defer_entries;
    IrDeferScopeStack defer_scopes;
    const AstStatement **fndefer_sites;
    size_t *fndefer_counter_local_ids;
    size_t fndefer_site_count;
    const char **function_value_parameter_names;
    const char *const *function_value_parameter_targets;
    size_t function_value_parameter_count;
    IrError *error;
} IrLowerContext;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} IrStringBuilder;

static long long ir_normalize_sysy_int_value(long long value) {
    uint32_t bits = (uint32_t)value;
    return (long long)(int32_t)bits;
}

static void ir_lower_scope_stack_free(IrLowerScopeStack *stack);
static int ir_lower_scope_stack_push(IrLowerScopeStack *stack);
static void ir_lower_scope_stack_pop(IrLowerScopeStack *stack);
static int ir_lower_scope_add_binding_with_metadata(IrLowerScopeStack *stack,
    const char *name,
    size_t local_id,
    IrLowerBindingKind kind,
    size_t array_rank,
    const size_t *array_extents,
    size_t pair_second_local_id,
    const char *type_name,
    int is_const,
    int has_const_value,
    long long const_value);
static int ir_lower_scope_lookup(const IrLowerScopeStack *stack,
    const char *name,
    size_t *out_local_id);
static int ir_lower_scope_lookup_binding(const IrLowerScopeStack *stack,
    const char *name,
    IrLowerBindingInfo *out_info);
static int ir_lower_scope_update_binding_const_value_by_local_id(IrLowerScopeStack *stack,
    size_t local_id,
    int has_const_value,
    long long const_value);
static int ir_lower_scope_stack_clone(const IrLowerScopeStack *src,
    IrLowerScopeStack *dest);
static int ir_lower_scope_stack_merge_const_facts(IrLowerScopeStack *dest,
    const IrLowerScopeStack *lhs,
    const IrLowerScopeStack *rhs);
static void ir_lower_defer_entry_stack_free(IrDeferEntryStack *stack);
static int ir_lower_defer_entry_stack_push(IrDeferEntryStack *stack, const AstStatement *payload_stmt);
static int ir_lower_defer_entry_stack_push_with_captures(IrDeferEntryStack *stack,
    const AstStatement *payload_stmt,
    const char *const *capture_names,
    const size_t *capture_local_ids,
    size_t capture_count);
static void ir_lower_defer_entry_stack_pop_to(IrDeferEntryStack *stack, size_t new_count);
static void ir_lower_function_defer_entry_stack_free(IrFunctionDeferEntryStack *stack);
static int ir_lower_function_defer_entry_stack_push_with_captures(IrFunctionDeferEntryStack *stack,
    const AstStatement *payload_stmt,
    const char *const *capture_names,
    const size_t *capture_local_ids,
    size_t capture_count);
static int ir_lower_function_defer_entry_stack_push_with_runtime_repeat_count(
    IrFunctionDeferEntryStack *stack,
    const AstStatement *payload_stmt,
    size_t runtime_repeat_count_local_id);
static void ir_lower_defer_scope_stack_free(IrDeferScopeStack *stack);
static int ir_lower_defer_scope_stack_push(IrDeferScopeStack *stack, size_t defer_base);
static void ir_lower_defer_scope_stack_pop(IrDeferScopeStack *stack);
static int ir_lower_emit_defer_entries(IrLowerContext *ctx, size_t begin_index);
static int ir_lower_emit_defer_entries_range(IrLowerContext *ctx, size_t begin_index, size_t end_index);
static int ir_lower_emit_function_defer_entries(IrLowerContext *ctx);
static int ir_lower_emit_function_defer_repeat_loop(IrLowerContext *ctx,
    size_t repeat_count_local_id,
    const AstStatement *payload_stmt);
static int ir_collect_fndefer_sites(const AstStatement *stmt,
    const AstStatement ***out_sites,
    size_t *out_count,
    size_t *out_capacity,
    IrError *error);
static int ir_lower_lookup_fndefer_site_counter_local_id(const IrLowerContext *ctx,
    const AstStatement *stmt,
    size_t *out_local_id);
static int ir_lower_emit_fndefer_site_registration(IrLowerContext *ctx,
    const AstStatement *stmt,
    size_t *out_repeat_count_local_id);

static void ir_basic_block_free(IrBasicBlock *block);
static void ir_function_free(IrFunction *function);
static void ir_global_free(IrGlobal *global);
static IrFunction *ir_program_find_function_mut(IrProgram *program, const char *name);
static IrGlobal *ir_program_find_global_mut(IrProgram *program, const char *name);
static const IrGlobal *ir_program_find_global(const IrProgram *program, const char *name);
static const IrGlobal *ir_program_get_global(const IrProgram *program, size_t global_id);
static int ir_program_append_global(IrProgram *program,
    const char *name,
    IrGlobal **out_global,
    IrError *error);
static int ir_program_append_function(IrProgram *program,
    const char *name,
    IrFunction **out_function,
    IrError *error);
static int ir_function_append_local(IrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    IrError *error);
static int ir_function_append_block(IrFunction *function,
    size_t *out_block_id,
    IrBasicBlock **out_block,
    IrError *error);
static int ir_block_append_instruction(IrBasicBlock *block,
    const IrInstruction *instruction,
    IrError *error);
static int ir_block_prepend_instruction(IrBasicBlock *block,
    const IrInstruction *instruction,
    IrError *error);
static int ir_block_set_return(IrBasicBlock *block, IrValueRef value, IrError *error);
static int ir_block_set_void_return(IrBasicBlock *block, IrError *error);
static int ir_block_set_jump(IrBasicBlock *block, size_t target_block_id, IrError *error);
static int ir_block_set_branch(IrBasicBlock *block,
    IrValueRef condition,
    size_t then_target,
    size_t else_target,
    IrError *error);
static IrBasicBlock *ir_function_get_block_mut(IrFunction *function, size_t block_id);
static const IrLocal *ir_function_get_local(const IrFunction *function, size_t local_id);
static IrBasicBlock *ir_lower_current_block(IrLowerContext *ctx);
static IrValueRef ir_value_immediate(long long value);
static IrValueRef ir_value_local(size_t id);
static IrValueRef ir_value_global(size_t id);
static IrValueRef ir_value_temp(size_t id);

static int ir_append_string(IrStringBuilder *builder, const char *text);
static int ir_append_format(IrStringBuilder *builder, const char *fmt, ...);
static int ir_append_value_name(IrStringBuilder *builder,
    const IrProgram *program,
    const IrFunction *function,
    IrValueRef value);

static int ir_emit_mov(IrLowerContext *ctx,
    IrValueRef destination,
    IrValueRef value);
static int ir_emit_binary(IrLowerContext *ctx,
    IrValueRef destination,
    IrBinaryOp op,
    IrValueRef lhs,
    IrValueRef rhs);
static int ir_emit_call(IrLowerContext *ctx,
    IrValueRef destination,
    const char *callee_name,
    const IrValueRef *args,
    size_t arg_count);
static int ir_emit_addr_local(IrLowerContext *ctx,
    IrValueRef destination,
    size_t local_id);
static int ir_emit_addr_global(IrLowerContext *ctx,
    IrValueRef destination,
    size_t global_id);
static int ir_emit_load_indirect(IrLowerContext *ctx,
    IrValueRef destination,
    IrValueRef address);
static int ir_emit_store_indirect(IrLowerContext *ctx,
    IrValueRef address,
    IrValueRef value);
static int ir_eval_constant_int_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    long long *out_value);
static int ir_compute_array_extents(IrLowerContext *ctx,
    AstExpression **extent_exprs,
    size_t rank,
    int allow_unspecified_first_extent,
    size_t **out_extents,
    size_t *out_total_element_count);
static int ir_append_array_local_slots(IrLowerContext *ctx,
    const char *name,
    size_t element_count,
    size_t *out_base_local_id);
static size_t ir_array_initializer_element_count(const size_t *extents, size_t rank);
static int ir_expand_array_initializer_slots(const AstExpression *initializer_expr,
    size_t rank,
    const size_t *extents,
    const AstExpression ***out_slots,
    size_t *out_slot_count,
    IrError *error);
static int ir_try_emit_compact_large_local_array_initializer(IrLowerContext *ctx,
    size_t base_local_id,
    const AstExpression **initializer_slots,
    size_t slot_count,
    int *out_applied);
static int ir_ensure_builtin_function_signature(IrProgram *program,
    const char *name,
    IrFunction **out_function,
    IrError *error);
static size_t ir_allocate_temp(IrFunction *function);

static int ir_lower_return_statement(IrLowerContext *ctx, const AstStatement *stmt);
static IrBinaryOp ir_binary_op_from_token(TokenType token_type, int *out_supported);
static const AstExpression *ir_unwrap_paren_expression(const AstExpression *expr);
static const char *ir_extract_direct_call_callee_name(const AstExpression *callee);
static int ir_classify_direct_identifier_expr(const AstExpression *expr, const char **out_name);
static const char *ir_lower_lookup_function_value_target(const IrLowerContext *ctx, const char *name);
static const AstStructType *ir_find_struct_type(const AstProgram *program, const char *name);
static int ir_lower_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
static int ir_lower_while_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_for_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_if_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_external_body_has_function_parameter_callee_calls(const AstExternal *external,
    const AstStatement *stmt);
static int ir_external_body_needs_function_value_specialization(const AstProgram *program,
    const AstExternal *current_external,
    const AstStatement *stmt);
static int ir_builtin_function_matches_signature(const char *name,
    AstFunctionReturnType expected_return_type,
    size_t expected_parameter_count);
static int ir_lower_function_core(const AstProgram *ast_program,
    IrProgram *program,
    IrFunction *function,
    const IrLowerOptions *options,
    const AstExternal *external,
    const char *const *bound_function_targets,
    IrError *error);
static int ir_lower_function(const AstProgram *ast_program,
    IrProgram *program,
    const IrLowerOptions *options,
    const AstExternal *external,
    IrError *error);
static int ir_build_function_value_specialized_name(const AstExternal *external,
    const char *const *bound_function_targets,
    char **out_name,
    IrError *error);
static int ir_lower_specialized_function_for_function_value_call(
    const AstProgram *ast_program,
    IrProgram *program,
    const IrLowerOptions *options,
    const AstExternal *external,
    const char *const *bound_function_targets,
    const char **out_specialized_name,
    IrError *error);
static int ir_dependency_try_eval_condition_truthiness(const AstExpression *expr,
    int *out_known,
    int *out_truthy);
static int ir_collect_global_initializer_dependencies(const IrProgram *program,
    const AstProgram *ast_program,
    const AstExpression *expr,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error);
static int ir_try_format_runtime_global_dependency_path(const IrProgram *program,
    const AstProgram *ast_program,
    size_t source_global_id,
    const AstExpression *source_expr,
    size_t blocked_global_id,
    char *out_path,
    size_t out_path_size,
    int *out_via_callee);
static int ir_try_format_runtime_global_cycle(const IrProgram *program,
    const AstProgram *ast_program,
    size_t source_global_id,
    size_t blocked_global_id,
    char *out_chain,
    size_t out_chain_size);
static int ir_verify_value_ref(const IrProgram *program,
    const IrFunction *function,
    IrValueRef value,
    const char *role,
    IrError *error);
static int ir_verify_function(const IrProgram *program,
    const IrFunction *function,
    IrError *error);

static void ir_set_error(IrError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }

    error->line = line;
    error->column = column;

    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static int ir_next_growth_capacity(size_t current,
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

static void ir_lower_defer_entry_stack_free(IrDeferEntryStack *stack) {
    size_t index;

    if (!stack) {
        return;
    }

    for (index = 0u; index < stack->count; ++index) {
        free((void *)stack->items[index].capture_names);
        free((void *)stack->items[index].capture_local_ids);
    }
    free(stack->items);
    stack->items = NULL;
    stack->count = 0u;
    stack->capacity = 0u;
}

static int ir_lower_defer_entry_stack_push(IrDeferEntryStack *stack, const AstStatement *payload_stmt) {
    return ir_lower_defer_entry_stack_push_with_captures(stack, payload_stmt, NULL, NULL, 0u);
}

static int ir_lower_defer_entry_stack_push_with_captures(IrDeferEntryStack *stack,
    const AstStatement *payload_stmt,
    const char *const *capture_names,
    const size_t *capture_local_ids,
    size_t capture_count) {
    IrDeferEntry *new_items;
    size_t next_capacity;

    if (!stack || !payload_stmt) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity, 8u, sizeof(IrDeferEntry), &next_capacity)) {
            return 0;
        }
        new_items = (IrDeferEntry *)realloc(stack->items, next_capacity * sizeof(IrDeferEntry));
        if (!new_items) {
            return 0;
        }
        stack->items = new_items;
        stack->capacity = next_capacity;
    }

    stack->items[stack->count].payload_stmt = payload_stmt;
    stack->items[stack->count].capture_names = capture_names;
    stack->items[stack->count].capture_local_ids = capture_local_ids;
    stack->items[stack->count].capture_count = capture_count;
    stack->count++;
    return 1;
}

static void ir_lower_defer_entry_stack_pop_to(IrDeferEntryStack *stack, size_t new_count) {
    if (!stack) {
        return;
    }
    if (new_count > stack->count) {
        return;
    }
    stack->count = new_count;
}

static void ir_lower_function_defer_entry_stack_free(IrFunctionDeferEntryStack *stack) {
    size_t index;

    if (!stack) {
        return;
    }

    for (index = 0u; index < stack->count; ++index) {
        free((void *)stack->items[index].capture_names);
        free((void *)stack->items[index].capture_local_ids);
    }
    free(stack->items);
    stack->items = NULL;
    stack->count = 0u;
    stack->capacity = 0u;
}

static int ir_lower_function_defer_entry_stack_push_with_captures(IrFunctionDeferEntryStack *stack,
    const AstStatement *payload_stmt,
    const char *const *capture_names,
    const size_t *capture_local_ids,
    size_t capture_count) {
    IrDeferEntry *new_items;
    size_t next_capacity;

    if (!stack || !payload_stmt) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity, 8u, sizeof(IrDeferEntry), &next_capacity)) {
            return 0;
        }
        new_items = (IrDeferEntry *)realloc(stack->items, next_capacity * sizeof(IrDeferEntry));
        if (!new_items) {
            return 0;
        }
        stack->items = new_items;
        stack->capacity = next_capacity;
    }

    stack->items[stack->count].payload_stmt = payload_stmt;
    stack->items[stack->count].capture_names = capture_names;
    stack->items[stack->count].capture_local_ids = capture_local_ids;
    stack->items[stack->count].capture_count = capture_count;
    stack->items[stack->count].has_runtime_repeat_count = 0;
    stack->items[stack->count].runtime_repeat_count_local_id = 0u;
    stack->count++;
    return 1;
}

static int ir_lower_function_defer_entry_stack_push_with_runtime_repeat_count(
    IrFunctionDeferEntryStack *stack,
    const AstStatement *payload_stmt,
    size_t runtime_repeat_count_local_id) {
    IrDeferEntry *new_items;
    size_t next_capacity;

    if (!stack || !payload_stmt) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity, 8u, sizeof(IrDeferEntry), &next_capacity)) {
            return 0;
        }
        new_items = (IrDeferEntry *)realloc(stack->items, next_capacity * sizeof(IrDeferEntry));
        if (!new_items) {
            return 0;
        }
        stack->items = new_items;
        stack->capacity = next_capacity;
    }

    stack->items[stack->count].payload_stmt = payload_stmt;
    stack->items[stack->count].capture_names = NULL;
    stack->items[stack->count].capture_local_ids = NULL;
    stack->items[stack->count].capture_count = 0u;
    stack->items[stack->count].has_runtime_repeat_count = 1;
    stack->items[stack->count].runtime_repeat_count_local_id = runtime_repeat_count_local_id;
    stack->count++;
    return 1;
}

static void ir_lower_defer_scope_stack_free(IrDeferScopeStack *stack) {
    if (!stack) {
        return;
    }

    free(stack->items);
    stack->items = NULL;
    stack->count = 0u;
    stack->capacity = 0u;
}

static int ir_lower_defer_scope_stack_push(IrDeferScopeStack *stack, size_t defer_base) {
    IrDeferScopeEntry *new_items;
    size_t next_capacity;

    if (!stack) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity, 8u, sizeof(IrDeferScopeEntry), &next_capacity)) {
            return 0;
        }
        new_items = (IrDeferScopeEntry *)realloc(stack->items, next_capacity * sizeof(IrDeferScopeEntry));
        if (!new_items) {
            return 0;
        }
        stack->items = new_items;
        stack->capacity = next_capacity;
    }

    stack->items[stack->count].defer_base = defer_base;
    stack->count++;
    return 1;
}

static void ir_lower_defer_scope_stack_pop(IrDeferScopeStack *stack) {
    if (!stack || stack->count == 0u) {
        return;
    }
    stack->count--;
}

static int ir_lower_emit_defer_entries(IrLowerContext *ctx, size_t begin_index) {
    if (!ctx) {
        return 0;
    }
    return ir_lower_emit_defer_entries_range(ctx, begin_index, ctx->defer_entries.count);
}

static int ir_lower_emit_defer_entries_range(IrLowerContext *ctx, size_t begin_index, size_t end_index) {
    size_t index;

    if (!ctx) {
        return 0;
    }
    if (begin_index > ctx->defer_entries.count || end_index > ctx->defer_entries.count || begin_index > end_index) {
        return 0;
    }

    for (index = end_index; index > begin_index; --index) {
        const IrDeferEntry *entry = &ctx->defer_entries.items[index - 1u];
        const AstStatement *payload_stmt = entry->payload_stmt;
        size_t capture_index;
        int pushed_capture_scope = 0;

        if (entry->capture_count > 0u) {
            if (!ir_lower_scope_stack_push(&ctx->scopes)) {
                return 0;
            }
            pushed_capture_scope = 1;
            for (capture_index = 0u; capture_index < entry->capture_count; ++capture_index) {
                if (!ir_lower_scope_add_binding_with_metadata(
                        &ctx->scopes,
                        entry->capture_names[capture_index],
                        entry->capture_local_ids[capture_index],
                        IR_LOWER_BINDING_SCALAR_LOCAL,
                        0u,
                        NULL,
                        0u,
                        NULL,
                        0,
                        0,
                        0)) {
                    ir_lower_scope_stack_pop(&ctx->scopes);
                    return 0;
                }
            }
        }

        if (!payload_stmt || !ir_lower_statement(ctx, payload_stmt)) {
            if (pushed_capture_scope) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        if (pushed_capture_scope) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        if (!ctx->has_block || ir_lower_current_block(ctx)->has_terminator) {
            return 1;
        }
    }

    return 1;
}

static int ir_lower_emit_function_defer_entries(IrLowerContext *ctx) {
    size_t index;

    if (!ctx) {
        return 0;
    }

    for (index = ctx->function_defer_entries.count; index > 0u; --index) {
        const IrDeferEntry *entry = &ctx->function_defer_entries.items[index - 1u];
        const AstStatement *payload_stmt = entry->payload_stmt;
        size_t capture_index;
        int pushed_capture_scope = 0;

        if (entry->capture_count > 0u) {
            if (!ir_lower_scope_stack_push(&ctx->scopes)) {
                return 0;
            }
            pushed_capture_scope = 1;
            for (capture_index = 0u; capture_index < entry->capture_count; ++capture_index) {
                if (!ir_lower_scope_add_binding_with_metadata(
                        &ctx->scopes,
                        entry->capture_names[capture_index],
                        entry->capture_local_ids[capture_index],
                        IR_LOWER_BINDING_SCALAR_LOCAL,
                        0u,
                        NULL,
                        0u,
                        NULL,
                        0,
                        0,
                        0)) {
                    ir_lower_scope_stack_pop(&ctx->scopes);
                    return 0;
                }
            }
        }

        if (entry->has_runtime_repeat_count) {
            if (!ir_lower_emit_function_defer_repeat_loop(
                    ctx,
                    entry->runtime_repeat_count_local_id,
                    payload_stmt)) {
                if (pushed_capture_scope) {
                    ir_lower_scope_stack_pop(&ctx->scopes);
                }
                return 0;
            }
        } else if (!payload_stmt || !ir_lower_statement(ctx, payload_stmt)) {
            if (pushed_capture_scope) {
                ir_lower_scope_stack_pop(&ctx->scopes);
            }
            return 0;
        }
        if (pushed_capture_scope) {
            ir_lower_scope_stack_pop(&ctx->scopes);
        }
        if (!ctx->has_block || ir_lower_current_block(ctx)->has_terminator) {
            return 1;
        }
    }

    return 1;
}

static int ir_lower_emit_function_defer_repeat_loop(IrLowerContext *ctx,
    size_t repeat_count_local_id,
    const AstStatement *payload_stmt) {
    size_t entry_block_id;
    size_t header_block_id;
    size_t body_block_id;
    size_t exit_block_id;
    size_t loop_index_local_id = (size_t)-1;
    IrValueRef loop_index_value;
    IrValueRef loop_count_value;
    IrValueRef loop_cond_value;
    IrValueRef one_value = ir_value_immediate(1);
    IrValueRef zero_value = ir_value_immediate(0);

    if (!ctx || !ctx->function || !ctx->has_block || !payload_stmt) {
        return 0;
    }

    entry_block_id = ctx->block_id;
    if (!ir_function_append_local(ctx->function, "__fndefer_i", 0, &loop_index_local_id, ctx->error)) {
        return 0;
    }

    if (!ir_emit_mov(ctx, ir_value_local(loop_index_local_id), zero_value) ||
        !ir_function_append_block(ctx->function, &header_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &body_block_id, NULL, ctx->error) ||
        !ir_function_append_block(ctx->function, &exit_block_id, NULL, ctx->error) ||
        !ir_block_set_jump(ir_function_get_block_mut(ctx->function, entry_block_id), header_block_id, ctx->error)) {
        return 0;
    }

    ctx->block_id = header_block_id;
    ctx->has_block = 1;
    {
        size_t temp_id = ir_allocate_temp(ctx->function);
        IrValueRef current_index = ir_value_temp(temp_id);
        temp_id = ir_allocate_temp(ctx->function);
        IrValueRef current_count = ir_value_temp(temp_id);
        temp_id = ir_allocate_temp(ctx->function);
        loop_cond_value = ir_value_temp(temp_id);

        loop_index_value = ir_value_local(loop_index_local_id);
        loop_count_value = ir_value_local(repeat_count_local_id);
        if (!ir_emit_mov(ctx, current_index, loop_index_value) ||
            !ir_emit_mov(ctx, current_count, loop_count_value) ||
            !ir_emit_binary(ctx, loop_cond_value, IR_BINARY_LT, current_index, current_count) ||
            !ir_block_set_branch(ir_lower_current_block(ctx), loop_cond_value, body_block_id, exit_block_id, ctx->error)) {
            return 0;
        }
    }

    ctx->block_id = body_block_id;
    ctx->has_block = 1;
    if (!ir_lower_statement(ctx, payload_stmt)) {
        return 0;
    }
    if (ctx->has_block && !ir_lower_current_block(ctx)->has_terminator) {
        size_t temp_id = ir_allocate_temp(ctx->function);
        IrValueRef next_index = ir_value_temp(temp_id);

        if (!ir_emit_binary(ctx, next_index, IR_BINARY_ADD, ir_value_local(loop_index_local_id), one_value) ||
            !ir_emit_mov(ctx, ir_value_local(loop_index_local_id), next_index) ||
            !ir_block_set_jump(ir_lower_current_block(ctx), header_block_id, ctx->error)) {
            return 0;
        }
    }

    ctx->block_id = exit_block_id;
    ctx->has_block = 1;
    (void)loop_index_value;
    (void)loop_count_value;
    (void)loop_cond_value;
    return 1;
}

static int ir_collect_fndefer_sites(const AstStatement *stmt,
    const AstStatement ***out_sites,
    size_t *out_count,
    size_t *out_capacity,
    IrError *error) {
    const AstStatement **sites;
    size_t count;
    size_t capacity;
    size_t i;

    if (!stmt || !out_sites || !out_count || !out_capacity) {
        return 1;
    }
    sites = *out_sites;
    count = *out_count;
    capacity = *out_capacity;

    if (stmt->kind == AST_STMT_FNDEFER) {
        if (stmt->child_count != 1u || !stmt->children || !stmt->children[0]) {
            ir_set_error(error, stmt->line, stmt->column, "IR-LOWER-026: malformed fndefer statement payload during IR lowering");
            return 0;
        }
        if (count == capacity) {
            size_t next_capacity = capacity == 0u ? 4u : capacity * 2u;
            const AstStatement **new_sites = (const AstStatement **)realloc((void *)sites, next_capacity * sizeof(*sites));
            if (!new_sites) {
                ir_set_error(error, stmt->line, stmt->column, "IR-INT-085: out of memory while collecting fndefer sites");
                return 0;
            }
            sites = new_sites;
            capacity = next_capacity;
        }
        sites[count++] = stmt;
    }

    for (i = 0u; i < stmt->child_count; ++i) {
        const AstStatement *child = stmt->children ? stmt->children[i] : NULL;

        if (!ir_collect_fndefer_sites(child, &sites, &count, &capacity, error)) {
            return 0;
        }
    }

    *out_sites = sites;
    *out_count = count;
    *out_capacity = capacity;
    return 1;
}

static int ir_lower_lookup_fndefer_site_counter_local_id(const IrLowerContext *ctx,
    const AstStatement *stmt,
    size_t *out_local_id) {
    size_t i;

    if (out_local_id) {
        *out_local_id = (size_t)-1;
    }
    if (!ctx || !stmt || !out_local_id) {
        return 0;
    }

    for (i = 0u; i < ctx->fndefer_site_count; ++i) {
        if (ctx->fndefer_sites && ctx->fndefer_sites[i] == stmt) {
            *out_local_id = ctx->fndefer_counter_local_ids[i];
            return 1;
        }
    }

    return 0;
}

static int ir_lower_emit_fndefer_site_registration(IrLowerContext *ctx,
    const AstStatement *stmt,
    size_t *out_repeat_count_local_id) {
    size_t counter_local_id;
    size_t temp_id;
    IrValueRef one_value = ir_value_immediate(1);
    IrValueRef current_value;
    IrValueRef next_value;

    if (out_repeat_count_local_id) {
        *out_repeat_count_local_id = (size_t)-1;
    }
    if (!ctx || !stmt || !ctx->function || !ctx->has_block || !out_repeat_count_local_id) {
        return 0;
    }
    if (!ir_lower_lookup_fndefer_site_counter_local_id(ctx, stmt, &counter_local_id)) {
        ir_set_error(ctx->error, stmt->line, stmt->column, "IR-LOWER-080: missing function-exit defer site registration metadata");
        return 0;
    }

    temp_id = ir_allocate_temp(ctx->function);
    current_value = ir_value_temp(temp_id);
    temp_id = ir_allocate_temp(ctx->function);
    next_value = ir_value_temp(temp_id);
    if (!ir_emit_mov(ctx, current_value, ir_value_local(counter_local_id)) ||
        !ir_emit_binary(ctx, next_value, IR_BINARY_ADD, current_value, one_value) ||
        !ir_emit_mov(ctx, ir_value_local(counter_local_id), next_value)) {
        return 0;
    }

    *out_repeat_count_local_id = counter_local_id;
    return 1;
}

static IrValueRef ir_value_immediate(long long value) {
    IrValueRef ref;

    ref.kind = IR_VALUE_IMMEDIATE;
    ref.immediate = value;
    ref.id = 0;
    return ref;
}

static IrValueRef ir_value_local(size_t id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_LOCAL;
    ref.immediate = 0;
    ref.id = id;
    return ref;
}

static IrValueRef ir_value_global(size_t id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_GLOBAL;
    ref.immediate = 0;
    ref.id = id;
    return ref;
}

static IrValueRef ir_value_temp(size_t id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_TEMP;
    ref.immediate = 0;
    ref.id = id;
    return ref;
}

static const char *ir_binary_op_name(IrBinaryOp op) {
    switch (op) {
    case IR_BINARY_ADD:
        return "add";
    case IR_BINARY_SUB:
        return "sub";
    case IR_BINARY_MUL:
        return "mul";
    case IR_BINARY_DIV:
        return "div";
    case IR_BINARY_MOD:
        return "mod";
    case IR_BINARY_BIT_AND:
        return "and";
    case IR_BINARY_BIT_XOR:
        return "xor";
    case IR_BINARY_BIT_OR:
        return "or";
    case IR_BINARY_SHIFT_LEFT:
        return "shl";
    case IR_BINARY_SHIFT_RIGHT:
        return "shr";
    case IR_BINARY_EQ:
        return "eq";
    case IR_BINARY_NE:
        return "ne";
    case IR_BINARY_LT:
        return "lt";
    case IR_BINARY_LE:
        return "le";
    case IR_BINARY_GT:
        return "gt";
    case IR_BINARY_GE:
        return "ge";
    default:
        return "unknown";
    }
}

#define IR_SPLIT_AGGREGATOR 1
#include "ir_core.inc"
#include "ir_lower_scope.inc"
#include "ir_lower_expr.inc"
#include "ir_lower_stmt.inc"
#include "ir_global_dep.inc"
#include "ir_global_init.inc"
#include "ir_verify.inc"
#include "ir_dump.inc"
#undef IR_SPLIT_AGGREGATOR
