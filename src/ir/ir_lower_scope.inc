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
    int is_const,
    int has_const_value,
    long long const_value) {
    IrLowerScope *frame;
    IrLowerBinding *new_bindings;
    size_t next_capacity;
    size_t *extent_copy = NULL;

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

    frame->bindings[frame->count].name = name;
    frame->bindings[frame->count].local_id = local_id;
    frame->bindings[frame->count].kind = kind;
    frame->bindings[frame->count].array_rank = array_rank;
    frame->bindings[frame->count].array_extents = extent_copy;
    frame->bindings[frame->count].is_const = is_const;
    frame->bindings[frame->count].has_const_value = has_const_value;
    frame->bindings[frame->count].const_value = const_value;
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
                    out_info->is_const = binding->is_const;
                    out_info->has_const_value = binding->has_const_value;
                    out_info->const_value = binding->const_value;
                }
                return 1;
            }
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
