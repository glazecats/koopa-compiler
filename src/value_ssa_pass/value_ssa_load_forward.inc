#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef struct {
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    ValueSsaRenameState *state;
} ValueSsaLocalLoadForwardContext;

typedef struct {
    const ValueSsaProgram *program;
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    ValueSsaRenameState *state;
} ValueSsaGlobalLoadForwardContext;

static int value_ssa_local_load_forward_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaLocalLoadForwardContext *context = (ValueSsaLocalLoadForwardContext *)user_data;
    ValueSsaBasicBlock *block;
    size_t instruction_index;
    size_t local_index;

    (void)function;

    if (!context || !context->function || !context->analysis || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-219: invalid local-load-forward contract");
        return 0;
    }

    if (!value_ssa_rename_state_begin_scope(context->state, error)) {
        return 0;
    }

    if (!value_ssa_pass_has_single_idom_entry(context->function, analysis, block_id)) {
        for (local_index = 0; local_index < context->function->local_count; ++local_index) {
            if (!value_ssa_rename_state_clear(context->state, local_index, error)) {
                return 0;
            }
        }
    }

    block = &context->function->blocks[block_id];
    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_STORE_LOCAL:
            if (instruction->as.store.slot.id < context->function->local_count &&
                !value_ssa_rename_state_bind(context->state,
                    instruction->as.store.slot.id,
                    instruction->as.store.value,
                    error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_LOAD_LOCAL:
            if (instruction->as.load_slot.id < context->function->local_count) {
                ValueSsaValueRef known_value;
                size_t slot_id = instruction->as.load_slot.id;

                if (context->state->has_current_value[slot_id]) {
                    if (!value_ssa_rename_state_lookup(context->state, slot_id, &known_value, error)) {
                        return 0;
                    }
                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = known_value;
                }

                if (instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    !value_ssa_rename_state_bind(context->state,
                        slot_id,
                        instruction->kind == VALUE_SSA_INSTR_MOV ? instruction->as.mov_value : instruction->result,
                        error)) {
                    return 0;
                }
            }
            break;
        default:
            break;
        }
    }

    return 1;
}

static int value_ssa_local_load_forward_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaLocalLoadForwardContext *context = (ValueSsaLocalLoadForwardContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-219: invalid local-load-forward contract");
        return 0;
    }

    return value_ssa_rename_state_end_scope(context->state, error);
}

static int value_ssa_global_load_forward_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaGlobalLoadForwardContext *context = (ValueSsaGlobalLoadForwardContext *)user_data;
    ValueSsaBasicBlock *block;
    size_t instruction_index;
    size_t global_index;

    (void)function;

    if (!context || !context->program || !context->function || !context->analysis || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-221: invalid global-load-forward contract");
        return 0;
    }

    if (!value_ssa_rename_state_begin_scope(context->state, error)) {
        return 0;
    }

    if (!value_ssa_pass_has_single_idom_entry(context->function, analysis, block_id)) {
        for (global_index = 0; global_index < context->program->global_count; ++global_index) {
            if (!value_ssa_rename_state_clear(context->state, global_index, error)) {
                return 0;
            }
        }
    }

    block = &context->function->blocks[block_id];
    for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
        ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        switch (instruction->kind) {
        case VALUE_SSA_INSTR_STORE_GLOBAL:
            if (instruction->as.store.slot.id < context->program->global_count &&
                !value_ssa_rename_state_bind(context->state,
                    instruction->as.store.slot.id,
                    instruction->as.store.value,
                    error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            if (instruction->as.load_slot.id < context->program->global_count) {
                ValueSsaValueRef known_value;
                size_t slot_id = instruction->as.load_slot.id;

                if (context->state->has_current_value[slot_id]) {
                    if (!value_ssa_rename_state_lookup(context->state, slot_id, &known_value, error)) {
                        return 0;
                    }
                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = known_value;
                }

                if (instruction->result.kind == VALUE_SSA_VALUE_ID &&
                    !value_ssa_rename_state_bind(context->state,
                        slot_id,
                        instruction->kind == VALUE_SSA_INSTR_MOV ? instruction->as.mov_value : instruction->result,
                        error)) {
                    return 0;
                }
            }
            break;
        case VALUE_SSA_INSTR_CALL:
            for (global_index = 0; global_index < context->program->global_count; ++global_index) {
                if (!value_ssa_rename_state_clear(context->state, global_index, error)) {
                    return 0;
                }
            }
            break;
        default:
            break;
        }
    }

    return 1;
}

static int value_ssa_global_load_forward_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaGlobalLoadForwardContext *context = (ValueSsaGlobalLoadForwardContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-221: invalid global-load-forward contract");
        return 0;
    }

    return value_ssa_rename_state_end_scope(context->state, error);
}

int value_ssa_forward_local_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-219: invalid local-load-forward contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis analysis;
        ValueSsaRenameState state;
        ValueSsaLocalLoadForwardContext context;
        int ok;

        if (!function->has_body || function->local_count == 0) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_rename_state_init(&state);
        context.function = function;
        context.analysis = &analysis;
        context.state = &state;

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_rename_state_prepare(&state, function->local_count, error) &&
            value_ssa_walk_dominator_tree(function,
                &analysis,
                value_ssa_local_load_forward_enter_block,
                value_ssa_local_load_forward_leave_block,
                &context,
                error);

        value_ssa_rename_state_free(&state);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

int value_ssa_forward_global_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-221: invalid global-load-forward contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis analysis;
        ValueSsaRenameState state;
        ValueSsaGlobalLoadForwardContext context;
        int ok;

        if (!function->has_body || program->global_count == 0) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_rename_state_init(&state);
        context.program = program;
        context.function = function;
        context.analysis = &analysis;
        context.state = &state;

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_rename_state_prepare(&state, program->global_count, error) &&
            value_ssa_walk_dominator_tree(function,
                &analysis,
                value_ssa_global_load_forward_enter_block,
                value_ssa_global_load_forward_leave_block,
                &context,
                error);

        value_ssa_rename_state_free(&state);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            if (error && !error->message[0]) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-222: failed while forwarding global loads in function '%s'",
                    function->name ? function->name : "<anon>");
            }
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
