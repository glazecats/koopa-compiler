#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

void value_ssa_rename_state_init(ValueSsaRenameState *state) {
    if (!state) {
        return;
    }

    state->binding_count = 0;
    state->current_values = NULL;
    state->has_current_value = NULL;
    state->history_binding_ids = NULL;
    state->history_previous_values = NULL;
    state->history_previous_has_value = NULL;
    state->history_count = 0;
    state->history_capacity = 0;
    state->scope_history_starts = NULL;
    state->scope_count = 0;
    state->scope_capacity = 0;
}

void value_ssa_rename_state_free(ValueSsaRenameState *state) {
    if (!state) {
        return;
    }

    free(state->current_values);
    free(state->has_current_value);
    free(state->history_binding_ids);
    free(state->history_previous_values);
    free(state->history_previous_has_value);
    free(state->scope_history_starts);
    value_ssa_rename_state_init(state);
}

int value_ssa_rename_state_prepare(ValueSsaRenameState *state, size_t binding_count, ValueSsaError *error) {
    if (!state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-148: invalid rename-state prepare contract");
        return 0;
    }

    value_ssa_rename_state_free(state);
    state->binding_count = binding_count;
    if (binding_count == 0) {
        return 1;
    }

    state->current_values = (ValueSsaValueRef *)calloc(binding_count, sizeof(ValueSsaValueRef));
    state->has_current_value = (unsigned char *)calloc(binding_count, sizeof(unsigned char));
    if (!state->current_values || !state->has_current_value) {
        value_ssa_rename_state_free(state);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-149: out of memory preparing rename-state bindings");
        return 0;
    }

    return 1;
}

int value_ssa_rename_state_begin_scope(ValueSsaRenameState *state, ValueSsaError *error) {
    size_t next_capacity;
    size_t *scope_history_starts;

    if (!state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-150: invalid rename-state begin-scope contract");
        return 0;
    }

    if (state->scope_count == state->scope_capacity) {
        if (!value_ssa_next_growth_capacity(state->scope_capacity, 4, sizeof(size_t), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-151: rename-state scope table capacity overflow");
            return 0;
        }
        scope_history_starts = (size_t *)realloc(state->scope_history_starts, next_capacity * sizeof(size_t));
        if (!scope_history_starts) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-152: out of memory growing rename-state scope table");
            return 0;
        }
        state->scope_history_starts = scope_history_starts;
        state->scope_capacity = next_capacity;
    }

    state->scope_history_starts[state->scope_count++] = state->history_count;
    return 1;
}

int value_ssa_rename_state_end_scope(ValueSsaRenameState *state, ValueSsaError *error) {
    size_t restore_index;

    if (!state || state->scope_count == 0) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-153: invalid rename-state end-scope contract");
        return 0;
    }

    restore_index = state->scope_history_starts[--state->scope_count];
    while (state->history_count > restore_index) {
        size_t binding_id;

        --state->history_count;
        binding_id = state->history_binding_ids[state->history_count];
        state->current_values[binding_id] = state->history_previous_values[state->history_count];
        state->has_current_value[binding_id] = state->history_previous_has_value[state->history_count];
    }

    return 1;
}

int value_ssa_rename_state_bind(ValueSsaRenameState *state,
    size_t binding_id,
    ValueSsaValueRef value,
    ValueSsaError *error) {
    size_t next_capacity;
    size_t *history_binding_ids = NULL;
    ValueSsaValueRef *history_previous_values = NULL;
    unsigned char *history_previous_has_value = NULL;

    if (!state || state->scope_count == 0 || binding_id >= state->binding_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-154: invalid rename-state bind contract");
        return 0;
    }

    if (state->history_count == state->history_capacity) {
        if (!value_ssa_next_growth_capacity(state->history_capacity, 8, sizeof(size_t), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-155: rename-state history capacity overflow");
            return 0;
        }

        history_binding_ids = (size_t *)malloc(next_capacity * sizeof(size_t));
        history_previous_values = (ValueSsaValueRef *)malloc(next_capacity * sizeof(ValueSsaValueRef));
        history_previous_has_value = (unsigned char *)malloc(next_capacity * sizeof(unsigned char));
        if (!history_binding_ids || !history_previous_values || !history_previous_has_value) {
            free(history_binding_ids);
            free(history_previous_values);
            free(history_previous_has_value);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-156: out of memory growing rename-state history");
            return 0;
        }

        if (state->history_count > 0) {
            memcpy(history_binding_ids, state->history_binding_ids, state->history_count * sizeof(size_t));
            memcpy(history_previous_values,
                state->history_previous_values,
                state->history_count * sizeof(ValueSsaValueRef));
            memcpy(history_previous_has_value,
                state->history_previous_has_value,
                state->history_count * sizeof(unsigned char));
        }

        free(state->history_binding_ids);
        free(state->history_previous_values);
        free(state->history_previous_has_value);

        state->history_binding_ids = history_binding_ids;
        state->history_previous_values = history_previous_values;
        state->history_previous_has_value = history_previous_has_value;
        state->history_capacity = next_capacity;
    }

    state->history_binding_ids[state->history_count] = binding_id;
    state->history_previous_values[state->history_count] = state->current_values[binding_id];
    state->history_previous_has_value[state->history_count] = state->has_current_value[binding_id];
    ++state->history_count;

    state->current_values[binding_id] = value;
    state->has_current_value[binding_id] = 1;
    return 1;
}

int value_ssa_rename_state_clear(ValueSsaRenameState *state,
    size_t binding_id,
    ValueSsaError *error) {
    size_t next_capacity;
    size_t *history_binding_ids = NULL;
    ValueSsaValueRef *history_previous_values = NULL;
    unsigned char *history_previous_has_value = NULL;

    if (!state || state->scope_count == 0 || binding_id >= state->binding_count) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-185: invalid rename-state clear contract");
        return 0;
    }

    if (state->history_count == state->history_capacity) {
        if (!value_ssa_next_growth_capacity(state->history_capacity, 8, sizeof(size_t), &next_capacity)) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-186: rename-state history capacity overflow during clear");
            return 0;
        }

        history_binding_ids = (size_t *)malloc(next_capacity * sizeof(size_t));
        history_previous_values = (ValueSsaValueRef *)malloc(next_capacity * sizeof(ValueSsaValueRef));
        history_previous_has_value = (unsigned char *)malloc(next_capacity * sizeof(unsigned char));
        if (!history_binding_ids || !history_previous_values || !history_previous_has_value) {
            free(history_binding_ids);
            free(history_previous_values);
            free(history_previous_has_value);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-187: out of memory growing rename-state history during clear");
            return 0;
        }

        if (state->history_count > 0) {
            memcpy(history_binding_ids, state->history_binding_ids, state->history_count * sizeof(size_t));
            memcpy(history_previous_values,
                state->history_previous_values,
                state->history_count * sizeof(ValueSsaValueRef));
            memcpy(history_previous_has_value,
                state->history_previous_has_value,
                state->history_count * sizeof(unsigned char));
        }

        free(state->history_binding_ids);
        free(state->history_previous_values);
        free(state->history_previous_has_value);

        state->history_binding_ids = history_binding_ids;
        state->history_previous_values = history_previous_values;
        state->history_previous_has_value = history_previous_has_value;
        state->history_capacity = next_capacity;
    }

    state->history_binding_ids[state->history_count] = binding_id;
    state->history_previous_values[state->history_count] = state->current_values[binding_id];
    state->history_previous_has_value[state->history_count] = state->has_current_value[binding_id];
    ++state->history_count;

    state->has_current_value[binding_id] = 0;
    return 1;
}

int value_ssa_rename_state_lookup(const ValueSsaRenameState *state,
    size_t binding_id,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    if (!state || !out_value || binding_id >= state->binding_count || !state->has_current_value[binding_id]) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-157: invalid rename-state lookup contract");
        return 0;
    }

    *out_value = state->current_values[binding_id];
    return 1;
}

int value_ssa_rename_rewrite_value_ref(ValueSsaValueRef *value,
    const ValueSsaRenameState *state,
    ValueSsaError *error) {
    if (!value || !state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-158: invalid rename rewrite-value contract");
        return 0;
    }
    if (value->kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    return value_ssa_rename_state_lookup(state, value->value_id, value, error);
}

int value_ssa_rename_rewrite_block_uses(ValueSsaBasicBlock *block,
    const ValueSsaRenameState *state,
    ValueSsaError *error) {
    size_t instruction_index;

    if (!block || !state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-159: invalid rename rewrite-block contract");
        return 0;
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];
        size_t arg_index;

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_MOV:
            if (!value_ssa_rename_rewrite_value_ref(&instruction->as.mov_value, state, error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_BINARY:
            if (!value_ssa_rename_rewrite_value_ref(&instruction->as.binary.lhs, state, error) ||
                !value_ssa_rename_rewrite_value_ref(&instruction->as.binary.rhs, state, error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_CALL:
            for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                if (!value_ssa_rename_rewrite_value_ref(&instruction->as.call.args[arg_index], state, error)) {
                    return 0;
                }
            }
            break;
        case VALUE_SSA_INSTR_LOAD_INDIRECT:
            if (!value_ssa_rename_rewrite_value_ref(&instruction->as.load_indirect_addr, state, error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_STORE_LOCAL:
        case VALUE_SSA_INSTR_STORE_GLOBAL:
            if (!value_ssa_rename_rewrite_value_ref(&instruction->as.store.value, state, error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_STORE_INDIRECT:
            if (!value_ssa_rename_rewrite_value_ref(&instruction->as.store_indirect.addr, state, error) ||
                !value_ssa_rename_rewrite_value_ref(&instruction->as.store_indirect.value, state, error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_ADDR_LOCAL:
        case VALUE_SSA_INSTR_ADDR_GLOBAL:
        case VALUE_SSA_INSTR_LOAD_LOCAL:
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            break;
        default:
            break;
        }
    }

    switch (block->terminator.kind) {
    case VALUE_SSA_TERM_RETURN:
        if (!block->terminator.has_return_value) {
            return 1;
        }
        return value_ssa_rename_rewrite_value_ref(&block->terminator.as.return_value, state, error);
    case VALUE_SSA_TERM_BRANCH:
        return value_ssa_rename_rewrite_value_ref(&block->terminator.as.branch.condition, state, error);
    case VALUE_SSA_TERM_JUMP:
    default:
        return 1;
    }
}

int value_ssa_rename_rewrite_phi_inputs_for_predecessor(ValueSsaBasicBlock *block,
    size_t predecessor_block_id,
    const ValueSsaRenameState *state,
    ValueSsaError *error) {
    size_t phi_index;

    if (!block || !state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-160: invalid rename rewrite-phi-input contract");
        return 0;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        ValueSsaPhi *phi = &block->phis[phi_index];
        size_t input_index;

        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            ValueSsaPhiInput *input = &phi->inputs[input_index];

            if (input->predecessor_block_id != predecessor_block_id) {
                continue;
            }
            if (!value_ssa_rename_rewrite_value_ref(&input->value, state, error)) {
                return 0;
            }
        }
    }

    return 1;
}

typedef struct {
    ValueSsaFunction *function;
    ValueSsaRenameState *state;
    size_t next_value_id;
} ValueSsaRenameFunctionContext;

static int value_ssa_rename_allocate_fresh_value(ValueSsaRenameFunctionContext *context,
    size_t *out_value_id,
    ValueSsaError *error) {
    if (!context || !out_value_id) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-161: invalid rename allocate contract");
        return 0;
    }
    if (context->next_value_id == (size_t)-1) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-162: rename value id overflow");
        return 0;
    }

    *out_value_id = context->next_value_id++;
    return 1;
}

static int value_ssa_rename_rewrite_terminator_uses(ValueSsaBasicBlock *block,
    const ValueSsaRenameState *state,
    ValueSsaError *error) {
    if (!block || !state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-163: invalid rename terminator contract");
        return 0;
    }

    switch (block->terminator.kind) {
    case VALUE_SSA_TERM_RETURN:
        if (!block->terminator.has_return_value) {
            return 1;
        }
        return value_ssa_rename_rewrite_value_ref(&block->terminator.as.return_value, state, error);
    case VALUE_SSA_TERM_BRANCH:
        return value_ssa_rename_rewrite_value_ref(&block->terminator.as.branch.condition, state, error);
    case VALUE_SSA_TERM_JUMP:
    default:
        return 1;
    }
}

static int value_ssa_rename_rewrite_instruction_uses(ValueSsaInstruction *instruction,
    const ValueSsaRenameState *state,
    ValueSsaError *error) {
    size_t arg_index;

    if (!instruction || !state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-164: invalid rename instruction contract");
        return 0;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_rename_rewrite_value_ref(&instruction->as.mov_value, state, error);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_rename_rewrite_value_ref(&instruction->as.binary.lhs, state, error) &&
            value_ssa_rename_rewrite_value_ref(&instruction->as.binary.rhs, state, error);
    case VALUE_SSA_INSTR_CALL:
        for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
            if (!value_ssa_rename_rewrite_value_ref(&instruction->as.call.args[arg_index], state, error)) {
                return 0;
            }
        }
        return 1;
    case VALUE_SSA_INSTR_LOAD_INDIRECT:
        return value_ssa_rename_rewrite_value_ref(&instruction->as.load_indirect_addr, state, error);
    case VALUE_SSA_INSTR_STORE_LOCAL:
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        return value_ssa_rename_rewrite_value_ref(&instruction->as.store.value, state, error);
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        return value_ssa_rename_rewrite_value_ref(&instruction->as.store_indirect.addr, state, error) &&
            value_ssa_rename_rewrite_value_ref(&instruction->as.store_indirect.value, state, error);
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
    default:
        return 1;
    }
}

static int value_ssa_rename_block_locally(ValueSsaBasicBlock *block,
    ValueSsaRenameFunctionContext *context,
    ValueSsaError *error) {
    size_t phi_index;
    size_t instruction_index;

    if (!block || !context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-165: invalid rename block contract");
        return 0;
    }

    for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
        ValueSsaPhi *phi = &block->phis[phi_index];
        size_t old_value_id = phi->result_id;
        size_t new_value_id;

        if (!value_ssa_rename_allocate_fresh_value(context, &new_value_id, error)) {
            return 0;
        }
        phi->result_id = new_value_id;
        if (!value_ssa_rename_state_bind(context->state, old_value_id, value_ssa_value_id(new_value_id), error)) {
            return 0;
        }
    }

    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        if (!value_ssa_rename_rewrite_instruction_uses(instruction, context->state, error)) {
            return 0;
        }

        if (instruction->has_result) {
            size_t old_value_id = instruction->result.value_id;
            size_t new_value_id;

            if (!value_ssa_rename_allocate_fresh_value(context, &new_value_id, error)) {
                return 0;
            }
            instruction->result.value_id = new_value_id;
            if (!value_ssa_rename_state_bind(context->state,
                    old_value_id,
                    value_ssa_value_id(new_value_id),
                    error)) {
                return 0;
            }
        }
    }

    return value_ssa_rename_rewrite_terminator_uses(block, context->state, error);
}

static int value_ssa_rename_rewrite_successor_phi_inputs(ValueSsaFunction *function,
    const ValueSsaBasicBlock *block,
    const ValueSsaRenameState *state,
    ValueSsaError *error) {
    if (!function || !block || !state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-166: invalid rename successor-phi contract");
        return 0;
    }

    switch (block->terminator.kind) {
    case VALUE_SSA_TERM_JUMP:
        return value_ssa_rename_rewrite_phi_inputs_for_predecessor(
            &function->blocks[block->terminator.as.jump_target], block->id, state, error);
    case VALUE_SSA_TERM_BRANCH:
        return value_ssa_rename_rewrite_phi_inputs_for_predecessor(
                   &function->blocks[block->terminator.as.branch.then_target], block->id, state, error) &&
            value_ssa_rename_rewrite_phi_inputs_for_predecessor(
                   &function->blocks[block->terminator.as.branch.else_target], block->id, state, error);
    case VALUE_SSA_TERM_RETURN:
    default:
        return 1;
    }
}

static int value_ssa_rename_function_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaRenameFunctionContext *context = (ValueSsaRenameFunctionContext *)user_data;
    ValueSsaBasicBlock *block;

    (void)function;
    (void)analysis;

    if (!context || !context->function || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-167: invalid rename enter-block contract");
        return 0;
    }

    block = &context->function->blocks[block_id];
    return value_ssa_rename_state_begin_scope(context->state, error) &&
        value_ssa_rename_block_locally(block, context, error) &&
        value_ssa_rename_rewrite_successor_phi_inputs(context->function, block, context->state, error);
}

static int value_ssa_rename_function_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaRenameFunctionContext *context = (ValueSsaRenameFunctionContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-168: invalid rename leave-block contract");
        return 0;
    }

    return value_ssa_rename_state_end_scope(context->state, error);
}

int value_ssa_rename_function_values(ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    ValueSsaError *error) {
    ValueSsaRenameState state;
    ValueSsaRenameFunctionContext context;
    size_t old_next_value_id;

    if (!function || !analysis) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-169: invalid rename function contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-170: cannot rename declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-171: rename analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }

    old_next_value_id = function->next_value_id;
    value_ssa_rename_state_init(&state);
    context.function = function;
    context.state = &state;
    context.next_value_id = 0;

    if (!value_ssa_rename_state_prepare(&state, old_next_value_id, error)) {
        value_ssa_rename_state_free(&state);
        return 0;
    }
    if (!value_ssa_walk_dominator_tree(function,
            analysis,
            value_ssa_rename_function_enter_block,
            value_ssa_rename_function_leave_block,
            &context,
            error)) {
        value_ssa_rename_state_free(&state);
        return 0;
    }

    function->next_value_id = context.next_value_id;
    value_ssa_rename_state_free(&state);
    return 1;
}

static int value_ssa_walk_dominator_tree_from(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    ValueSsaDominatorTreeWalkFn enter_block,
    ValueSsaDominatorTreeWalkFn leave_block,
    void *user_data,
    ValueSsaError *error) {
    size_t child_index;
    size_t block_count;

    if (enter_block && !enter_block(function, analysis, block_id, user_data, error)) {
        return 0;
    }

    block_count = function->block_count;
    for (child_index = 0; child_index < block_count; ++child_index) {
        if (!analysis->dominator_tree_children[block_id * block_count + child_index]) {
            continue;
        }

        if (!value_ssa_walk_dominator_tree_from(function,
                analysis,
                child_index,
                enter_block,
                leave_block,
                user_data,
                error)) {
            return 0;
        }
    }

    if (leave_block && !leave_block(function, analysis, block_id, user_data, error)) {
        return 0;
    }

    return 1;
}

int value_ssa_walk_dominator_tree(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    ValueSsaDominatorTreeWalkFn enter_block,
    ValueSsaDominatorTreeWalkFn leave_block,
    void *user_data,
    ValueSsaError *error) {
    if (!function || !analysis) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-145: invalid dominator-tree walk contract");
        return 0;
    }
    if (analysis->block_count != function->block_count) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-146: dominator-tree walk analysis block_count=%zu does not match function block_count=%zu",
            analysis->block_count,
            function->block_count);
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }
    if (!analysis->reachable[0]) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-147: dominator-tree walk requires reachable entry block");
        return 0;
    }

    return value_ssa_walk_dominator_tree_from(
        function, analysis, 0, enter_block, leave_block, user_data, error);
}

#endif
