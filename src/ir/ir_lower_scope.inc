#if defined(__INTELLISENSE__) && !defined(IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_SPLIT_AGGREGATOR)
#define IR_SPLIT_INTELLISENSE_CONTEXT
#include "ir.c"
#else

static void ir_lower_scope_stack_free(IrLowerScopeStack *stack) {
    size_t i;

    if (!stack) {
        return;
    }

    for (i = 0; i < stack->count; ++i) {
        size_t j;
        for (j = 0; j < stack->frames[i].count; ++j) {
            free(stack->frames[i].bindings[j].array_extents);
            free(stack->frames[i].bindings[j].closure_capture_local_ids);
        }
        free(stack->frames[i].bindings);
    }
    free(stack->frames);
    stack->frames = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static int ir_lower_scope_stack_push(IrLowerScopeStack *stack) {
    IrLowerScope *new_frames;
    size_t next_capacity;

    if (!stack) {
        return 0;
    }

    if (stack->count == stack->capacity) {
        if (!ir_next_growth_capacity(stack->capacity,
                4,
                sizeof(IrLowerScope),
                &next_capacity)) {
            return 0;
        }
        new_frames = (IrLowerScope *)realloc(stack->frames,
            next_capacity * sizeof(IrLowerScope));
        if (!new_frames) {
            return 0;
        }
        stack->frames = new_frames;
        stack->capacity = next_capacity;
    }

    stack->frames[stack->count].bindings = NULL;
    stack->frames[stack->count].count = 0;
    stack->frames[stack->count].capacity = 0;
    stack->count++;
    return 1;
}

static void ir_lower_scope_stack_pop(IrLowerScopeStack *stack) {
    IrLowerScope *frame;

    if (!stack || stack->count == 0) {
        return;
    }

    frame = &stack->frames[stack->count - 1];
    if (frame->bindings) {
        size_t i;
        for (i = 0; i < frame->count; ++i) {
            free(frame->bindings[i].array_extents);
            free(frame->bindings[i].closure_capture_local_ids);
        }
    }
    free(frame->bindings);
    frame->bindings = NULL;
    frame->count = 0;
    frame->capacity = 0;
    stack->count--;
}

static int ir_lower_scope_add_binding_with_metadata(IrLowerScopeStack *stack,
    const char *name,
    size_t local_id,
    IrLowerBindingKind kind,
    size_t array_rank,
    const size_t *array_extents,
    size_t pair_second_local_id,
    size_t aggregate_slot_count,
    const char *type_name,
    size_t closure_capture_count,
    const size_t *closure_capture_local_ids,
    size_t function_target_tag_local_id,
    int is_const,
    int has_const_value,
    long long const_value,
    int from_returned_function_value_call) {
    IrLowerScope *frame;
    IrLowerBinding *new_bindings;
    size_t next_capacity;
    size_t *extent_copy = NULL;
    size_t *closure_capture_copy = NULL;

    if (!stack || stack->count == 0 || !name) {
        return 0;
    }

    frame = &stack->frames[stack->count - 1];
    if (frame->count == frame->capacity) {
        if (!ir_next_growth_capacity(frame->capacity,
                4,
                sizeof(const char *),
                &next_capacity)) {
            return 0;
        }
        new_bindings = (IrLowerBinding *)realloc(frame->bindings, next_capacity * sizeof(IrLowerBinding));
        if (!new_bindings) {
            return 0;
        }
        frame->bindings = new_bindings;
        frame->capacity = next_capacity;
    }

    if (array_rank > 0u) {
        size_t i;

        extent_copy = (size_t *)malloc(array_rank * sizeof(size_t));
        if (!extent_copy) {
            return 0;
        }
        for (i = 0; i < array_rank; ++i) {
            extent_copy[i] = array_extents ? array_extents[i] : 0u;
        }
    }
    if (closure_capture_count > 0u) {
        size_t i;

        closure_capture_copy = (size_t *)malloc(closure_capture_count * sizeof(size_t));
        if (!closure_capture_copy) {
            free(extent_copy);
            return 0;
        }
        for (i = 0u; i < closure_capture_count; ++i) {
            closure_capture_copy[i] =
                closure_capture_local_ids ? closure_capture_local_ids[i] : (size_t)-1;
        }
    }

    frame->bindings[frame->count].name = name;
    frame->bindings[frame->count].local_id = local_id;
    frame->bindings[frame->count].kind = kind;
    frame->bindings[frame->count].array_rank = array_rank;
    frame->bindings[frame->count].array_extents = extent_copy;
    frame->bindings[frame->count].pair_second_local_id = pair_second_local_id;
    frame->bindings[frame->count].aggregate_slot_count = aggregate_slot_count;
    frame->bindings[frame->count].type_name = type_name;
    frame->bindings[frame->count].closure_capture_count = closure_capture_count;
    frame->bindings[frame->count].closure_capture_local_ids = closure_capture_copy;
    frame->bindings[frame->count].function_target_tag_local_id = function_target_tag_local_id;
    frame->bindings[frame->count].is_const = is_const;
    frame->bindings[frame->count].has_const_value = has_const_value;
    frame->bindings[frame->count].const_value = const_value;
    frame->bindings[frame->count].from_returned_function_value_call =
        from_returned_function_value_call ? 1 : 0;
    frame->count++;
    return 1;
}

static int ir_lower_scope_lookup(const IrLowerScopeStack *stack,
    const char *name,
    size_t *out_local_id) {
    size_t frame_index;

    if (!stack || !name) {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        size_t i;
        const IrLowerScope *frame = &stack->frames[frame_index - 1];

        for (i = frame->count; i > 0; --i) {
            if (strcmp(frame->bindings[i - 1].name, name) == 0) {
                if (out_local_id) {
                    *out_local_id = frame->bindings[i - 1].local_id;
                }
                return 1;
            }
        }
    }

    return 0;
}

static int ir_lower_scope_lookup_binding(const IrLowerScopeStack *stack,
    const char *name,
    IrLowerBindingInfo *out_info) {
    size_t frame_index;

    if (!stack || !name) {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        size_t i;
        const IrLowerScope *frame = &stack->frames[frame_index - 1];

        for (i = frame->count; i > 0; --i) {
            const IrLowerBinding *binding = &frame->bindings[i - 1];
            if (strcmp(binding->name, name) == 0) {
                if (out_info) {
                    out_info->local_id = binding->local_id;
                    out_info->kind = binding->kind;
                    out_info->array_rank = binding->array_rank;
                    out_info->array_extents = binding->array_extents;
                    out_info->pair_second_local_id = binding->pair_second_local_id;
                    out_info->aggregate_slot_count = binding->aggregate_slot_count;
                    out_info->type_name = binding->type_name;
                    out_info->closure_capture_count = binding->closure_capture_count;
                    out_info->closure_capture_local_ids = binding->closure_capture_local_ids;
                    out_info->function_target_tag_local_id = binding->function_target_tag_local_id;
                    out_info->is_const = binding->is_const;
                    out_info->has_const_value = binding->has_const_value;
                    out_info->const_value = binding->const_value;
                    out_info->from_returned_function_value_call =
                        binding->from_returned_function_value_call;
                }
                return 1;
            }
        }
    }

    return 0;
}

static int ir_lower_scope_lookup_binding_by_local_id(const IrLowerScopeStack *stack,
    size_t local_id,
    IrLowerBindingInfo *out_info) {
    size_t frame_index;

    if (!stack) {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        size_t i;
        const IrLowerScope *frame = &stack->frames[frame_index - 1];

        for (i = frame->count; i > 0; --i) {
            const IrLowerBinding *binding = &frame->bindings[i - 1];
            if (binding->local_id != local_id) {
                continue;
            }
            if (out_info) {
                out_info->local_id = binding->local_id;
                out_info->kind = binding->kind;
                out_info->array_rank = binding->array_rank;
                out_info->array_extents = binding->array_extents;
                out_info->pair_second_local_id = binding->pair_second_local_id;
                out_info->aggregate_slot_count = binding->aggregate_slot_count;
                out_info->type_name = binding->type_name;
                out_info->closure_capture_count = binding->closure_capture_count;
                out_info->closure_capture_local_ids = binding->closure_capture_local_ids;
                out_info->function_target_tag_local_id = binding->function_target_tag_local_id;
                out_info->is_const = binding->is_const;
                out_info->has_const_value = binding->has_const_value;
                out_info->const_value = binding->const_value;
                out_info->from_returned_function_value_call =
                    binding->from_returned_function_value_call;
            }
            return 1;
        }
    }

    return 0;
}

static int ir_lower_scope_update_binding_const_value_by_local_id(IrLowerScopeStack *stack,
    size_t local_id,
    int has_const_value,
    long long const_value) {
    size_t frame_index;

    if (!stack) {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        size_t i;
        IrLowerScope *frame = &stack->frames[frame_index - 1];

        for (i = frame->count; i > 0; --i) {
            IrLowerBinding *binding = &frame->bindings[i - 1];

            if (binding->local_id != local_id ||
                binding->kind != IR_LOWER_BINDING_SCALAR_LOCAL) {
                continue;
            }

            binding->has_const_value = has_const_value ? 1 : 0;
            binding->const_value = const_value;
            return 1;
        }
    }

    return 0;
}

static int ir_lower_scope_update_function_binding_by_name(IrLowerScopeStack *stack,
    const char *name,
    IrLowerBindingKind kind,
    const char *type_name,
    size_t closure_capture_count,
    const size_t *closure_capture_local_ids,
    size_t function_target_tag_local_id,
    int has_const_value,
    long long const_value,
    int from_returned_function_value_call) {
    size_t frame_index;

    if (!stack || !name || name[0] == '\0' || !type_name || type_name[0] == '\0') {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        size_t i;
        IrLowerScope *frame = &stack->frames[frame_index - 1];

        for (i = frame->count; i > 0; --i) {
            IrLowerBinding *binding = &frame->bindings[i - 1];
            size_t *closure_capture_copy = NULL;
            size_t capture_index;

            if (strcmp(binding->name, name) != 0 ||
                (binding->kind != IR_LOWER_BINDING_FUNCTION_LOCAL &&
                    binding->kind != IR_LOWER_BINDING_CLOSURE_LOCAL)) {
                continue;
            }
            if (closure_capture_count > 0u) {
                closure_capture_copy = (size_t *)malloc(closure_capture_count * sizeof(size_t));
                if (!closure_capture_copy) {
                    return 0;
                }
                for (capture_index = 0u; capture_index < closure_capture_count; ++capture_index) {
                    closure_capture_copy[capture_index] =
                        closure_capture_local_ids ? closure_capture_local_ids[capture_index] : (size_t)-1;
                }
            }

            free(binding->closure_capture_local_ids);
            binding->closure_capture_local_ids = closure_capture_copy;
            binding->closure_capture_count = closure_capture_count;
            binding->kind = kind;
            binding->type_name = type_name;
            binding->function_target_tag_local_id = function_target_tag_local_id;
            binding->has_const_value = has_const_value ? 1 : 0;
            binding->const_value = const_value;
            binding->from_returned_function_value_call =
                from_returned_function_value_call ? 1 : 0;
            return 1;
        }
    }

    return 0;
}

static void ir_lower_function_value_target_sets_free(IrFunctionValueTargetSet *sets,
    size_t count) {
    size_t index;

    if (!sets) {
        return;
    }
    for (index = 0u; index < count; ++index) {
        free(sets[index].target_names);
    }
    free(sets);
}

static IrFunctionValueTargetSet *ir_lower_find_function_value_target_set(
    const IrLowerContext *ctx,
    const char *binding_name) {
    size_t index;

    if (!ctx || !binding_name || binding_name[0] == '\0') {
        return NULL;
    }
    for (index = 0u; index < ctx->function_value_target_set_count; ++index) {
        if (ctx->function_value_target_sets[index].binding_name &&
            strcmp(ctx->function_value_target_sets[index].binding_name, binding_name) == 0) {
            return &ctx->function_value_target_sets[index];
        }
    }
    return NULL;
}

static int ir_lower_register_function_value_target(IrLowerContext *ctx,
    const char *binding_name,
    const char *target_name,
    long long *out_tag_value) {
    IrFunctionValueTargetSet *set = NULL;
    const char **new_target_names;
    IrFunctionValueTargetSet *new_sets;
    size_t next_capacity;
    size_t index;

    if (out_tag_value) {
        *out_tag_value = 0;
    }
    if (!ctx || !binding_name || binding_name[0] == '\0' || !target_name || target_name[0] == '\0') {
        return 0;
    }

    set = ir_lower_find_function_value_target_set(ctx, binding_name);
    if (!set) {
        if (ctx->function_value_target_set_count == ctx->function_value_target_set_capacity) {
            if (!ir_next_growth_capacity(ctx->function_value_target_set_capacity,
                    4u,
                    sizeof(IrFunctionValueTargetSet),
                    &next_capacity)) {
                return 0;
            }
            new_sets = (IrFunctionValueTargetSet *)realloc(
                ctx->function_value_target_sets,
                next_capacity * sizeof(IrFunctionValueTargetSet));
            if (!new_sets) {
                return 0;
            }
            ctx->function_value_target_sets = new_sets;
            ctx->function_value_target_set_capacity = next_capacity;
        }
        set = &ctx->function_value_target_sets[ctx->function_value_target_set_count++];
        memset(set, 0, sizeof(*set));
        set->binding_name = binding_name;
    }

    for (index = 0u; index < set->target_count; ++index) {
        if (set->target_names[index] &&
            strcmp(set->target_names[index], target_name) == 0) {
            if (out_tag_value) {
                *out_tag_value = (long long)(index + 1u);
            }
            return 1;
        }
    }

    if (set->target_count == set->target_capacity) {
        if (!ir_next_growth_capacity(set->target_capacity,
                4u,
                sizeof(const char *),
                &next_capacity)) {
            return 0;
        }
        new_target_names = (const char **)realloc(
            set->target_names,
            next_capacity * sizeof(const char *));
        if (!new_target_names) {
            return 0;
        }
        set->target_names = new_target_names;
        set->target_capacity = next_capacity;
    }

    set->target_names[set->target_count++] = target_name;
    if (out_tag_value) {
        *out_tag_value = (long long)set->target_count;
    }
    return 1;
}

static int ir_lower_reset_function_value_target_set(IrLowerContext *ctx,
    const char *binding_name) {
    IrFunctionValueTargetSet *set;

    if (!ctx || !binding_name || binding_name[0] == '\0') {
        return 0;
    }

    set = ir_lower_find_function_value_target_set(ctx, binding_name);
    if (!set) {
        return 1;
    }

    free(set->target_names);
    set->target_names = NULL;
    set->target_count = 0u;
    set->target_capacity = 0u;
    return 1;
}

static const char *ir_lower_function_value_target_name_for_tag(const IrLowerContext *ctx,
    const char *binding_name,
    long long tag_value) {
    IrFunctionValueTargetSet *set;
    size_t index;

    if (!ctx || !binding_name || binding_name[0] == '\0' || tag_value <= 0) {
        return NULL;
    }
    set = ir_lower_find_function_value_target_set(ctx, binding_name);
    if (!set) {
        return NULL;
    }
    index = (size_t)(tag_value - 1);
    if (index >= set->target_count || !set->target_names[index]) {
        return NULL;
    }
    return set->target_names[index];
}

static int ir_lower_clone_function_value_target_set(IrLowerContext *ctx,
    const char *dest_name,
    const char *source_name) {
    IrFunctionValueTargetSet *source_set;
    const char **source_target_names = NULL;
    size_t source_target_count = 0u;
    size_t index;
    long long ignored_tag = 0;

    if (!ctx || !dest_name || dest_name[0] == '\0' || !source_name || source_name[0] == '\0') {
        return 0;
    }

    source_set = ir_lower_find_function_value_target_set(ctx, source_name);
    if (!source_set) {
        return 0;
    }
    source_target_count = source_set->target_count;
    if (source_target_count > 0u) {
        source_target_names =
            (const char **)malloc(source_target_count * sizeof(const char *));
        if (!source_target_names) {
            return 0;
        }
        for (index = 0u; index < source_target_count; ++index) {
            source_target_names[index] = source_set->target_names[index];
        }
    }

    for (index = 0u; index < source_target_count; ++index) {
        if (!source_target_names[index] ||
            !ir_lower_register_function_value_target(
                ctx,
                dest_name,
                source_target_names[index],
                &ignored_tag)) {
            free(source_target_names);
            return 0;
        }
    }

    free(source_target_names);
    return 1;
}

static int ir_lower_scope_stack_clone(const IrLowerScopeStack *src,
    IrLowerScopeStack *dest) {
    size_t frame_index;

    if (!src || !dest) {
        return 0;
    }

    dest->frames = NULL;
    dest->count = 0;
    dest->capacity = 0;

    if (src->count == 0u) {
        return 1;
    }

    dest->frames = (IrLowerScope *)calloc(src->count, sizeof(IrLowerScope));
    if (!dest->frames) {
        return 0;
    }
    dest->count = src->count;
    dest->capacity = src->count;

    for (frame_index = 0; frame_index < src->count; ++frame_index) {
        const IrLowerScope *src_frame = &src->frames[frame_index];
        IrLowerScope *dest_frame = &dest->frames[frame_index];
        size_t binding_index;

        if (src_frame->count == 0u) {
            continue;
        }

        dest_frame->bindings = (IrLowerBinding *)calloc(src_frame->count, sizeof(IrLowerBinding));
        if (!dest_frame->bindings) {
            ir_lower_scope_stack_free(dest);
            return 0;
        }
        dest_frame->count = src_frame->count;
        dest_frame->capacity = src_frame->count;

        for (binding_index = 0; binding_index < src_frame->count; ++binding_index) {
            const IrLowerBinding *src_binding = &src_frame->bindings[binding_index];
            IrLowerBinding *dest_binding = &dest_frame->bindings[binding_index];

            *dest_binding = *src_binding;
            dest_binding->array_extents = NULL;
            dest_binding->closure_capture_local_ids = NULL;

            if (src_binding->array_rank > 0u && src_binding->array_extents) {
                size_t extent_count = src_binding->array_rank;

                dest_binding->array_extents = (size_t *)malloc(extent_count * sizeof(size_t));
                if (!dest_binding->array_extents) {
                    ir_lower_scope_stack_free(dest);
                    return 0;
                }
                memcpy(dest_binding->array_extents,
                    src_binding->array_extents,
                    extent_count * sizeof(size_t));
            }
            if (src_binding->closure_capture_count > 0u &&
                src_binding->closure_capture_local_ids) {
                size_t capture_count = src_binding->closure_capture_count;

                dest_binding->closure_capture_local_ids =
                    (size_t *)malloc(capture_count * sizeof(size_t));
                if (!dest_binding->closure_capture_local_ids) {
                    ir_lower_scope_stack_free(dest);
                    return 0;
                }
                memcpy(dest_binding->closure_capture_local_ids,
                    src_binding->closure_capture_local_ids,
                    capture_count * sizeof(size_t));
            }
        }
    }

    return 1;
}

static int ir_lower_scope_stack_merge_const_facts(IrLowerScopeStack *dest,
    const IrLowerScopeStack *lhs,
    const IrLowerScopeStack *rhs) {
    size_t frame_index;

    if (!dest || !lhs || !rhs) {
        return 0;
    }

    if (dest->count != lhs->count || lhs->count != rhs->count) {
        return 0;
    }

    for (frame_index = 0; frame_index < dest->count; ++frame_index) {
        IrLowerScope *dest_frame = &dest->frames[frame_index];
        const IrLowerScope *lhs_frame = &lhs->frames[frame_index];
        const IrLowerScope *rhs_frame = &rhs->frames[frame_index];
        size_t binding_index;

        if (dest_frame->count != lhs_frame->count || lhs_frame->count != rhs_frame->count) {
            return 0;
        }

        for (binding_index = 0; binding_index < dest_frame->count; ++binding_index) {
            IrLowerBinding *dest_binding = &dest_frame->bindings[binding_index];
            const IrLowerBinding *lhs_binding = &lhs_frame->bindings[binding_index];
            const IrLowerBinding *rhs_binding = &rhs_frame->bindings[binding_index];

            if (dest_binding->name != lhs_binding->name ||
                lhs_binding->name != rhs_binding->name ||
                dest_binding->local_id != lhs_binding->local_id ||
                lhs_binding->local_id != rhs_binding->local_id ||
                dest_binding->pair_second_local_id != lhs_binding->pair_second_local_id ||
                lhs_binding->pair_second_local_id != rhs_binding->pair_second_local_id ||
                dest_binding->aggregate_slot_count != lhs_binding->aggregate_slot_count ||
                lhs_binding->aggregate_slot_count != rhs_binding->aggregate_slot_count ||
                dest_binding->function_target_tag_local_id != lhs_binding->function_target_tag_local_id ||
                lhs_binding->function_target_tag_local_id != rhs_binding->function_target_tag_local_id ||
                dest_binding->from_returned_function_value_call !=
                    lhs_binding->from_returned_function_value_call ||
                lhs_binding->from_returned_function_value_call !=
                    rhs_binding->from_returned_function_value_call ||
                dest_binding->kind != lhs_binding->kind ||
                lhs_binding->kind != rhs_binding->kind) {
                return 0;
            }

            if (!(lhs_binding->has_const_value &&
                    rhs_binding->has_const_value &&
                    lhs_binding->const_value == rhs_binding->const_value)) {
                dest_binding->has_const_value = 0;
                dest_binding->const_value = 0;
            } else {
                dest_binding->has_const_value = 1;
                dest_binding->const_value = lhs_binding->const_value;
            }
        }
    }

    return 1;
}

#endif
