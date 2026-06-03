#if defined(__INTELLISENSE__) && !defined(SEMANTIC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(SEMANTIC_SPLIT_AGGREGATOR)
#define SEMANTIC_SPLIT_INTELLISENSE_CONTEXT
#include "semantic.c"
#else

typedef struct {
    const char **names;
    unsigned char *is_const_flags;
    unsigned char *value_kinds;
    const char **type_names;
    AstFunctionReturnType *function_return_types;
    size_t *function_parameter_counts;
    const int **function_parameter_value_kinds;
    const AstFunctionReturnType **function_parameter_return_types;
    const size_t **function_parameter_parameter_counts;
    const int *const **function_parameter_parameter_value_kinds;
    const AstFunctionReturnType *const **function_parameter_parameter_return_types;
    const size_t *const **function_parameter_parameter_parameter_counts;
    size_t count;
    size_t capacity;
    size_t *name_index_slots;
    size_t name_index_capacity;
} ScopeFrame;

typedef struct {
    ScopeFrame *frames;
    size_t count;
    size_t capacity;
} ScopeStack;

static int semantic_expression_resolve_scalar_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type,
    SemanticError *error);
static int semantic_expression_contains_float_scalar(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes);
static int semantic_validate_scalar_expression_usage(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_set_float_expression_usage_error(const AstExpression *expr,
    SemanticError *error);
static int semantic_set_float_conversion_usage_error(const AstExpression *expr,
    SemanticError *error);
static int semantic_validate_call_arguments_against_function_type(
    const AstExpression *call_expr,
    const SemanticTypeDescriptor *expected_function_type,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_call_argument_scalar_types(const AstExpression *call_expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_scalar_initializer_type(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    int allow_extension_features,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_scalar_assignment_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
    const AstExternal *current_func,
    const char *name,
    SemanticTypeDescriptor *out_type);
static int semantic_validate_scalar_comparison_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_scalar_arithmetic_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_control_condition_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_return_expression_type(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_expression_check_extension_mode(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int allow_extension_features,
    SemanticError *error,
    size_t depth);
static int semantic_validate_closure_literal_initializer(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_closure_signature_matches_expected(const AstExpression *closure_expr,
    const SemanticTypeDescriptor *expected_type);
static int semantic_name_is_in_list(const char *const *names,
    size_t count,
    const char *name);
static int semantic_closure_body_is_phase1_supported(const AstStatement *body,
    AstFunctionReturnType return_type,
    const AstProgram *program,
    size_t func_index,
    const char *const *capture_names,
    size_t capture_count,
    const char *const *parameter_names,
    size_t parameter_count,
    SemanticError *error);
static int semantic_closure_capture_is_supported(const char *capture_name,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error,
    int line,
    int column);
static int semantic_call_expression_extension_mode_accepts_direct_closure_argument(
    const AstExpression *call_expr,
    const AstProgram *program,
    size_t func_index,
    const AstExpression *candidate_arg);
static int semantic_validate_closure_literal_function_argument_expression(
    const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_function_valued_return_closure_expression(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_validate_function_valued_return_expression(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error);
static int semantic_function_valued_return_definition_has_consistent_closure_family(
    const AstExternal *func,
    const AstProgram *program,
    size_t func_index,
    SemanticError *error);
static int semantic_expression_is_signed_float_literal(const AstExpression *expr);
static int semantic_expression_is_direct_float_transport_root(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes);
static int semantic_expression_is_float_condition_root(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes);
static int semantic_resolve_local_aggregate_member_type(const AstExpression *expr,
    const AstProgram *program,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type,
    const char **out_base_name,
    size_t *out_field_index);
static int semantic_resolve_supported_aggregate_value_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type);
static int scope_stack_find_local_frame_index(const ScopeStack *stack,
    const char *name,
    size_t *out_frame_index);

static int scope_next_growth_capacity(size_t current,
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

static size_t scope_hash_name(const char *name) {
    const unsigned char *cursor = (const unsigned char *)name;
    size_t hash = 1469598103934665603ull;

    if (!name) {
        return 0u;
    }

    while (*cursor != '\0') {
        hash ^= (size_t)(*cursor++);
        hash *= 1099511628211ull;
    }

    return hash;
}

static int scope_frame_rebuild_name_index(ScopeFrame *frame, size_t new_capacity) {
    size_t *new_slots;
    size_t index;

    if (!frame) {
        return 0;
    }
    if (new_capacity == 0u) {
        free(frame->name_index_slots);
        frame->name_index_slots = NULL;
        frame->name_index_capacity = 0u;
        return 1;
    }

    new_slots = (size_t *)calloc(new_capacity, sizeof(size_t));
    if (!new_slots) {
        return 0;
    }

    for (index = 0u; index < frame->count; ++index) {
        const char *name = frame->names[index];
        size_t slot;

        if (!name || name[0] == '\0') {
            continue;
        }

        slot = scope_hash_name(name) % new_capacity;
        while (new_slots[slot] != 0u) {
            slot = (slot + 1u) % new_capacity;
        }
        new_slots[slot] = index + 1u;
    }

    free(frame->name_index_slots);
    frame->name_index_slots = new_slots;
    frame->name_index_capacity = new_capacity;
    return 1;
}

static int scope_frame_ensure_name_index_capacity(ScopeFrame *frame, size_t next_count) {
    size_t desired_capacity;

    if (!frame) {
        return 0;
    }
    if (next_count == 0u) {
        return 1;
    }

    desired_capacity = frame->name_index_capacity > 0u ? frame->name_index_capacity : 16u;
    while (next_count * 2u >= desired_capacity) {
        if (desired_capacity > ((size_t)-1) / 2u) {
            return 0;
        }
        desired_capacity *= 2u;
    }

    if (desired_capacity == frame->name_index_capacity) {
        return 1;
    }

    return scope_frame_rebuild_name_index(frame, desired_capacity);
}

static int scope_frame_lookup_name_index(const ScopeFrame *frame,
    const char *name,
    size_t *out_index) {
    size_t slot;
    size_t start_slot;

    if (out_index) {
        *out_index = 0u;
    }
    if (!frame || !name || name[0] == '\0' || !frame->name_index_slots || frame->name_index_capacity == 0u) {
        return 0;
    }

    slot = scope_hash_name(name) % frame->name_index_capacity;
    start_slot = slot;
    while (frame->name_index_slots[slot] != 0u) {
        size_t name_index = frame->name_index_slots[slot] - 1u;
        const char *candidate = frame->names[name_index];

        if (candidate && strcmp(candidate, name) == 0) {
            if (out_index) {
                *out_index = name_index;
            }
            return 1;
        }
        slot = (slot + 1u) % frame->name_index_capacity;
        if (slot == start_slot) {
            break;
        }
    }

    return 0;
}

static void scope_frame_free(ScopeFrame *frame) {
    if (!frame) {
        return;
    }

    free(frame->names);
    free(frame->is_const_flags);
    free(frame->value_kinds);
    free(frame->type_names);
    free(frame->function_return_types);
    free(frame->function_parameter_counts);
    free(frame->function_parameter_value_kinds);
    free(frame->function_parameter_return_types);
    free(frame->function_parameter_parameter_counts);
    free(frame->function_parameter_parameter_value_kinds);
    free(frame->function_parameter_parameter_return_types);
    free(frame->function_parameter_parameter_parameter_counts);
    free(frame->name_index_slots);
    frame->names = NULL;
    frame->is_const_flags = NULL;
    frame->value_kinds = NULL;
    frame->type_names = NULL;
    frame->function_return_types = NULL;
    frame->function_parameter_counts = NULL;
    frame->function_parameter_value_kinds = NULL;
    frame->function_parameter_return_types = NULL;
    frame->function_parameter_parameter_counts = NULL;
    frame->function_parameter_parameter_value_kinds = NULL;
    frame->function_parameter_parameter_return_types = NULL;
    frame->function_parameter_parameter_parameter_counts = NULL;
    frame->name_index_slots = NULL;
    frame->count = 0;
    frame->capacity = 0;
    frame->name_index_capacity = 0;
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
        if (!scope_next_growth_capacity(stack->capacity,
            8,
            sizeof(ScopeFrame),
            &next_capacity)) {
            return 0;
        }
        new_frames = (ScopeFrame *)realloc(stack->frames, next_capacity * sizeof(ScopeFrame));
        if (!new_frames) {
            return 0;
        }
        stack->frames = new_frames;
        stack->capacity = next_capacity;
    }

    stack->frames[stack->count].names = NULL;
    stack->frames[stack->count].is_const_flags = NULL;
    stack->frames[stack->count].value_kinds = NULL;
    stack->frames[stack->count].type_names = NULL;
    stack->frames[stack->count].function_return_types = NULL;
    stack->frames[stack->count].function_parameter_counts = NULL;
    stack->frames[stack->count].function_parameter_value_kinds = NULL;
    stack->frames[stack->count].function_parameter_return_types = NULL;
    stack->frames[stack->count].function_parameter_parameter_counts = NULL;
    stack->frames[stack->count].function_parameter_parameter_value_kinds = NULL;
    stack->frames[stack->count].function_parameter_parameter_return_types = NULL;
    stack->frames[stack->count].function_parameter_parameter_parameter_counts = NULL;
    stack->frames[stack->count].name_index_slots = NULL;
    stack->frames[stack->count].count = 0;
    stack->frames[stack->count].capacity = 0;
    stack->frames[stack->count].name_index_capacity = 0;
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
    if (!frame || !name || name[0] == '\0') {
        return 0;
    }
    return scope_frame_lookup_name_index(frame, name, NULL);
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

static int scope_stack_add_to_current(ScopeStack *stack,
    const char *name,
    int is_const_qualified,
    int declaration_value_kind,
    const char *type_name,
    AstFunctionReturnType function_return_type,
    size_t function_parameter_count,
    const int *function_parameter_value_kinds,
    const AstFunctionReturnType *function_parameter_return_types,
    const size_t *function_parameter_parameter_counts,
    const int *const *function_parameter_parameter_value_kinds,
    const AstFunctionReturnType *const *function_parameter_parameter_return_types,
    const size_t *const *function_parameter_parameter_parameter_counts) {
    ScopeFrame *frame;
    const char **new_names;
    unsigned char *new_flags;
    unsigned char *new_value_kinds;
    const char **new_type_names;
    AstFunctionReturnType *new_function_return_types;
    size_t *new_function_parameter_counts;
    const int **new_function_parameter_value_kinds;
    const AstFunctionReturnType **new_function_parameter_return_types;
    const size_t **new_function_parameter_parameter_counts;
    const int *const **new_function_parameter_parameter_value_kinds;
    const AstFunctionReturnType *const **new_function_parameter_parameter_return_types;
    const size_t *const **new_function_parameter_parameter_parameter_counts;
    size_t next_capacity;

    if (!stack || stack->count == 0 || !name || name[0] == '\0') {
        return 0;
    }

    frame = &stack->frames[stack->count - 1];
    if (frame->count == frame->capacity) {
        if (!scope_next_growth_capacity(frame->capacity,
            8,
            sizeof(const char *),
            &next_capacity)) {
            return 0;
        }
        new_names = (const char **)realloc(frame->names, next_capacity * sizeof(const char *));
        if (!new_names) {
            return 0;
        }
        new_flags = (unsigned char *)realloc(frame->is_const_flags, next_capacity * sizeof(unsigned char));
        if (!new_flags) {
            return 0;
        }
        new_value_kinds = (unsigned char *)realloc(frame->value_kinds, next_capacity * sizeof(unsigned char));
        if (!new_value_kinds) {
            return 0;
        }
        new_type_names = (const char **)realloc(frame->type_names, next_capacity * sizeof(const char *));
        if (!new_type_names) {
            return 0;
        }
        new_function_return_types = (AstFunctionReturnType *)realloc(
            frame->function_return_types,
            next_capacity * sizeof(AstFunctionReturnType));
        if (!new_function_return_types) {
            return 0;
        }
        new_function_parameter_counts = (size_t *)realloc(
            frame->function_parameter_counts,
            next_capacity * sizeof(size_t));
        if (!new_function_parameter_counts) {
            return 0;
        }
        new_function_parameter_value_kinds = (const int **)realloc(
            frame->function_parameter_value_kinds,
            next_capacity * sizeof(const int *));
        if (!new_function_parameter_value_kinds) {
            return 0;
        }
        new_function_parameter_return_types = (const AstFunctionReturnType **)realloc(
            frame->function_parameter_return_types,
            next_capacity * sizeof(const AstFunctionReturnType *));
        if (!new_function_parameter_return_types) {
            return 0;
        }
        new_function_parameter_parameter_counts = (const size_t **)realloc(
            frame->function_parameter_parameter_counts,
            next_capacity * sizeof(const size_t *));
        if (!new_function_parameter_parameter_counts) {
            return 0;
        }
        new_function_parameter_parameter_value_kinds = (const int *const **)realloc(
            frame->function_parameter_parameter_value_kinds,
            next_capacity * sizeof(const int *const *));
        if (!new_function_parameter_parameter_value_kinds) {
            return 0;
        }
        new_function_parameter_parameter_return_types = (const AstFunctionReturnType *const **)realloc(
            frame->function_parameter_parameter_return_types,
            next_capacity * sizeof(const AstFunctionReturnType *const *));
        if (!new_function_parameter_parameter_return_types) {
            return 0;
        }
        new_function_parameter_parameter_parameter_counts = (const size_t *const **)realloc(
            frame->function_parameter_parameter_parameter_counts,
            next_capacity * sizeof(const size_t *const *));
        if (!new_function_parameter_parameter_parameter_counts) {
            return 0;
        }
        frame->names = new_names;
        frame->is_const_flags = new_flags;
        frame->value_kinds = new_value_kinds;
        frame->type_names = new_type_names;
        frame->function_return_types = new_function_return_types;
        frame->function_parameter_counts = new_function_parameter_counts;
        frame->function_parameter_value_kinds = new_function_parameter_value_kinds;
        frame->function_parameter_return_types = new_function_parameter_return_types;
        frame->function_parameter_parameter_counts = new_function_parameter_parameter_counts;
        frame->function_parameter_parameter_value_kinds =
            new_function_parameter_parameter_value_kinds;
        frame->function_parameter_parameter_return_types =
            new_function_parameter_parameter_return_types;
        frame->function_parameter_parameter_parameter_counts =
            new_function_parameter_parameter_parameter_counts;
        frame->capacity = next_capacity;
    }

    frame->names[frame->count] = name;
    frame->is_const_flags[frame->count] = (unsigned char)(is_const_qualified != 0);
    frame->value_kinds[frame->count] = (unsigned char)declaration_value_kind;
    frame->type_names[frame->count] = type_name;
    frame->function_return_types[frame->count] = function_return_type;
    frame->function_parameter_counts[frame->count] = function_parameter_count;
    frame->function_parameter_value_kinds[frame->count] = function_parameter_value_kinds;
    frame->function_parameter_return_types[frame->count] = function_parameter_return_types;
    frame->function_parameter_parameter_counts[frame->count] = function_parameter_parameter_counts;
    frame->function_parameter_parameter_value_kinds[frame->count] =
        function_parameter_parameter_value_kinds;
    frame->function_parameter_parameter_return_types[frame->count] =
        function_parameter_parameter_return_types;
    frame->function_parameter_parameter_parameter_counts[frame->count] =
        function_parameter_parameter_parameter_counts;
    frame->count++;
    if (!scope_frame_ensure_name_index_capacity(frame, frame->count)) {
        return 0;
    }
    {
        size_t slot = scope_hash_name(name) % frame->name_index_capacity;

        while (frame->name_index_slots[slot] != 0u) {
            slot = (slot + 1u) % frame->name_index_capacity;
        }
        frame->name_index_slots[slot] = frame->count;
    }
    return 1;
}

static int scope_stack_lookup_local_const_flag(const ScopeStack *stack,
    const char *name,
    int *out_found,
    int *out_is_const) {
    size_t frame_index;

    if (out_found) {
        *out_found = 0;
    }
    if (out_is_const) {
        *out_is_const = 0;
    }
    if (!stack || !name || name[0] == '\0') {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        const ScopeFrame *frame = &stack->frames[frame_index - 1];
        size_t name_index = 0u;

        if (scope_frame_lookup_name_index(frame, name, &name_index)) {
            if (out_found) {
                *out_found = 1;
            }
            if (out_is_const) {
                *out_is_const = frame->is_const_flags && frame->is_const_flags[name_index] != 0;
            }
            return 1;
        }
    }

    return 0;
}

static int scope_stack_lookup_local_value_kind(const ScopeStack *stack,
    const char *name,
    int *out_found,
    int *out_value_kind) {
    size_t frame_index;

    if (out_found) {
        *out_found = 0;
    }
    if (out_value_kind) {
        *out_value_kind = AST_DECLARATION_VALUE_INT;
    }
    if (!stack || !name || name[0] == '\0') {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        const ScopeFrame *frame = &stack->frames[frame_index - 1];
        size_t name_index = 0u;

        if (scope_frame_lookup_name_index(frame, name, &name_index)) {
            if (out_found) {
                *out_found = 1;
            }
            if (out_value_kind) {
                *out_value_kind = frame->value_kinds ? frame->value_kinds[name_index] : AST_DECLARATION_VALUE_INT;
            }
            return 1;
        }
    }

    return 0;
}

static int scope_stack_lookup_local_type_name(const ScopeStack *stack,
    const char *name,
    int *out_found,
    const char **out_type_name) {
    size_t frame_index;

    if (out_found) {
        *out_found = 0;
    }
    if (out_type_name) {
        *out_type_name = NULL;
    }
    if (!stack || !name || name[0] == '\0') {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        const ScopeFrame *frame = &stack->frames[frame_index - 1];
        size_t name_index = 0u;

        if (scope_frame_lookup_name_index(frame, name, &name_index)) {
            if (out_found) {
                *out_found = 1;
            }
            if (out_type_name) {
                *out_type_name = frame->type_names ? frame->type_names[name_index] : NULL;
            }
            return 1;
        }
    }

    return 0;
}

static int semantic_top_level_name_visible(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const char *name) {
    size_t i;
    size_t visible_external_count;

    if (!program || !name || name[0] == '\0') {
        return 0;
    }

    visible_external_count = include_current_external ? (func_index + 1) : func_index;
    if (visible_external_count > program->count) {
        visible_external_count = program->count;
    }

    for (i = 0; i < visible_external_count; ++i) {
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
    int include_current_external,
    const ScopeStack *scopes,
    const char *name) {
    if (scope_stack_contains_local_name(scopes, name)) {
        return 1;
    }

    if (semantic_find_builtin_function(name)) {
        return 1;
    }

    return semantic_top_level_name_visible(program,
        func_index,
        include_current_external,
        name);
}

static int semantic_top_level_name_is_const(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const char *name) {
    size_t i;
    size_t visible_external_count;

    if (!program || !name || name[0] == '\0') {
        return 0;
    }

    visible_external_count = include_current_external ? (func_index + 1u) : func_index;
    if (visible_external_count > program->count) {
        visible_external_count = program->count;
    }

    for (i = visible_external_count; i > 0; --i) {
        const AstExternal *ext = &program->externals[i - 1u];

        if (!ext->name) {
            continue;
        }
        if (strcmp(ext->name, name) == 0) {
            return ext->is_const_qualified != 0;
        }
    }

    return 0;
}

static int semantic_scope_name_is_const(const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    const char *name) {
    int found = 0;
    int is_const = 0;

    if (scope_stack_lookup_local_const_flag(scopes, name, &found, &is_const) && found) {
        return is_const;
    }
    return semantic_top_level_name_is_const(program, func_index, include_current_external, name);
}

static int semantic_scope_lookup_local_scalar_type_descriptor(const ScopeStack *scopes,
    const char *name,
    SemanticTypeDescriptor *out_type) {
    int found = 0;
    int value_kind = AST_DECLARATION_VALUE_INT;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!scopes || !name || name[0] == '\0' || !out_type) {
        return 0;
    }

    if (!scope_stack_lookup_local_value_kind(scopes, name, &found, &value_kind) || !found) {
        return 0;
    }

    switch (value_kind) {
    case AST_DECLARATION_VALUE_INT:
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    case AST_DECLARATION_VALUE_FLOAT:
        out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
        return 1;
    default:
        return 0;
    }
}

static int semantic_scope_lookup_local_function_type_descriptor(const ScopeStack *scopes,
    const char *name,
    SemanticTypeDescriptor *out_type) {
    int found = 0;
    int value_kind = AST_DECLARATION_VALUE_INT;
    size_t frame_index;
    const ScopeFrame *frame;
    size_t name_index = 0u;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!scopes || !name || name[0] == '\0' || !out_type) {
        return 0;
    }

    if (!scope_stack_lookup_local_value_kind(scopes, name, &found, &value_kind) ||
        !found ||
        value_kind != AST_DECLARATION_VALUE_FUNCTION ||
        !scope_stack_find_local_frame_index(scopes, name, &frame_index)) {
        return 0;
    }

    frame = &scopes->frames[frame_index];
    if (!scope_frame_lookup_name_index(frame, name, &name_index)) {
        return 0;
    }

    out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
    out_type->function_return_type =
        frame->function_return_types ? frame->function_return_types[name_index] : AST_FUNCTION_RETURN_INT;
    out_type->parameter_count =
        frame->function_parameter_counts ? frame->function_parameter_counts[name_index] : 0u;
    out_type->parameter_value_kinds =
        frame->function_parameter_value_kinds ? frame->function_parameter_value_kinds[name_index] : NULL;
    out_type->parameter_function_return_types =
        frame->function_parameter_return_types ? frame->function_parameter_return_types[name_index] : NULL;
    out_type->parameter_function_parameter_counts =
        frame->function_parameter_parameter_counts ? frame->function_parameter_parameter_counts[name_index] : NULL;
    out_type->parameter_function_parameter_value_kinds =
        frame->function_parameter_parameter_value_kinds
            ? frame->function_parameter_parameter_value_kinds[name_index]
            : NULL;
    out_type->parameter_function_parameter_return_types =
        frame->function_parameter_parameter_return_types
            ? frame->function_parameter_parameter_return_types[name_index]
            : NULL;
    out_type->parameter_function_parameter_parameter_counts =
        frame->function_parameter_parameter_parameter_counts
            ? frame->function_parameter_parameter_parameter_counts[name_index]
            : NULL;
    return 1;
}

static int semantic_scope_is_allowed_function_value_assignment_expr(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    const AstExternal *current_func) {
    const AstExpression *base = unwrap_paren_expression(expr);
    const AstExpression *lhs_base = NULL;
    const AstExpression *rhs_base = NULL;
    const char *rhs_name = NULL;
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;

    if (!base || base->kind != AST_EXPR_BINARY || base->as.binary.op != TOKEN_ASSIGN) {
        return 0;
    }
    lhs_base = unwrap_paren_expression(base->as.binary.left);
    rhs_base = semantic_unwrap_function_value_wrapper_expression(base->as.binary.right);
    if (!lhs_base || !rhs_base ||
        lhs_base->kind != AST_EXPR_IDENTIFIER || !lhs_base->as.identifier.name) {
        return 0;
    }
    if (!semantic_scope_lookup_local_function_type_descriptor(scopes, lhs_base->as.identifier.name, &lhs_type) ||
        lhs_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        if (!(current_func &&
                semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
                    current_func,
                    lhs_base->as.identifier.name,
                    &lhs_type) &&
                lhs_type.kind == SEMANTIC_TYPE_KIND_FUNCTION)) {
            return 0;
        }
    }

    if (rhs_base->kind == AST_EXPR_TERNARY) {
        if (!semantic_validate_function_valued_return_expression(
                rhs_base->as.ternary.then_expr,
                &lhs_type,
                program,
                func_index,
                scopes,
                NULL) ||
            !semantic_validate_function_valued_return_expression(
                rhs_base->as.ternary.else_expr,
                &lhs_type,
                program,
                func_index,
                scopes,
                NULL)) {
            return 0;
        }
        return 1;
    }

    if (rhs_base->kind == AST_EXPR_CALL) {
        return semantic_validate_function_valued_return_expression(
            rhs_base,
            &lhs_type,
            program,
            func_index,
            scopes,
            NULL);
    }

    if (rhs_base->kind != AST_EXPR_IDENTIFIER || !rhs_base->as.identifier.name) {
        return 0;
    }

    rhs_name = rhs_base->as.identifier.name;
    if (current_func &&
        semantic_function_parameter_name_is_function_valued(current_func, rhs_name)) {
        return 1;
    }
    if (semantic_scope_lookup_local_function_type_descriptor(scopes, rhs_name, &rhs_type) &&
        rhs_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return semantic_type_descriptors_are_compatible(&lhs_type, &rhs_type);
    }
    if (current_func &&
        semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
            current_func,
            rhs_name,
            &rhs_type) &&
        rhs_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return semantic_type_descriptors_are_compatible(&lhs_type, &rhs_type);
    }
    if (program) {
        const AstExternal *rhs_function = semantic_find_visible_function_external(
            program,
            func_index,
            include_current_external,
            rhs_name);
        if (rhs_function && rhs_function->kind == AST_EXTERNAL_FUNCTION) {
            return semantic_function_signature_matches_type_descriptor(
                rhs_function,
                &lhs_type);
        }
    }
    {
        const SemanticBuiltinFunctionInfo *builtin = semantic_find_builtin_function(rhs_name);
        if (builtin) {
            return semantic_builtin_signature_matches_type_descriptor(
                builtin,
                &lhs_type);
        }
    }

    return 0;
}

static int semantic_scope_parameter_value_kind_to_declaration_value_kind(int parameter_value_kind) {
    switch (parameter_value_kind) {
    case AST_PARAMETER_VALUE_FLOAT:
        return AST_DECLARATION_VALUE_FLOAT;
    case AST_PARAMETER_VALUE_PAIR:
        return AST_DECLARATION_VALUE_PAIR;
    case AST_PARAMETER_VALUE_STRUCT:
        return AST_DECLARATION_VALUE_STRUCT;
    case AST_PARAMETER_VALUE_INT:
    case AST_PARAMETER_VALUE_FUNCTION:
    default:
        return AST_DECLARATION_VALUE_INT;
    }
}

static int scope_stack_find_local_frame_index(const ScopeStack *stack,
    const char *name,
    size_t *out_frame_index) {
    size_t frame_index;

    if (out_frame_index) {
        *out_frame_index = 0u;
    }
    if (!stack || !name || name[0] == '\0') {
        return 0;
    }

    for (frame_index = stack->count; frame_index > 0; --frame_index) {
        if (scope_frame_contains_name(&stack->frames[frame_index - 1u], name)) {
            if (out_frame_index) {
                *out_frame_index = frame_index - 1u;
            }
            return 1;
        }
    }

    return 0;
}

static int semantic_binary_op_is_assignment(TokenType op) {
    switch (op) {
    case TOKEN_ASSIGN:
    case TOKEN_PLUS_ASSIGN:
    case TOKEN_MINUS_ASSIGN:
    case TOKEN_STAR_ASSIGN:
    case TOKEN_SLASH_ASSIGN:
    case TOKEN_PERCENT_ASSIGN:
    case TOKEN_AMP_ASSIGN:
    case TOKEN_CARET_ASSIGN:
    case TOKEN_PIPE_ASSIGN:
    case TOKEN_SHIFT_LEFT_ASSIGN:
    case TOKEN_SHIFT_RIGHT_ASSIGN:
        return 1;
    default:
        return 0;
    }
}

static const char *semantic_mutated_lvalue_identifier_name(const AstExpression *expr) {
    const AstExpression *base = unwrap_paren_expression(expr);

    if (!base) {
        return NULL;
    }

    switch (base->kind) {
    case AST_EXPR_IDENTIFIER:
        return base->as.identifier.name;
    case AST_EXPR_SUBSCRIPT:
        return semantic_mutated_lvalue_identifier_name(base->as.subscript.base);
    case AST_EXPR_MEMBER:
        return semantic_mutated_lvalue_identifier_name(base->as.member.base);
    case AST_EXPR_PAREN:
        return semantic_mutated_lvalue_identifier_name(base->as.inner);
    default:
        return NULL;
    }
}

static const AstStructType *semantic_find_struct_type(const AstProgram *program, const char *name) {
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

static int semantic_scope_lookup_local_aggregate_type_descriptor(const ScopeStack *scopes,
    const char *name,
    SemanticTypeDescriptor *out_type) {
    int found = 0;
    int value_kind = AST_DECLARATION_VALUE_INT;
    const char *type_name = NULL;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }

    if (!scopes || !name || name[0] == '\0' || !out_type) {
        return 0;
    }

    if (!scope_stack_lookup_local_value_kind(scopes, name, &found, &value_kind) || !found) {
        return 0;
    }

    out_type->aggregate_kind = semantic_aggregate_kind_from_declaration_value_kind(value_kind);
    out_type->kind = out_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_NONE
        ? SEMANTIC_TYPE_KIND_INVALID
        : SEMANTIC_TYPE_KIND_AGGREGATE;
    if (out_type->kind != SEMANTIC_TYPE_KIND_AGGREGATE) {
        return 0;
    }

    if (out_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
        if (!scope_stack_lookup_local_type_name(scopes, name, &found, &type_name) || !found || !type_name) {
            return 0;
        }
        out_type->named_type_name = type_name;
    }

    return 1;
}

static int semantic_validate_function_valued_return_closure_expression(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    size_t capture_index;

    if (!base || base->kind != AST_EXPR_CLOSURE || !expected_type ||
        expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        semantic_set_error(error,
            expr ? expr->line : 0,
            expr ? expr->column : 0,
            "SEMA-EXT-048: function-valued returns currently require returning one closure literal");
        return 0;
    }
    if (!semantic_closure_signature_matches_expected(base, expected_type)) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-045: closure literal signature must match the target function-valued return type in the current extension slice");
        return 0;
    }
    if (!semantic_closure_body_is_phase1_supported(
            base->as.closure.body,
            base->as.closure.return_type,
            program,
            func_index,
            (const char *const *)base->as.closure.capture_names,
            base->as.closure.capture_name_count,
            (const char *const *)base->as.closure.parameter_names,
            base->as.closure.parameter_count,
            error)) {
        return 0;
    }
    for (capture_index = 0u; capture_index < base->as.closure.capture_name_count; ++capture_index) {
        const char *capture_name =
            base->as.closure.capture_names ? base->as.closure.capture_names[capture_index] : NULL;
        size_t prior_index;

        for (prior_index = 0u; prior_index < capture_index; ++prior_index) {
            if (capture_name &&
                base->as.closure.capture_names &&
                base->as.closure.capture_names[prior_index] &&
                strcmp(capture_name, base->as.closure.capture_names[prior_index]) == 0) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-042: closure captures must be unique in the first extension slice");
                return 0;
            }
        }
        if (!semantic_name_is_in_list(
                (const char *const *)base->as.closure.parameter_names,
                base->as.closure.parameter_count,
                capture_name) &&
            !semantic_closure_capture_is_supported(
                capture_name,
                program,
                func_index,
                scopes,
                error,
                expr->line,
                expr->column)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_validate_function_valued_return_expression(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = semantic_unwrap_function_value_wrapper_expression(expr);
    SemanticTypeDescriptor actual_type;
    const AstExternal *visible_function = NULL;
    const AstExternal *current_func = NULL;
    const SemanticBuiltinFunctionInfo *builtin = NULL;

    if (!base || !expected_type || expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }
    if (program && func_index < program->count &&
        program->externals[func_index].kind == AST_EXTERNAL_FUNCTION) {
        current_func = &program->externals[func_index];
    }
    if (base->kind == AST_EXPR_CLOSURE) {
        return semantic_validate_function_valued_return_closure_expression(
            expr,
            expected_type,
            program,
            func_index,
            scopes,
            error);
    }
    if (base->kind == AST_EXPR_TERNARY) {
        const AstExpression *then_expr = base->as.ternary.then_expr;
        const AstExpression *else_expr = base->as.ternary.else_expr;

        if (!then_expr || !else_expr) {
            semantic_set_error(error,
                expr ? expr->line : 0,
                expr ? expr->column : 0,
                "SEMA-EXT-048: function-valued returns currently require returning one closure literal or one compatible non-capturing function value");
            return 0;
        }
        return semantic_validate_function_valued_return_expression(
                then_expr,
                expected_type,
                program,
                func_index,
                scopes,
                error) &&
            semantic_validate_function_valued_return_expression(
                else_expr,
                expected_type,
                program,
                func_index,
                scopes,
                error);
    }
    if (base->kind == AST_EXPR_CALL) {
        const char *called_name = NULL;

        if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || called_name[0] == '\0') {
            semantic_set_error(error,
                expr ? expr->line : 0,
                expr ? expr->column : 0,
                "SEMA-EXT-048: function-valued returns currently require returning one closure literal or one compatible non-capturing function value");
            return 0;
        }
        visible_function = semantic_find_visible_function_external(
            program,
            func_index,
            1,
            called_name);
        if (!visible_function ||
            !semantic_function_value_return_matches_type_descriptor(
                visible_function,
                expected_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-045: function-valued return expression must match the target function-valued return type in the current extension slice");
            return 0;
        }
        return 1;
    }
    if (base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        semantic_set_error(error,
            expr ? expr->line : 0,
            expr ? expr->column : 0,
            "SEMA-EXT-048: function-valued returns currently require returning one closure literal or one compatible non-capturing function value");
        return 0;
    }
    if (semantic_scope_lookup_local_function_type_descriptor(scopes, base->as.identifier.name, &actual_type) &&
        actual_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        if (!semantic_type_descriptors_are_compatible(expected_type, &actual_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-045: function-valued return expression must match the target function-valued return type in the current extension slice");
            return 0;
        }
        return 1;
    }
    if (current_func &&
        semantic_find_named_function_parameter_type_descriptor(
            current_func,
            base->as.identifier.name,
            NULL,
            &actual_type) &&
        actual_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        if (!semantic_type_descriptors_are_compatible(expected_type, &actual_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-045: function-valued return expression must match the target function-valued return type in the current extension slice");
            return 0;
        }
        return 1;
    }

    visible_function = semantic_find_visible_function_external(
        program,
        func_index,
        1,
        base->as.identifier.name);
    if (visible_function) {
        if (!semantic_function_signature_matches_type_descriptor(
                visible_function,
                expected_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-045: function-valued return expression must match the target function-valued return type in the current extension slice");
            return 0;
        }
        return 1;
    }

    builtin = semantic_find_builtin_function(base->as.identifier.name);
    if (builtin) {
        if (!semantic_builtin_signature_matches_type_descriptor(
                builtin,
                expected_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-045: function-valued return expression must match the target function-valued return type in the current extension slice");
            return 0;
        }
        return 1;
    }

    semantic_set_error(error,
        expr->line,
        expr->column,
        "SEMA-EXT-048: function-valued returns currently require returning one closure literal or one compatible non-capturing function value");
    return 0;
}

static int semantic_scope_build_declaration_type_descriptor(const AstProgram *program,
    const ScopeStack *scopes,
    const AstStatement *stmt,
    size_t declaration_index,
    SemanticTypeDescriptor *out_type,
    SemanticError *error) {
    int declaration_value_kind;
    const char *type_name = NULL;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!stmt || !out_type || declaration_index >= stmt->declaration_name_count) {
        return 0;
    }
    (void)scopes;

    declaration_value_kind = stmt->declaration_value_kinds
        ? stmt->declaration_value_kinds[declaration_index]
        : AST_DECLARATION_VALUE_INT;

    switch (declaration_value_kind) {
    case AST_DECLARATION_VALUE_INT:
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        break;
    case AST_DECLARATION_VALUE_FLOAT:
        out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
        break;
    case AST_DECLARATION_VALUE_FUNCTION:
        out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
        out_type->function_return_type =
            stmt->declaration_function_return_types
                ? stmt->declaration_function_return_types[declaration_index]
                : AST_FUNCTION_RETURN_INT;
        out_type->parameter_count =
            stmt->declaration_function_parameter_counts
                ? stmt->declaration_function_parameter_counts[declaration_index]
                : 0u;
        out_type->parameter_value_kinds =
            stmt->declaration_function_parameter_value_kinds
                ? stmt->declaration_function_parameter_value_kinds[declaration_index]
                : NULL;
        out_type->parameter_function_return_types =
            stmt->declaration_function_parameter_return_types
                ? stmt->declaration_function_parameter_return_types[declaration_index]
                : NULL;
        out_type->parameter_function_parameter_counts =
            stmt->declaration_function_parameter_parameter_counts
                ? stmt->declaration_function_parameter_parameter_counts[declaration_index]
                : NULL;
        out_type->parameter_function_parameter_value_kinds =
            stmt->declaration_function_parameter_parameter_value_kinds
                ? (const int *const *)stmt->declaration_function_parameter_parameter_value_kinds[declaration_index]
                : NULL;
        out_type->parameter_function_parameter_return_types =
            stmt->declaration_function_parameter_parameter_return_types
                ? (const AstFunctionReturnType *const *)stmt->declaration_function_parameter_parameter_return_types[declaration_index]
                : NULL;
        out_type->parameter_function_parameter_parameter_counts =
            stmt->declaration_function_parameter_parameter_parameter_counts
                ? (const size_t *const *)stmt->declaration_function_parameter_parameter_parameter_counts[declaration_index]
                : NULL;
        break;
    case AST_DECLARATION_VALUE_PAIR:
    case AST_DECLARATION_VALUE_STRUCT:
        out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        out_type->aggregate_kind = semantic_aggregate_kind_from_declaration_value_kind(declaration_value_kind);
        break;
    default:
        return 0;
    }

    if (stmt->declaration_array_ranks && stmt->declaration_array_ranks[declaration_index] > 0u) {
        out_type->array_rank = stmt->declaration_array_ranks[declaration_index];
        out_type->array_extent_exprs = stmt->declaration_array_extent_exprs
            ? stmt->declaration_array_extent_exprs[declaration_index]
            : NULL;
    }

    if (out_type->kind == SEMANTIC_TYPE_KIND_AGGREGATE &&
        out_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
        type_name = stmt->declaration_type_names ? stmt->declaration_type_names[declaration_index] : NULL;
        if (!type_name) {
            if (error) {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-STRUCT-004: unknown struct type in local declaration");
            }
            return 0;
        }
        out_type->named_type_name = type_name;
        if (!semantic_find_struct_type(program, type_name)) {
            if (error) {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-STRUCT-004: unknown struct type in local declaration");
            }
            return 0;
        }
    }

    if (out_type->kind == SEMANTIC_TYPE_KIND_AGGREGATE &&
        out_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_PAIR &&
        stmt->declaration_array_ranks &&
        stmt->declaration_array_ranks[declaration_index] > 0u) {
        if (error) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-AGG-001: arrays of 'pair' are not supported in the current extension slice");
        }
        return 0;
    }

    if (out_type->kind == SEMANTIC_TYPE_KIND_AGGREGATE &&
        out_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT &&
        stmt->declaration_array_ranks &&
        stmt->declaration_array_ranks[declaration_index] > 0u) {
        if (error) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-STRUCT-005: arrays of extension structs are not supported in the current slice");
        }
        return 0;
    }

    if (out_type->kind == SEMANTIC_TYPE_KIND_FLOAT &&
        stmt->declaration_array_ranks &&
        stmt->declaration_array_ranks[declaration_index] > 0u) {
        if (error) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-037: arrays of 'float' are not supported in the current extension slice");
        }
        return 0;
    }

    return 1;
}

static int semantic_scope_build_function_return_type_descriptor(const AstExternal *func,
    SemanticTypeDescriptor *out_type) {
    if (!func || !out_type) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    if (func->has_function_return_signature) {
        out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
        out_type->function_return_type = func->function_return_function_return_type;
        out_type->named_type_name = func->function_return_function_return_type_name;
        out_type->parameter_count = func->function_return_function_parameter_count;
        out_type->parameter_value_kinds = func->function_return_function_parameter_value_kinds;
        out_type->parameter_function_return_types =
            func->function_return_function_parameter_return_types;
        out_type->parameter_function_parameter_counts =
            func->function_return_function_parameter_parameter_counts;
        out_type->parameter_function_parameter_value_kinds =
            (const int *const *)func->function_return_function_parameter_parameter_value_kinds;
        out_type->parameter_function_parameter_return_types =
            (const AstFunctionReturnType *const *)func->function_return_function_parameter_parameter_return_types;
        out_type->parameter_function_parameter_parameter_counts =
            (const size_t *const *)func->function_return_function_parameter_parameter_parameter_counts;
        return 1;
    }

    switch (func->function_return_type) {
    case AST_FUNCTION_RETURN_INT:
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    case AST_FUNCTION_RETURN_FLOAT:
        out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
        return 1;
    case AST_FUNCTION_RETURN_PAIR:
        out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        out_type->aggregate_kind = SEMANTIC_AGGREGATE_KIND_PAIR;
        return 1;
    case AST_FUNCTION_RETURN_STRUCT:
        out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        out_type->aggregate_kind = SEMANTIC_AGGREGATE_KIND_STRUCT;
        out_type->named_type_name = func->function_return_type_name;
        return out_type->named_type_name != NULL;
    case AST_FUNCTION_RETURN_VOID:
        out_type->kind = SEMANTIC_TYPE_KIND_VOID;
        return 1;
    default:
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
        return 0;
    }
}

static int semantic_expression_is_local_aggregate_member_access(const AstExpression *expr,
    const AstProgram *program,
    const ScopeStack *scopes,
    SemanticAggregateKind aggregate_kind,
    const char **out_base_name,
    const char **out_type_name,
    size_t *out_field_index) {
    const AstExpression *base = unwrap_paren_expression(expr);
    const AstExpression *aggregate_base = NULL;
    SemanticTypeDescriptor aggregate_type;

    if (out_base_name) {
        *out_base_name = NULL;
    }
    if (out_type_name) {
        *out_type_name = NULL;
    }
    if (out_field_index) {
        *out_field_index = 0u;
    }
    if (!base || base->kind != AST_EXPR_MEMBER || !base->as.member.field_name) {
        return 0;
    }

    aggregate_base = unwrap_paren_expression(base->as.member.base);
    if (!aggregate_base) {
        return 0;
    }

    if (aggregate_base->kind == AST_EXPR_IDENTIFIER) {
        if (!aggregate_base->as.identifier.name ||
            !semantic_scope_lookup_local_aggregate_type_descriptor(
                scopes,
                aggregate_base->as.identifier.name,
                &aggregate_type)) {
            return 0;
        }
        if (out_base_name) {
            *out_base_name = aggregate_base->as.identifier.name;
        }
    } else if (aggregate_base->kind == AST_EXPR_MEMBER) {
        if (!semantic_resolve_local_aggregate_member_type(
                aggregate_base,
                program,
                scopes,
                &aggregate_type,
                out_base_name,
                NULL) ||
            aggregate_type.kind != SEMANTIC_TYPE_KIND_AGGREGATE) {
            return 0;
        }
    } else {
        return 0;
    }

    if (aggregate_type.aggregate_kind != aggregate_kind) {
        return 0;
    }

    if (aggregate_kind == SEMANTIC_AGGREGATE_KIND_PAIR) {
        if (!(strcmp(base->as.member.field_name, "first") == 0 ||
                strcmp(base->as.member.field_name, "second") == 0)) {
            return 0;
        }
        if (out_field_index) {
            *out_field_index = strcmp(base->as.member.field_name, "second") == 0 ? 1u : 0u;
        }
        return 1;
    }

    if (aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
        const AstStructType *struct_type = semantic_find_struct_type(program, aggregate_type.named_type_name);
        size_t field_index;

        if (!struct_type || !struct_type->field_names) {
            return 0;
        }
        for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
            if (struct_type->field_names[field_index] &&
                strcmp(struct_type->field_names[field_index], base->as.member.field_name) == 0) {
                if (out_field_index) {
                    *out_field_index = field_index;
                }
                if (out_type_name) {
                    *out_type_name = aggregate_type.named_type_name;
                }
                return 1;
            }
        }
    }

    return 0;
}

static int semantic_resolve_local_aggregate_member_type(const AstExpression *expr,
    const AstProgram *program,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type,
    const char **out_base_name,
    size_t *out_field_index) {
    const AstExpression *base = unwrap_paren_expression(expr);
    const AstExpression *aggregate_base = NULL;
    SemanticTypeDescriptor aggregate_type;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (out_base_name) {
        *out_base_name = NULL;
    }
    if (out_field_index) {
        *out_field_index = 0u;
    }
    if (!base || !out_type || base->kind != AST_EXPR_MEMBER || !base->as.member.field_name) {
        return 0;
    }

    aggregate_base = unwrap_paren_expression(base->as.member.base);
    if (!aggregate_base) {
        return 0;
    }

    if (aggregate_base->kind == AST_EXPR_IDENTIFIER) {
        if (!aggregate_base->as.identifier.name ||
            !semantic_scope_lookup_local_aggregate_type_descriptor(scopes,
                aggregate_base->as.identifier.name,
                &aggregate_type)) {
            return 0;
        }
        if (out_base_name) {
            *out_base_name = aggregate_base->as.identifier.name;
        }
    } else if (aggregate_base->kind == AST_EXPR_MEMBER) {
        if (!semantic_resolve_local_aggregate_member_type(
                aggregate_base,
                program,
                scopes,
                &aggregate_type,
                out_base_name,
                NULL)) {
            return 0;
        }
        if (aggregate_type.kind != SEMANTIC_TYPE_KIND_AGGREGATE) {
            return 0;
        }
    } else {
        return 0;
    }

    if (aggregate_type.aggregate_kind == SEMANTIC_AGGREGATE_KIND_PAIR) {
        if (strcmp(base->as.member.field_name, "first") != 0 &&
            strcmp(base->as.member.field_name, "second") != 0) {
            return 0;
        }
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        if (out_field_index) {
            *out_field_index = strcmp(base->as.member.field_name, "second") == 0 ? 1u : 0u;
        }
        return 1;
    }

    if (aggregate_type.aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
        const AstStructType *struct_type = semantic_find_struct_type(program, aggregate_type.named_type_name);
        size_t field_index;

        if (!struct_type || !struct_type->field_names || !struct_type->field_value_kinds) {
            return 0;
        }
        for (field_index = 0u; field_index < struct_type->field_count; ++field_index) {
            if (struct_type->field_names[field_index] &&
                strcmp(struct_type->field_names[field_index], base->as.member.field_name) == 0) {
                int field_kind = struct_type->field_value_kinds[field_index];

                if (out_field_index) {
                    *out_field_index = field_index;
                }
                switch (field_kind) {
                case AST_DECLARATION_VALUE_INT:
                    out_type->kind = SEMANTIC_TYPE_KIND_INT;
                    return 1;
                case AST_DECLARATION_VALUE_PAIR:
                    out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
                    out_type->aggregate_kind = SEMANTIC_AGGREGATE_KIND_PAIR;
                    return 1;
                case AST_DECLARATION_VALUE_STRUCT:
                    out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
                    out_type->aggregate_kind = SEMANTIC_AGGREGATE_KIND_STRUCT;
                    out_type->named_type_name =
                        struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL;
                    return out_type->named_type_name != NULL;
                default:
                    return 0;
                }
            }
        }
    }

    return 0;
}

static int semantic_expression_get_local_aggregate_kind(const AstExpression *expr,
    const AstProgram *program,
    const ScopeStack *scopes,
    SemanticAggregateKind *out_kind,
    const char **out_name,
    const char **out_type_name) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor aggregate_type;

    if (out_kind) {
        *out_kind = SEMANTIC_AGGREGATE_KIND_NONE;
    }
    if (out_name) {
        *out_name = NULL;
    }
    if (out_type_name) {
        *out_type_name = NULL;
    }
    if (!base) {
        return 0;
    }

    if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
        if (!semantic_scope_lookup_local_aggregate_type_descriptor(scopes, base->as.identifier.name, &aggregate_type)) {
            return 0;
        }
        if (out_kind) {
            *out_kind = aggregate_type.aggregate_kind;
        }
        if (out_name) {
            *out_name = base->as.identifier.name;
        }
        if (out_type_name) {
            *out_type_name = aggregate_type.named_type_name;
        }
        return 1;
    }

    if (base->kind == AST_EXPR_MEMBER &&
        semantic_resolve_local_aggregate_member_type(base,
            program,
            scopes,
            &aggregate_type,
            out_name,
            NULL) &&
        aggregate_type.kind == SEMANTIC_TYPE_KIND_AGGREGATE) {
        if (out_kind) {
            *out_kind = aggregate_type.aggregate_kind;
        }
        if (out_type_name) {
            *out_type_name = aggregate_type.named_type_name;
        }
        return 1;
    }

    return 0;
}

static int semantic_resolve_local_aggregate_value_type(const AstExpression *expr,
    const AstProgram *program,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type) {
    SemanticAggregateKind aggregate_kind = SEMANTIC_AGGREGATE_KIND_NONE;
    const char *type_name = NULL;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!out_type) {
        return 0;
    }

    if (semantic_expression_get_local_aggregate_kind(expr,
            program,
            scopes,
            &aggregate_kind,
            NULL,
            &type_name)) {
        out_type->kind = SEMANTIC_TYPE_KIND_AGGREGATE;
        out_type->aggregate_kind = aggregate_kind;
        out_type->named_type_name = type_name;
        return 1;
    }

    return 0;
}

static int semantic_resolve_supported_aggregate_value_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type) {
    const AstExpression *base = unwrap_paren_expression(expr);
    const char *called_name = NULL;
    const AstExternal *callee_external = NULL;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!out_type) {
        return 0;
    }

    if (semantic_resolve_local_aggregate_value_type(expr, program, scopes, out_type)) {
        return 1;
    }

    if (!base || base->kind != AST_EXPR_CALL ||
        classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !called_name || called_name[0] == '\0') {
        return 0;
    }

    callee_external = semantic_find_visible_function_external(
        program,
        func_index,
        include_current_external,
        called_name);
    if (!callee_external || callee_external->kind != AST_EXTERNAL_FUNCTION) {
        return 0;
    }

    if (!semantic_build_function_return_type_descriptor(callee_external, out_type) ||
        out_type->kind != SEMANTIC_TYPE_KIND_AGGREGATE) {
        if (out_type) {
            memset(out_type, 0, sizeof(*out_type));
            out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
        }
        return 0;
    }

    return 1;
}

static int semantic_validate_recursive_aggregate_initializer_shape(const AstExpression *expr,
    const AstProgram *program,
    const ScopeStack *scopes,
    const SemanticTypeDescriptor *expected_type,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);

    if (!base || !expected_type || expected_type->kind != SEMANTIC_TYPE_KIND_AGGREGATE) {
        return 1;
    }
    if (base->kind != AST_EXPR_INIT_LIST) {
        return 1;
    }

    if (expected_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_PAIR) {
        if (base->as.init_list.item_count > 2u) {
            semantic_set_error(error,
                base->line,
                base->column,
                "SEMA-AGG-002: aggregate initializers currently require no more items than declared fields");
            return 0;
        }
        return 1;
    }

    if (expected_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
        const AstStructType *struct_type = semantic_find_struct_type(program, expected_type->named_type_name);
        size_t field_index;

        if (!struct_type || !struct_type->field_value_kinds) {
            return 0;
        }
        if (base->as.init_list.item_count > struct_type->field_count) {
            semantic_set_error(error,
                base->line,
                base->column,
                "SEMA-AGG-002: aggregate initializers currently require no more items than declared fields");
            return 0;
        }
        for (field_index = 0u; field_index < base->as.init_list.item_count; ++field_index) {
            const AstExpression *item =
                base->as.init_list.items ? base->as.init_list.items[field_index] : NULL;
            SemanticTypeDescriptor field_type;

            if (!item) {
                continue;
            }
            memset(&field_type, 0, sizeof(field_type));
            field_type.kind = SEMANTIC_TYPE_KIND_INVALID;
            switch (struct_type->field_value_kinds[field_index]) {
            case AST_DECLARATION_VALUE_INT:
            case AST_DECLARATION_VALUE_FLOAT:
                continue;
            case AST_DECLARATION_VALUE_PAIR:
                field_type.kind = SEMANTIC_TYPE_KIND_AGGREGATE;
                field_type.aggregate_kind = SEMANTIC_AGGREGATE_KIND_PAIR;
                break;
            case AST_DECLARATION_VALUE_STRUCT:
                field_type.kind = SEMANTIC_TYPE_KIND_AGGREGATE;
                field_type.aggregate_kind = SEMANTIC_AGGREGATE_KIND_STRUCT;
                field_type.named_type_name =
                    struct_type->field_type_names ? struct_type->field_type_names[field_index] : NULL;
                break;
            default:
                return 0;
            }
            if (!semantic_validate_recursive_aggregate_initializer_shape(
                    item,
                    program,
                    scopes,
                    &field_type,
                    error)) {
                return 0;
            }
        }
    }

    return 1;
}

static int semantic_aggregate_expression_usage_is_supported(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    int allow_aggregate_value_root,
    SemanticError *error,
    size_t depth) {
    SemanticTypeDescriptor aggregate_type;
    size_t i;

    if (!expr) {
        return 1;
    }
    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    if (semantic_resolve_supported_aggregate_value_type(
            expr,
            program,
            func_index,
            include_current_external,
            scopes,
            &aggregate_type)) {
        if (allow_aggregate_value_root) {
            return 1;
        }
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-AGG-004: plain aggregate values are not yet enabled in this expression position");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0u; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_aggregate_expression_usage_is_supported(
                    expr->as.init_list.items ? expr->as.init_list.items[i] : NULL,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    0,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_aggregate_expression_usage_is_supported(
            expr->as.inner,
            program,
            func_index,
            include_current_external,
            scopes,
            allow_aggregate_value_root,
            error,
            depth + 1u);
    case AST_EXPR_UNARY:
        return semantic_aggregate_expression_usage_is_supported(
            expr->as.unary.operand,
            program,
            func_index,
            include_current_external,
            scopes,
            0,
            error,
            depth + 1u);
    case AST_EXPR_POSTFIX:
        return semantic_aggregate_expression_usage_is_supported(
            expr->as.postfix.operand,
            program,
            func_index,
            include_current_external,
            scopes,
            0,
            error,
            depth + 1u);
    case AST_EXPR_SUBSCRIPT:
        return semantic_aggregate_expression_usage_is_supported(
                expr->as.subscript.base, program, func_index, include_current_external, scopes, 0, error, depth + 1u) &&
            semantic_aggregate_expression_usage_is_supported(
                expr->as.subscript.index, program, func_index, include_current_external, scopes, 0, error, depth + 1u);
    case AST_EXPR_MEMBER:
        if (!semantic_expression_is_local_aggregate_member_access(expr,
                program,
                scopes,
                SEMANTIC_AGGREGATE_KIND_PAIR,
                NULL,
                NULL,
                NULL) &&
            !semantic_expression_is_local_aggregate_member_access(expr,
                program,
                scopes,
                SEMANTIC_AGGREGATE_KIND_STRUCT,
                NULL,
                NULL,
                NULL)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-AGG-003: aggregate field access currently requires a supported local pair/struct value and a declared field");
            return 0;
        }
        return semantic_aggregate_expression_usage_is_supported(
            expr->as.member.base, program, func_index, include_current_external, scopes, 1, error, depth + 1u);
    case AST_EXPR_CALL:
        if (!semantic_aggregate_expression_usage_is_supported(
                expr->as.call.callee, program, func_index, include_current_external, scopes, 0, error, depth + 1u)) {
            return 0;
        }
        for (i = 0u; i < expr->as.call.arg_count; ++i) {
            if (!semantic_aggregate_expression_usage_is_supported(
                    expr->as.call.args ? expr->as.call.args[i] : NULL,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    1,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_ASSIGN) {
            SemanticTypeDescriptor lhs_type;
            SemanticTypeDescriptor rhs_type;
            const AstExpression *rhs = unwrap_paren_expression(expr->as.binary.right);
            const AstExpression *lhs_base = unwrap_paren_expression(expr->as.binary.left);

            if (!semantic_expression_get_local_aggregate_kind(expr->as.binary.left,
                    program,
                    scopes,
                    NULL,
                    NULL,
                    NULL)) {
                break;
            }
            if (!lhs_base || lhs_base->kind != AST_EXPR_IDENTIFIER || !lhs_base->as.identifier.name) {
                break;
            }
            if (!semantic_scope_lookup_local_aggregate_type_descriptor(scopes,
                    lhs_base->as.identifier.name,
                    &lhs_type)) {
                break;
            }

            if (rhs && rhs->kind == AST_EXPR_INIT_LIST) {
                size_t aggregate_limit = 2u;

                if (lhs_type.aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
                    const AstStructType *target_struct = semantic_find_struct_type(program, lhs_type.named_type_name);

                    if (target_struct && target_struct->field_count > 0u) {
                        aggregate_limit = target_struct->field_count;
                    }
                }
                if (rhs->as.init_list.item_count > aggregate_limit) {
                    semantic_set_error(error,
                        expr->line,
                        expr->column,
                        "SEMA-AGG-002: aggregate initializers currently require no more items than declared fields");
                    return 0;
                }
                for (i = 0u; i < rhs->as.init_list.item_count; ++i) {
                    if (!semantic_aggregate_expression_usage_is_supported(
                            rhs->as.init_list.items ? rhs->as.init_list.items[i] : NULL,
                            program,
                            func_index,
                            include_current_external,
                            scopes,
                            0,
                            error,
                            depth + 1u)) {
                        return 0;
                    }
                }
                if (!semantic_validate_recursive_aggregate_initializer_shape(
                        rhs,
                        program,
                        scopes,
                        &lhs_type,
                        error)) {
                    return 0;
                }
                return 1;
            }

            if (!semantic_resolve_supported_aggregate_value_type(rhs,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &rhs_type)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-AGG-005: aggregate assignment currently requires another local aggregate value or initializer list on the right-hand side");
                return 0;
            }
            if (lhs_type.aggregate_kind != rhs_type.aggregate_kind) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-AGG-006: aggregate assignment currently requires matching aggregate kinds");
                return 0;
            }
            if (lhs_type.aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT &&
                lhs_type.named_type_name && rhs_type.named_type_name &&
                strcmp(lhs_type.named_type_name, rhs_type.named_type_name) != 0) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-AGG-006: struct assignment currently requires matching struct types");
                return 0;
            }
            return 1;
        }
        return semantic_aggregate_expression_usage_is_supported(
                expr->as.binary.left, program, func_index, include_current_external, scopes, 0, error, depth + 1u) &&
            semantic_aggregate_expression_usage_is_supported(
                expr->as.binary.right, program, func_index, include_current_external, scopes, 0, error, depth + 1u);
    case AST_EXPR_TERNARY:
        return semantic_aggregate_expression_usage_is_supported(
                expr->as.ternary.condition, program, func_index, include_current_external, scopes, 0, error, depth + 1u) &&
            semantic_aggregate_expression_usage_is_supported(
                expr->as.ternary.then_expr, program, func_index, include_current_external, scopes, 0, error, depth + 1u) &&
            semantic_aggregate_expression_usage_is_supported(
                expr->as.ternary.else_expr, program, func_index, include_current_external, scopes, 0, error, depth + 1u);
    default:
        return 1;
    }

    return 1;
}

static int semantic_scope_add_declaration_names(const ScopeStack *scopes,
    const AstProgram *program,
    size_t func_index,
    ScopeStack *mutable_scopes,
    char *const *names,
    const int *is_const_flags,
    const int *declaration_value_kinds,
    const AstFunctionReturnType *declaration_function_return_types,
    const size_t *declaration_function_parameter_counts,
    const int *const *declaration_function_parameter_value_kinds,
    const AstFunctionReturnType *const *declaration_function_parameter_return_types,
    const size_t *const *declaration_function_parameter_parameter_counts,
    const int *const *const *declaration_function_parameter_parameter_value_kinds,
    const AstFunctionReturnType *const *const *declaration_function_parameter_parameter_return_types,
    const size_t *const *const *declaration_function_parameter_parameter_parameter_counts,
    char *const *declaration_type_names,
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

        if (!scope_stack_add_to_current(
                mutable_scopes,
                name,
                is_const_flags ? is_const_flags[i] : 0,
                declaration_value_kinds ? declaration_value_kinds[i] : AST_DECLARATION_VALUE_INT,
                declaration_type_names ? declaration_type_names[i] : NULL,
                declaration_function_return_types
                    ? declaration_function_return_types[i]
                    : AST_FUNCTION_RETURN_INT,
                declaration_function_parameter_counts
                    ? declaration_function_parameter_counts[i]
                    : 0u,
                declaration_function_parameter_value_kinds
                    ? declaration_function_parameter_value_kinds[i]
                    : NULL,
                declaration_function_parameter_return_types
                    ? declaration_function_parameter_return_types[i]
                    : NULL,
                declaration_function_parameter_parameter_counts
                    ? declaration_function_parameter_parameter_counts[i]
                    : NULL,
                declaration_function_parameter_parameter_value_kinds
                    ? declaration_function_parameter_parameter_value_kinds[i]
                    : NULL,
                declaration_function_parameter_parameter_return_types
                    ? declaration_function_parameter_parameter_return_types[i]
                    : NULL,
                declaration_function_parameter_parameter_parameter_counts
                    ? declaration_function_parameter_parameter_parameter_counts[i]
                    : NULL)) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-INT-011: failed to record declaration name in scope");
            return 0;
        }
    }

    return 1;
}

static int semantic_aggregate_declaration_is_supported(const AstExpression *init_expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    const SemanticTypeDescriptor *declaration_type,
    int line,
    int column,
    SemanticError *error) {
    const AstStructType *struct_type = NULL;
    const AstExpression *init_base = NULL;
    SemanticAggregateKind aggregate_kind;
    SemanticTypeDescriptor source_type;

    if (!declaration_type) {
        return 1;
    }

    aggregate_kind = declaration_type->aggregate_kind;

    if (aggregate_kind == SEMANTIC_AGGREGATE_KIND_PAIR) {
        if (init_expr) {
            init_base = unwrap_paren_expression(init_expr);
            if (init_base && init_base->kind == AST_EXPR_INIT_LIST &&
                init_base->as.init_list.item_count > 2u) {
                semantic_set_error(error,
                    line,
                    column,
                    "SEMA-AGG-002: aggregate initializers currently require no more items than declared fields");
                return 0;
            }
            if (init_base && init_base->kind != AST_EXPR_INIT_LIST) {
                if (!semantic_resolve_supported_aggregate_value_type(
                        init_base,
                        program,
                        func_index,
                        include_current_external,
                        scopes,
                        &source_type) ||
                    source_type.aggregate_kind != SEMANTIC_AGGREGATE_KIND_PAIR) {
                    semantic_set_error(error,
                        line,
                        column,
                        "SEMA-AGG-005: aggregate initialization currently requires another local aggregate value or initializer list");
                    return 0;
                }
            }
        }
        if (init_expr &&
            !semantic_aggregate_expression_usage_is_supported(
                init_expr, program, func_index, include_current_external, scopes, 1, error, 0u)) {
            return 0;
        }
        if (init_expr &&
            !semantic_validate_recursive_aggregate_initializer_shape(
                init_expr,
                program,
                scopes,
                declaration_type,
                error)) {
            return 0;
        }
        return 1;
    }

    if (aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT) {
        struct_type = semantic_find_struct_type(program, declaration_type->named_type_name);
        if (!struct_type) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-STRUCT-004: unknown struct type in local declaration");
            return 0;
        }
        if (init_expr) {
            init_base = unwrap_paren_expression(init_expr);
            if (init_base && init_base->kind == AST_EXPR_INIT_LIST &&
                init_base->as.init_list.item_count > struct_type->field_count) {
                semantic_set_error(error,
                    line,
                    column,
                    "SEMA-AGG-002: aggregate initializers currently require no more items than declared fields");
                return 0;
            }
            if (init_base && init_base->kind != AST_EXPR_INIT_LIST) {
                if (!semantic_resolve_supported_aggregate_value_type(
                        init_base,
                        program,
                        func_index,
                        include_current_external,
                        scopes,
                        &source_type)) {
                    semantic_set_error(error,
                        line,
                        column,
                        "SEMA-AGG-005: aggregate initialization currently requires another local aggregate value or initializer list");
                    return 0;
                }
                if (source_type.aggregate_kind != SEMANTIC_AGGREGATE_KIND_STRUCT ||
                    !source_type.named_type_name ||
                    !declaration_type->named_type_name ||
                    strcmp(source_type.named_type_name, declaration_type->named_type_name) != 0) {
                    semantic_set_error(error,
                        line,
                        column,
                        "SEMA-AGG-006: struct initialization currently requires matching struct types");
                    return 0;
                }
            }
        }
        if (init_expr &&
            !semantic_validate_recursive_aggregate_initializer_shape(
                init_expr,
                program,
                scopes,
                declaration_type,
                error)) {
            return 0;
        }
        return 1;
    }

    return 1;
}

static int semantic_scope_callee_shadowed_by_local_non_function(const AstExpression *callee,
    const ScopeStack *scopes,
    const char **out_name) {
    const AstExpression *base = unwrap_paren_expression(callee);
    SemanticTypeDescriptor local_function_type;

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

    if (semantic_scope_lookup_local_function_type_descriptor(
            scopes,
            base->as.identifier.name,
            &local_function_type) &&
        local_function_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    if (out_name) {
        *out_name = base->as.identifier.name;
    }

    return 1;
}

static int semantic_expression_is_signed_float_literal(const AstExpression *expr) {
    const AstExpression *base = unwrap_paren_expression(expr);
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

    operand = unwrap_paren_expression(base->as.unary.operand);
    return operand && operand->kind == AST_EXPR_FLOAT_LITERAL;
}

static int semantic_expression_is_direct_float_transport_root(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor scalar_type;

    if (!base) {
        return 0;
    }

    if (semantic_expression_is_signed_float_literal(base)) {
        return 1;
    }

    switch (base->kind) {
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_CALL:
    case AST_EXPR_FLOAT_LITERAL:
        return semantic_expression_resolve_scalar_type(base,
                program,
                func_index,
                include_current_external,
                scopes,
                &scalar_type,
                NULL) &&
            scalar_type.kind == SEMANTIC_TYPE_KIND_FLOAT;
    case AST_EXPR_UNARY:
        if (base->as.unary.op == TOKEN_PLUS || base->as.unary.op == TOKEN_MINUS) {
            return semantic_expression_is_direct_float_transport_root(
                base->as.unary.operand,
                program,
                func_index,
                include_current_external,
                scopes);
        }
        return 0;
    default:
        return 0;
    }
}

static int semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
    const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor scalar_type;

    if (!base) {
        return 0;
    }
    if (semantic_expression_is_signed_float_literal(base)) {
        return 1;
    }

    switch (base->kind) {
    case AST_EXPR_IDENTIFIER:
        return base->as.identifier.name &&
            semantic_scope_lookup_local_scalar_type_descriptor(
                scopes,
                base->as.identifier.name,
                &scalar_type) &&
            scalar_type.kind == SEMANTIC_TYPE_KIND_FLOAT;
    case AST_EXPR_CALL:
        return semantic_expression_resolve_scalar_type(base,
                program,
                func_index,
                include_current_external,
                scopes,
                &scalar_type,
                NULL) &&
            scalar_type.kind == SEMANTIC_TYPE_KIND_FLOAT;
    case AST_EXPR_UNARY:
        if (base->as.unary.op == TOKEN_PLUS || base->as.unary.op == TOKEN_MINUS) {
            return semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
                base->as.unary.operand,
                program,
                func_index,
                include_current_external,
                scopes);
        }
        return 0;
    default:
        return 0;
    }
}

static int semantic_expression_is_float_condition_root(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor scalar_type;

    if (!base) {
        return 0;
    }
    if (semantic_expression_is_direct_float_transport_root(
            base,
            program,
            func_index,
            include_current_external,
            scopes)) {
        return 1;
    }
    if (base->kind == AST_EXPR_CONVERSION &&
        base->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT &&
        semantic_expression_resolve_scalar_type(
            base,
            program,
            func_index,
            include_current_external,
            scopes,
            &scalar_type,
            NULL) &&
        scalar_type.kind == SEMANTIC_TYPE_KIND_FLOAT) {
        return 1;
    }
    if (base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_PLUS &&
            base->as.binary.op != TOKEN_MINUS &&
            base->as.binary.op != TOKEN_STAR &&
            base->as.binary.op != TOKEN_SLASH) ||
        !semantic_expression_resolve_scalar_type(
            base,
            program,
            func_index,
            include_current_external,
            scopes,
            &scalar_type,
            NULL) ||
        scalar_type.kind != SEMANTIC_TYPE_KIND_FLOAT) {
        return 0;
    }
    if ((unwrap_paren_expression(base->as.binary.left) &&
            unwrap_paren_expression(base->as.binary.left)->kind == AST_EXPR_TERNARY) ||
        (unwrap_paren_expression(base->as.binary.right) &&
            unwrap_paren_expression(base->as.binary.right)->kind == AST_EXPR_TERNARY)) {
        return 0;
    }
    return semantic_expression_is_float_condition_root(
            base->as.binary.left,
            program,
            func_index,
            include_current_external,
            scopes) &&
        semantic_expression_is_float_condition_root(
            base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes);
}

static int semantic_scope_call_callee_is_function_parameter_name(const AstExpression *callee,
    const AstExternal *func) {
    const AstExpression *base = unwrap_paren_expression(callee);

    if (!base || base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }

    return semantic_function_parameter_name_is_function_valued(func, base->as.identifier.name);
}

static int semantic_expression_call_returns_void(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external) {
    const char *called_name = NULL;
    const AstExternal *external = NULL;
    const SemanticBuiltinFunctionInfo *builtin = NULL;
    SemanticTypeDescriptor return_type;

    if (!expr || expr->kind != AST_EXPR_CALL) {
        return 0;
    }

    if (classify_ast_call_callee(expr->as.call.callee, &called_name) == AST_CALL_CALLEE_CALL_RESULT) {
        const AstExpression *callee_base = unwrap_paren_expression(expr->as.call.callee);

        if (!callee_base || callee_base->kind != AST_EXPR_CALL) {
            return 0;
        }
        if (classify_ast_call_callee(callee_base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || called_name[0] == '\0') {
            return 0;
        }
        external = semantic_find_visible_function_external(program, func_index, include_current_external, called_name);
        if (!external || !external->has_function_return_signature ||
            !semantic_build_function_return_type_descriptor(external, &return_type)) {
            return 0;
        }
        return return_type.kind == SEMANTIC_TYPE_KIND_FUNCTION &&
            return_type.function_return_type == AST_FUNCTION_RETURN_VOID;
    }

    if (classify_ast_call_callee(expr->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !called_name || called_name[0] == '\0') {
        return 0;
    }

    external = semantic_find_visible_function_external(program, func_index, include_current_external, called_name);
    if (external) {
        if (!semantic_build_function_return_type_descriptor(external, &return_type)) {
            return 0;
        }
        return return_type.kind == SEMANTIC_TYPE_KIND_VOID;
    }

    builtin = semantic_find_builtin_function(called_name);
    return builtin && builtin->return_type == AST_FUNCTION_RETURN_VOID;
}

static int semantic_scope_check_local_function_call(const AstExpression *expr,
    const ScopeStack *scopes,
    SemanticError *error) {
    const char *called_name = NULL;
    SemanticTypeDescriptor local_function_type;

    if (!expr || expr->kind != AST_EXPR_CALL) {
        return 1;
    }
    if (classify_ast_call_callee(expr->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !called_name || called_name[0] == '\0') {
        return 1;
    }
    if (!semantic_scope_lookup_local_function_type_descriptor(scopes, called_name, &local_function_type) ||
        local_function_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 1;
    }
    if (expr->as.call.arg_count != local_function_type.parameter_count) {
        char preview[128];
        char msg[512];

        format_callee_preview(called_name, preview, sizeof(preview));
        snprintf(msg,
            sizeof(msg),
            "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
            preview,
            local_function_type.parameter_count,
            expr->as.call.arg_count);
        semantic_set_error(error, expr->line, expr->column, msg);
        return 0;
    }
    return 1;
}

static int semantic_resolve_immediate_function_value_call_type(const AstExpression *callee_expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    SemanticTypeDescriptor *out_type) {
    const AstExpression *base = unwrap_paren_expression(callee_expr);
    const char *called_name = NULL;
    const AstExternal *external = NULL;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!out_type || !base || base->kind != AST_EXPR_CALL) {
        return 0;
    }
    if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !called_name || called_name[0] == '\0') {
        return 0;
    }

    external = semantic_find_visible_function_external(
        program,
        func_index,
        include_current_external,
        called_name);
    if (!external || external->kind != AST_EXTERNAL_FUNCTION || !external->has_function_return_signature) {
        return 0;
    }

    return semantic_build_function_return_type_descriptor(external, out_type) &&
        out_type->kind == SEMANTIC_TYPE_KIND_FUNCTION;
}

static int semantic_function_type_missing_nested_metadata(const SemanticTypeDescriptor *type) {
    size_t param_index;

    if (!type || type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    for (param_index = 0u; param_index < type->parameter_count; ++param_index) {
        int param_kind =
            type->parameter_value_kinds
                ? type->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT;
        if (param_kind == AST_PARAMETER_VALUE_FUNCTION &&
            (!type->parameter_function_return_types ||
                !type->parameter_function_parameter_counts ||
                !type->parameter_function_parameter_value_kinds)) {
            return 1;
        }
        if (param_kind == AST_PARAMETER_VALUE_FUNCTION) {
            SemanticTypeDescriptor nested_type;

            if (!semantic_build_function_type_parameter_descriptor(
                    type,
                    param_index,
                    &nested_type)) {
                return 1;
            }
            if (semantic_function_type_missing_nested_metadata(&nested_type)) {
                return 1;
            }
        }
    }

    return 0;
}

static int semantic_function_type_matches_with_missing_nested_fallback(
    const SemanticTypeDescriptor *expected_type,
    const SemanticTypeDescriptor *actual_type) {
    size_t param_index;

    if (!expected_type || !actual_type ||
        expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION ||
        actual_type->kind != SEMANTIC_TYPE_KIND_FUNCTION ||
        expected_type->function_return_type != actual_type->function_return_type ||
        expected_type->parameter_count != actual_type->parameter_count) {
        return 0;
    }

    for (param_index = 0u; param_index < expected_type->parameter_count; ++param_index) {
        int expected_kind =
            expected_type->parameter_value_kinds
                ? expected_type->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT;
        int actual_kind =
            actual_type->parameter_value_kinds
                ? actual_type->parameter_value_kinds[param_index]
                : AST_PARAMETER_VALUE_INT;

        if (expected_kind != actual_kind) {
            return 0;
        }
        if (expected_kind == AST_PARAMETER_VALUE_FUNCTION) {
            SemanticTypeDescriptor expected_nested_type;
            SemanticTypeDescriptor actual_nested_type;
            AstFunctionReturnType expected_nested_return_type =
                expected_type->parameter_function_return_types
                    ? expected_type->parameter_function_return_types[param_index]
                    : AST_FUNCTION_RETURN_INT;
            AstFunctionReturnType actual_nested_return_type =
                actual_type->parameter_function_return_types
                    ? actual_type->parameter_function_return_types[param_index]
                    : AST_FUNCTION_RETURN_INT;
            size_t expected_nested_param_count =
                expected_type->parameter_function_parameter_counts
                    ? expected_type->parameter_function_parameter_counts[param_index]
                    : 0u;
            size_t actual_nested_param_count =
                actual_type->parameter_function_parameter_counts
                    ? actual_type->parameter_function_parameter_counts[param_index]
                    : 0u;

            if (expected_nested_return_type != actual_nested_return_type ||
                expected_nested_param_count != actual_nested_param_count) {
                return 0;
            }
            if (!expected_type->parameter_function_parameter_value_kinds) {
                continue;
            }

            if (!semantic_build_function_type_parameter_descriptor(
                    expected_type,
                    param_index,
                    &expected_nested_type) ||
                !semantic_build_function_type_parameter_descriptor(
                    actual_type,
                    param_index,
                    &actual_nested_type)) {
                return 0;
            }
            if (semantic_function_type_missing_nested_metadata(&expected_nested_type)) {
                if (!semantic_function_type_matches_with_missing_nested_fallback(
                        &expected_nested_type,
                        &actual_nested_type)) {
                    return 0;
                }
                continue;
            }
            if (!semantic_type_descriptors_are_compatible(
                    &expected_nested_type,
                    &actual_nested_type)) {
                return 0;
            }
        }
    }

    return 1;
}

static int semantic_build_external_function_signature_type_descriptor(
    const AstExternal *external,
    SemanticTypeDescriptor *out_type) {
    if (!external || !out_type || external->kind != AST_EXTERNAL_FUNCTION) {
        return 0;
    }

    memset(out_type, 0, sizeof(*out_type));
    out_type->kind = SEMANTIC_TYPE_KIND_FUNCTION;
    out_type->function_return_type = external->function_return_type;
    out_type->named_type_name = external->function_return_type_name;
    out_type->parameter_count = external->parameter_count;
    out_type->parameter_value_kinds = external->parameter_value_kinds;
    out_type->parameter_function_return_types = external->parameter_function_return_types;
    out_type->parameter_function_parameter_counts =
        external->parameter_function_parameter_counts;
    out_type->parameter_function_parameter_value_kinds =
        (const int *const *)external->parameter_function_parameter_value_kinds;
    out_type->parameter_function_parameter_return_types =
        (const AstFunctionReturnType *const *)external->parameter_function_parameter_return_types;
    out_type->parameter_function_parameter_parameter_counts =
        (const size_t *const *)external->parameter_function_parameter_parameter_counts;
    out_type->parameter_function_parameter_parameter_value_kinds =
        (const int *const *const *)external->parameter_function_parameter_parameter_value_kinds;
    out_type->parameter_function_parameter_parameter_return_types =
        (const AstFunctionReturnType *const *const *)external->parameter_function_parameter_parameter_return_types;
    out_type->parameter_function_parameter_parameter_parameter_counts =
        (const size_t *const *const *)external->parameter_function_parameter_parameter_parameter_counts;
    return 1;
}

static int semantic_try_resolve_function_value_expression_type_descriptor(
    const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type) {
    const AstExpression *base = semantic_unwrap_function_value_wrapper_expression(expr);
    const AstExternal *current_func = NULL;
    const char *name = NULL;
    const AstExternal *visible_function = NULL;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!base || !out_type) {
        return 0;
    }

    if (program && func_index < program->count &&
        program->externals[func_index].kind == AST_EXTERNAL_FUNCTION) {
        current_func = &program->externals[func_index];
    }

    if (base->kind == AST_EXPR_TERNARY) {
        SemanticTypeDescriptor then_type;
        SemanticTypeDescriptor else_type;

        if (!semantic_try_resolve_function_value_expression_type_descriptor(
                base->as.ternary.then_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                &then_type) ||
            !semantic_try_resolve_function_value_expression_type_descriptor(
                base->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                &else_type) ||
            !semantic_type_descriptors_are_compatible(&then_type, &else_type)) {
            return 0;
        }
        *out_type = then_type;
        return 1;
    }

    if (base->kind == AST_EXPR_CALL) {
        return semantic_resolve_immediate_function_value_call_type(
            base,
            program,
            func_index,
            include_current_external,
            out_type);
    }

    if (base->kind != AST_EXPR_IDENTIFIER || !base->as.identifier.name) {
        return 0;
    }
    name = base->as.identifier.name;

    if (current_func &&
        semantic_find_named_function_parameter_type_descriptor(
            current_func,
            name,
            NULL,
            out_type) &&
        out_type->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return 1;
    }
    if (scopes &&
        semantic_scope_lookup_local_function_type_descriptor(scopes, name, out_type) &&
        out_type->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return 1;
    }
    if (current_func &&
        semantic_current_function_find_direct_local_function_value_declaration_type_descriptor(
            current_func,
            name,
            out_type) &&
        out_type->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return 1;
    }

    visible_function = semantic_find_visible_function_external(
        program,
        func_index,
        include_current_external,
        name);
    if (visible_function &&
        semantic_build_external_function_signature_type_descriptor(visible_function, out_type)) {
        return 1;
    }

    return 0;
}

static int semantic_scope_check_direct_call_target(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    const AstExternal *current_func,
    SemanticError *error) {
    const char *called_name = NULL;
    int callee_kind;
    SemanticTypeDescriptor param_type;
    SemanticTypeDescriptor local_function_type;
    const AstExternal *visible_external = NULL;
    const AstExternal *later_function = NULL;
    const SemanticBuiltinFunctionInfo *builtin = NULL;
    size_t visible_external_count;
    size_t i;
    char callee_preview[128];

    if (!expr || expr->kind != AST_EXPR_CALL) {
        return 1;
    }

    {
        const AstExpression *callee_base = unwrap_paren_expression(expr->as.call.callee);

        if (callee_base && callee_base->kind == AST_EXPR_BINARY &&
            (callee_base->as.binary.op == TOKEN_COMMA || callee_base->as.binary.op == TOKEN_ASSIGN)) {
            const AstExpression *resolved_callee =
                callee_base->as.binary.op == TOKEN_COMMA
                    ? callee_base->as.binary.right
                    : callee_base->as.binary.left;
            AstExpression rewritten_call = *expr;

            rewritten_call.as.call.callee = (AstExpression *)resolved_callee;
            return semantic_scope_check_direct_call_target(
                &rewritten_call,
                program,
                func_index,
                include_current_external,
                scopes,
                current_func,
                error);
        }
        if (callee_base && callee_base->kind == AST_EXPR_TERNARY) {
            const AstExpression *then_base = unwrap_paren_expression(callee_base->as.ternary.then_expr);
            const AstExpression *else_base = unwrap_paren_expression(callee_base->as.ternary.else_expr);
            AstExpression rewritten_call = *expr;

            if (then_base && else_base &&
                semantic_extension_check_function_value_expression(
                    expr->as.call.callee,
                    program,
                    func_index,
                    include_current_external,
                    current_func,
                    1,
                    error,
                    0u)) {
                rewritten_call.as.call.callee = (AstExpression *)then_base;
                if (!semantic_scope_check_direct_call_target(
                        &rewritten_call,
                        program,
                        func_index,
                        include_current_external,
                        scopes,
                        current_func,
                        error)) {
                    return 0;
                }
                rewritten_call.as.call.callee = (AstExpression *)else_base;
                return semantic_scope_check_direct_call_target(
                    &rewritten_call,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    current_func,
                    error);
            }
        }
    }

    callee_kind = classify_ast_call_callee(expr->as.call.callee, &called_name);
    if (callee_kind == AST_CALL_CALLEE_CALL_RESULT) {
        SemanticTypeDescriptor returned_function_type;

        if (semantic_resolve_immediate_function_value_call_type(
                expr->as.call.callee,
                program,
                func_index,
                include_current_external,
                &returned_function_type)) {
            if (expr->as.call.arg_count != returned_function_type.parameter_count) {
                char msg[512];

                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CALL-004: callee=<call_result>; expected=%zu; got=%zu; call argument count mismatch",
                    returned_function_type.parameter_count,
                    expr->as.call.arg_count);
                semantic_set_error(error, expr->line, expr->column, msg);
                return 0;
            } else {
                return 1;
            }
        }
    }
    if (callee_kind != AST_CALL_CALLEE_DIRECT_IDENTIFIER || !called_name || called_name[0] == '\0') {
        semantic_set_error(error,
            expr->line,
            expr->column,
            callee_kind == AST_CALL_CALLEE_CALL_RESULT
                ? "SEMA-CALL-005: call result is not callable in this semantic subset; callee_kind=call_result"
                : "SEMA-CALL-006: non-identifier callee is not supported in this semantic subset; callee_kind=non_identifier");
        return 0;
    }

    format_callee_preview(called_name, callee_preview, sizeof(callee_preview));

    if (current_func &&
        semantic_find_named_function_parameter_type_descriptor(
            current_func,
            called_name,
            NULL,
            &param_type) &&
        param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        if (expr->as.call.arg_count != param_type.parameter_count) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
                callee_preview,
                param_type.parameter_count,
                expr->as.call.arg_count);
            semantic_set_error(error, expr->line, expr->column, msg);
            return 0;
        }
        return 1;
    }

    if (semantic_scope_lookup_local_function_type_descriptor(scopes, called_name, &local_function_type) &&
        local_function_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        if (expr->as.call.arg_count != local_function_type.parameter_count) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
                callee_preview,
                local_function_type.parameter_count,
                expr->as.call.arg_count);
            semantic_set_error(error, expr->line, expr->column, msg);
            return 0;
        }
        return 1;
    }

    visible_external_count = include_current_external ? (func_index + 1u) : func_index;
    if (program && visible_external_count > program->count) {
        visible_external_count = program->count;
    }
    if (program) {
        for (i = 0u; i < visible_external_count; ++i) {
            const AstExternal *candidate = &program->externals[i];
            if (candidate->name && strcmp(candidate->name, called_name) == 0) {
                visible_external = candidate;
            }
        }
    }

    if (visible_external) {
        if (visible_external->kind != AST_EXTERNAL_FUNCTION) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                "SEMA-CALL-003: callee=%s; symbol_kind=non_function; call to non-function symbol",
                callee_preview);
            semantic_set_error(error, expr->line, expr->column, msg);
            return 0;
        }
        if (expr->as.call.arg_count != visible_external->parameter_count) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; call argument count mismatch",
                callee_preview,
                visible_external->parameter_count,
                expr->as.call.arg_count);
            semantic_set_error(error, expr->line, expr->column, msg);
            return 0;
        }
        return 1;
    }

    builtin = semantic_find_builtin_function(called_name);
    if (builtin) {
        if (expr->as.call.arg_count != builtin->parameter_count) {
            char msg[512];
            snprintf(msg,
                sizeof(msg),
                "SEMA-CALL-004: callee=%s; expected=%zu; got=%zu; builtin call argument count mismatch",
                callee_preview,
                builtin->parameter_count,
                expr->as.call.arg_count);
            semantic_set_error(error, expr->line, expr->column, msg);
            return 0;
        }
        return 1;
    }

    if (program) {
        for (i = func_index + 1u; i < program->count; ++i) {
            const AstExternal *candidate = &program->externals[i];
            if (candidate->kind == AST_EXTERNAL_FUNCTION &&
                candidate->name &&
                strcmp(candidate->name, called_name) == 0) {
                later_function = candidate;
                break;
            }
        }
    }

    if (later_function) {
        char msg[512];
        snprintf(msg,
            sizeof(msg),
            "SEMA-CALL-002: callee=%s; decl_line=%d; decl_col=%d; call before declaration",
            callee_preview,
            later_function->line,
            later_function->column);
        semantic_set_error(error, expr->line, expr->column, msg);
        return 0;
    }

    {
        char msg[512];
        snprintf(msg,
            sizeof(msg),
            "SEMA-CALL-001: callee=%s; call to undeclared function",
            callee_preview);
        semantic_set_error(error, expr->line, expr->column, msg);
    }
    return 0;
}

static int semantic_scope_add_declaration_names_shadow_pass(ScopeStack *scopes,
    char *const *names,
    const int *is_const_flags,
    const int *declaration_value_kinds,
    const AstFunctionReturnType *declaration_function_return_types,
    const size_t *declaration_function_parameter_counts,
    const int *const *declaration_function_parameter_value_kinds,
    const AstFunctionReturnType *const *declaration_function_parameter_return_types,
    const size_t *const *declaration_function_parameter_parameter_counts,
    const int *const *const *declaration_function_parameter_parameter_value_kinds,
    const AstFunctionReturnType *const *const *declaration_function_parameter_parameter_return_types,
    const size_t *const *const *declaration_function_parameter_parameter_parameter_counts,
    char *const *declaration_type_names,
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

        if (!scope_stack_add_to_current(scopes,
                name,
                is_const_flags ? is_const_flags[i] : 0,
                declaration_value_kinds ? declaration_value_kinds[i] : AST_DECLARATION_VALUE_INT,
                declaration_type_names ? declaration_type_names[i] : NULL,
                declaration_function_return_types
                    ? declaration_function_return_types[i]
                    : AST_FUNCTION_RETURN_INT,
                declaration_function_parameter_counts
                    ? declaration_function_parameter_counts[i]
                    : 0u,
                declaration_function_parameter_value_kinds
                    ? declaration_function_parameter_value_kinds[i]
                    : NULL,
                declaration_function_parameter_return_types
                    ? declaration_function_parameter_return_types[i]
                    : NULL,
                declaration_function_parameter_parameter_counts
                    ? declaration_function_parameter_parameter_counts[i]
                    : NULL,
                declaration_function_parameter_parameter_value_kinds
                    ? declaration_function_parameter_parameter_value_kinds[i]
                    : NULL,
                declaration_function_parameter_parameter_return_types
                    ? declaration_function_parameter_parameter_return_types[i]
                    : NULL,
                declaration_function_parameter_parameter_parameter_counts
                    ? declaration_function_parameter_parameter_parameter_counts[i]
                    : NULL)) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }
    }

    return 1;
}

static int semantic_scope_check_call_shadow_expression_with_depth(const AstExpression *expr,
    const ScopeStack *scopes,
    const AstExternal *current_func,
    SemanticError *error,
    size_t depth) {
    size_t i;

    if (!expr) {
        return 1;
    }

    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_INIT_LIST:
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_scope_check_call_shadow_expression_with_depth(expr->as.init_list.items[i],
                    scopes,
                    current_func,
                    error,
                    depth + 1)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.inner,
            scopes,
            current_func,
            error,
            depth + 1);
    case AST_EXPR_UNARY:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.unary.operand,
            scopes,
            current_func,
            error,
            depth + 1);
    case AST_EXPR_POSTFIX:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.postfix.operand,
            scopes,
            current_func,
            error,
            depth + 1);
    case AST_EXPR_SUBSCRIPT:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.subscript.base,
                scopes,
                current_func,
                error,
                depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.subscript.index,
                scopes,
                current_func,
                error,
                depth + 1);
    case AST_EXPR_MEMBER:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.member.base,
            scopes,
            current_func,
            error,
            depth + 1);
    case AST_EXPR_BINARY:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.binary.left,
            scopes,
            current_func,
            error,
            depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.binary.right,
                scopes,
                current_func,
                error,
                depth + 1);
    case AST_EXPR_TERNARY:
        return semantic_scope_check_call_shadow_expression_with_depth(expr->as.ternary.condition,
            scopes,
            current_func,
            error,
            depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.ternary.then_expr,
                scopes,
                current_func,
                error,
                depth + 1) &&
            semantic_scope_check_call_shadow_expression_with_depth(expr->as.ternary.else_expr,
                scopes,
                current_func,
                error,
                depth + 1);
    case AST_EXPR_CALL: {
        const char *shadowed_callee_name = NULL;
        if (!semantic_scope_call_callee_is_function_parameter_name(expr->as.call.callee, current_func) &&
            semantic_scope_callee_shadowed_by_local_non_function(expr->as.call.callee,
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

        if (!semantic_scope_check_call_shadow_expression_with_depth(expr->as.call.callee,
            scopes,
            current_func,
            error,
            depth + 1)) {
            return 0;
        }

        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_scope_check_call_shadow_expression_with_depth(expr->as.call.args[i],
                scopes,
                current_func,
                error,
                depth + 1)) {
                return 0;
            }
        }

        return 1;
    }
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
    default:
        return 1;
    }
}

static int semantic_scope_check_call_shadow_expression(const AstExpression *expr,
    const ScopeStack *scopes,
    const AstExternal *current_func,
    SemanticError *error) {
    return semantic_scope_check_call_shadow_expression_with_depth(expr, scopes, current_func, error, 0);
}

static int semantic_scope_check_call_shadow_statement(const AstStatement *stmt,
    ScopeStack *scopes,
    const AstExternal *current_func,
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
            if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, current_func, error)) {
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

                if (!semantic_scope_check_call_shadow_expression(init_expr, scopes, current_func, error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }

                if (!semantic_scope_add_declaration_names_shadow_pass(scopes,
                    &single_name,
                    NULL,
                    stmt->declaration_value_kinds ? &stmt->declaration_value_kinds[i] : NULL,
                    stmt->declaration_function_return_types
                        ? &stmt->declaration_function_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_counts
                        ? &stmt->declaration_function_parameter_counts[i]
                        : NULL,
                    stmt->declaration_function_parameter_value_kinds
                        ? (const int *const *)&stmt->declaration_function_parameter_value_kinds[i]
                        : NULL,
                    stmt->declaration_function_parameter_return_types
                        ? (const AstFunctionReturnType *const *)&stmt->declaration_function_parameter_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_counts
                        ? (const size_t *const *)&stmt->declaration_function_parameter_parameter_counts[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_value_kinds
                        ? (const int *const *const *)&stmt->declaration_function_parameter_parameter_value_kinds[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_return_types
                        ? (const AstFunctionReturnType *const *const *)&stmt->declaration_function_parameter_parameter_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_parameter_counts
                        ? (const size_t *const *const *)&stmt->declaration_function_parameter_parameter_parameter_counts[i]
                        : NULL,
                    stmt->declaration_type_names ? &stmt->declaration_type_names[i] : NULL,
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
            if (!semantic_scope_check_call_shadow_expression(stmt->expressions[i], scopes, current_func, error)) {
                scope_stack_pop(scopes);
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, current_func, error)) {
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

            if (!semantic_scope_check_call_shadow_expression(init_expr, scopes, current_func, error)) {
                return 0;
            }

            if (!semantic_scope_add_declaration_names_shadow_pass(scopes,
                &single_name,
                NULL,
                stmt->declaration_value_kinds ? &stmt->declaration_value_kinds[i] : NULL,
                stmt->declaration_function_return_types
                    ? &stmt->declaration_function_return_types[i]
                    : NULL,
                stmt->declaration_function_parameter_counts
                    ? &stmt->declaration_function_parameter_counts[i]
                    : NULL,
                stmt->declaration_function_parameter_value_kinds
                    ? (const int *const *)&stmt->declaration_function_parameter_value_kinds[i]
                    : NULL,
                stmt->declaration_function_parameter_return_types
                    ? (const AstFunctionReturnType *const *)&stmt->declaration_function_parameter_return_types[i]
                    : NULL,
                stmt->declaration_function_parameter_parameter_counts
                    ? (const size_t *const *)&stmt->declaration_function_parameter_parameter_counts[i]
                    : NULL,
                stmt->declaration_function_parameter_parameter_value_kinds
                    ? (const int *const *const *)&stmt->declaration_function_parameter_parameter_value_kinds[i]
                    : NULL,
                stmt->declaration_function_parameter_parameter_return_types
                    ? (const AstFunctionReturnType *const *const *)&stmt->declaration_function_parameter_parameter_return_types[i]
                    : NULL,
                stmt->declaration_function_parameter_parameter_parameter_counts
                    ? (const size_t *const *const *)&stmt->declaration_function_parameter_parameter_parameter_counts[i]
                    : NULL,
                stmt->declaration_type_names ? &stmt->declaration_type_names[i] : NULL,
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
            if (!semantic_scope_check_call_shadow_expression(stmt->expressions[i], scopes, current_func, error)) {
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, current_func, error)) {
                return 0;
            }
        }

        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (!semantic_scope_check_call_shadow_expression(stmt->expressions[i], scopes, current_func, error)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_scope_check_call_shadow_statement(stmt->children[i], scopes, current_func, error)) {
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
        func->parameter_is_const,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
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
            if (!semantic_scope_check_call_shadow_statement(body->children[i], &scopes, func, error)) {
                scope_stack_free(&scopes);
                return 0;
            }
        }
    } else if (!semantic_scope_check_call_shadow_statement(body, &scopes, func, error)) {
        scope_stack_free(&scopes);
        return 0;
    }

    scope_stack_free(&scopes);
    return 1;
}

static int semantic_scope_check_expression_with_visibility_impl(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    int in_call_callee,
    int allow_void_result,
    SemanticError *error,
    size_t depth) {
    size_t i;
    SemanticTypeDescriptor local_type;

    if (!expr) {
        return 1;
    }

    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        if (!in_call_callee &&
            expr->as.identifier.name &&
            expr->as.identifier.name[0] != '\0' &&
            semantic_scope_lookup_local_function_type_descriptor(scopes,
                expr->as.identifier.name,
                &local_type) &&
            local_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-019: function values are not yet enabled in the current extension semantic slice");
            return 0;
        }
        if (!in_call_callee && expr->as.identifier.name && expr->as.identifier.name[0] != '\0' &&
            !semantic_scope_name_resolved(program,
                func_index,
                include_current_external,
                scopes,
                expr->as.identifier.name)) {
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
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_scope_check_expression_with_visibility_impl(expr->as.init_list.items[i],
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    in_call_callee,
                    0,
                    error,
                    depth + 1)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.inner,
            program,
            func_index,
            include_current_external,
            scopes,
            in_call_callee,
            allow_void_result,
            error,
            depth + 1);
    case AST_EXPR_UNARY:
        if ((expr->as.unary.op == TOKEN_PLUS_PLUS || expr->as.unary.op == TOKEN_MINUS_MINUS) &&
            semantic_scope_name_is_const(
                program,
                func_index,
                include_current_external,
                scopes,
                semantic_mutated_lvalue_identifier_name(expr->as.unary.operand))) {
            if (error) {
                char preview[128];
                char msg[512];
                const char *name = semantic_mutated_lvalue_identifier_name(expr->as.unary.operand);
                format_callee_preview(name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CONST-001: identifier=%s; assignment to const object is not allowed",
                    preview);
                semantic_set_error(error, expr->line, expr->column, msg);
            }
            return 0;
        }
        return semantic_scope_check_expression_with_visibility_impl(expr->as.unary.operand,
            program,
            func_index,
            include_current_external,
            scopes,
            in_call_callee,
            0,
            error,
            depth + 1);
    case AST_EXPR_POSTFIX:
        if ((expr->as.postfix.op == TOKEN_PLUS_PLUS || expr->as.postfix.op == TOKEN_MINUS_MINUS) &&
            semantic_scope_name_is_const(
                program,
                func_index,
                include_current_external,
                scopes,
                semantic_mutated_lvalue_identifier_name(expr->as.postfix.operand))) {
            if (error) {
                char preview[128];
                char msg[512];
                const char *name = semantic_mutated_lvalue_identifier_name(expr->as.postfix.operand);
                format_callee_preview(name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CONST-001: identifier=%s; assignment to const object is not allowed",
                    preview);
                semantic_set_error(error, expr->line, expr->column, msg);
            }
            return 0;
        }
        return semantic_scope_check_expression_with_visibility_impl(expr->as.postfix.operand,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1);
    case AST_EXPR_SUBSCRIPT:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.subscript.base,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.subscript.index,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1);
    case AST_EXPR_MEMBER: {
        const char *base_name = NULL;

        if (!semantic_scope_check_expression_with_visibility_impl(expr->as.member.base,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1)) {
            return 0;
        }
        if (!semantic_expression_is_local_aggregate_member_access(expr,
                program,
                scopes,
                SEMANTIC_AGGREGATE_KIND_PAIR,
                &base_name,
                NULL,
                NULL) &&
            !semantic_expression_is_local_aggregate_member_access(expr,
                program,
                scopes,
                SEMANTIC_AGGREGATE_KIND_STRUCT,
                &base_name,
                NULL,
                NULL)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-AGG-003: aggregate field access currently requires a supported local pair/struct value and a declared field");
            return 0;
        }
        return 1;
    }
    case AST_EXPR_CONVERSION:
        if (!semantic_scope_check_expression_with_visibility_impl(
                expr->as.conversion.operand,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1)) {
            return 0;
        }
        if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_INT) {
            SemanticTypeDescriptor operand_type;
            if (semantic_expression_resolve_scalar_type(
                    expr->as.conversion.operand,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &operand_type,
                    NULL) &&
                operand_type.kind == SEMANTIC_TYPE_KIND_FLOAT) {
                return 1;
            }
        } else if (expr->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT) {
            SemanticTypeDescriptor operand_type;
            if (semantic_expression_resolve_scalar_type(
                    expr->as.conversion.operand,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &operand_type,
                    NULL) &&
                operand_type.kind == SEMANTIC_TYPE_KIND_INT) {
                return 1;
            }
        }
        return semantic_set_float_conversion_usage_error(expr, error);
    case AST_EXPR_BINARY:
        if (semantic_scope_is_allowed_function_value_assignment_expr(
                expr,
                program,
                func_index,
                include_current_external,
                scopes,
                program && include_current_external &&
                        func_index < program->count &&
                        program->externals[func_index].kind == AST_EXTERNAL_FUNCTION
                    ? &program->externals[func_index]
                    : NULL)) {
            return 1;
        }
        if (semantic_binary_op_is_assignment(expr->as.binary.op) &&
            semantic_scope_name_is_const(
                program,
                func_index,
                include_current_external,
                scopes,
                semantic_mutated_lvalue_identifier_name(expr->as.binary.left))) {
            if (error) {
                char preview[128];
                char msg[512];
                const char *name = semantic_mutated_lvalue_identifier_name(expr->as.binary.left);
                format_callee_preview(name, preview, sizeof(preview));
                snprintf(msg,
                    sizeof(msg),
                    "SEMA-CONST-001: identifier=%s; assignment to const object is not allowed",
                    preview);
                semantic_set_error(error, expr->line, expr->column, msg);
            }
            return 0;
        }
        if (in_call_callee && expr->as.binary.op == TOKEN_ASSIGN) {
            return semantic_scope_check_expression_with_visibility_impl(expr->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    1,
                    1,
                    error,
                    depth + 1) &&
                semantic_scope_check_expression_with_visibility_impl(expr->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    1,
                    0,
                    error,
                    depth + 1);
        }
        return semantic_scope_check_expression_with_visibility_impl(expr->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                expr->as.binary.op == TOKEN_COMMA ? 1 : 0,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.binary.right,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1);
    case AST_EXPR_TERNARY:
        return semantic_scope_check_expression_with_visibility_impl(expr->as.ternary.condition,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.ternary.then_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1) &&
            semantic_scope_check_expression_with_visibility_impl(expr->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                in_call_callee,
                0,
                error,
                depth + 1);
    case AST_EXPR_CLOSURE:
        return 1;
    case AST_EXPR_CALL: {
        const char *shadowed_callee_name = NULL;
        const AstExternal *current_func = NULL;
        const char *called_name = NULL;
        const AstExternal *callee_external = NULL;
        const SemanticBuiltinFunctionInfo *builtin = NULL;
        SemanticTypeDescriptor returned_callee_type;
        int callee_is_returned_function_value = 0;
        if (program &&
            include_current_external &&
            func_index < program->count &&
            program->externals[func_index].kind == AST_EXTERNAL_FUNCTION) {
            current_func = &program->externals[func_index];
        }
        if (!semantic_scope_call_callee_is_function_parameter_name(expr->as.call.callee, current_func) &&
            semantic_scope_callee_shadowed_by_local_non_function(expr->as.call.callee,
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
        if (!semantic_scope_check_expression_with_visibility_impl(expr->as.call.callee,
            program,
            func_index,
            include_current_external,
            scopes,
            1,
            0,
                error,
                depth + 1)) {
            return 0;
        }
        if (!semantic_scope_check_direct_call_target(
                expr,
                program,
                func_index,
                include_current_external,
                scopes,
                current_func,
                error)) {
            return 0;
        }
        if (classify_ast_call_callee(expr->as.call.callee, &called_name) == AST_CALL_CALLEE_DIRECT_IDENTIFIER &&
            called_name && called_name[0] != '\0') {
            callee_external = semantic_find_visible_function_external(
                program,
                func_index,
                include_current_external,
                called_name);
            if (!callee_external || callee_external->kind != AST_EXTERNAL_FUNCTION) {
                builtin = semantic_find_builtin_function(called_name);
            }
        } else if (semantic_resolve_immediate_function_value_call_type(
                expr->as.call.callee,
                program,
                func_index,
                include_current_external,
                &returned_callee_type) &&
            returned_callee_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
            callee_is_returned_function_value = 1;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (callee_external && i < callee_external->parameter_count) {
                SemanticTypeDescriptor param_type;

                if (semantic_build_function_parameter_type_descriptor(
                        callee_external,
                        i,
                        &param_type) &&
                    param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                    continue;
                }
            } else if (callee_is_returned_function_value) {
                SemanticTypeDescriptor param_type;

                if (semantic_build_function_type_parameter_descriptor(
                        &returned_callee_type,
                        i,
                        &param_type) &&
                    param_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                    continue;
                }
            } else if (builtin) {
                /* Current builtins do not take function-valued parameters. */
            }
            if (!semantic_scope_check_expression_with_visibility_impl(expr->as.call.args[i],
                program,
                func_index,
                include_current_external,
                scopes,
                0,
                0,
                error,
                depth + 1)) {
                return 0;
            }
        }
        if (!allow_void_result &&
            semantic_expression_call_returns_void(expr, program, func_index, include_current_external)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-CALL-007: void call result cannot be used in a value context");
            return 0;
        }
        if (!semantic_scope_check_local_function_call(expr, scopes, error)) {
            return 0;
        }
        if (!semantic_validate_call_argument_scalar_types(
                expr,
                program,
                func_index,
                include_current_external,
                scopes,
                error)) {
            return 0;
        }
        return 1;
    }
    default:
        return 1;
    }
}

static int semantic_scope_check_expression_with_visibility(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    int in_call_callee,
    int allow_void_result,
    SemanticError *error) {
    return semantic_scope_check_expression_with_visibility_impl(expr,
        program,
        func_index,
        include_current_external,
        scopes,
        in_call_callee,
        allow_void_result,
        error,
        0);
}

static int semantic_scope_check_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    int allow_extension_features,
    int in_call_callee,
    int allow_void_result,
    SemanticError *error) {
    int closure_function_arg_bypass = 0;

    if (!semantic_expression_check_extension_mode(
            expr,
            program,
            func_index,
            allow_extension_features,
            error,
            0u)) {
        const AstExpression *closure_base = unwrap_paren_expression(expr);
        (void)closure_function_arg_bypass;

        if (allow_extension_features &&
            in_call_callee &&
            closure_base &&
            closure_base->kind == AST_EXPR_CLOSURE) {
            return semantic_scope_check_expression_with_visibility(expr,
                program,
                func_index,
                1,
                scopes,
                in_call_callee,
                allow_void_result,
                error);
        }
        return 0;
    }
    if (!semantic_scope_check_expression_with_visibility(expr,
        program,
        func_index,
        1,
        scopes,
        in_call_callee,
        allow_void_result,
        error)) {
        return 0;
    }
    if (!semantic_validate_scalar_assignment_type(
            expr,
            program,
            func_index,
            1,
            scopes,
            error)) {
        return 0;
    }
    if (!semantic_validate_scalar_comparison_type(
            expr,
            program,
            func_index,
            1,
            scopes,
            error)) {
        return 0;
    }
    if (!semantic_validate_scalar_arithmetic_type(
            expr,
            program,
            func_index,
            1,
            scopes,
            error)) {
        return 0;
    }
    return semantic_validate_scalar_expression_usage(
        expr,
        program,
        func_index,
        1,
        scopes,
        error);
}

static int semantic_scope_check_top_level_initializer_expression(const AstExpression *expr,
    const AstProgram *program,
    size_t external_index,
    int allow_extension_features,
    SemanticError *error) {
    if (!semantic_expression_check_extension_mode(
            expr,
            program,
            external_index,
            allow_extension_features,
            error,
            0u)) {
        return 0;
    }
    if (!semantic_scope_check_expression_with_visibility(expr,
        program,
        external_index,
        0,
        NULL,
        0,
        0,
        error)) {
        return 0;
    }
    return semantic_validate_scalar_expression_usage(
        expr,
        program,
        external_index,
        0,
        NULL,
        error);
}

static int semantic_expression_resolve_scalar_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticTypeDescriptor *out_type,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    const AstExternal *external = NULL;
    const SemanticBuiltinFunctionInfo *builtin = NULL;
    SemanticTypeDescriptor local_type;

    if (out_type) {
        memset(out_type, 0, sizeof(*out_type));
        out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
    }
    if (!base || !out_type) {
        return 0;
    }

    switch (base->kind) {
    case AST_EXPR_NUMBER:
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    case AST_EXPR_FLOAT_LITERAL:
        out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
        return 1;
    case AST_EXPR_CONVERSION: {
        SemanticTypeDescriptor operand_type;
        SemanticTypeDescriptor target_type;

        if (!base->as.conversion.operand ||
            !semantic_expression_resolve_scalar_type(base->as.conversion.operand,
                program,
                func_index,
                include_current_external,
                scopes,
                &operand_type,
                error)) {
            return 0;
        }

        if (base->as.conversion.target_type_kind == AST_DECLARATION_VALUE_INT) {
            target_type.kind = SEMANTIC_TYPE_KIND_INT;
        } else if (base->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT) {
            target_type.kind = SEMANTIC_TYPE_KIND_FLOAT;
        } else {
            return 0;
        }

        if (!((operand_type.kind == SEMANTIC_TYPE_KIND_INT &&
                target_type.kind == SEMANTIC_TYPE_KIND_FLOAT) ||
              (operand_type.kind == SEMANTIC_TYPE_KIND_FLOAT &&
                target_type.kind == SEMANTIC_TYPE_KIND_INT))) {
            return 0;
        }

        *out_type = target_type;
        return 1;
    }
    case AST_EXPR_IDENTIFIER:
        if (base->as.identifier.name &&
            semantic_scope_lookup_local_scalar_type_descriptor(scopes,
                base->as.identifier.name,
                &local_type)) {
            *out_type = local_type;
            return 1;
        }
        if (base->as.identifier.name &&
            semantic_scope_lookup_local_function_type_descriptor(scopes,
                base->as.identifier.name,
                &local_type)) {
            *out_type = local_type;
            return 1;
        }
        if (base->as.identifier.name) {
            external = semantic_find_visible_external(
                program,
                func_index,
                include_current_external,
                base->as.identifier.name);
            if (external && external->kind == AST_EXTERNAL_DECLARATION &&
                semantic_build_top_level_declaration_type_descriptor(external, out_type) &&
                out_type->array_rank == 0u) {
                return out_type->kind == SEMANTIC_TYPE_KIND_INT ||
                    out_type->kind == SEMANTIC_TYPE_KIND_FLOAT;
            }
        }
        return 0;
    case AST_EXPR_MEMBER:
        if (semantic_resolve_local_aggregate_member_type(
                base,
                program,
                scopes,
                &local_type,
                NULL,
                NULL) &&
            (local_type.kind == SEMANTIC_TYPE_KIND_INT ||
                local_type.kind == SEMANTIC_TYPE_KIND_AGGREGATE)) {
            *out_type = local_type;
            return 1;
        }
        return 0;
    case AST_EXPR_CALL: {
        const char *called_name = NULL;
        SemanticTypeDescriptor immediate_call_type;

        if (semantic_resolve_immediate_function_value_call_type(
                base->as.call.callee,
                program,
                func_index,
                include_current_external,
                &immediate_call_type)) {
            switch (immediate_call_type.function_return_type) {
            case AST_FUNCTION_RETURN_INT:
                out_type->kind = SEMANTIC_TYPE_KIND_INT;
                return 1;
            case AST_FUNCTION_RETURN_FLOAT:
                out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
                return 1;
            case AST_FUNCTION_RETURN_VOID:
                out_type->kind = SEMANTIC_TYPE_KIND_VOID;
                return 1;
            default:
                out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
                return 0;
            }
        }
        if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || called_name[0] == '\0') {
            return 0;
        }
        if (semantic_scope_lookup_local_function_type_descriptor(scopes, called_name, out_type) &&
            out_type->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
            switch (out_type->function_return_type) {
            case AST_FUNCTION_RETURN_INT:
                out_type->kind = SEMANTIC_TYPE_KIND_INT;
                return 1;
            case AST_FUNCTION_RETURN_FLOAT:
                out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
                return 1;
            case AST_FUNCTION_RETURN_VOID:
                out_type->kind = SEMANTIC_TYPE_KIND_VOID;
                return 1;
            default:
                out_type->kind = SEMANTIC_TYPE_KIND_INVALID;
                return 0;
            }
        }
        external = semantic_find_visible_function_external(
            program,
            func_index,
            include_current_external,
            called_name);
        if (external && external->kind == AST_EXTERNAL_FUNCTION &&
            semantic_build_function_return_type_descriptor(external, out_type)) {
            return out_type->kind == SEMANTIC_TYPE_KIND_INT ||
                out_type->kind == SEMANTIC_TYPE_KIND_FLOAT ||
                out_type->kind == SEMANTIC_TYPE_KIND_VOID;
        }
        builtin = semantic_find_builtin_function(called_name);
        if (builtin) {
            out_type->kind = builtin->return_type == AST_FUNCTION_RETURN_VOID
                ? SEMANTIC_TYPE_KIND_VOID
                : SEMANTIC_TYPE_KIND_INT;
            return 1;
        }
        return 0;
    }
    case AST_EXPR_UNARY: {
        SemanticTypeDescriptor operand_type;

        if (base->as.unary.op == TOKEN_PLUS || base->as.unary.op == TOKEN_MINUS) {
            if (semantic_expression_is_direct_float_transport_root(
                    base,
                    program,
                    func_index,
                    include_current_external,
                    scopes)) {
                out_type->kind = SEMANTIC_TYPE_KIND_FLOAT;
                return 1;
            }
            return 0;
        }
        if (base->as.unary.op == TOKEN_BANG) {
            if (!semantic_expression_resolve_scalar_type(base->as.unary.operand,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &operand_type,
                    error) ||
                (operand_type.kind != SEMANTIC_TYPE_KIND_INT &&
                    operand_type.kind != SEMANTIC_TYPE_KIND_FLOAT)) {
                return 0;
            }
            out_type->kind = SEMANTIC_TYPE_KIND_INT;
            return 1;
        }
        return 0;
    }
    case AST_EXPR_BINARY: {
        SemanticTypeDescriptor lhs_type;
        SemanticTypeDescriptor rhs_type;

        if (base->as.binary.op == TOKEN_ASSIGN) {
            return semantic_expression_resolve_scalar_type(base->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                out_type,
                error);
        }
        if (base->as.binary.op == TOKEN_EQ ||
            base->as.binary.op == TOKEN_NE ||
            base->as.binary.op == TOKEN_LT ||
            base->as.binary.op == TOKEN_LE ||
            base->as.binary.op == TOKEN_GT ||
            base->as.binary.op == TOKEN_GE) {
            if (!semantic_expression_resolve_scalar_type(base->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &lhs_type,
                    error) ||
                !semantic_expression_resolve_scalar_type(base->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &rhs_type,
                    error) ||
                lhs_type.kind != rhs_type.kind ||
                (lhs_type.kind != SEMANTIC_TYPE_KIND_INT &&
                    lhs_type.kind != SEMANTIC_TYPE_KIND_FLOAT)) {
                return 0;
            }
            out_type->kind = SEMANTIC_TYPE_KIND_INT;
            return 1;
        }
        if (base->as.binary.op == TOKEN_PLUS ||
            base->as.binary.op == TOKEN_MINUS ||
            base->as.binary.op == TOKEN_STAR ||
            base->as.binary.op == TOKEN_SLASH) {
            if (!semantic_expression_resolve_scalar_type(base->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &lhs_type,
                    error) ||
                !semantic_expression_resolve_scalar_type(base->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &rhs_type,
                    error) ||
                lhs_type.kind != rhs_type.kind ||
                (lhs_type.kind != SEMANTIC_TYPE_KIND_FLOAT &&
                    lhs_type.kind != SEMANTIC_TYPE_KIND_INT)) {
                return 0;
            }
            out_type->kind = lhs_type.kind;
            return 1;
        }
        if (base->as.binary.op != TOKEN_AND_AND &&
            base->as.binary.op != TOKEN_OR_OR) {
            return 0;
        }
        if (!semantic_expression_resolve_scalar_type(base->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                &lhs_type,
                error) ||
            !semantic_expression_resolve_scalar_type(base->as.binary.right,
                program,
                func_index,
                include_current_external,
                scopes,
                &rhs_type,
                error) ||
            (lhs_type.kind != SEMANTIC_TYPE_KIND_INT &&
                lhs_type.kind != SEMANTIC_TYPE_KIND_FLOAT) ||
            (rhs_type.kind != SEMANTIC_TYPE_KIND_INT &&
                rhs_type.kind != SEMANTIC_TYPE_KIND_FLOAT)) {
            return 0;
        }
        out_type->kind = SEMANTIC_TYPE_KIND_INT;
        return 1;
    }
    case AST_EXPR_TERNARY: {
        SemanticTypeDescriptor condition_type;
        SemanticTypeDescriptor then_type;
        SemanticTypeDescriptor else_type;

        if (!semantic_expression_resolve_scalar_type(base->as.ternary.condition,
                program,
                func_index,
                include_current_external,
                scopes,
                &condition_type,
                error) ||
            (condition_type.kind != SEMANTIC_TYPE_KIND_INT &&
                condition_type.kind != SEMANTIC_TYPE_KIND_FLOAT) ||
            !semantic_expression_resolve_scalar_type(base->as.ternary.then_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                &then_type,
                error) ||
            !semantic_expression_resolve_scalar_type(base->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                scopes,
                &else_type,
                error) ||
            then_type.kind != else_type.kind ||
            (then_type.kind != SEMANTIC_TYPE_KIND_INT &&
                then_type.kind != SEMANTIC_TYPE_KIND_FLOAT)) {
            return 0;
        }
        *out_type = then_type;
        return 1;
    }
    default:
        (void)error;
        return 0;
    }
}

static int semantic_validate_scalar_initializer_type(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    int allow_extension_features,
    const ScopeStack *scopes,
    SemanticError *error) {
    SemanticTypeDescriptor actual_type;
    SemanticTypeDescriptor aggregate_type;

    if (!expr || !expected_type) {
        return 1;
    }
    if (!semantic_expression_check_extension_mode(
            expr,
            program,
            func_index,
            allow_extension_features,
            error,
            0u)) {
        const AstExpression *closure_base = unwrap_paren_expression(expr);

        if (allow_extension_features &&
            expected_type->kind == SEMANTIC_TYPE_KIND_FUNCTION &&
            closure_base &&
            closure_base->kind == AST_EXPR_CLOSURE) {
            return semantic_validate_closure_literal_initializer(
                expr,
                expected_type,
                program,
                func_index,
                scopes,
                error);
        }
        return 0;
    }
    if (expected_type->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        const AstExternal *current_func = NULL;
        int closure_backed = 0;
        SemanticTypeDescriptor recovered_type;
        const SemanticTypeDescriptor *effective_expected_type = expected_type;

        if (program && func_index < program->count &&
            program->externals[func_index].kind == AST_EXTERNAL_FUNCTION) {
            current_func = &program->externals[func_index];
        }
        if (semantic_function_type_missing_nested_metadata(expected_type) &&
            semantic_try_resolve_function_value_expression_type_descriptor(
                expr,
                program,
                func_index,
                1,
                scopes,
                &recovered_type) &&
            semantic_function_type_matches_with_missing_nested_fallback(
                expected_type,
                &recovered_type)) {
            effective_expected_type = &recovered_type;
        }

        if (semantic_extension_function_value_initializer_expr_matches_expected(
                expr,
                program,
                func_index,
                1,
                current_func,
                effective_expected_type,
                &closure_backed)) {
            const AstExpression *base = semantic_unwrap_function_value_wrapper_expression(expr);

            if (base && base->kind == AST_EXPR_CALL) {
                SemanticTypeDescriptor actual_function_type;

                if (!semantic_scope_check_expression(
                        expr,
                        program,
                        func_index,
                        scopes,
                        allow_extension_features,
                        0,
                        0,
                        error)) {
                    return 0;
                }
                if (!(semantic_try_resolve_function_value_expression_type_descriptor(
                        expr,
                        program,
                        func_index,
                        1,
                        scopes,
                        &actual_function_type) &&
                        actual_function_type.kind == SEMANTIC_TYPE_KIND_FUNCTION &&
                        semantic_type_descriptors_are_compatible(
                            effective_expected_type,
                            &actual_function_type))) {
                    const char *called_name = NULL;
                    const AstExternal *visible_function = NULL;

                    if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
                        !called_name || called_name[0] == '\0') {
                        semantic_set_error(error,
                            expr->line,
                            expr->column,
                            "SEMA-EXT-018: function-valued local initialization currently requires a direct function-valued return call");
                        return 0;
                    }
                    visible_function = semantic_find_visible_function_external(
                        program,
                        func_index,
                        1,
                        called_name);
                    if (!visible_function ||
                        !semantic_function_value_return_matches_type_descriptor(
                            visible_function,
                            effective_expected_type)) {
                        semantic_set_error(error,
                            expr->line,
                            expr->column,
                            "SEMA-EXT-018: function-valued local initialization currently requires a matching direct function-valued return call");
                        return 0;
                    }
                }
            }
            return 1;
        }
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-018: function-valued local initialization currently requires a compatible direct identifier, matching direct function-valued return call, or matching ternary over those forms");
        return 0;
    }
    if (expected_type->kind != SEMANTIC_TYPE_KIND_INT &&
        expected_type->kind != SEMANTIC_TYPE_KIND_FLOAT) {
        return 1;
    }
    if (semantic_resolve_supported_aggregate_value_type(expr,
            program,
            func_index,
            1,
            scopes,
            &aggregate_type)) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-TYPE-004: declaration initializer scalar type mismatch");
        return 0;
    }
    if (!semantic_expression_resolve_scalar_type(expr,
            program,
            func_index,
            1,
            scopes,
            &actual_type,
            error)) {
        return 1;
    }
    if (actual_type.kind != expected_type->kind) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-TYPE-004: declaration initializer scalar type mismatch");
        return 0;
    }
    return 1;
}

static int semantic_validate_return_expression_type(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error) {
    SemanticTypeDescriptor actual_type;

    if (!expr || !expected_type) {
        return 1;
    }
    if (expected_type->kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return semantic_validate_function_valued_return_expression(
            expr,
            expected_type,
            program,
            func_index,
            scopes,
            error);
    }
    if (expected_type->kind == SEMANTIC_TYPE_KIND_AGGREGATE) {
        if (!semantic_resolve_supported_aggregate_value_type(expr,
                program,
                func_index,
                1,
                scopes,
                &actual_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-AGG-005: aggregate return currently requires another local aggregate value or a direct aggregate-returning call");
            return 0;
        }
        if (!semantic_type_descriptors_are_compatible(expected_type, &actual_type)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                expected_type->aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT &&
                        actual_type.aggregate_kind == SEMANTIC_AGGREGATE_KIND_STRUCT
                    ? "SEMA-AGG-006: struct return currently requires matching struct types"
                    : "SEMA-AGG-006: aggregate return currently requires matching aggregate kinds");
            return 0;
        }
        return 1;
    }
    if (expected_type->kind != SEMANTIC_TYPE_KIND_INT &&
        expected_type->kind != SEMANTIC_TYPE_KIND_FLOAT) {
        return 1;
    }
    if (!semantic_expression_resolve_scalar_type(expr,
            program,
            func_index,
            1,
            scopes,
            &actual_type,
            error)) {
        return 1;
    }
    if (actual_type.kind != expected_type->kind) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-TYPE-005: return expression scalar type mismatch");
        return 0;
    }
    return 1;
}

static int semantic_validate_scalar_assignment_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    const AstExpression *rhs_base = NULL;
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;
    SemanticTypeDescriptor rhs_aggregate_type;

    if (!base || base->kind != AST_EXPR_BINARY ||
        base->as.binary.op != TOKEN_ASSIGN) {
        return 1;
    }
    rhs_base = unwrap_paren_expression(base->as.binary.right);
    if (semantic_resolve_supported_aggregate_value_type(
            base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes,
            &rhs_aggregate_type) &&
        semantic_expression_resolve_scalar_type(base->as.binary.left,
            program,
            func_index,
            include_current_external,
            scopes,
            &lhs_type,
            NULL) &&
        (lhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT)) {
        semantic_set_error(error,
            base->line,
            base->column,
            "SEMA-TYPE-006: assignment scalar type mismatch");
        return 0;
    }
    if (semantic_expression_contains_float_scalar(
            base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes) &&
        !semantic_expression_resolve_scalar_type(base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes,
            &rhs_type,
            NULL)) {
        return semantic_set_float_expression_usage_error(base->as.binary.right, error);
    }
    if (semantic_expression_contains_float_scalar(
            base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes) &&
        rhs_base &&
        !(rhs_base->kind == AST_EXPR_CONVERSION &&
            semantic_expression_resolve_scalar_type(
                rhs_base,
                program,
                func_index,
                include_current_external,
                scopes,
                &rhs_type,
                NULL) &&
            (rhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
                rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT)) &&
        !(rhs_base->kind == AST_EXPR_TERNARY &&
            semantic_expression_resolve_scalar_type(
                rhs_base,
                program,
                func_index,
                include_current_external,
                scopes,
                &rhs_type,
                NULL) &&
            rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        !semantic_expression_is_direct_float_transport_root(
            rhs_base,
            program,
            func_index,
            include_current_external,
            scopes) &&
        !(rhs_base->kind == AST_EXPR_BINARY &&
            (rhs_base->as.binary.op == TOKEN_PLUS ||
                rhs_base->as.binary.op == TOKEN_MINUS ||
                rhs_base->as.binary.op == TOKEN_STAR ||
                rhs_base->as.binary.op == TOKEN_SLASH) &&
            semantic_expression_resolve_scalar_type(
                rhs_base->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                &lhs_type,
                NULL) &&
            semantic_expression_resolve_scalar_type(
                rhs_base->as.binary.right,
                program,
                func_index,
                include_current_external,
                scopes,
                &rhs_type,
                NULL) &&
            lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT &&
            rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT)) {
        return semantic_set_float_expression_usage_error(rhs_base, error);
    }
    if (!semantic_expression_resolve_scalar_type(base->as.binary.left,
            program,
            func_index,
            include_current_external,
            scopes,
            &lhs_type,
            error) ||
        !semantic_expression_resolve_scalar_type(base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes,
            &rhs_type,
            error)) {
        return 1;
    }
    if ((lhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        (rhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        lhs_type.kind != rhs_type.kind) {
        semantic_set_error(error,
            base->line,
            base->column,
            "SEMA-TYPE-006: assignment scalar type mismatch");
        return 0;
    }
    return 1;
}

static int semantic_validate_control_condition_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor actual_type;

    if (!expr) {
        return 1;
    }
    if (!semantic_expression_resolve_scalar_type(expr,
            program,
            func_index,
            1,
            scopes,
            &actual_type,
            error)) {
        return 1;
    }
    if (actual_type.kind == SEMANTIC_TYPE_KIND_FLOAT &&
        (!base ||
            !semantic_expression_is_float_condition_root(
                base,
                program,
                func_index,
                1,
                scopes))) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-036: float control-flow conditions currently support only direct transport roots, explicit float conversion roots, and recursive pure-float arithmetic");
        return 0;
    }
    return 1;
}

static int semantic_validate_scalar_comparison_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;

    if (!base || base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_EQ &&
            base->as.binary.op != TOKEN_NE &&
            base->as.binary.op != TOKEN_LT &&
            base->as.binary.op != TOKEN_LE &&
            base->as.binary.op != TOKEN_GT &&
            base->as.binary.op != TOKEN_GE) ||
        !semantic_expression_contains_float_scalar(base,
            program,
            func_index,
            include_current_external,
            scopes)) {
        return 1;
    }

    if (!semantic_expression_resolve_scalar_type(base->as.binary.left,
            program,
            func_index,
            include_current_external,
            scopes,
            &lhs_type,
            error) ||
        !semantic_expression_resolve_scalar_type(base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes,
            &rhs_type,
            error)) {
        return 1;
    }

    if ((lhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        (rhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        lhs_type.kind != rhs_type.kind) {
        semantic_set_error(error,
            base->line,
            base->column,
            "SEMA-TYPE-007: comparison scalar type mismatch");
        return 0;
    }

    return 1;
}

static int semantic_validate_scalar_arithmetic_type(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;
    const AstExpression *lhs = NULL;
    const AstExpression *rhs = NULL;
    int lhs_typecheck_root = 0;
    int rhs_typecheck_root = 0;

    if (!base || base->kind != AST_EXPR_BINARY ||
        (base->as.binary.op != TOKEN_PLUS &&
            base->as.binary.op != TOKEN_MINUS &&
            base->as.binary.op != TOKEN_STAR &&
            base->as.binary.op != TOKEN_SLASH) ||
        !semantic_expression_contains_float_scalar(base,
            program,
            func_index,
            include_current_external,
            scopes)) {
        return 1;
    }

    lhs = base->as.binary.left;
    rhs = base->as.binary.right;
    lhs_typecheck_root = semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
            lhs,
            program,
            func_index,
            include_current_external,
            scopes);
    rhs_typecheck_root = semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
            rhs,
            program,
            func_index,
            include_current_external,
            scopes);

    if (!semantic_expression_resolve_scalar_type(lhs,
            program,
            func_index,
            include_current_external,
            scopes,
            &lhs_type,
            error) ||
        !semantic_expression_resolve_scalar_type(rhs,
            program,
            func_index,
            include_current_external,
            scopes,
            &rhs_type,
            error)) {
        return 1;
    }

    if ((lhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        (rhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
            rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
        lhs_type.kind != rhs_type.kind &&
        (lhs_typecheck_root || rhs_typecheck_root)) {
        semantic_set_error(error,
            base->line,
            base->column,
            "SEMA-TYPE-008: arithmetic scalar type mismatch");
        return 0;
    }

    return 1;
}

static int semantic_set_float_expression_usage_error(const AstExpression *expr,
    SemanticError *error) {
    if (!expr) {
        return 0;
    }
    semantic_set_error(error,
        expr->line,
        expr->column,
        "SEMA-EXT-035: float expressions currently support only direct transport, unary-sign transport, same-type + - * / on direct roots, logical-condition composition, same-type float comparisons, and same-type float ternary values");
    return 0;
}

static int semantic_set_float_conversion_usage_error(const AstExpression *expr,
    SemanticError *error) {
    if (!expr) {
        return 0;
    }
    semantic_set_error(error,
        expr->line,
        expr->column,
        "SEMA-EXT-038: explicit scalar conversion currently supports only int(float_expr) and float(int_expr) on scalar operands");
    return 0;
}

static int semantic_validate_scalar_expression_usage_impl(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor lhs_type;
    SemanticTypeDescriptor rhs_type;

    if (!expr || !base) {
        return 1;
    }
    if (!semantic_expression_contains_float_scalar(
            base,
            program,
            func_index,
            include_current_external,
            scopes)) {
        return 1;
    }

    if (base->kind == AST_EXPR_BINARY &&
        (base->as.binary.op == TOKEN_PLUS ||
            base->as.binary.op == TOKEN_MINUS ||
            base->as.binary.op == TOKEN_STAR ||
            base->as.binary.op == TOKEN_SLASH) &&
        semantic_expression_resolve_scalar_type(
            base,
            program,
            func_index,
            include_current_external,
            scopes,
            &lhs_type,
            NULL) &&
        lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT &&
        !(unwrap_paren_expression(base->as.binary.left) &&
            unwrap_paren_expression(base->as.binary.left)->kind == AST_EXPR_TERNARY) &&
        !(unwrap_paren_expression(base->as.binary.right) &&
            unwrap_paren_expression(base->as.binary.right)->kind == AST_EXPR_TERNARY) &&
        !semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
            base->as.binary.left,
            program,
            func_index,
            include_current_external,
            scopes) &&
        !semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
            base->as.binary.right,
            program,
            func_index,
            include_current_external,
            scopes) &&
        !semantic_expression_is_signed_float_literal(base->as.binary.left) &&
        !semantic_expression_is_signed_float_literal(base->as.binary.right) &&
        !(unwrap_paren_expression(base->as.binary.left) &&
            unwrap_paren_expression(base->as.binary.left)->kind == AST_EXPR_BINARY) &&
        !(unwrap_paren_expression(base->as.binary.right) &&
            unwrap_paren_expression(base->as.binary.right)->kind == AST_EXPR_BINARY)) {
        return semantic_set_float_expression_usage_error(base, error);
    }

    switch (base->kind) {
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_CALL: {
        size_t arg_index;

        for (arg_index = 0u; arg_index < base->as.call.arg_count; ++arg_index) {
            if (!semantic_validate_scalar_expression_usage_impl(
                    base->as.call.args[arg_index],
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error)) {
                return 0;
            }
        }
        return 1;
    }
    case AST_EXPR_CONVERSION:
        if (base->as.conversion.target_type_kind == AST_DECLARATION_VALUE_INT ||
            base->as.conversion.target_type_kind == AST_DECLARATION_VALUE_FLOAT) {
            SemanticTypeDescriptor conversion_type;
            if (semantic_expression_resolve_scalar_type(
                    base,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &conversion_type,
                    NULL) &&
                (conversion_type.kind == SEMANTIC_TYPE_KIND_INT ||
                    conversion_type.kind == SEMANTIC_TYPE_KIND_FLOAT)) {
                return 1;
            }
        }
        (void)lhs_type;
        return semantic_set_float_conversion_usage_error(base, error);
    case AST_EXPR_UNARY:
        if (base->as.unary.op == TOKEN_BANG) {
            return semantic_validate_scalar_expression_usage_impl(
                base->as.unary.operand,
                program,
                func_index,
                include_current_external,
                scopes,
                error);
        }
        if ((base->as.unary.op == TOKEN_PLUS || base->as.unary.op == TOKEN_MINUS) &&
            semantic_expression_is_direct_float_transport_root(
                base,
                program,
                func_index,
                include_current_external,
                scopes)) {
            return 1;
        }
        return semantic_set_float_expression_usage_error(base, error);
    case AST_EXPR_POSTFIX:
    case AST_EXPR_SUBSCRIPT:
    case AST_EXPR_MEMBER:
        return semantic_set_float_expression_usage_error(base, error);
    case AST_EXPR_BINARY:
        if (base->as.binary.op == TOKEN_ASSIGN) {
            return 1;
        }
        if ((base->as.binary.op == TOKEN_PLUS ||
                base->as.binary.op == TOKEN_MINUS ||
                base->as.binary.op == TOKEN_STAR ||
                base->as.binary.op == TOKEN_SLASH) &&
            semantic_expression_resolve_scalar_type(
                base->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                &lhs_type,
                NULL) &&
            semantic_expression_resolve_scalar_type(
                base->as.binary.right,
                program,
                func_index,
                include_current_external,
                scopes,
                &rhs_type,
                NULL) &&
            lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT &&
            rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) {
            if ((unwrap_paren_expression(base->as.binary.left) &&
                    unwrap_paren_expression(base->as.binary.left)->kind == AST_EXPR_TERNARY) ||
                (unwrap_paren_expression(base->as.binary.right) &&
                    unwrap_paren_expression(base->as.binary.right)->kind == AST_EXPR_TERNARY)) {
                return semantic_set_float_expression_usage_error(base, error);
            }
            return semantic_validate_scalar_expression_usage_impl(
                    base->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error) &&
                semantic_validate_scalar_expression_usage_impl(
                    base->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error);
        }
        if (base->as.binary.op == TOKEN_EQ ||
            base->as.binary.op == TOKEN_NE ||
            base->as.binary.op == TOKEN_LT ||
            base->as.binary.op == TOKEN_LE ||
            base->as.binary.op == TOKEN_GT ||
            base->as.binary.op == TOKEN_GE) {
            if ((unwrap_paren_expression(base->as.binary.left) &&
                    unwrap_paren_expression(base->as.binary.left)->kind == AST_EXPR_TERNARY) ||
                (unwrap_paren_expression(base->as.binary.right) &&
                    unwrap_paren_expression(base->as.binary.right)->kind == AST_EXPR_TERNARY)) {
                return semantic_set_float_expression_usage_error(base, error);
            }
            return semantic_validate_scalar_expression_usage_impl(
                    base->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error) &&
                semantic_validate_scalar_expression_usage_impl(
                    base->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error);
        }
        if (base->as.binary.op == TOKEN_AND_AND ||
            base->as.binary.op == TOKEN_OR_OR) {
            return semantic_validate_scalar_expression_usage_impl(
                    base->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error) &&
                semantic_validate_scalar_expression_usage_impl(
                    base->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                scopes,
                error);
        }
        if ((base->as.binary.op == TOKEN_PLUS ||
                base->as.binary.op == TOKEN_MINUS ||
                base->as.binary.op == TOKEN_STAR ||
                base->as.binary.op == TOKEN_SLASH) &&
            semantic_expression_resolve_scalar_type(
                base->as.binary.left,
                program,
                func_index,
                include_current_external,
                scopes,
                &lhs_type,
                NULL) &&
            semantic_expression_resolve_scalar_type(
                base->as.binary.right,
                program,
                func_index,
                include_current_external,
                scopes,
                &rhs_type,
                NULL) &&
            (lhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
                lhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
            (rhs_type.kind == SEMANTIC_TYPE_KIND_INT ||
                rhs_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
            lhs_type.kind != rhs_type.kind) {
            if (semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
                    base->as.binary.left,
                    program,
                    func_index,
                    include_current_external,
                    scopes) ||
                semantic_expression_is_non_global_direct_float_arithmetic_typecheck_root(
                    base->as.binary.right,
                    program,
                    func_index,
                    include_current_external,
                    scopes)) {
                return 1;
            }
            return semantic_set_float_expression_usage_error(base, error);
        }
        return semantic_set_float_expression_usage_error(base, error);
    case AST_EXPR_TERNARY:
        if (!semantic_validate_scalar_expression_usage_impl(
                base->as.ternary.condition,
                program,
                func_index,
                include_current_external,
                scopes,
                error)) {
            return 0;
        }
        if (semantic_expression_contains_float_scalar(
                base->as.ternary.then_expr,
                program,
                func_index,
                include_current_external,
                scopes) ||
            semantic_expression_contains_float_scalar(
                base->as.ternary.else_expr,
                program,
                func_index,
                include_current_external,
                scopes)) {
            SemanticTypeDescriptor then_type;
            SemanticTypeDescriptor else_type;

            if (!semantic_expression_resolve_scalar_type(
                    base->as.ternary.then_expr,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &then_type,
                    NULL) ||
                !semantic_expression_resolve_scalar_type(
                    base->as.ternary.else_expr,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &else_type,
                    NULL) ||
                then_type.kind != SEMANTIC_TYPE_KIND_FLOAT ||
                else_type.kind != SEMANTIC_TYPE_KIND_FLOAT) {
                return semantic_set_float_expression_usage_error(base, error);
            }
            return semantic_validate_scalar_expression_usage_impl(
                    base->as.ternary.then_expr,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error) &&
                semantic_validate_scalar_expression_usage_impl(
                    base->as.ternary.else_expr,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error);
        }
        return 1;
    default:
        return 1;
    }
}

static int semantic_expression_contains_float_scalar(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes) {
    size_t i;
    const AstExpression *base = unwrap_paren_expression(expr);
    SemanticTypeDescriptor scalar_type;

    if (!base) {
        return 0;
    }

    if (semantic_expression_resolve_scalar_type(base,
            program,
            func_index,
            include_current_external,
            scopes,
            &scalar_type,
            NULL) &&
        scalar_type.kind == SEMANTIC_TYPE_KIND_FLOAT) {
        return 1;
    }

    switch (base->kind) {
    case AST_EXPR_INIT_LIST:
        for (i = 0u; i < base->as.init_list.item_count; ++i) {
            if (semantic_expression_contains_float_scalar(
                    base->as.init_list.items ? base->as.init_list.items[i] : NULL,
                    program,
                    func_index,
                    include_current_external,
                    scopes)) {
                return 1;
            }
        }
        return 0;
    case AST_EXPR_PAREN:
        return semantic_expression_contains_float_scalar(
            base->as.inner, program, func_index, include_current_external, scopes);
    case AST_EXPR_UNARY:
        return semantic_expression_contains_float_scalar(
            base->as.unary.operand, program, func_index, include_current_external, scopes);
    case AST_EXPR_POSTFIX:
        return semantic_expression_contains_float_scalar(
            base->as.postfix.operand, program, func_index, include_current_external, scopes);
    case AST_EXPR_SUBSCRIPT:
        return semantic_expression_contains_float_scalar(
                base->as.subscript.base, program, func_index, include_current_external, scopes) ||
            semantic_expression_contains_float_scalar(
                base->as.subscript.index, program, func_index, include_current_external, scopes);
    case AST_EXPR_MEMBER:
        return semantic_expression_contains_float_scalar(
            base->as.member.base, program, func_index, include_current_external, scopes);
    case AST_EXPR_BINARY:
        return semantic_expression_contains_float_scalar(
                base->as.binary.left, program, func_index, include_current_external, scopes) ||
            semantic_expression_contains_float_scalar(
                base->as.binary.right, program, func_index, include_current_external, scopes);
    case AST_EXPR_TERNARY:
        return semantic_expression_contains_float_scalar(
                base->as.ternary.condition, program, func_index, include_current_external, scopes) ||
            semantic_expression_contains_float_scalar(
                base->as.ternary.then_expr, program, func_index, include_current_external, scopes) ||
            semantic_expression_contains_float_scalar(
                base->as.ternary.else_expr, program, func_index, include_current_external, scopes);
    default:
        return 0;
    }
}

static int semantic_validate_scalar_expression_usage(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    return semantic_validate_scalar_expression_usage_impl(
        expr,
        program,
        func_index,
        include_current_external,
        scopes,
        error);
}

static int semantic_validate_call_arguments_against_function_type(
    const AstExpression *call_expr,
    const SemanticTypeDescriptor *expected_function_type,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    size_t arg_index;

    if (!call_expr || call_expr->kind != AST_EXPR_CALL || !expected_function_type ||
        expected_function_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 1;
    }

    for (arg_index = 0u;
         arg_index < call_expr->as.call.arg_count &&
            arg_index < expected_function_type->parameter_count;
         ++arg_index) {
        int expected_param_kind =
            expected_function_type->parameter_value_kinds
                ? expected_function_type->parameter_value_kinds[arg_index]
                : AST_PARAMETER_VALUE_INT;
        SemanticTypeDescriptor actual_type;
        SemanticTypeDescriptor expected_arg_type;

        memset(&expected_arg_type, 0, sizeof(expected_arg_type));

        if (expected_param_kind == AST_PARAMETER_VALUE_PAIR ||
            expected_param_kind == AST_PARAMETER_VALUE_STRUCT) {
            expected_arg_type.kind = SEMANTIC_TYPE_KIND_AGGREGATE;
            expected_arg_type.aggregate_kind =
                expected_param_kind == AST_PARAMETER_VALUE_STRUCT
                    ? SEMANTIC_AGGREGATE_KIND_STRUCT
                    : SEMANTIC_AGGREGATE_KIND_PAIR;

            if (semantic_resolve_supported_aggregate_value_type(
                    call_expr->as.call.args[arg_index],
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    &actual_type)) {
                if (!semantic_type_descriptors_are_compatible(&expected_arg_type, &actual_type)) {
                    semantic_set_error(error,
                        call_expr->line,
                        call_expr->column,
                        "SEMA-AGG-006: aggregate call arguments currently require matching aggregate kinds");
                    return 0;
                }
                continue;
            }

            semantic_set_error(error,
                call_expr->line,
                call_expr->column,
                "SEMA-AGG-005: aggregate call arguments currently require another local aggregate value");
            return 0;
        }
        if (expected_param_kind == AST_PARAMETER_VALUE_FUNCTION) {
            int actual_type_ok = 0;

            if (!semantic_build_function_type_parameter_descriptor(
                    expected_function_type,
                    arg_index,
                    &expected_arg_type) ||
                expected_arg_type.kind != SEMANTIC_TYPE_KIND_FUNCTION) {
                semantic_set_error(error,
                    call_expr->line,
                    call_expr->column,
                    "SEMA-EXT-018: calls to functions with function-valued parameters are not yet enabled in the current extension semantic slice");
                return 0;
            }

            actual_type_ok = semantic_try_resolve_function_value_expression_type_descriptor(
                call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes,
                &actual_type);
            if (!actual_type_ok ||
                actual_type.kind != SEMANTIC_TYPE_KIND_FUNCTION ||
                !(
                    semantic_type_descriptors_are_compatible(
                        &expected_arg_type,
                        &actual_type) ||
                    (semantic_function_type_missing_nested_metadata(&expected_arg_type) &&
                        semantic_function_type_matches_with_missing_nested_fallback(
                            &expected_arg_type,
                            &actual_type))
                )) {
                semantic_set_error(error,
                    call_expr->line,
                    call_expr->column,
                    "SEMA-EXT-018: calls to functions with function-valued parameters are not yet enabled in the current extension semantic slice");
                return 0;
            }
            continue;
        }
        if (expected_param_kind != AST_PARAMETER_VALUE_INT &&
            expected_param_kind != AST_PARAMETER_VALUE_FLOAT) {
            continue;
        }
        if (semantic_expression_contains_float_scalar(
                call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes) &&
            !semantic_expression_resolve_scalar_type(
                call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes,
                &actual_type,
                NULL)) {
            return semantic_set_float_expression_usage_error(
                call_expr->as.call.args[arg_index],
                error);
        }
        if (!semantic_expression_resolve_scalar_type(
                call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes,
                &actual_type,
                error)) {
            continue;
        }
        if ((expected_param_kind == AST_PARAMETER_VALUE_INT &&
                actual_type.kind != SEMANTIC_TYPE_KIND_INT) ||
            (expected_param_kind == AST_PARAMETER_VALUE_FLOAT &&
                actual_type.kind != SEMANTIC_TYPE_KIND_FLOAT)) {
            semantic_set_error(error,
                call_expr->line,
                call_expr->column,
                "SEMA-TYPE-003: call argument scalar type mismatch");
            return 0;
        }
    }

    return 1;
}

static int semantic_validate_call_argument_scalar_types(const AstExpression *call_expr,
    const AstProgram *program,
    size_t func_index,
    int include_current_external,
    const ScopeStack *scopes,
    SemanticError *error) {
    AstExpression rewritten_call;
    const char *called_name = NULL;
    const AstExternal *current_func = NULL;
    const AstExternal *callee_external = NULL;
    const SemanticBuiltinFunctionInfo *builtin = NULL;
    SemanticTypeDescriptor expected_type;
    size_t arg_index;

    if (!call_expr || call_expr->kind != AST_EXPR_CALL || !program) {
        return 1;
    }
    {
        const AstExpression *callee_base = unwrap_paren_expression(call_expr->as.call.callee);

        if (callee_base && callee_base->kind == AST_EXPR_BINARY &&
            (callee_base->as.binary.op == TOKEN_COMMA || callee_base->as.binary.op == TOKEN_ASSIGN)) {
            const AstExpression *resolved_callee =
                callee_base->as.binary.op == TOKEN_COMMA
                    ? callee_base->as.binary.right
                    : callee_base->as.binary.left;

            rewritten_call = *call_expr;
            rewritten_call.as.call.callee = (AstExpression *)resolved_callee;
            return semantic_validate_call_argument_scalar_types(
                &rewritten_call,
                program,
                func_index,
                include_current_external,
                scopes,
                error);
        }
        if (callee_base && callee_base->kind == AST_EXPR_TERNARY) {
            const AstExpression *then_base = unwrap_paren_expression(callee_base->as.ternary.then_expr);
            const AstExpression *else_base = unwrap_paren_expression(callee_base->as.ternary.else_expr);

            if (then_base && else_base) {
                rewritten_call = *call_expr;
                rewritten_call.as.call.callee = (AstExpression *)then_base;
                if (!semantic_validate_call_argument_scalar_types(
                        &rewritten_call,
                        program,
                        func_index,
                        include_current_external,
                        scopes,
                        error)) {
                    return 0;
                }
                rewritten_call.as.call.callee = (AstExpression *)else_base;
                return semantic_validate_call_argument_scalar_types(
                    &rewritten_call,
                    program,
                    func_index,
                    include_current_external,
                    scopes,
                    error);
            }
        }
    }
    if (func_index < program->count &&
        program->externals &&
        program->externals[func_index].kind == AST_EXTERNAL_FUNCTION) {
        current_func = &program->externals[func_index];
    }
    if (semantic_resolve_immediate_function_value_call_type(
            call_expr->as.call.callee,
            program,
            func_index,
            include_current_external,
            &expected_type)) {
        return semantic_validate_call_arguments_against_function_type(
            call_expr,
            &expected_type,
            program,
            func_index,
            include_current_external,
            scopes,
            error);
    }
    if (classify_ast_call_callee(call_expr->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !called_name || called_name[0] == '\0') {
        return 1;
    }

    if (current_func &&
        semantic_find_named_function_parameter_type_descriptor(
            current_func,
            called_name,
            NULL,
            &expected_type) &&
        expected_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return semantic_validate_call_arguments_against_function_type(
            call_expr,
            &expected_type,
            program,
            func_index,
            include_current_external,
            scopes,
            error);
    }
    if (semantic_scope_lookup_local_function_type_descriptor(scopes, called_name, &expected_type) &&
        expected_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
        return semantic_validate_call_arguments_against_function_type(
            call_expr,
            &expected_type,
            program,
            func_index,
            include_current_external,
            scopes,
            error);
    }

    callee_external = semantic_find_visible_function_external(
        program,
        func_index,
        include_current_external,
        called_name);
    if ((!callee_external || callee_external->kind != AST_EXTERNAL_FUNCTION) &&
        !(builtin = semantic_find_builtin_function(called_name))) {
        return 1;
    }

    for (arg_index = 0u;
         arg_index < call_expr->as.call.arg_count &&
            ((callee_external && arg_index < callee_external->parameter_count) ||
                (builtin && arg_index < builtin->parameter_count));
         ++arg_index) {
        SemanticTypeDescriptor expected_type;
        SemanticTypeDescriptor actual_type;

        memset(&expected_type, 0, sizeof(expected_type));
        expected_type.kind = SEMANTIC_TYPE_KIND_INVALID;
        if (callee_external) {
            if (!semantic_build_function_parameter_type_descriptor(callee_external, arg_index, &expected_type) ||
                expected_type.kind == SEMANTIC_TYPE_KIND_FUNCTION ||
                expected_type.kind == SEMANTIC_TYPE_KIND_VOID) {
                continue;
            }
        } else {
            expected_type.kind = SEMANTIC_TYPE_KIND_INT;
        }

        if (semantic_resolve_supported_aggregate_value_type(
                call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes,
                &actual_type)) {
            if (expected_type.kind != SEMANTIC_TYPE_KIND_AGGREGATE) {
                semantic_set_error(error,
                    call_expr->line,
                    call_expr->column,
                    "SEMA-TYPE-003: call argument scalar type mismatch");
                return 0;
            }
            if (!semantic_type_descriptors_are_compatible(&expected_type, &actual_type)) {
                semantic_set_error(error,
                    call_expr->line,
                    call_expr->column,
                    "SEMA-AGG-006: aggregate call arguments currently require matching aggregate kinds");
                return 0;
            }
            continue;
        }
        if (expected_type.kind == SEMANTIC_TYPE_KIND_AGGREGATE) {
            semantic_set_error(error,
                call_expr->line,
                call_expr->column,
                "SEMA-AGG-005: aggregate call arguments currently require another local aggregate value");
            return 0;
        }
        if (semantic_expression_contains_float_scalar(
                call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes) &&
            !semantic_expression_resolve_scalar_type(call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes,
                &actual_type,
                NULL)) {
            return semantic_set_float_expression_usage_error(
                call_expr->as.call.args[arg_index],
                error);
        }
        if (!semantic_expression_resolve_scalar_type(call_expr->as.call.args[arg_index],
                program,
                func_index,
                include_current_external,
                scopes,
                &actual_type,
                error)) {
            continue;
        }
        if ((expected_type.kind == SEMANTIC_TYPE_KIND_INT ||
                expected_type.kind == SEMANTIC_TYPE_KIND_FLOAT) &&
            actual_type.kind != expected_type.kind) {
            semantic_set_error(error,
                call_expr->line,
                call_expr->column,
                "SEMA-TYPE-003: call argument scalar type mismatch");
            return 0;
        }
    }

    return 1;
}

static int semantic_fndefer_expression_uses_only_stable_function_scope_names(
    const AstExpression *expr,
    const ScopeStack *scopes,
    SemanticError *error,
    size_t depth) {
    size_t i;

    if (!expr) {
        return 1;
    }
    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER: {
        size_t frame_index = 0u;

        if (expr->as.identifier.name &&
            expr->as.identifier.name[0] != '\0' &&
            scope_stack_find_local_frame_index(scopes, expr->as.identifier.name, &frame_index) &&
            frame_index != 0u) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-013: fndefer may reference only globals, parameters, or function-top-level locals; loop-local and nested-block locals are rejected");
            return 0;
        }
        return 1;
    }
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_fndefer_expression_uses_only_stable_function_scope_names(
                    expr->as.init_list.items[i], scopes, error, depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
            expr->as.inner, scopes, error, depth + 1u);
    case AST_EXPR_UNARY:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
            expr->as.unary.operand, scopes, error, depth + 1u);
    case AST_EXPR_POSTFIX:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
            expr->as.postfix.operand, scopes, error, depth + 1u);
    case AST_EXPR_SUBSCRIPT:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.subscript.base, scopes, error, depth + 1u) &&
            semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.subscript.index, scopes, error, depth + 1u);
    case AST_EXPR_MEMBER:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
            expr->as.member.base, scopes, error, depth + 1u);
    case AST_EXPR_CALL:
        if (!semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.call.callee, scopes, error, depth + 1u)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_fndefer_expression_uses_only_stable_function_scope_names(
                    expr->as.call.args[i], scopes, error, depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.binary.left, scopes, error, depth + 1u) &&
            semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.binary.right, scopes, error, depth + 1u);
    case AST_EXPR_TERNARY:
        return semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.ternary.condition, scopes, error, depth + 1u) &&
            semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.ternary.then_expr, scopes, error, depth + 1u) &&
            semantic_fndefer_expression_uses_only_stable_function_scope_names(
                expr->as.ternary.else_expr, scopes, error, depth + 1u);
    default:
        return 1;
    }
}

static int semantic_fndefer_statement_uses_only_stable_function_scope_names(
    const AstStatement *stmt,
    const ScopeStack *scopes,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        if (!semantic_fndefer_expression_uses_only_stable_function_scope_names(
                stmt->expressions ? stmt->expressions[i] : NULL,
                scopes,
                error,
                0u)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_fndefer_statement_uses_only_stable_function_scope_names(
                stmt->children ? stmt->children[i] : NULL,
                scopes,
                error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_capdefer_expression_is_capture_safe(const AstExpression *expr,
    const ScopeStack *scopes,
    SemanticError *error,
    size_t depth) {
    size_t i;

    if (!expr) {
        return 1;
    }
    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER: {
        size_t frame_index = 0u;

        if (expr->as.identifier.name &&
            expr->as.identifier.name[0] != '\0' &&
            scope_stack_find_local_frame_index(scopes, expr->as.identifier.name, &frame_index) &&
            frame_index != 0u) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-022: capdefer may reference only globals, parameters, or function-top-level locals");
            return 0;
        }
        return 1;
    }
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_capdefer_expression_is_capture_safe(
                    expr->as.init_list.items ? expr->as.init_list.items[i] : NULL,
                    scopes,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_capdefer_expression_is_capture_safe(
            expr->as.inner, scopes, error, depth + 1u);
    case AST_EXPR_UNARY:
        return semantic_capdefer_expression_is_capture_safe(
            expr->as.unary.operand, scopes, error, depth + 1u);
    case AST_EXPR_POSTFIX:
        return semantic_capdefer_expression_is_capture_safe(
            expr->as.postfix.operand, scopes, error, depth + 1u);
    case AST_EXPR_SUBSCRIPT:
        return semantic_capdefer_expression_is_capture_safe(
                expr->as.subscript.base, scopes, error, depth + 1u) &&
            semantic_capdefer_expression_is_capture_safe(
                expr->as.subscript.index, scopes, error, depth + 1u);
    case AST_EXPR_MEMBER:
        return semantic_capdefer_expression_is_capture_safe(
            expr->as.member.base, scopes, error, depth + 1u);
    case AST_EXPR_CALL:
        if (!semantic_capdefer_expression_is_capture_safe(
                expr->as.call.callee, scopes, error, depth + 1u)) {
            return 0;
        }
        for (i = 0; i < expr->as.call.arg_count; ++i) {
            if (!semantic_capdefer_expression_is_capture_safe(
                    expr->as.call.args[i], scopes, error, depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        return semantic_capdefer_expression_is_capture_safe(
                expr->as.binary.left, scopes, error, depth + 1u) &&
            semantic_capdefer_expression_is_capture_safe(
                expr->as.binary.right, scopes, error, depth + 1u);
    case AST_EXPR_TERNARY:
        return semantic_capdefer_expression_is_capture_safe(
                expr->as.ternary.condition, scopes, error, depth + 1u) &&
            semantic_capdefer_expression_is_capture_safe(
                expr->as.ternary.then_expr, scopes, error, depth + 1u) &&
            semantic_capdefer_expression_is_capture_safe(
                expr->as.ternary.else_expr, scopes, error, depth + 1u);
    case AST_EXPR_CLOSURE:
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-040: closures are not yet enabled in the current extension semantic slice");
        return 0;
    default:
        return 1;
    }
}

static int semantic_expression_check_extension_mode(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    int allow_extension_features,
    SemanticError *error,
    size_t depth) {
    size_t i;

    if (!expr) {
        return 1;
    }
    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
    case AST_EXPR_NUMBER:
    case AST_EXPR_FLOAT_LITERAL:
        return 1;
    case AST_EXPR_INIT_LIST:
        for (i = 0u; i < expr->as.init_list.item_count; ++i) {
            if (!semantic_expression_check_extension_mode(
                    expr->as.init_list.items ? expr->as.init_list.items[i] : NULL,
                    program,
                    func_index,
                    allow_extension_features,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_PAREN:
        return semantic_expression_check_extension_mode(
            expr->as.inner,
            program,
            func_index,
            allow_extension_features,
            error,
            depth + 1u);
    case AST_EXPR_UNARY:
        return semantic_expression_check_extension_mode(
            expr->as.unary.operand,
            program,
            func_index,
            allow_extension_features,
            error,
            depth + 1u);
    case AST_EXPR_POSTFIX:
        return semantic_expression_check_extension_mode(
            expr->as.postfix.operand,
            program,
            func_index,
            allow_extension_features,
            error,
            depth + 1u);
    case AST_EXPR_SUBSCRIPT:
        return semantic_expression_check_extension_mode(
                expr->as.subscript.base,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u) &&
            semantic_expression_check_extension_mode(
                expr->as.subscript.index,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u);
    case AST_EXPR_MEMBER:
        return semantic_expression_check_extension_mode(
            expr->as.member.base,
            program,
            func_index,
            allow_extension_features,
            error,
            depth + 1u);
    case AST_EXPR_CALL:
        if (!semantic_expression_check_extension_mode(
                expr->as.call.callee,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u)) {
            return 0;
        }
        for (i = 0u; i < expr->as.call.arg_count; ++i) {
            const AstExpression *arg_expr = expr->as.call.args ? expr->as.call.args[i] : NULL;
            if (allow_extension_features &&
                arg_expr &&
                unwrap_paren_expression(arg_expr) &&
                unwrap_paren_expression(arg_expr)->kind == AST_EXPR_CLOSURE &&
                semantic_call_expression_extension_mode_accepts_direct_closure_argument(
                    expr,
                    program,
                    func_index,
                    arg_expr)) {
                continue;
            }
            if (!semantic_expression_check_extension_mode(
                    arg_expr,
                    program,
                    func_index,
                    allow_extension_features,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        return semantic_expression_check_extension_mode(
                expr->as.binary.left,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u) &&
            semantic_expression_check_extension_mode(
                expr->as.binary.right,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u);
    case AST_EXPR_TERNARY:
        return semantic_expression_check_extension_mode(
                expr->as.ternary.condition,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u) &&
            semantic_expression_check_extension_mode(
                expr->as.ternary.then_expr,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u) &&
            semantic_expression_check_extension_mode(
                expr->as.ternary.else_expr,
                program,
                func_index,
                allow_extension_features,
                error,
                depth + 1u);
    case AST_EXPR_CLOSURE:
        if (!allow_extension_features) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-039: closures are available only under -extension mode");
            return 0;
        }
        return 1;
    default:
        return 1;
    }
}

static int semantic_call_expression_extension_mode_accepts_direct_closure_argument(
    const AstExpression *call_expr,
    const AstProgram *program,
    size_t func_index,
    const AstExpression *candidate_arg) {
    const char *called_name = NULL;
    const AstExternal *visible_function = NULL;
    size_t arg_index;

    if (!call_expr || call_expr->kind != AST_EXPR_CALL || !candidate_arg ||
        !program) {
        return 0;
    }

    if (classify_ast_call_callee(call_expr->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
        !called_name || called_name[0] == '\0') {
        return 0;
    }

    visible_function = semantic_find_visible_function_external(
        program,
        func_index,
        1,
        called_name);
    if (!visible_function || !visible_function->parameter_value_kinds || !call_expr->as.call.args) {
        return 0;
    }

    for (arg_index = 0u; arg_index < call_expr->as.call.arg_count &&
            arg_index < visible_function->parameter_count; ++arg_index) {
        if (call_expr->as.call.args[arg_index] == candidate_arg &&
            visible_function->parameter_value_kinds[arg_index] == AST_PARAMETER_VALUE_FUNCTION) {
            return 1;
        }
    }

    return 0;
}

static int semantic_closure_signature_matches_expected(const AstExpression *closure_expr,
    const SemanticTypeDescriptor *expected_type) {
    SemanticTypeDescriptor actual_type;

    if (!closure_expr || closure_expr->kind != AST_EXPR_CLOSURE || !expected_type ||
        expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }

    if (!semantic_build_closure_type_descriptor(closure_expr, &actual_type) ||
        !semantic_type_descriptors_are_compatible(expected_type, &actual_type)) {
        return 0;
    }
    return 1;
}

static int semantic_name_is_in_list(const char *const *names,
    size_t count,
    const char *name) {
    size_t index;

    if (!name || name[0] == '\0') {
        return 0;
    }
    for (index = 0u; index < count; ++index) {
        if (names && names[index] && strcmp(names[index], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int semantic_append_name_copy(char ***items,
    size_t *count,
    size_t *capacity,
    const char *name) {
    char **new_items;
    char *name_copy;
    size_t next_capacity;
    size_t name_len;

    if (!items || !count || !capacity || !name || name[0] == '\0') {
        return 0;
    }

    if (*count == *capacity) {
        next_capacity = (*capacity == 0u) ? 4u : (*capacity * 2u);
        new_items = (char **)realloc(*items, next_capacity * sizeof(char *));
        if (!new_items) {
            return 0;
        }
        *items = new_items;
        *capacity = next_capacity;
    }

    name_len = strlen(name);
    name_copy = (char *)malloc(name_len + 1u);
    if (!name_copy) {
        return 0;
    }
    memcpy(name_copy, name, name_len + 1u);
    (*items)[*count] = name_copy;
    *count += 1u;
    return 1;
}

static void semantic_free_name_copies(char **items, size_t count) {
    size_t index;

    if (!items) {
        return;
    }
    for (index = 0u; index < count; ++index) {
        free(items[index]);
    }
    free(items);
}

static int semantic_closure_body_expr_is_phase1_supported(const AstExpression *expr,
    const AstProgram *program,
    size_t func_index,
    const char *const *capture_names,
    size_t capture_count,
    const char *const *visible_scalar_names,
    size_t visible_scalar_name_count,
    int allow_assignments,
    SemanticError *error,
    size_t depth) {
    size_t i;

    if (!expr) {
        return 1;
    }
    if (depth > SEMANTIC_MAX_EXPRESSION_DEPTH) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-INT-012: expression depth exceeds semantic analysis limit");
        return 0;
    }

    switch (expr->kind) {
    case AST_EXPR_IDENTIFIER:
        if (!semantic_name_is_in_list(capture_names, capture_count, expr->as.identifier.name) &&
            !semantic_name_is_in_list(visible_scalar_names, visible_scalar_name_count, expr->as.identifier.name)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-044: closure body may reference only captured names, closure parameters, and direct global/builtin callees in the first extension slice");
            return 0;
        }
        return 1;
    case AST_EXPR_NUMBER:
        return 1;
    case AST_EXPR_PAREN:
        return semantic_closure_body_expr_is_phase1_supported(
            expr->as.inner,
            program,
            func_index,
            capture_names,
            capture_count,
            visible_scalar_names,
            visible_scalar_name_count,
            allow_assignments,
            error,
            depth + 1u);
    case AST_EXPR_CALL:
        if (expr->as.call.callee &&
            expr->as.call.callee->kind == AST_EXPR_IDENTIFIER &&
            expr->as.call.callee->as.identifier.name &&
            !semantic_name_is_in_list(capture_names, capture_count, expr->as.call.callee->as.identifier.name) &&
            !semantic_name_is_in_list(visible_scalar_names, visible_scalar_name_count, expr->as.call.callee->as.identifier.name)) {
            if (!semantic_find_visible_function_external(
                    program,
                    func_index,
                    1,
                    expr->as.call.callee->as.identifier.name) &&
                !semantic_find_builtin_function(expr->as.call.callee->as.identifier.name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-044: closure body may reference only captured names, closure parameters, and direct global/builtin callees in the first extension slice");
                return 0;
            }
        } else if (!semantic_closure_body_expr_is_phase1_supported(
                expr->as.call.callee,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u)) {
            return 0;
        }
        for (i = 0u; i < expr->as.call.arg_count; ++i) {
            if (!semantic_closure_body_expr_is_phase1_supported(
                    expr->as.call.args ? expr->as.call.args[i] : NULL,
                    program,
                    func_index,
                    capture_names,
                    capture_count,
                    visible_scalar_names,
                    visible_scalar_name_count,
                    allow_assignments,
                    error,
                    depth + 1u)) {
                return 0;
            }
        }
        return 1;
    case AST_EXPR_BINARY:
        if (expr->as.binary.op == TOKEN_ASSIGN) {
            const AstExpression *lhs = unwrap_paren_expression(expr->as.binary.left);
            const char *lhs_name = (lhs && lhs->kind == AST_EXPR_IDENTIFIER)
                ? lhs->as.identifier.name
                : NULL;

            if (!allow_assignments ||
                !lhs_name ||
                lhs_name[0] == '\0' ||
                semantic_name_is_in_list(capture_names, capture_count, lhs_name) ||
                !semantic_name_is_in_list(visible_scalar_names, visible_scalar_name_count, lhs_name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-047: closure assignment targets currently support only closure parameters and closure-local int scalars");
                return 0;
            }
            return semantic_closure_body_expr_is_phase1_supported(
                expr->as.binary.right,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u);
        }
        if (expr->as.binary.op == TOKEN_PLUS_ASSIGN ||
            expr->as.binary.op == TOKEN_MINUS_ASSIGN ||
            expr->as.binary.op == TOKEN_STAR_ASSIGN ||
            expr->as.binary.op == TOKEN_SLASH_ASSIGN ||
            expr->as.binary.op == TOKEN_PERCENT_ASSIGN ||
            expr->as.binary.op == TOKEN_AMP_ASSIGN ||
            expr->as.binary.op == TOKEN_CARET_ASSIGN ||
            expr->as.binary.op == TOKEN_PIPE_ASSIGN ||
            expr->as.binary.op == TOKEN_SHIFT_LEFT_ASSIGN ||
            expr->as.binary.op == TOKEN_SHIFT_RIGHT_ASSIGN) {
            const AstExpression *lhs = unwrap_paren_expression(expr->as.binary.left);
            const char *lhs_name = (lhs && lhs->kind == AST_EXPR_IDENTIFIER)
                ? lhs->as.identifier.name
                : NULL;

            if (!allow_assignments ||
                !lhs_name ||
                lhs_name[0] == '\0' ||
                semantic_name_is_in_list(capture_names, capture_count, lhs_name) ||
                !semantic_name_is_in_list(visible_scalar_names, visible_scalar_name_count, lhs_name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-047: closure assignment targets currently support only closure parameters and closure-local int scalars");
                return 0;
            }
            return semantic_closure_body_expr_is_phase1_supported(
                expr->as.binary.right,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u);
        }
        if (expr->as.binary.op != TOKEN_PLUS &&
            expr->as.binary.op != TOKEN_MINUS &&
            expr->as.binary.op != TOKEN_STAR &&
            expr->as.binary.op != TOKEN_SLASH &&
            expr->as.binary.op != TOKEN_PERCENT &&
            expr->as.binary.op != TOKEN_AMP &&
            expr->as.binary.op != TOKEN_CARET &&
            expr->as.binary.op != TOKEN_PIPE &&
            expr->as.binary.op != TOKEN_SHIFT_LEFT &&
            expr->as.binary.op != TOKEN_SHIFT_RIGHT &&
            expr->as.binary.op != TOKEN_EQ &&
            expr->as.binary.op != TOKEN_NE &&
            expr->as.binary.op != TOKEN_LT &&
            expr->as.binary.op != TOKEN_LE &&
            expr->as.binary.op != TOKEN_GT &&
            expr->as.binary.op != TOKEN_GE &&
            expr->as.binary.op != TOKEN_AND_AND &&
            expr->as.binary.op != TOKEN_OR_OR &&
            expr->as.binary.op != TOKEN_COMMA) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-044: closure body currently supports only pure int expressions and calls");
            return 0;
        }
        return semantic_closure_body_expr_is_phase1_supported(
                expr->as.binary.left,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u) &&
            semantic_closure_body_expr_is_phase1_supported(
                expr->as.binary.right,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u);
    case AST_EXPR_UNARY:
        if ((expr->as.unary.op == TOKEN_PLUS_PLUS ||
                expr->as.unary.op == TOKEN_MINUS_MINUS) &&
            allow_assignments) {
            const AstExpression *operand = unwrap_paren_expression(expr->as.unary.operand);
            const char *operand_name =
                (operand && operand->kind == AST_EXPR_IDENTIFIER)
                    ? operand->as.identifier.name
                    : NULL;

            if (!operand_name ||
                operand_name[0] == '\0' ||
                semantic_name_is_in_list(capture_names, capture_count, operand_name) ||
                !semantic_name_is_in_list(visible_scalar_names, visible_scalar_name_count, operand_name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-047: closure assignment targets currently support only closure parameters and closure-local int scalars");
                return 0;
            }
            return 1;
        }
        if (expr->as.unary.op != TOKEN_PLUS &&
            expr->as.unary.op != TOKEN_MINUS &&
            expr->as.unary.op != TOKEN_BANG &&
            expr->as.unary.op != TOKEN_TILDE) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-044: closure body currently supports only pure int expressions and calls");
            return 0;
        }
        return semantic_closure_body_expr_is_phase1_supported(
            expr->as.unary.operand,
            program,
            func_index,
            capture_names,
            capture_count,
            visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u);
    case AST_EXPR_POSTFIX:
        if ((expr->as.postfix.op == TOKEN_PLUS_PLUS ||
                expr->as.postfix.op == TOKEN_MINUS_MINUS) &&
            allow_assignments) {
            const AstExpression *operand = unwrap_paren_expression(expr->as.postfix.operand);
            const char *operand_name =
                (operand && operand->kind == AST_EXPR_IDENTIFIER)
                    ? operand->as.identifier.name
                    : NULL;

            if (!operand_name ||
                operand_name[0] == '\0' ||
                semantic_name_is_in_list(capture_names, capture_count, operand_name) ||
                !semantic_name_is_in_list(visible_scalar_names, visible_scalar_name_count, operand_name)) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-047: closure assignment targets currently support only closure parameters and closure-local int scalars");
                return 0;
            }
            return 1;
        }
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-044: closure body currently supports only pure int expressions and calls");
        return 0;
    case AST_EXPR_TERNARY:
        return semantic_closure_body_expr_is_phase1_supported(
                expr->as.ternary.condition,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u) &&
            semantic_closure_body_expr_is_phase1_supported(
                expr->as.ternary.then_expr,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u) &&
            semantic_closure_body_expr_is_phase1_supported(
                expr->as.ternary.else_expr,
                program,
                func_index,
                capture_names,
                capture_count,
                visible_scalar_names,
                visible_scalar_name_count,
                allow_assignments,
                error,
                depth + 1u);
    default:
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-044: closure body currently supports only pure int expressions and calls");
        return 0;
    }
}

static int semantic_closure_body_is_phase1_supported(const AstStatement *body,
    AstFunctionReturnType return_type,
    const AstProgram *program,
    size_t func_index,
    const char *const *capture_names,
    size_t capture_count,
    const char *const *parameter_names,
    size_t parameter_count,
    SemanticError *error) {
    size_t child_index;
    char **local_names = NULL;
    size_t local_name_count = 0u;
    size_t local_name_capacity = 0u;
    const char **visible_parameter_names = NULL;
    size_t visible_parameter_count = 0u;
    size_t total_visible_name_count = 0u;
    const char **visible_names = NULL;
    size_t closure_local_decl_capacity = 0u;

    if (body && body->children) {
        for (child_index = 0u; child_index < body->child_count; ++child_index) {
            const AstStatement *child = body->children[child_index];
            if (child && child->kind == AST_STMT_DECLARATION) {
                closure_local_decl_capacity += child->declaration_name_count;
            }
        }
    }
    total_visible_name_count = capture_count + parameter_count + closure_local_decl_capacity;
    if (total_visible_name_count > 0u) {
        visible_names = (const char **)malloc(total_visible_name_count * sizeof(const char *));
        if (!visible_names) {
            semantic_set_error(error, body ? body->line : 0, body ? body->column : 0,
                "SEMA-INT-011: failed to allocate closure semantic workspace");
            return 0;
        }
    }
    visible_parameter_names = visible_names;
    visible_parameter_count = capture_count + parameter_count;
    for (child_index = 0u; child_index < capture_count; ++child_index) {
        visible_names[child_index] = capture_names ? capture_names[child_index] : NULL;
    }
    for (child_index = 0u; child_index < parameter_count; ++child_index) {
        visible_names[capture_count + child_index] =
            parameter_names ? parameter_names[child_index] : NULL;
    }

    if (!body || body->kind != AST_STMT_COMPOUND) {
        semantic_set_error(error, 0, 0,
            "SEMA-EXT-041: closure body currently requires a compound body with a single return statement");
        free(visible_names);
        return 0;
    }
    if (!body->children || body->child_count == 0u) {
        semantic_set_error(error,
            body->line,
            body->column,
            "SEMA-EXT-041: closure body currently requires a compound body with a single return statement");
        free(visible_names);
        return 0;
    }

    if (return_type == AST_FUNCTION_RETURN_VOID) {
        for (child_index = 0u; child_index < body->child_count; ++child_index) {
            const AstStatement *child = body->children[child_index];

            if (!child) {
                semantic_set_error(error,
                    body->line,
                    body->column,
                    "SEMA-EXT-041: closure body currently requires a final 'return;' after pure expression statements");
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 0;
            }
            if (child_index + 1u == body->child_count) {
                if (child->kind != AST_STMT_RETURN || child->expression_count != 0u) {
                    semantic_set_error(error,
                        child->line,
                        child->column,
                        "SEMA-EXT-041: closure body currently requires a final 'return;' after pure expression statements");
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 1;
            }
            if (child->kind == AST_STMT_DECLARATION) {
                size_t decl_index;

                if (!child->declaration_names || !child->declaration_value_kinds ||
                    child->declaration_name_count == 0u) {
                    semantic_set_error(error,
                        child->line,
                        child->column,
                        "SEMA-EXT-041: void closure bodies currently support only pure int local declarations, pure expression statements, and a final return");
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
                for (decl_index = 0u; decl_index < child->declaration_name_count; ++decl_index) {
                    const char *decl_name =
                        child->declaration_names ? child->declaration_names[decl_index] : NULL;
                    const AstExpression *init_expr =
                        (child->expressions && decl_index < child->expression_count)
                            ? child->expressions[decl_index]
                            : NULL;

                    if (!decl_name || decl_name[0] == '\0' ||
                        !child->declaration_value_kinds ||
                        child->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_INT) {
                        semantic_set_error(error,
                            child->line,
                            child->column,
                            "SEMA-EXT-041: void closure bodies currently support only pure int local declarations, pure expression statements, and a final return");
                        semantic_free_name_copies(local_names, local_name_count);
                        free(visible_names);
                        return 0;
                    }
                    if (semantic_name_is_in_list((const char *const *)visible_parameter_names,
                            visible_parameter_count,
                            decl_name) ||
                        semantic_name_is_in_list((const char *const *)local_names,
                            local_name_count,
                            decl_name)) {
                        semantic_set_error(error,
                            child->line,
                            child->column,
                            "SEMA-EXT-046: closure local declarations may not shadow captures, parameters, or earlier closure locals");
                        semantic_free_name_copies(local_names, local_name_count);
                        free(visible_names);
                        return 0;
                    }
                    if (init_expr &&
                        !semantic_closure_body_expr_is_phase1_supported(
                            init_expr,
                            program,
                            func_index,
                            capture_names,
                            capture_count,
                            visible_names,
                            visible_parameter_count + local_name_count,
                            0,
                            error,
                            0u)) {
                        semantic_free_name_copies(local_names, local_name_count);
                        free(visible_names);
                        return 0;
                    }
                    if (!semantic_append_name_copy(
                            &local_names,
                            &local_name_count,
                            &local_name_capacity,
                            decl_name)) {
                        semantic_set_error(error,
                            child->line,
                            child->column,
                            "SEMA-INT-011: failed to allocate closure semantic workspace");
                        semantic_free_name_copies(local_names, local_name_count);
                        free(visible_names);
                        return 0;
                    }
                    visible_names[visible_parameter_count + local_name_count - 1u] = local_names[local_name_count - 1u];
                }
                continue;
            }
            if (child->kind != AST_STMT_EXPRESSION) {
                semantic_set_error(error,
                    child->line,
                    child->column,
                    "SEMA-EXT-041: void closure bodies currently support only pure expression statements before the final return");
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 0;
            }
            for (size_t expr_index = 0u; expr_index < child->expression_count; ++expr_index) {
                if (!semantic_closure_body_expr_is_phase1_supported(
                        child->expressions ? child->expressions[expr_index] : NULL,
                        program,
                        func_index,
                        capture_names,
                        capture_count,
                        visible_names,
                        visible_parameter_count + local_name_count,
                        1,
                        error,
                        0u)) {
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
            }
        }
        semantic_free_name_copies(local_names, local_name_count);
        free(visible_names);
        return 1;
    }

    for (child_index = 0u; child_index < body->child_count; ++child_index) {
        const AstStatement *child = body->children[child_index];

        if (!child) {
            semantic_set_error(error,
                body->line,
                body->column,
                "SEMA-EXT-041: int closure bodies currently require pure expression statements followed by a final return statement");
            semantic_free_name_copies(local_names, local_name_count);
            free(visible_names);
            return 0;
        }
        if (child_index + 1u == body->child_count) {
            if (child->kind != AST_STMT_RETURN ||
                child->expression_count != 1u ||
                !child->expressions ||
                !child->expressions[0]) {
                    semantic_set_error(error,
                    child->line,
                    child->column,
                    "SEMA-EXT-041: int closure bodies currently require pure expression statements followed by a final return statement");
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 0;
            }
            if (!semantic_closure_body_expr_is_phase1_supported(
                child->expressions[0],
                program,
                func_index,
                capture_names,
                capture_count,
                visible_names,
                visible_parameter_count + local_name_count,
                1,
                error,
                0u)) {
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 0;
            }
            semantic_free_name_copies(local_names, local_name_count);
            free(visible_names);
            return 1;
        }
        if (child->kind == AST_STMT_DECLARATION) {
            size_t decl_index;

            if (!child->declaration_names || !child->declaration_value_kinds ||
                child->declaration_name_count == 0u) {
                semantic_set_error(error,
                    child->line,
                    child->column,
                    "SEMA-EXT-041: int closure bodies currently support only pure int local declarations, pure expression statements, and a final return");
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 0;
            }
            for (decl_index = 0u; decl_index < child->declaration_name_count; ++decl_index) {
                const char *decl_name =
                    child->declaration_names ? child->declaration_names[decl_index] : NULL;
                const AstExpression *init_expr =
                    (child->expressions && decl_index < child->expression_count)
                        ? child->expressions[decl_index]
                        : NULL;

                if (!decl_name || decl_name[0] == '\0' ||
                    !child->declaration_value_kinds ||
                    child->declaration_value_kinds[decl_index] != AST_DECLARATION_VALUE_INT) {
                    semantic_set_error(error,
                        child->line,
                        child->column,
                        "SEMA-EXT-041: int closure bodies currently support only pure int local declarations, pure expression statements, and a final return");
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
                if (semantic_name_is_in_list((const char *const *)visible_parameter_names,
                        visible_parameter_count,
                        decl_name) ||
                    semantic_name_is_in_list((const char *const *)local_names,
                        local_name_count,
                        decl_name)) {
                    semantic_set_error(error,
                        child->line,
                        child->column,
                        "SEMA-EXT-046: closure local declarations may not shadow captures, parameters, or earlier closure locals");
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
                if (init_expr &&
                    !semantic_closure_body_expr_is_phase1_supported(
                        init_expr,
                        program,
                        func_index,
                        capture_names,
                        capture_count,
                        visible_names,
                        visible_parameter_count + local_name_count,
                        0,
                        error,
                        0u)) {
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
                if (!semantic_append_name_copy(
                        &local_names,
                        &local_name_count,
                        &local_name_capacity,
                        decl_name)) {
                    semantic_set_error(error,
                        child->line,
                        child->column,
                        "SEMA-INT-011: failed to allocate closure semantic workspace");
                    semantic_free_name_copies(local_names, local_name_count);
                    free(visible_names);
                    return 0;
                }
                visible_names[visible_parameter_count + local_name_count - 1u] = local_names[local_name_count - 1u];
            }
            continue;
        }
        if (child->kind != AST_STMT_EXPRESSION) {
            semantic_set_error(error,
                child->line,
                child->column,
                "SEMA-EXT-041: int closure bodies currently support only pure expression statements before the final return");
            semantic_free_name_copies(local_names, local_name_count);
            free(visible_names);
            return 0;
        }
        for (size_t expr_index = 0u; expr_index < child->expression_count; ++expr_index) {
            if (!semantic_closure_body_expr_is_phase1_supported(
                    child->expressions ? child->expressions[expr_index] : NULL,
                    program,
                    func_index,
                    capture_names,
                    capture_count,
                    visible_names,
                    visible_parameter_count + local_name_count,
                    1,
                    error,
                    0u)) {
                semantic_free_name_copies(local_names, local_name_count);
                free(visible_names);
                return 0;
            }
        }
    }

    semantic_free_name_copies(local_names, local_name_count);
    free(visible_names);
    return 1;
}

static int semantic_closure_capture_is_supported(const char *capture_name,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error,
    int line,
    int column) {
    SemanticTypeDescriptor local_type;
    const AstExternal *external = NULL;
    size_t frame_index = 0u;

    if (!capture_name || capture_name[0] == '\0') {
        semantic_set_error(error,
            line,
            column,
            "SEMA-EXT-042: closure captures must be previously declared int globals, parameters, or function-top-level locals");
        return 0;
    }

    if (scope_stack_find_local_frame_index(scopes, capture_name, &frame_index)) {
        if (frame_index != 0u) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-EXT-042: closure captures must be previously declared int globals, parameters, or function-top-level locals");
            return 0;
        }
        if (!semantic_scope_lookup_local_scalar_type_descriptor(scopes, capture_name, &local_type) ||
            local_type.kind != SEMANTIC_TYPE_KIND_INT) {
            semantic_set_error(error,
                line,
                column,
                "SEMA-EXT-043: closure captures currently support copied int values only");
            return 0;
        }
        return 1;
    }

    external = semantic_find_visible_external(program, func_index, 1, capture_name);
    if (!external || external->kind != AST_EXTERNAL_DECLARATION ||
        external->declaration_array_rank > 0u ||
        external->declaration_value_kind != AST_DECLARATION_VALUE_INT) {
        semantic_set_error(error,
            line,
            column,
            "SEMA-EXT-042: closure captures must be previously declared int globals, parameters, or function-top-level locals");
        return 0;
    }
    return 1;
}

static int semantic_validate_closure_literal_function_argument_expression(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    size_t capture_index;

    if (!base || base->kind != AST_EXPR_CLOSURE || !expected_type ||
        expected_type->kind != SEMANTIC_TYPE_KIND_FUNCTION) {
        return 0;
    }
    if (!semantic_closure_signature_matches_expected(base, expected_type)) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-045: closure literal signature must match the target function-valued parameter type in the first extension slice");
        return 0;
    }
    if (!semantic_closure_body_is_phase1_supported(
            base->as.closure.body,
            base->as.closure.return_type,
            program,
            func_index,
            (const char *const *)base->as.closure.capture_names,
            base->as.closure.capture_name_count,
            (const char *const *)base->as.closure.parameter_names,
            base->as.closure.parameter_count,
            error)) {
        return 0;
    }
    for (capture_index = 0u; capture_index < base->as.closure.capture_name_count; ++capture_index) {
        const char *capture_name =
            base->as.closure.capture_names ? base->as.closure.capture_names[capture_index] : NULL;
        size_t prior_index;

        for (prior_index = 0u; prior_index < capture_index; ++prior_index) {
            if (capture_name &&
                base->as.closure.capture_names &&
                base->as.closure.capture_names[prior_index] &&
                strcmp(capture_name, base->as.closure.capture_names[prior_index]) == 0) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-042: closure captures must be unique in the first extension slice");
                return 0;
            }
        }
        if (!semantic_name_is_in_list(
                (const char *const *)base->as.closure.parameter_names,
                base->as.closure.parameter_count,
                capture_name) &&
            !semantic_closure_capture_is_supported(
                capture_name,
                program,
                func_index,
                scopes,
                error,
                expr->line,
                expr->column)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_validate_closure_literal_initializer(const AstExpression *expr,
    const SemanticTypeDescriptor *expected_type,
    const AstProgram *program,
    size_t func_index,
    const ScopeStack *scopes,
    SemanticError *error) {
    const AstExpression *base = unwrap_paren_expression(expr);
    size_t capture_index;

    if (!base || base->kind != AST_EXPR_CLOSURE) {
        return 0;
    }
    if (!scopes || scopes->count != 1u) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-041: closure literals currently require top-level function-scope local initialization of a function-typed local");
        return 0;
    }
    if (!semantic_closure_signature_matches_expected(base, expected_type)) {
        semantic_set_error(error,
            expr->line,
            expr->column,
            "SEMA-EXT-045: closure literal signature must match the target local function type in the first extension slice");
        return 0;
    }
    if (!semantic_closure_body_is_phase1_supported(
            base->as.closure.body,
            base->as.closure.return_type,
            program,
            func_index,
            (const char *const *)base->as.closure.capture_names,
            base->as.closure.capture_name_count,
            (const char *const *)base->as.closure.parameter_names,
            base->as.closure.parameter_count,
            error)) {
        return 0;
    }
    for (capture_index = 0u; capture_index < base->as.closure.capture_name_count; ++capture_index) {
        size_t prior_index;
        const char *capture_name =
            base->as.closure.capture_names ? base->as.closure.capture_names[capture_index] : NULL;

        for (prior_index = 0u; prior_index < capture_index; ++prior_index) {
            if (capture_name &&
                base->as.closure.capture_names &&
                base->as.closure.capture_names[prior_index] &&
                strcmp(capture_name, base->as.closure.capture_names[prior_index]) == 0) {
                semantic_set_error(error,
                    expr->line,
                    expr->column,
                    "SEMA-EXT-042: closure captures must be previously declared int globals, parameters, or function-top-level locals");
                return 0;
            }
        }
        if (semantic_name_is_in_list(
                (const char *const *)base->as.closure.parameter_names,
                base->as.closure.parameter_count,
                capture_name)) {
            semantic_set_error(error,
                expr->line,
                expr->column,
                "SEMA-EXT-042: closure captures must be previously declared int globals, parameters, or function-top-level locals");
            return 0;
        }
        if (!semantic_closure_capture_is_supported(
                capture_name,
                program,
                func_index,
                scopes,
                error,
                base->line,
                base->column)) {
            return 0;
        }
    }
    return 1;
}

static int semantic_capdefer_body_is_supported(const AstStatement *stmt,
    const ScopeStack *scopes,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_EXPRESSION:
        for (i = 0; i < stmt->expression_count; ++i) {
            if (!semantic_capdefer_expression_is_capture_safe(
                    stmt->expressions ? stmt->expressions[i] : NULL,
                    scopes,
                    error,
                    0u)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_COMPOUND:
        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_capdefer_body_is_supported(
                    stmt->children ? stmt->children[i] : NULL,
                    scopes,
                    error)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_IF:
        for (i = 0; i < stmt->expression_count; ++i) {
            if (!semantic_capdefer_expression_is_capture_safe(
                    stmt->expressions ? stmt->expressions[i] : NULL,
                    scopes,
                    error,
                    0u)) {
                return 0;
            }
        }
        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_capdefer_body_is_supported(
                    stmt->children ? stmt->children[i] : NULL,
                    scopes,
                    error)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_RETURN:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-023: capdefer body may not contain return in the first extension version");
        return 0;
    case AST_STMT_BREAK:
    case AST_STMT_CONTINUE:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-024: capdefer body may not contain loop control in the first extension version");
        return 0;
    case AST_STMT_DEFER:
    case AST_STMT_FNDEFER:
    case AST_STMT_CAPDEFER:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-025: capdefer body may not contain nested defer/fndefer/capdefer in the first extension version");
        return 0;
    case AST_STMT_DECLARATION:
    case AST_STMT_WHILE:
    case AST_STMT_FOR:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-026: capdefer body currently supports only expression/if/compound statements");
        return 0;
    default:
        return 1;
    }
}

static int semantic_defer_body_is_supported(const AstStatement *stmt,
    SemanticError *error);
static int semantic_fndefer_body_is_supported(const AstStatement *stmt,
    SemanticError *error);
static int semantic_check_loop_nested_fndefer_site_limit(const AstStatement *stmt,
    size_t loop_depth,
    size_t *io_loop_nested_site_count,
    SemanticError *error);
static int semantic_scope_check_statement(const AstStatement *stmt,
    const AstProgram *program,
    size_t func_index,
    ScopeStack *scopes,
    const SemanticAnalyzeConfig *config,
    int in_dynamic_registration_context,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    if (stmt->extension_origin == AST_EXTENSION_ORIGIN_UNLESS) {
        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-006: 'unless' is available only under -extension mode");
            return 0;
        }
    }
    if (stmt->extension_origin == AST_EXTENSION_ORIGIN_PAIR) {
        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-020: 'pair' is available only under -extension mode");
            return 0;
        }
    }
    if (stmt->extension_origin == AST_EXTENSION_ORIGIN_STRUCT) {
        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-021: 'struct' is available only under -extension mode");
            return 0;
        }
    }
    if (stmt->extension_origin == AST_EXTENSION_ORIGIN_CAPDEFER) {
        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-027: 'capdefer' is available only under -extension mode");
            return 0;
        }
    }

    if (stmt->kind == AST_STMT_DEFER) {
        const AstStatement *payload = NULL;

        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-001: 'defer' is available only under -extension mode");
            return 0;
        }

        if (stmt->child_count == 0u || !stmt->children || !stmt->children[0]) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-002: defer statement requires a non-empty payload");
            return 0;
        }

        payload = stmt->children[0];
        if (!semantic_defer_body_is_supported(payload, error)) {
            return 0;
        }

        return semantic_scope_check_statement(payload,
            program,
            func_index,
            scopes,
            config,
            in_dynamic_registration_context,
            error);
    }

    if (stmt->kind == AST_STMT_FNDEFER) {
        const AstStatement *payload = NULL;

        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-007: 'fndefer' is available only under -extension mode");
            return 0;
        }

        if (stmt->child_count == 0u || !stmt->children || !stmt->children[0]) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-014: fndefer statement requires a non-empty payload");
            return 0;
        }

        payload = stmt->children[0];
        if (!semantic_fndefer_body_is_supported(payload, error)) {
            return 0;
        }
        if (!semantic_fndefer_statement_uses_only_stable_function_scope_names(
                payload,
                scopes,
                error)) {
            return 0;
        }

        return semantic_scope_check_statement(payload,
            program,
            func_index,
            scopes,
            config,
            in_dynamic_registration_context,
            error);
    }

    if (stmt->kind == AST_STMT_CAPDEFER) {
        const AstStatement *payload = NULL;
        size_t capture_index;
        int ok = 0;

        if (!config || !config->allow_extension_features) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-027: 'capdefer' is available only under -extension mode");
            return 0;
        }
        if (stmt->capture_name_count == 0u || !stmt->capture_names ||
            stmt->capture_expression_count != stmt->capture_name_count ||
            !stmt->capture_expressions) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-028: capdefer requires one or more capture bindings");
            return 0;
        }
        if (stmt->child_count == 0u || !stmt->children || !stmt->children[0]) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-029: capdefer statement requires a non-empty payload");
            return 0;
        }

        for (capture_index = 0u; capture_index < stmt->capture_name_count; ++capture_index) {
            const char *capture_name = stmt->capture_names[capture_index];

            if (!capture_name || capture_name[0] == '\0') {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-EXT-030: capdefer capture names must be non-empty identifiers");
                return 0;
            }
            if (!semantic_capdefer_expression_is_capture_safe(
                    stmt->capture_expressions[capture_index],
                    scopes,
                    error,
                    0u)) {
                return 0;
            }
            if (!semantic_scope_check_expression(stmt->capture_expressions[capture_index],
                    program,
                    func_index,
                    scopes,
                    config ? config->allow_extension_features : 0,
                    0,
                    0,
                    error)) {
                return 0;
            }
        }

        payload = stmt->children[0];
        if (!semantic_capdefer_body_is_supported(payload, scopes, error)) {
            return 0;
        }
        if (in_dynamic_registration_context) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-034: capdefer sites inside conditional or loop control flow are not yet supported in the first extension version");
            return 0;
        }

        if (!scope_stack_push(scopes)) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-INT-010: failed to extend semantic scope stack");
            return 0;
        }

        for (capture_index = 0u; capture_index < stmt->capture_name_count; ++capture_index) {
            const char *capture_name = stmt->capture_names[capture_index];

            if (scope_stack_contains_local_name(scopes, capture_name)) {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-EXT-031: capdefer capture names may not shadow an existing local name");
                scope_stack_pop(scopes);
                return 0;
            }
            if (!scope_stack_add_to_current(
                    scopes,
                    capture_name,
                    0,
                    AST_DECLARATION_VALUE_INT,
                    NULL,
                    AST_FUNCTION_RETURN_INT,
                    0u,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL)) {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-INT-011: failed to record capdefer capture name in scope");
                scope_stack_pop(scopes);
                return 0;
            }
        }

        ok = semantic_scope_check_statement(payload,
            program,
            func_index,
            scopes,
            config,
            in_dynamic_registration_context,
            error);
        scope_stack_pop(scopes);
        return ok;
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
                config,
                1,
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
                SemanticTypeDescriptor declaration_type;

                if (stmt->declaration_is_const && !init_expr) {
                    if (error) {
                        char preview[128];
                        char msg[512];
                        format_callee_preview(single_name ? single_name : "<unnamed>", preview, sizeof(preview));
                        snprintf(msg,
                            sizeof(msg),
                            "SEMA-CONST-002: identifier=%s; const declaration requires initializer",
                            preview);
                        semantic_set_error(error, stmt->line, stmt->column, msg);
                    }
                    scope_stack_pop(scopes);
                    return 0;
                }

                if (!semantic_scope_build_declaration_type_descriptor(
                        program,
                        scopes,
                        stmt,
                        i,
                        &declaration_type,
                        error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }
                if (!semantic_aggregate_declaration_is_supported(init_expr,
                        program,
                        func_index,
                        0,
                        scopes,
                        &declaration_type,
                        stmt->line,
                        stmt->column,
                        error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }

                if (declaration_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                    if (!semantic_validate_scalar_initializer_type(init_expr,
                            &declaration_type,
                            program,
                            func_index,
                            config ? config->allow_extension_features : 0,
                            scopes,
                            error)) {
                        scope_stack_pop(scopes);
                        return 0;
                    }
                    goto for_decl_function_initializer_checked;
                }
                if (!semantic_scope_check_expression(init_expr,
                    program,
                    func_index,
                    scopes,
                    config ? config->allow_extension_features : 0,
                    0,
                    0,
                    error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }
                if (!semantic_validate_scalar_initializer_type(init_expr,
                        &declaration_type,
                        program,
                        func_index,
                        config ? config->allow_extension_features : 0,
                        scopes,
                        error)) {
                    scope_stack_pop(scopes);
                    return 0;
                }
for_decl_function_initializer_checked:

                if (!semantic_scope_add_declaration_names(scopes,
                    program,
                    func_index,
                    scopes,
                    &single_name,
                    stmt->declaration_is_const ? (const int[]){1} : NULL,
                    stmt->declaration_value_kinds ? &stmt->declaration_value_kinds[i] : NULL,
                    stmt->declaration_function_return_types
                        ? &stmt->declaration_function_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_counts
                        ? &stmt->declaration_function_parameter_counts[i]
                        : NULL,
                    stmt->declaration_function_parameter_value_kinds
                        ? (const int *const *)&stmt->declaration_function_parameter_value_kinds[i]
                        : NULL,
                    stmt->declaration_function_parameter_return_types
                        ? (const AstFunctionReturnType *const *)&stmt->declaration_function_parameter_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_counts
                        ? (const size_t *const *)&stmt->declaration_function_parameter_parameter_counts[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_value_kinds
                        ? (const int *const *const *)&stmt->declaration_function_parameter_parameter_value_kinds[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_return_types
                        ? (const AstFunctionReturnType *const *const *)&stmt->declaration_function_parameter_parameter_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_parameter_counts
                        ? (const size_t *const *const *)&stmt->declaration_function_parameter_parameter_parameter_counts[i]
                        : NULL,
                    stmt->declaration_type_names ? &stmt->declaration_type_names[i] : NULL,
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
                config ? config->allow_extension_features : 0,
                0,
                0,
                error)) {
                scope_stack_pop(scopes);
                return 0;
            }
            if (!semantic_aggregate_expression_usage_is_supported(
                    stmt->expressions[i], program, func_index, 0, scopes, 0, error, 0u)) {
                scope_stack_pop(scopes);
                return 0;
            }
            if (stmt->has_condition_expression &&
                stmt->condition_expression_index == i &&
                !semantic_validate_control_condition_type(
                    stmt->expressions[i],
                    program,
                    func_index,
                    scopes,
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
                config,
                1,
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
            SemanticTypeDescriptor declaration_type;
            SemanticTypeDescriptor recovered_function_type;
            const SemanticTypeDescriptor *effective_function_type = &declaration_type;
            int use_recovered_function_type = 0;

            if (stmt->declaration_is_const && !init_expr) {
                if (error) {
                    char preview[128];
                    char msg[512];
                    format_callee_preview(single_name ? single_name : "<unnamed>", preview, sizeof(preview));
                    snprintf(msg,
                        sizeof(msg),
                        "SEMA-CONST-002: identifier=%s; const declaration requires initializer",
                        preview);
                    semantic_set_error(error, stmt->line, stmt->column, msg);
                }
                return 0;
            }

            if (!semantic_scope_build_declaration_type_descriptor(
                    program,
                    scopes,
                    stmt,
                    i,
                    &declaration_type,
                    error)) {
                return 0;
            }
            if (declaration_type.kind == SEMANTIC_TYPE_KIND_FUNCTION &&
                semantic_function_type_missing_nested_metadata(&declaration_type) &&
                semantic_try_resolve_function_value_expression_type_descriptor(
                    init_expr,
                    program,
                    func_index,
                    1,
                    scopes,
                    &recovered_function_type) &&
                semantic_function_type_matches_with_missing_nested_fallback(
                    &declaration_type,
                    &recovered_function_type)) {
                effective_function_type = &recovered_function_type;
                use_recovered_function_type = 1;
            }
            if (!semantic_aggregate_declaration_is_supported(init_expr,
                    program,
                    func_index,
                    0,
                    scopes,
                    &declaration_type,
                    stmt->line,
                    stmt->column,
                    error)) {
                return 0;
            }

            if (declaration_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                if (init_expr &&
                    !semantic_scope_check_expression(init_expr,
                        program,
                        func_index,
                        scopes,
                        config ? config->allow_extension_features : 0,
                        1,
                        0,
                        error)) {
                    return 0;
                }
                if (!semantic_validate_scalar_initializer_type(init_expr,
                        effective_function_type,
                        program,
                        func_index,
                        config ? config->allow_extension_features : 0,
                        scopes,
                        error)) {
                    return 0;
                }
                goto decl_function_initializer_checked;
            }
            if (!semantic_scope_check_expression(init_expr,
                program,
                func_index,
                scopes,
                config ? config->allow_extension_features : 0,
                0,
                0,
                error)) {
                return 0;
            }
            if (!semantic_validate_scalar_initializer_type(init_expr,
                    &declaration_type,
                    program,
                    func_index,
                    config ? config->allow_extension_features : 0,
                    scopes,
                    error)) {
                return 0;
            }
decl_function_initializer_checked:
            if (use_recovered_function_type) {
                if (scope_frame_contains_name(&scopes->frames[scopes->count - 1], single_name) ||
                    !scope_stack_add_to_current(
                        scopes,
                        single_name,
                        stmt->declaration_is_const ? 1 : 0,
                        AST_DECLARATION_VALUE_FUNCTION,
                        stmt->declaration_type_names ? stmt->declaration_type_names[i] : NULL,
                        effective_function_type->function_return_type,
                        effective_function_type->parameter_count,
                        effective_function_type->parameter_value_kinds,
                        effective_function_type->parameter_function_return_types,
                        effective_function_type->parameter_function_parameter_counts,
                        effective_function_type->parameter_function_parameter_value_kinds,
                        effective_function_type->parameter_function_parameter_return_types,
                        effective_function_type->parameter_function_parameter_parameter_counts)) {
                    semantic_set_error(error,
                        stmt->line,
                        stmt->column,
                        "SEMA-INT-011: failed to record declaration name in scope");
                    return 0;
                }
            } else if (!semantic_scope_add_declaration_names(scopes,
                    program,
                    func_index,
                    scopes,
                    &single_name,
                    stmt->declaration_is_const ? (const int[]){1} : NULL,
                    stmt->declaration_value_kinds ? &stmt->declaration_value_kinds[i] : NULL,
                    stmt->declaration_function_return_types
                        ? &stmt->declaration_function_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_counts
                        ? &stmt->declaration_function_parameter_counts[i]
                        : NULL,
                    stmt->declaration_function_parameter_value_kinds
                        ? (const int *const *)&stmt->declaration_function_parameter_value_kinds[i]
                        : NULL,
                    stmt->declaration_function_parameter_return_types
                        ? (const AstFunctionReturnType *const *)&stmt->declaration_function_parameter_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_counts
                        ? (const size_t *const *)&stmt->declaration_function_parameter_parameter_counts[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_value_kinds
                        ? (const int *const *const *)&stmt->declaration_function_parameter_parameter_value_kinds[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_return_types
                        ? (const AstFunctionReturnType *const *const *)&stmt->declaration_function_parameter_parameter_return_types[i]
                        : NULL,
                    stmt->declaration_function_parameter_parameter_parameter_counts
                        ? (const size_t *const *const *)&stmt->declaration_function_parameter_parameter_parameter_counts[i]
                        : NULL,
                    stmt->declaration_type_names ? &stmt->declaration_type_names[i] : NULL,
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
                    config ? config->allow_extension_features : 0,
                    0,
                    0,
                    error)) {
                return 0;
            }
            if (!semantic_aggregate_expression_usage_is_supported(
                    stmt->expressions[i], program, func_index, 0, scopes, 0, error, 0u)) {
                return 0;
            }
        }

        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_scope_check_statement(stmt->children[i],
                program,
                func_index,
                scopes,
                config,
                1,
                error)) {
                return 0;
            }
        }

        return 1;
    }

    for (i = 0; i < stmt->expression_count; ++i) {
        int allow_aggregate_value_root = 0;
        int skip_general_expression_check = 0;
        if (stmt->kind == AST_STMT_RETURN && program && func_index < program->count) {
            SemanticTypeDescriptor return_type;

            if (semantic_scope_build_function_return_type_descriptor(
                    &program->externals[func_index],
                    &return_type) &&
                return_type.kind == SEMANTIC_TYPE_KIND_FUNCTION) {
                skip_general_expression_check = 1;
                if (!semantic_scope_check_expression(stmt->expressions[i],
                    program,
                    func_index,
                    scopes,
                    config ? config->allow_extension_features : 0,
                    1,
                    0,
                    error)) {
                    return 0;
                }
            }
        }

        if (!skip_general_expression_check) {
            if (!semantic_scope_check_expression(stmt->expressions[i],
                    program,
                    func_index,
                    scopes,
                    config ? config->allow_extension_features : 0,
                    0,
                    stmt->kind == AST_STMT_EXPRESSION ? 1 : 0,
                    error)) {
                return 0;
            }
        }
        if ((stmt->kind == AST_STMT_IF || stmt->kind == AST_STMT_WHILE || stmt->kind == AST_STMT_FOR) &&
            stmt->has_condition_expression &&
            stmt->condition_expression_index == i &&
            !semantic_validate_control_condition_type(
                stmt->expressions[i],
                program,
                func_index,
                scopes,
                error)) {
            return 0;
        }
        if (stmt->kind == AST_STMT_RETURN && program && func_index < program->count) {
            SemanticTypeDescriptor return_type;

            if (!semantic_scope_build_function_return_type_descriptor(
                    &program->externals[func_index],
                    &return_type) ||
                !semantic_validate_return_expression_type(
                    stmt->expressions[i],
                    &return_type,
                    program,
                    func_index,
                    scopes,
                    error)) {
                return 0;
            }
            allow_aggregate_value_root = return_type.kind == SEMANTIC_TYPE_KIND_AGGREGATE;
        }
        if (!semantic_aggregate_expression_usage_is_supported(
                stmt->expressions[i],
                program,
                func_index,
                0,
                scopes,
                allow_aggregate_value_root,
                error,
                0u)) {
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_scope_check_statement(stmt->children[i],
            program,
            func_index,
            scopes,
            config,
            in_dynamic_registration_context || stmt->kind == AST_STMT_IF ||
                stmt->kind == AST_STMT_WHILE || stmt->kind == AST_STMT_FOR,
            error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_defer_body_is_supported_with_loop_depth(const AstStatement *stmt,
    size_t loop_depth,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    if (stmt->kind == AST_STMT_RETURN) {
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-003: defer body may not contain return in the first extension version");
        return 0;
    }
    if (stmt->kind == AST_STMT_BREAK) {
        if (loop_depth == 0u) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-004: defer body may not contain break targeting an outer loop in the first extension version");
            return 0;
        }
        return 1;
    }
    if (stmt->kind == AST_STMT_CONTINUE) {
        if (loop_depth == 0u) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-005: defer body may not contain continue targeting an outer loop in the first extension version");
            return 0;
        }
        return 1;
    }

    if (stmt->kind == AST_STMT_WHILE || stmt->kind == AST_STMT_FOR) {
        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_defer_body_is_supported_with_loop_depth(stmt->children[i], loop_depth + 1u, error)) {
                return 0;
            }
        }
        return 1;
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_defer_body_is_supported_with_loop_depth(stmt->children[i], loop_depth, error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_defer_body_is_supported(const AstStatement *stmt,
    SemanticError *error) {
    return semantic_defer_body_is_supported_with_loop_depth(stmt, 0u, error);
}

static int semantic_fndefer_body_is_supported(const AstStatement *stmt,
    SemanticError *error) {
    size_t i;

    if (!stmt) {
        return 1;
    }

    switch (stmt->kind) {
    case AST_STMT_EXPRESSION:
        return 1;
    case AST_STMT_COMPOUND:
        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_fndefer_body_is_supported(stmt->children[i], error)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_IF:
        for (i = 0; i < stmt->child_count; ++i) {
            if (!semantic_fndefer_body_is_supported(stmt->children[i], error)) {
                return 0;
            }
        }
        return 1;
    case AST_STMT_RETURN:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-008: fndefer body may not contain return in the first extension version");
        return 0;
    case AST_STMT_BREAK:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-009: fndefer body may not contain break in the first extension version");
        return 0;
    case AST_STMT_CONTINUE:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-010: fndefer body may not contain continue in the first extension version");
        return 0;
    case AST_STMT_DEFER:
    case AST_STMT_FNDEFER:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-011: fndefer body may not contain nested defer/fndefer in the first extension version");
        return 0;
    case AST_STMT_DECLARATION:
    case AST_STMT_WHILE:
    case AST_STMT_FOR:
        semantic_set_error(error,
            stmt->line,
            stmt->column,
            "SEMA-EXT-012: fndefer body currently supports only expression/if/compound statements");
        return 0;
    default:
        return 1;
    }
}

static int semantic_check_loop_nested_fndefer_site_limit(const AstStatement *stmt,
    size_t loop_depth,
    size_t *io_loop_nested_site_count,
    SemanticError *error) {
    size_t i;

    if (!stmt || !io_loop_nested_site_count) {
        return 1;
    }

    if (stmt->kind == AST_STMT_FNDEFER && loop_depth > 0u) {
        if (*io_loop_nested_site_count > 0u) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-033: multiple loop-nested fndefer sites are not yet supported in the first extension version");
            return 0;
        }
        *io_loop_nested_site_count += 1u;
    }

    if (stmt->kind == AST_STMT_WHILE || stmt->kind == AST_STMT_FOR) {
        for (i = 0u; i < stmt->child_count; ++i) {
            if (!semantic_check_loop_nested_fndefer_site_limit(
                    stmt->children ? stmt->children[i] : NULL,
                    loop_depth + 1u,
                    io_loop_nested_site_count,
                    error)) {
                return 0;
            }
        }
        return 1;
    }

    for (i = 0u; i < stmt->child_count; ++i) {
        if (!semantic_check_loop_nested_fndefer_site_limit(
                stmt->children ? stmt->children[i] : NULL,
                loop_depth,
                io_loop_nested_site_count,
                error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_check_function_scope_rules(const AstProgram *program,
    size_t func_index,
    const AstExternal *func,
    const SemanticAnalyzeConfig *config,
    SemanticError *error) {
    ScopeStack scopes;
    const AstStatement *body;
    size_t i;
    size_t loop_nested_fndefer_site_count = 0u;

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
            int declaration_line = func->line;
            int declaration_column = func->column;
            int parameter_decl_value_kind =
                semantic_scope_parameter_value_kind_to_declaration_value_kind(
                    func->parameter_value_kinds
                        ? func->parameter_value_kinds[i]
                        : AST_PARAMETER_VALUE_INT);
            const char *parameter_type_name =
                func->parameter_type_names ? func->parameter_type_names[i] : NULL;
            char *parameter_type_name_mutable = (char *)parameter_type_name;
            size_t parameter_array_rank =
                func->parameter_array_ranks ? func->parameter_array_ranks[i] : 0u;
            if (!single_name || single_name[0] == '\0') {
                continue;
            }
            if (func->parameter_name_lines && func->parameter_name_lines[i] > 0) {
                declaration_line = func->parameter_name_lines[i];
            }
            if (func->parameter_name_columns && func->parameter_name_columns[i] > 0) {
                declaration_column = func->parameter_name_columns[i];
            }
            if (parameter_decl_value_kind == AST_DECLARATION_VALUE_FLOAT &&
                parameter_array_rank > 0u) {
                semantic_set_error(error,
                    declaration_line,
                    declaration_column,
                    "SEMA-EXT-037: arrays of 'float' are not supported in the current extension slice");
                scope_stack_free(&scopes);
                return 0;
            }
            if (parameter_decl_value_kind == AST_DECLARATION_VALUE_PAIR &&
                parameter_array_rank > 0u) {
                semantic_set_error(error,
                    declaration_line,
                    declaration_column,
                    "SEMA-AGG-001: arrays of 'pair' are not supported in the current extension slice");
                scope_stack_free(&scopes);
                return 0;
            }
            if (parameter_decl_value_kind == AST_DECLARATION_VALUE_STRUCT) {
                if (!parameter_type_name || !semantic_find_struct_type(program, parameter_type_name)) {
                    semantic_set_error(error,
                        declaration_line,
                        declaration_column,
                        "SEMA-STRUCT-004: unknown struct type in local declaration");
                    scope_stack_free(&scopes);
                    return 0;
                }
                if (parameter_array_rank > 0u) {
                    semantic_set_error(error,
                        declaration_line,
                        declaration_column,
                        "SEMA-STRUCT-005: arrays of extension structs are not supported in the current slice");
                    scope_stack_free(&scopes);
                    return 0;
                }
            }
            if (!semantic_scope_add_declaration_names(&scopes,
                program,
                func_index,
                &scopes,
                &single_name,
                func->parameter_is_const ? &func->parameter_is_const[i] : NULL,
                &parameter_decl_value_kind,
                func->parameter_function_return_types
                    ? &func->parameter_function_return_types[i]
                    : NULL,
                func->parameter_function_parameter_counts
                    ? &func->parameter_function_parameter_counts[i]
                    : NULL,
                func->parameter_function_parameter_value_kinds
                    ? (const int *const *)&func->parameter_function_parameter_value_kinds[i]
                    : NULL,
                func->parameter_function_parameter_return_types
                    ? (const AstFunctionReturnType *const *)&func->parameter_function_parameter_return_types[i]
                    : NULL,
                func->parameter_function_parameter_parameter_counts
                    ? (const size_t *const *)&func->parameter_function_parameter_parameter_counts[i]
                    : NULL,
                func->parameter_function_parameter_parameter_value_kinds
                    ? (const int *const *const *)&func->parameter_function_parameter_parameter_value_kinds[i]
                    : NULL,
                func->parameter_function_parameter_parameter_return_types
                    ? (const AstFunctionReturnType *const *const *)&func->parameter_function_parameter_parameter_return_types[i]
                    : NULL,
                func->parameter_function_parameter_parameter_parameter_counts
                    ? (const size_t *const *const *)&func->parameter_function_parameter_parameter_parameter_counts[i]
                    : NULL,
                &parameter_type_name_mutable,
                1,
                declaration_line,
                declaration_column,
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
                config,
                0,
                error)) {
                scope_stack_free(&scopes);
                return 0;
            }
        }
    } else if (!semantic_scope_check_statement(body,
        program,
        func_index,
        &scopes,
        config,
        0,
        error)) {
        scope_stack_free(&scopes);
        return 0;
    }

    if (!semantic_check_loop_nested_fndefer_site_limit(body,
            0u,
            &loop_nested_fndefer_site_count,
            error)) {
        scope_stack_free(&scopes);
        return 0;
    }

    scope_stack_free(&scopes);
    return 1;
}

static int semantic_check_return_statement_shape(const AstExternal *func,
    const AstStatement *stmt,
    SemanticError *error) {
    size_t i;
    int has_return_expression = 0;
    SemanticTypeDescriptor return_type;

    if (!func || !stmt) {
        return 1;
    }
    if (!semantic_scope_build_function_return_type_descriptor(func, &return_type)) {
        return 1;
    }

    if (stmt->kind == AST_STMT_RETURN) {
        has_return_expression = stmt->has_primary_expression ||
            stmt->expression_count > 0;

        if (return_type.kind == SEMANTIC_TYPE_KIND_VOID) {
            if (has_return_expression) {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-RET-002: void function return statement must not return a value");
                return 0;
            }
        } else if (!has_return_expression) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-RET-001: non-void function return statement must return a value");
            return 0;
        }
    }

    for (i = 0; i < stmt->child_count; ++i) {
        if (!semantic_check_return_statement_shape(func, stmt->children[i], error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_check_function_return_shape_rules(const AstExternal *func,
    SemanticError *error) {
    if (!func || !func->is_function_definition || !func->function_body) {
        return 1;
    }

    return semantic_check_return_statement_shape(func, func->function_body, error);
}

static int semantic_return_closure_family_matches_reference(const AstExpression *expr,
    const AstExpression *reference_expr) {
    const AstExpression *base = semantic_unwrap_function_value_wrapper_expression(expr);
    const AstExpression *reference_base =
        semantic_unwrap_function_value_wrapper_expression(reference_expr);

    if (!base || !reference_base ||
        base->kind != AST_EXPR_CLOSURE ||
        reference_base->kind != AST_EXPR_CLOSURE) {
        return 0;
    }

    return base == reference_base;
}

static int semantic_function_value_return_expr_matches_reference(const AstExpression *expr,
    const AstExpression *reference_expr) {
    const AstExpression *base = semantic_unwrap_function_value_wrapper_expression(expr);
    const AstExpression *reference_base =
        semantic_unwrap_function_value_wrapper_expression(reference_expr);

    if (!base || !reference_base) {
        return 0;
    }
    if (base->kind == AST_EXPR_TERNARY || reference_base->kind == AST_EXPR_TERNARY) {
        const AstExpression *base_then = NULL;
        const AstExpression *base_else = NULL;
        const AstExpression *ref_then = NULL;
        const AstExpression *ref_else = NULL;

        if (base->kind != AST_EXPR_TERNARY || reference_base->kind != AST_EXPR_TERNARY) {
            return 0;
        }
        base_then = base->as.ternary.then_expr;
        base_else = base->as.ternary.else_expr;
        ref_then = reference_base->as.ternary.then_expr;
        ref_else = reference_base->as.ternary.else_expr;
        if (!base_then || !base_else || !ref_then || !ref_else) {
            return 0;
        }
        return semantic_function_value_return_expr_matches_reference(base_then, ref_then) &&
            semantic_function_value_return_expr_matches_reference(base_else, ref_else);
    }
    if (base->kind == AST_EXPR_CLOSURE || reference_base->kind == AST_EXPR_CLOSURE) {
        return semantic_return_closure_family_matches_reference(expr, reference_expr);
    }
    if (base->kind == AST_EXPR_CALL && reference_base->kind == AST_EXPR_CALL) {
        const char *called_name = NULL;
        const char *reference_called_name = NULL;

        if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            classify_ast_call_callee(reference_base->as.call.callee, &reference_called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
            !called_name || !reference_called_name) {
            return 0;
        }
        return strcmp(called_name, reference_called_name) == 0;
    }
    if (base->kind == AST_EXPR_IDENTIFIER &&
        reference_base->kind == AST_EXPR_IDENTIFIER &&
        base->as.identifier.name &&
        reference_base->as.identifier.name) {
        return strcmp(base->as.identifier.name, reference_base->as.identifier.name) == 0;
    }
    return 0;
}

static int semantic_function_valued_return_definition_has_consistent_closure_family_impl(
    const AstStatement *stmt,
    const AstExpression **io_reference_expr,
    SemanticError *error) {
    size_t child_index;

    if (!stmt) {
        return 1;
    }
    if (stmt->kind == AST_STMT_RETURN) {
        const AstExpression *return_expr =
            (stmt->expressions && stmt->expression_count > 0u) ? stmt->expressions[0] : NULL;
        const AstExpression *base =
            semantic_unwrap_function_value_wrapper_expression(return_expr);

        if (!base) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-048: function-valued returns currently require each return site to return one closure literal or one compatible non-capturing function value");
            return 0;
        }
        if (base->kind == AST_EXPR_CLOSURE) {
            /* accepted below */
        } else if (base->kind == AST_EXPR_TERNARY) {
            const AstExpression *then_base =
                semantic_unwrap_function_value_wrapper_expression(base->as.ternary.then_expr);
            const AstExpression *else_base =
                semantic_unwrap_function_value_wrapper_expression(base->as.ternary.else_expr);

            if (!then_base || !else_base) {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-EXT-048: function-valued returns currently require each return site to return one closure literal or one compatible non-capturing function value");
                return 0;
            }
        } else if (base->kind == AST_EXPR_IDENTIFIER && base->as.identifier.name) {
            /* accepted below */
        } else if (base->kind == AST_EXPR_CALL) {
            const char *called_name = NULL;

            if (classify_ast_call_callee(base->as.call.callee, &called_name) != AST_CALL_CALLEE_DIRECT_IDENTIFIER ||
                !called_name || called_name[0] == '\0') {
                semantic_set_error(error,
                    stmt->line,
                    stmt->column,
                    "SEMA-EXT-048: function-valued returns currently require each return site to return one closure literal or one compatible non-capturing function value");
                return 0;
            }
        } else {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-048: function-valued returns currently require each return site to return one closure literal or one compatible non-capturing function value");
            return 0;
        }
        if (!*io_reference_expr) {
            *io_reference_expr = return_expr;
        } else if (!semantic_function_value_return_expr_matches_reference(return_expr, *io_reference_expr)) {
            semantic_set_error(error,
                stmt->line,
                stmt->column,
                "SEMA-EXT-049: function-valued return definitions currently require all return sites to use the same function-value family");
            return 0;
        }
    }

    for (child_index = 0u; child_index < stmt->child_count; ++child_index) {
        if (!semantic_function_valued_return_definition_has_consistent_closure_family_impl(
                stmt->children ? stmt->children[child_index] : NULL,
                io_reference_expr,
                error)) {
            return 0;
        }
    }

    return 1;
}

static int semantic_function_valued_return_definition_has_consistent_closure_family(
    const AstExternal *func,
    const AstProgram *program,
    size_t func_index,
    SemanticError *error) {
    const AstExpression *reference_expr = NULL;

    (void)program;
    (void)func_index;
    if (!func || !func->function_body) {
        return 1;
    }

    return semantic_function_valued_return_definition_has_consistent_closure_family_impl(
        func->function_body,
        &reference_expr,
        error);
}

#endif
