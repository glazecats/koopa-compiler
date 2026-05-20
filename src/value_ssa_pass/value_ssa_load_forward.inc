#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

typedef struct {
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    ValueSsaRenameState *state;
    unsigned char *address_taken_locals;
    unsigned char *immutable_parameter_locals;
} ValueSsaLocalLoadForwardContext;

typedef struct {
    const ValueSsaProgram *program;
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    const ValueSsaDefUseAnalysis *def_use;
    ValueSsaRenameState *state;
} ValueSsaGlobalLoadForwardContext;

typedef struct {
    const ValueSsaProgram *program;
    ValueSsaFunction *function;
    const ValueSsaCfgAnalysis *analysis;
    const ValueSsaDefUseAnalysis *def_use;
    ValueSsaRenameState *state;
    const unsigned char *address_taken_globals;
    const unsigned char *global_write_matrix;
    const unsigned char *unknown_call_clobber;
} ValueSsaNonAddressTakenGlobalLoadForwardContext;

static int value_ssa_non_address_taken_store_indirect_may_alias_global(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef addr,
    size_t global_id) {
    ValueSsaPassAddressRoot root;

    if (!program || !function || !analysis || global_id >= program->global_count) {
        return 1;
    }
    if (!value_ssa_pass_extract_address_root(function, analysis, addr, &root, 0u)) {
        return 1;
    }

    switch (root.kind) {
    case VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT:
        return 0;
    case VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT:
        return root.id == global_id;
    case VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER:
        /*
         * SysY array parameters may alias caller-provided arrays (including
         * global arrays), but they do not alias non-address-taken scalar
         * globals like `mod`.
         */
        return program->globals[global_id].byte_size != 4u;
    case VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN:
    default:
        return 1;
    }
}

static size_t value_ssa_non_address_taken_global_load_forward_find_function_index_by_name(
    const ValueSsaProgram *program,
    const char *name) {
    size_t function_index;

    if (!program || !name) {
        return (size_t)-1;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, name) == 0) {
            return function_index;
        }
    }

    return (size_t)-1;
}

static int value_ssa_collect_non_address_taken_global_call_clobbers(
    const ValueSsaProgram *program,
    const unsigned char *address_taken_globals,
    unsigned char *global_write_matrix,
    unsigned char *unknown_call_clobber) {
    size_t function_index;
    size_t global_count;
    int changed = 1;

    if (!program || !address_taken_globals || !global_write_matrix || !unknown_call_clobber) {
        return 0;
    }

    global_count = program->global_count;
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            unknown_call_clobber[function_index] = 1u;
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL &&
                    instruction->as.store.slot.id < global_count &&
                    !address_taken_globals[instruction->as.store.slot.id]) {
                    global_write_matrix[function_index * global_count + instruction->as.store.slot.id] = 1u;
                } else if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                    size_t callee_index =
                        value_ssa_non_address_taken_global_load_forward_find_function_index_by_name(
                            program,
                            instruction->as.call.callee_name);

                    if (callee_index == (size_t)-1 ||
                        !program->functions[callee_index].has_body) {
                        unknown_call_clobber[function_index] = 1u;
                    }
                }
            }
        }
    }

    while (changed) {
        changed = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            const ValueSsaFunction *function = &program->functions[function_index];
            size_t block_index;

            if (!function->has_body) {
                continue;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                    size_t callee_index;
                    size_t global_index;

                    if (instruction->kind != VALUE_SSA_INSTR_CALL) {
                        continue;
                    }

                    callee_index =
                        value_ssa_non_address_taken_global_load_forward_find_function_index_by_name(
                            program,
                            instruction->as.call.callee_name);
                    if (callee_index == (size_t)-1 ||
                        !program->functions[callee_index].has_body) {
                        continue;
                    }

                    if (unknown_call_clobber[callee_index] && !unknown_call_clobber[function_index]) {
                        unknown_call_clobber[function_index] = 1u;
                        changed = 1;
                    }

                    for (global_index = 0; global_index < global_count; ++global_index) {
                        if (address_taken_globals[global_index]) {
                            continue;
                        }
                        if (global_write_matrix[callee_index * global_count + global_index] &&
                            !global_write_matrix[function_index * global_count + global_index]) {
                            global_write_matrix[function_index * global_count + global_index] = 1u;
                            changed = 1;
                        }
                    }
                }
            }
        }
    }

    return 1;
}

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
            if (context->immutable_parameter_locals &&
                context->immutable_parameter_locals[local_index]) {
                continue;
            }
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
                (!context->address_taken_locals ||
                    !context->address_taken_locals[instruction->as.store.slot.id]) &&
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

                if (context->address_taken_locals && context->address_taken_locals[slot_id]) {
                    break;
                }
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

    if (!context || !context->program || !context->function || !context->analysis ||
        !context->def_use || !context->state) {
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
        case VALUE_SSA_INSTR_STORE_INDIRECT:
            for (global_index = 0; global_index < context->program->global_count; ++global_index) {
                if (value_ssa_pass_store_indirect_may_alias_global(
                        context->function,
                        context->def_use,
                        instruction->as.store_indirect.addr,
                        global_index) &&
                    !value_ssa_rename_state_clear(context->state, global_index, error)) {
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
        unsigned char *address_taken_locals = NULL;
        unsigned char *immutable_parameter_locals = NULL;
        int ok;
        size_t block_index;

        if (!function->has_body || function->local_count == 0) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_rename_state_init(&state);
        context.function = function;
        context.analysis = &analysis;
        context.state = &state;
        context.address_taken_locals = NULL;
        context.immutable_parameter_locals = NULL;

        address_taken_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        immutable_parameter_locals = (unsigned char *)calloc(function->local_count, sizeof(unsigned char));
        if (!address_taken_locals || !immutable_parameter_locals) {
            free(immutable_parameter_locals);
            free(address_taken_locals);
            value_ssa_rename_state_free(&state);
            value_ssa_cfg_analysis_free(&analysis);
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-249: out of memory preparing local-address-taken flags");
            return 0;
        }
        for (block_index = 0; block_index < function->local_count; ++block_index) {
            if (function->locals[block_index].is_parameter) {
                immutable_parameter_locals[block_index] = 1u;
            }
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_ADDR_LOCAL &&
                    instruction->as.addr_slot.id < function->local_count) {
                    address_taken_locals[instruction->as.addr_slot.id] = 1u;
                } else if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL &&
                           instruction->as.store.slot.id < function->local_count) {
                    immutable_parameter_locals[instruction->as.store.slot.id] = 0u;
                }
            }
        }
        context.address_taken_locals = address_taken_locals;
        for (block_index = 0; block_index < function->local_count; ++block_index) {
            if (address_taken_locals[block_index]) {
                immutable_parameter_locals[block_index] = 0u;
            }
        }
        context.immutable_parameter_locals = immutable_parameter_locals;

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_rename_state_prepare(&state, function->local_count, error) &&
            value_ssa_walk_dominator_tree(function,
                &analysis,
                value_ssa_local_load_forward_enter_block,
                value_ssa_local_load_forward_leave_block,
                &context,
                error);

        free(immutable_parameter_locals);
        free(address_taken_locals);
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
        ValueSsaDefUseAnalysis def_use;
        ValueSsaRenameState state;
        ValueSsaGlobalLoadForwardContext context;
        int ok;

        if (!function->has_body || program->global_count == 0) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_def_use_analysis_init(&def_use);
        value_ssa_rename_state_init(&state);
        context.program = program;
        context.function = function;
        context.analysis = &analysis;
        context.def_use = &def_use;
        context.state = &state;

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_compute_def_use_analysis(function, &def_use, error) &&
            value_ssa_rename_state_prepare(&state, program->global_count, error) &&
            value_ssa_walk_dominator_tree(function,
                &analysis,
                value_ssa_global_load_forward_enter_block,
                value_ssa_global_load_forward_leave_block,
                &context,
                error);

        value_ssa_rename_state_free(&state);
        value_ssa_def_use_analysis_free(&def_use);
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

static int value_ssa_collect_address_taken_globals(const ValueSsaProgram *program, unsigned char *address_taken_globals) {
    size_t function_index;

    if (!program || !address_taken_globals) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                if (instruction->kind == VALUE_SSA_INSTR_ADDR_GLOBAL &&
                    instruction->as.addr_slot.id < program->global_count) {
                    address_taken_globals[instruction->as.addr_slot.id] = 1u;
                }
            }
        }
    }

    return 1;
}

static int value_ssa_non_address_taken_global_load_forward_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaNonAddressTakenGlobalLoadForwardContext *context =
        (ValueSsaNonAddressTakenGlobalLoadForwardContext *)user_data;
    ValueSsaBasicBlock *block;
    size_t instruction_index;
    size_t global_index;

    (void)function;

    if (!context || !context->program || !context->function || !context->analysis ||
        !context->def_use || !context->state ||
        !context->address_taken_globals) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-253: invalid non-address-taken global-load-forward contract");
        return 0;
    }

    if (!value_ssa_rename_state_begin_scope(context->state, error)) {
        return 0;
    }

    if (!value_ssa_pass_has_single_idom_entry(context->function, analysis, block_id)) {
        for (global_index = 0; global_index < context->program->global_count; ++global_index) {
            if (!context->address_taken_globals[global_index] &&
                !value_ssa_rename_state_clear(context->state, global_index, error)) {
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
                !context->address_taken_globals[instruction->as.store.slot.id] &&
                !value_ssa_rename_state_bind(context->state,
                    instruction->as.store.slot.id,
                    instruction->as.store.value,
                    error)) {
                return 0;
            }
            break;
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            if (instruction->as.load_slot.id < context->program->global_count &&
                !context->address_taken_globals[instruction->as.load_slot.id]) {
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
            {
                size_t callee_index =
                    value_ssa_non_address_taken_global_load_forward_find_function_index_by_name(
                        context->program,
                        instruction->as.call.callee_name);

                if (callee_index == (size_t)-1 ||
                    !context->global_write_matrix ||
                    !context->unknown_call_clobber ||
                    context->unknown_call_clobber[callee_index]) {
                    for (global_index = 0; global_index < context->program->global_count; ++global_index) {
                        if (!context->address_taken_globals[global_index] &&
                            !value_ssa_rename_state_clear(context->state, global_index, error)) {
                            return 0;
                        }
                    }
                    break;
                }

                for (global_index = 0; global_index < context->program->global_count; ++global_index) {
                    if (!context->address_taken_globals[global_index] &&
                        context->global_write_matrix[callee_index * context->program->global_count + global_index] &&
                        !value_ssa_rename_state_clear(context->state, global_index, error)) {
                        return 0;
                    }
                }
            }
            break;
        case VALUE_SSA_INSTR_STORE_INDIRECT:
            for (global_index = 0; global_index < context->program->global_count; ++global_index) {
                if (!context->address_taken_globals[global_index] &&
                    value_ssa_non_address_taken_store_indirect_may_alias_global(
                        context->program,
                        context->function,
                        context->def_use,
                        instruction->as.store_indirect.addr,
                        global_index) &&
                    !value_ssa_rename_state_clear(context->state, global_index, error)) {
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

static int value_ssa_non_address_taken_global_load_forward_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *error) {
    ValueSsaNonAddressTakenGlobalLoadForwardContext *context =
        (ValueSsaNonAddressTakenGlobalLoadForwardContext *)user_data;

    (void)function;
    (void)analysis;
    (void)block_id;

    if (!context || !context->state) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-253: invalid non-address-taken global-load-forward contract");
        return 0;
    }

    return value_ssa_rename_state_end_scope(context->state, error);
}

int value_ssa_forward_non_address_taken_global_loads(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;
    unsigned char *address_taken_globals = NULL;
    unsigned char *global_write_matrix = NULL;
    unsigned char *unknown_call_clobber = NULL;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-253: invalid non-address-taken global-load-forward contract");
        return 0;
    }
    if (program->global_count == 0) {
        return value_ssa_verify_program(program, error);
    }

    address_taken_globals = (unsigned char *)calloc(program->global_count, sizeof(unsigned char));
    if (!address_taken_globals) {
        value_ssa_set_error(
            error, 0, 0, "VALUE-SSA-254: out of memory preparing non-address-taken global flags");
        return 0;
    }
    if (!value_ssa_collect_address_taken_globals(program, address_taken_globals)) {
        free(address_taken_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-255: failed collecting address-taken globals");
        return 0;
    }
    global_write_matrix = (unsigned char *)calloc(
        (program->function_count ? program->function_count : 1u) *
            (program->global_count ? program->global_count : 1u),
        sizeof(unsigned char));
    unknown_call_clobber = (unsigned char *)calloc(
        program->function_count ? program->function_count : 1u,
        sizeof(unsigned char));
    if (!global_write_matrix || !unknown_call_clobber) {
        free(unknown_call_clobber);
        free(global_write_matrix);
        free(address_taken_globals);
        value_ssa_set_error(
            error, 0, 0, "VALUE-SSA-256: out of memory preparing global-call clobber map");
        return 0;
    }
    if (!value_ssa_collect_non_address_taken_global_call_clobbers(
            program,
            address_taken_globals,
            global_write_matrix,
            unknown_call_clobber)) {
        free(unknown_call_clobber);
        free(global_write_matrix);
        free(address_taken_globals);
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-257: failed collecting global-call clobber map");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaCfgAnalysis analysis;
        ValueSsaDefUseAnalysis def_use;
        ValueSsaRenameState state;
        ValueSsaNonAddressTakenGlobalLoadForwardContext context;
        int ok;

        if (!function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&analysis);
        value_ssa_def_use_analysis_init(&def_use);
        value_ssa_rename_state_init(&state);
        context.program = program;
        context.function = function;
        context.analysis = &analysis;
        context.def_use = &def_use;
        context.state = &state;
        context.address_taken_globals = address_taken_globals;
        context.global_write_matrix = global_write_matrix;
        context.unknown_call_clobber = unknown_call_clobber;

        ok = value_ssa_compute_cfg_analysis(function, &analysis, error) &&
            value_ssa_compute_def_use_analysis(function, &def_use, error) &&
            value_ssa_rename_state_prepare(&state, program->global_count, error) &&
            value_ssa_walk_dominator_tree(function,
                &analysis,
                value_ssa_non_address_taken_global_load_forward_enter_block,
                value_ssa_non_address_taken_global_load_forward_leave_block,
                &context,
                error);

        value_ssa_rename_state_free(&state);
        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_cfg_analysis_free(&analysis);
        if (!ok) {
            free(unknown_call_clobber);
            free(global_write_matrix);
            free(address_taken_globals);
            if (error && !error->message[0]) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-258: failed while forwarding non-address-taken global loads in function '%s'",
                    function->name ? function->name : "<anon>");
            }
            return 0;
        }
    }

    free(unknown_call_clobber);
    free(global_write_matrix);
    free(address_taken_globals);
    return value_ssa_verify_program(program, error);
}

#endif
