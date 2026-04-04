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
        free(stack->frames[i].names);
        free(stack->frames[i].local_ids);
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

    stack->frames[stack->count].names = NULL;
    stack->frames[stack->count].local_ids = NULL;
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
    free(frame->names);
    free(frame->local_ids);
    frame->names = NULL;
    frame->local_ids = NULL;
    frame->count = 0;
    frame->capacity = 0;
    stack->count--;
}

static int ir_lower_scope_add_binding(IrLowerScopeStack *stack,
    const char *name,
    size_t local_id) {
    IrLowerScope *frame;
    const char **new_names;
    size_t *new_local_ids;
    size_t next_capacity;

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
        new_names = (const char **)realloc(frame->names, next_capacity * sizeof(const char *));
        if (!new_names) {
            return 0;
        }
        new_local_ids = (size_t *)realloc(frame->local_ids, next_capacity * sizeof(size_t));
        if (!new_local_ids) {
            return 0;
        }
        frame->names = new_names;
        frame->local_ids = new_local_ids;
        frame->capacity = next_capacity;
    }

    frame->names[frame->count] = name;
    frame->local_ids[frame->count] = local_id;
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
            if (strcmp(frame->names[i - 1], name) == 0) {
                if (out_local_id) {
                    *out_local_id = frame->local_ids[i - 1];
                }
                return 1;
            }
        }
    }

    return 0;
}

#endif