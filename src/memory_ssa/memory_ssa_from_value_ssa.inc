#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_SPLIT_AGGREGATOR)
#define MEMORY_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa.c"
#else

typedef struct {
    const ValueSsaProgram *input_program;
    const ValueSsaFunction *input_function;
    MemorySsaFunction *output_function;
    const ValueSsaCfgAnalysis *cfg_analysis;
    const unsigned char *call_global_reads;
    const unsigned char *call_global_writes;
    size_t *current_versions;
    size_t *entry_history_counts;
    size_t *block_history_offsets;
    size_t *block_history_capacities;
    size_t *history_slots;
    size_t *history_previous_versions;
    unsigned char *has_phi_block;
} MemorySsaBuildState;

static int memory_ssa_find_function_index_by_name(const ValueSsaProgram *program,
    const char *name,
    size_t *out_function_index) {
    size_t function_index;

    if (!program || !name || !out_function_index) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, name) == 0) {
            *out_function_index = function_index;
            return 1;
        }
    }

    return 0;
}

static int memory_ssa_compute_call_global_effects(const ValueSsaProgram *program,
    unsigned char **out_reads,
    unsigned char **out_writes,
    MemorySsaError *error) {
    unsigned char *reads = NULL;
    unsigned char *writes = NULL;
    int changed = 1;

    if (!out_reads || !out_writes) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-075: invalid call-summary contract");
        return 0;
    }

    *out_reads = NULL;
    *out_writes = NULL;
    if (!program || program->function_count == 0 || program->global_count == 0) {
        return 1;
    }

    reads = (unsigned char *)calloc(program->function_count * program->global_count, sizeof(unsigned char));
    writes = (unsigned char *)calloc(program->function_count * program->global_count, sizeof(unsigned char));
    if (!reads || !writes) {
        free(reads);
        free(writes);
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-075: out of memory while computing call summaries");
        return 0;
    }

    while (changed) {
        size_t function_index;

        changed = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            const ValueSsaFunction *function = &program->functions[function_index];
            size_t block_index;

            if (!function->has_body) {
                for (block_index = 0; block_index < program->global_count; ++block_index) {
                    size_t offset = function_index * program->global_count + block_index;

                    if (!reads[offset] || !writes[offset]) {
                        reads[offset] = 1;
                        writes[offset] = 1;
                        changed = 1;
                    }
                }
                continue;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const ValueSsaBasicBlock *block = &function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                    const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

                    if (instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL) {
                        size_t global_id = instruction->as.load_slot.id;
                        size_t offset = function_index * program->global_count + global_id;

                        if (global_id < program->global_count && !reads[offset]) {
                            reads[offset] = 1;
                            changed = 1;
                        }
                    } else if (instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
                        size_t global_id = instruction->as.store.slot.id;
                        size_t offset = function_index * program->global_count + global_id;

                        if (global_id < program->global_count && !writes[offset]) {
                            writes[offset] = 1;
                            changed = 1;
                        }
                    } else if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                        size_t callee_index;
                        size_t global_index;

                        if (!instruction->as.call.callee_name ||
                            !memory_ssa_find_function_index_by_name(
                                program, instruction->as.call.callee_name, &callee_index)) {
                            for (global_index = 0; global_index < program->global_count; ++global_index) {
                                size_t offset = function_index * program->global_count + global_index;

                                if (!reads[offset] || !writes[offset]) {
                                    reads[offset] = 1;
                                    writes[offset] = 1;
                                    changed = 1;
                                }
                            }
                            continue;
                        }

                        for (global_index = 0; global_index < program->global_count; ++global_index) {
                            size_t caller_offset = function_index * program->global_count + global_index;
                            size_t callee_offset = callee_index * program->global_count + global_index;

                            if (reads[callee_offset] && !reads[caller_offset]) {
                                reads[caller_offset] = 1;
                                changed = 1;
                            }
                            if (writes[callee_offset] && !writes[caller_offset]) {
                                writes[caller_offset] = 1;
                                changed = 1;
                            }
                        }
                    }
                }
            }
        }
    }

    *out_reads = reads;
    *out_writes = writes;
    return 1;
}

static void memory_ssa_free_call_global_effects(unsigned char **reads, unsigned char **writes) {
    if (reads) {
        free(*reads);
        *reads = NULL;
    }
    if (writes) {
        free(*writes);
        *writes = NULL;
    }
}

static int memory_ssa_get_call_global_effect(const MemorySsaBuildState *state,
    const char *callee_name,
    size_t slot_id,
    int *out_reads,
    int *out_writes) {
    size_t callee_index;
    size_t global_id;

    if (!state || !state->input_program || !state->output_function || !out_reads || !out_writes ||
        slot_id >= state->output_function->tracked_slot_count) {
        return 0;
    }

    *out_reads = 0;
    *out_writes = 0;
    if (state->output_function->tracked_slots[slot_id].slot.kind != VALUE_SSA_SLOT_GLOBAL) {
        return 1;
    }

    global_id = state->output_function->tracked_slots[slot_id].slot.id;
    if (global_id >= state->input_program->global_count) {
        *out_reads = 1;
        *out_writes = 1;
        return 1;
    }
    if (!callee_name || !memory_ssa_find_function_index_by_name(state->input_program, callee_name, &callee_index)) {
        *out_reads = 1;
        *out_writes = 1;
        return 1;
    }
    if (!state->call_global_reads || !state->call_global_writes) {
        *out_reads = 1;
        *out_writes = 1;
        return 1;
    }

    *out_reads = state->call_global_reads[callee_index * state->input_program->global_count + global_id] ? 1 : 0;
    *out_writes = state->call_global_writes[callee_index * state->input_program->global_count + global_id] ? 1 : 0;
    return 1;
}

static int memory_ssa_slot_ref_equal_for_build(ValueSsaSlotRef lhs, ValueSsaSlotRef rhs) {
    return lhs.kind == rhs.kind && lhs.id == rhs.id;
}

static int memory_ssa_append_slot_debug_name(const ValueSsaProgram *program,
    const ValueSsaFunction *function,
    ValueSsaSlotRef slot,
    char *buffer,
    size_t buffer_size) {
    if (!program || !function || !buffer || buffer_size == 0) {
        return 0;
    }

    if (slot.kind == VALUE_SSA_SLOT_LOCAL) {
        const char *name = (slot.id < function->local_count && function->locals[slot.id].source_name)
            ? function->locals[slot.id].source_name
            : "local";
        return snprintf(buffer, buffer_size, "%s.%zu", name, slot.id) > 0;
    }

    if (slot.kind == VALUE_SSA_SLOT_GLOBAL) {
        const char *name = (slot.id < program->global_count && program->globals[slot.id].name)
            ? program->globals[slot.id].name
            : "global";
        return snprintf(buffer, buffer_size, "%s.%zu", name, slot.id) > 0;
    }

    return 0;
}

static int memory_ssa_find_tracked_slot_id(const MemorySsaFunction *function,
    ValueSsaSlotRef slot,
    size_t *out_slot_id) {
    size_t slot_index;

    if (!function || !out_slot_id) {
        return 0;
    }

    for (slot_index = 0; slot_index < function->tracked_slot_count; ++slot_index) {
        if (memory_ssa_slot_ref_equal_for_build(function->tracked_slots[slot_index].slot, slot)) {
            *out_slot_id = slot_index;
            return 1;
        }
    }

    return 0;
}

static int memory_ssa_find_or_add_tracked_slot(const ValueSsaProgram *program,
    const ValueSsaFunction *input_function,
    MemorySsaFunction *output_function,
    ValueSsaSlotRef slot,
    size_t *out_slot_id,
    MemorySsaError *error) {
    char debug_name[128];

    if (memory_ssa_find_tracked_slot_id(output_function, slot, out_slot_id)) {
        return 1;
    }

    if (!memory_ssa_append_slot_debug_name(program, input_function, slot, debug_name, sizeof(debug_name)) ||
        !memory_ssa_function_append_tracked_slot(output_function, slot, debug_name, out_slot_id, error)) {
        return 0;
    }

    return 1;
}

static int memory_ssa_collect_tracked_slots(const ValueSsaProgram *program,
    const ValueSsaFunction *input_function,
    MemorySsaFunction *output_function,
    MemorySsaError *error) {
    size_t block_index;

    for (block_index = 0; block_index < input_function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &input_function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            ValueSsaSlotRef slot;
            size_t slot_id;

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                slot = instruction->as.load_slot;
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                slot = instruction->as.store.slot;
                break;
            default:
                continue;
            }

            if (!memory_ssa_find_or_add_tracked_slot(
                    program, input_function, output_function, slot, &slot_id, error)) {
                return 0;
            }
        }
    }

    return 1;
}

static int memory_ssa_copy_terminator(MemorySsaBasicBlock *output_block,
    const ValueSsaBasicBlock *input_block,
    MemorySsaError *error) {
    switch (input_block->terminator.kind) {
    case VALUE_SSA_TERM_RETURN:
        if (!input_block->terminator.has_return_value) {
            return memory_ssa_block_set_void_return(output_block, error);
        }
        return memory_ssa_block_set_return(output_block, input_block->terminator.as.return_value, error);
    case VALUE_SSA_TERM_JUMP:
        return memory_ssa_block_set_jump(output_block, input_block->terminator.as.jump_target, error);
    case VALUE_SSA_TERM_BRANCH:
        return memory_ssa_block_set_branch(output_block,
            input_block->terminator.as.branch.condition,
            input_block->terminator.as.branch.then_target,
            input_block->terminator.as.branch.else_target,
            error);
    default:
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-063: unsupported terminator during bridge");
        return 0;
    }
}

static int memory_ssa_create_entry_seed_versions(const ValueSsaFunction *input_function,
    MemorySsaFunction *output_function,
    MemorySsaError *error) {
    size_t slot_index;

    for (slot_index = 0; slot_index < output_function->tracked_slot_count; ++slot_index) {
        const ValueSsaSlotRef slot = output_function->tracked_slots[slot_index].slot;
        int needs_entry_seed = 0;

        if (slot.kind == VALUE_SSA_SLOT_GLOBAL) {
            needs_entry_seed = 1;
        } else if (slot.kind == VALUE_SSA_SLOT_LOCAL &&
            slot.id < input_function->local_count &&
            input_function->locals[slot.id].is_parameter) {
            needs_entry_seed = 1;
        }

        if (needs_entry_seed) {
            size_t version_id = memory_ssa_function_allocate_version(output_function);

            if (version_id == (size_t)-1 ||
                !memory_ssa_function_set_entry_version(output_function, slot_index, version_id, error)) {
                return 0;
            }
        }
    }

    return 1;
}

static int memory_ssa_prepare_build_state(const ValueSsaProgram *input_program,
    const ValueSsaFunction *input_function,
    MemorySsaFunction *output_function,
    const ValueSsaCfgAnalysis *cfg_analysis,
    const unsigned char *call_global_reads,
    const unsigned char *call_global_writes,
    MemorySsaBuildState *state,
    MemorySsaError *error) {
    size_t slot_index;
    size_t total_history_capacity = 0;

    memset(state, 0, sizeof(*state));
    state->input_program = input_program;
    state->input_function = input_function;
    state->output_function = output_function;
    state->cfg_analysis = cfg_analysis;
    state->call_global_reads = call_global_reads;
    state->call_global_writes = call_global_writes;
    if (output_function->tracked_slot_count == 0) {
        return 1;
    }

    state->current_versions = (size_t *)malloc(output_function->tracked_slot_count * sizeof(size_t));
    state->entry_history_counts = (size_t *)calloc(input_function->block_count, sizeof(size_t));
    state->block_history_offsets = (size_t *)calloc(input_function->block_count, sizeof(size_t));
    state->block_history_capacities = (size_t *)calloc(input_function->block_count, sizeof(size_t));
    state->has_phi_block =
        (unsigned char *)calloc(input_function->block_count * output_function->tracked_slot_count, sizeof(unsigned char));
    if (!state->current_versions || !state->entry_history_counts || !state->block_history_offsets ||
        !state->block_history_capacities || !state->has_phi_block) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-065: out of memory while preparing bridge state");
        return 0;
    }

    for (slot_index = 0; slot_index < output_function->tracked_slot_count; ++slot_index) {
        state->current_versions[slot_index] = (size_t)-1;
    }

    for (slot_index = 0; slot_index < input_function->block_count; ++slot_index) {
        const ValueSsaBasicBlock *block = &input_function->blocks[slot_index];
        size_t instruction_index;

        state->block_history_offsets[slot_index] = total_history_capacity;
        state->block_history_capacities[slot_index] = output_function->tracked_slot_count;
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
                ++state->block_history_capacities[slot_index];
            } else if (instruction->kind == VALUE_SSA_INSTR_CALL) {
                size_t tracked_slot_index;

                for (tracked_slot_index = 0; tracked_slot_index < output_function->tracked_slot_count;
                     ++tracked_slot_index) {
                    int reads_global = 0;
                    int writes_global = 0;

                    if (!memory_ssa_get_call_global_effect(
                            state, instruction->as.call.callee_name, tracked_slot_index, &reads_global, &writes_global)) {
                        memory_ssa_set_error(error,
                            0,
                            0,
                            "MEMORY-SSA-076: failed to classify call effects while sizing bridge history");
                        return 0;
                    }
                    if (writes_global) {
                        ++state->block_history_capacities[slot_index];
                    }
                }
            }
        }
        total_history_capacity += state->block_history_capacities[slot_index];
    }

    state->history_slots = (size_t *)malloc((total_history_capacity ? total_history_capacity : 1) * sizeof(size_t));
    state->history_previous_versions =
        (size_t *)malloc((total_history_capacity ? total_history_capacity : 1) * sizeof(size_t));
    if (!state->history_slots || !state->history_previous_versions) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-065: out of memory while preparing bridge history");
        return 0;
    }

    for (slot_index = 0; slot_index < output_function->tracked_slot_count; ++slot_index) {
        if (output_function->tracked_slots[slot_index].has_entry_version) {
            state->current_versions[slot_index] = output_function->tracked_slots[slot_index].entry_version_id;
        }
    }

    return 1;
}

static void memory_ssa_free_build_state(MemorySsaBuildState *state) {
    if (!state) {
        return;
    }

    free(state->current_versions);
    free(state->entry_history_counts);
    free(state->block_history_offsets);
    free(state->block_history_capacities);
    free(state->history_slots);
    free(state->history_previous_versions);
    free(state->has_phi_block);
    memset(state, 0, sizeof(*state));
}

static int memory_ssa_push_history(MemorySsaBuildState *state,
    size_t block_id,
    size_t slot_id,
    size_t new_version,
    MemorySsaError *error) {
    size_t history_index;

    if (!state || !state->current_versions || block_id >= state->input_function->block_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-066: invalid version history contract");
        return 0;
    }
    if (state->entry_history_counts[block_id] >= state->block_history_capacities[block_id]) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-066: version history overflow in bb.%zu", block_id);
        return 0;
    }

    history_index = state->block_history_offsets[block_id] + state->entry_history_counts[block_id];
    state->history_slots[history_index] = slot_id;
    state->history_previous_versions[history_index] = state->current_versions[slot_id];
    state->current_versions[slot_id] = new_version;
    ++state->entry_history_counts[block_id];
    return 1;
}

static int memory_ssa_collect_slot_live_in_blocks(const ValueSsaFunction *input_function,
    const MemorySsaBuildState *state,
    size_t slot_index,
    unsigned char *out_live_in_blocks,
    MemorySsaError *error) {
    const ValueSsaSlotRef tracked_slot = state->output_function->tracked_slots[slot_index].slot;
    unsigned char *use_before_def_blocks = NULL;
    unsigned char *def_before_end_blocks = NULL;
    int changed = 1;
    size_t block_index;

    if (!input_function || !state || !out_live_in_blocks ||
        slot_index >= state->output_function->tracked_slot_count) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-079: invalid slot live-in analysis contract");
        return 0;
    }

    use_before_def_blocks = (unsigned char *)calloc(input_function->block_count, sizeof(unsigned char));
    def_before_end_blocks = (unsigned char *)calloc(input_function->block_count, sizeof(unsigned char));
    if (!use_before_def_blocks || !def_before_end_blocks) {
        free(use_before_def_blocks);
        free(def_before_end_blocks);
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-080: out of memory while computing slot live-in blocks");
        return 0;
    }

    for (block_index = 0; block_index < input_function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &input_function->blocks[block_index];
        int has_def = 0;
        size_t instruction_index;

        out_live_in_blocks[block_index] = 0;
        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL ||
                instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL) {
                if (memory_ssa_slot_ref_equal_for_build(tracked_slot, instruction->as.load_slot) && !has_def) {
                    use_before_def_blocks[block_index] = 1;
                }
                continue;
            }

            if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
                if (memory_ssa_slot_ref_equal_for_build(tracked_slot, instruction->as.store.slot)) {
                    has_def = 1;
                    def_before_end_blocks[block_index] = 1;
                }
                continue;
            }

            if (instruction->kind == VALUE_SSA_INSTR_CALL && tracked_slot.kind == VALUE_SSA_SLOT_GLOBAL) {
                int reads_global = 0;
                int writes_global = 0;

                if (!memory_ssa_get_call_global_effect(
                        state, instruction->as.call.callee_name, slot_index, &reads_global, &writes_global)) {
                    free(use_before_def_blocks);
                    free(def_before_end_blocks);
                    memory_ssa_set_error(
                        error, 0, 0, "MEMORY-SSA-081: failed to classify call effects during slot liveness");
                    return 0;
                }
                if (reads_global && !has_def) {
                    use_before_def_blocks[block_index] = 1;
                }
                if (writes_global) {
                    has_def = 1;
                    def_before_end_blocks[block_index] = 1;
                }
            }
        }
    }

    while (changed) {
        changed = 0;
        for (block_index = 0; block_index < input_function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &input_function->blocks[block_index];
            unsigned char live_in = use_before_def_blocks[block_index];

            if (!def_before_end_blocks[block_index]) {
                switch (block->terminator.kind) {
                case VALUE_SSA_TERM_JUMP:
                    if (block->terminator.as.jump_target < input_function->block_count &&
                        out_live_in_blocks[block->terminator.as.jump_target]) {
                        live_in = 1;
                    }
                    break;
                case VALUE_SSA_TERM_BRANCH:
                    if ((block->terminator.as.branch.then_target < input_function->block_count &&
                            out_live_in_blocks[block->terminator.as.branch.then_target]) ||
                        (block->terminator.as.branch.else_target < input_function->block_count &&
                            out_live_in_blocks[block->terminator.as.branch.else_target])) {
                        live_in = 1;
                    }
                    break;
                case VALUE_SSA_TERM_RETURN:
                default:
                    break;
                }
            }

            if (out_live_in_blocks[block_index] != live_in) {
                out_live_in_blocks[block_index] = live_in;
                changed = 1;
            }
        }
    }

    free(use_before_def_blocks);
    free(def_before_end_blocks);
    return 1;
}

static int memory_ssa_mark_phi_blocks(const ValueSsaFunction *input_function,
    const ValueSsaCfgAnalysis *cfg_analysis,
    MemorySsaBuildState *state,
    MemorySsaError *error) {
    size_t slot_index;

    for (slot_index = 0; slot_index < state->output_function->tracked_slot_count; ++slot_index) {
        unsigned char *definition_blocks;
        unsigned char *phi_blocks;
        unsigned char *live_in_blocks;
        size_t block_index;

        definition_blocks = (unsigned char *)calloc(input_function->block_count, sizeof(unsigned char));
        phi_blocks = (unsigned char *)calloc(input_function->block_count, sizeof(unsigned char));
        live_in_blocks = (unsigned char *)calloc(input_function->block_count, sizeof(unsigned char));
        if (!definition_blocks || !phi_blocks || !live_in_blocks) {
            free(definition_blocks);
            free(phi_blocks);
            free(live_in_blocks);
            memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-067: out of memory while computing slot phi placement");
            return 0;
        }

        for (block_index = 0; block_index < input_function->block_count; ++block_index) {
            const ValueSsaBasicBlock *block = &input_function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                ValueSsaSlotRef slot;

                if (instruction->kind == VALUE_SSA_INSTR_STORE_LOCAL ||
                    instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
                    slot = instruction->as.store.slot;
                    if (memory_ssa_slot_ref_equal_for_build(state->output_function->tracked_slots[slot_index].slot, slot)) {
                        definition_blocks[block_index] = 1;
                    }
                } else if (instruction->kind == VALUE_SSA_INSTR_CALL &&
                    state->output_function->tracked_slots[slot_index].slot.kind == VALUE_SSA_SLOT_GLOBAL) {
                    int reads_global = 0;
                    int writes_global = 0;

                    if (!memory_ssa_get_call_global_effect(
                            state, instruction->as.call.callee_name, slot_index, &reads_global, &writes_global)) {
                        free(definition_blocks);
                        free(phi_blocks);
                        memory_ssa_set_error(
                            error, 0, 0, "MEMORY-SSA-077: failed to classify call effects during phi placement");
                        return 0;
                    }
                    if (writes_global) {
                        definition_blocks[block_index] = 1;
                    }
                }
            }
        }

        if (!value_ssa_compute_phi_placement(input_function, cfg_analysis, definition_blocks, phi_blocks,
                (ValueSsaError *)error)) {
            free(definition_blocks);
            free(phi_blocks);
            free(live_in_blocks);
            return 0;
        }
        if (!memory_ssa_collect_slot_live_in_blocks(input_function, state, slot_index, live_in_blocks, error)) {
            free(definition_blocks);
            free(phi_blocks);
            free(live_in_blocks);
            return 0;
        }

        for (block_index = 0; block_index < input_function->block_count; ++block_index) {
            state->has_phi_block[block_index * state->output_function->tracked_slot_count + slot_index] =
                (unsigned char)(phi_blocks[block_index] && live_in_blocks[block_index]);
        }

        free(definition_blocks);
        free(phi_blocks);
        free(live_in_blocks);
    }

    return 1;
}

static int memory_ssa_precreate_all_phis(MemorySsaBuildState *state, MemorySsaError *error) {
    size_t block_id;

    for (block_id = 0; block_id < state->input_function->block_count; ++block_id) {
        MemorySsaBasicBlock *output_block = &state->output_function->blocks[block_id];
        size_t slot_index;

        for (slot_index = 0; slot_index < state->output_function->tracked_slot_count; ++slot_index) {
            MemorySsaPhiInput *phi_inputs = NULL;
            size_t predecessor_count = state->cfg_analysis->predecessor_counts[block_id];
            size_t predecessor_index = 0;
            size_t pred_block_id;
            size_t version_id;

            if (!state->has_phi_block[block_id * state->output_function->tracked_slot_count + slot_index]) {
                continue;
            }

            version_id = memory_ssa_function_allocate_version(state->output_function);
            if (version_id == (size_t)-1) {
                memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-068: version allocation failed while precreating phi");
                return 0;
            }
            if (predecessor_count > 0) {
                phi_inputs = (MemorySsaPhiInput *)calloc(predecessor_count, sizeof(MemorySsaPhiInput));
                if (!phi_inputs) {
                    memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-069: out of memory while precreating phi");
                    return 0;
                }
                for (pred_block_id = 0; pred_block_id < state->input_function->block_count; ++pred_block_id) {
                    if (!state->cfg_analysis->predecessor_matrix[
                            pred_block_id * state->input_function->block_count + block_id]) {
                        continue;
                    }
                    phi_inputs[predecessor_index].predecessor_block_id = pred_block_id;
                    phi_inputs[predecessor_index].version_id = 0;
                    ++predecessor_index;
                }
            }

            if (!memory_ssa_block_append_phi(output_block, slot_index, version_id, phi_inputs, predecessor_count, error)) {
                free(phi_inputs);
                return 0;
            }
            free(phi_inputs);
        }
    }

    return 1;
}

static int memory_ssa_bind_block_phi_versions(MemorySsaBuildState *state,
    size_t block_id,
    MemorySsaError *error) {
    MemorySsaBasicBlock *output_block = &state->output_function->blocks[block_id];
    size_t phi_index;

    for (phi_index = 0; phi_index < output_block->phi_count; ++phi_index) {
        MemorySsaPhi *phi = &output_block->phis[phi_index];

        if (!memory_ssa_push_history(state, block_id, phi->slot_id, phi->result_version_id, error)) {
            return 0;
        }
    }

    return 1;
}

static int memory_ssa_fill_successor_phi_inputs(MemorySsaBuildState *state,
    size_t predecessor_block_id,
    size_t successor_block_id,
    MemorySsaError *error) {
    MemorySsaBasicBlock *successor = &state->output_function->blocks[successor_block_id];
    size_t phi_index;

    for (phi_index = 0; phi_index < successor->phi_count; ++phi_index) {
        MemorySsaPhi *phi = &successor->phis[phi_index];
        size_t input_index;
        size_t current_version = state->current_versions[phi->slot_id];

        if (current_version == (size_t)-1) {
            memory_ssa_set_error(error,
                0,
                0,
                "MEMORY-SSA-070: bb.%zu has no current memory version for slot.%zu while filling successor bb.%zu phi",
                predecessor_block_id,
                phi->slot_id,
                successor_block_id);
            return 0;
        }

        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            if (phi->inputs[input_index].predecessor_block_id == predecessor_block_id) {
                phi->inputs[input_index].version_id = current_version;
                break;
            }
        }
        if (input_index == phi->input_count) {
            memory_ssa_set_error(error,
                0,
                0,
                "MEMORY-SSA-071: successor bb.%zu is missing predecessor bb.%zu in phi",
                successor_block_id,
                predecessor_block_id);
            return 0;
        }
    }

    return 1;
}

static int memory_ssa_process_block(const ValueSsaBasicBlock *input_block,
    MemorySsaBuildState *state,
    MemorySsaError *error) {
    MemorySsaBasicBlock *output_block = &state->output_function->blocks[input_block->id];
    size_t instruction_index;

    for (instruction_index = 0; instruction_index < input_block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *input_instruction = &input_block->instructions[instruction_index];
        MemorySsaInstruction output_instruction;
        int is_memory = 0;
        ValueSsaSlotRef slot;

        memset(&output_instruction, 0, sizeof(output_instruction));
        if (!memory_ssa_copy_value_instruction(&output_instruction.instruction, input_instruction, error)) {
            return 0;
        }

        switch (input_instruction->kind) {
        case VALUE_SSA_INSTR_LOAD_LOCAL:
        case VALUE_SSA_INSTR_LOAD_GLOBAL:
            is_memory = 1;
            slot = input_instruction->as.load_slot;
            break;
        case VALUE_SSA_INSTR_STORE_LOCAL:
        case VALUE_SSA_INSTR_STORE_GLOBAL:
            is_memory = 1;
            slot = input_instruction->as.store.slot;
            break;
        default:
            break;
        }

        if (is_memory) {
            MemorySsaAccess access;
            size_t slot_id;

            if (!memory_ssa_find_tracked_slot_id(state->output_function, slot, &slot_id)) {
                memory_ssa_instruction_free(&output_instruction);
                memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-072: missing tracked slot during bridge");
                return 0;
            }

            memset(&access, 0, sizeof(access));
            access.slot_id = slot_id;
            if (input_instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL ||
                input_instruction->kind == VALUE_SSA_INSTR_LOAD_GLOBAL) {
                if (state->current_versions[slot_id] == (size_t)-1) {
                    memory_ssa_instruction_free(&output_instruction);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-073: bb.%zu reads slot.%zu before any tracked definition",
                        input_block->id,
                        slot_id);
                    return 0;
                }
                access.has_use_version = 1;
                access.use_version_id = state->current_versions[slot_id];
            } else {
                size_t version_id = memory_ssa_function_allocate_version(state->output_function);

                if (state->current_versions[slot_id] != (size_t)-1) {
                    access.has_use_version = 1;
                    access.use_version_id = state->current_versions[slot_id];
                }
                if (version_id == (size_t)-1 ||
                    !memory_ssa_push_history(state, input_block->id, slot_id, version_id, error)) {
                    memory_ssa_instruction_free(&output_instruction);
                    return 0;
                }
                access.has_def_version = 1;
                access.def_version_id = version_id;
            }

            if (!memory_ssa_instruction_append_access(&output_instruction, &access, error)) {
                memory_ssa_instruction_free(&output_instruction);
                return 0;
            }
        } else if (input_instruction->kind == VALUE_SSA_INSTR_CALL) {
            size_t slot_id;

            for (slot_id = 0; slot_id < state->output_function->tracked_slot_count; ++slot_id) {
                const MemorySsaTrackedSlot *tracked_slot = &state->output_function->tracked_slots[slot_id];
                int reads_global = 0;
                int writes_global = 0;

                if (tracked_slot->slot.kind != VALUE_SSA_SLOT_GLOBAL) {
                    continue;
                }
                if (!memory_ssa_get_call_global_effect(
                        state, input_instruction->as.call.callee_name, slot_id, &reads_global, &writes_global)) {
                    memory_ssa_instruction_free(&output_instruction);
                    memory_ssa_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-078: failed to classify call effects during instruction bridging");
                    return 0;
                }
                if (!reads_global && !writes_global) {
                    continue;
                }

                if (state->current_versions[slot_id] != (size_t)-1) {
                    MemorySsaAccess access;
                    size_t previous_version = state->current_versions[slot_id];
                    size_t version_id = (size_t)-1;

                    memset(&access, 0, sizeof(access));
                    access.slot_id = slot_id;
                    if (reads_global) {
                        access.has_use_version = 1;
                        access.use_version_id = previous_version;
                    }
                    if (writes_global) {
                        version_id = memory_ssa_function_allocate_version(state->output_function);
                        if (version_id == (size_t)-1 ||
                            !memory_ssa_push_history(state, input_block->id, slot_id, version_id, error)) {
                            memory_ssa_instruction_free(&output_instruction);
                            return 0;
                        }
                        access.has_def_version = 1;
                        access.def_version_id = version_id;
                    }
                    if (!memory_ssa_instruction_append_access(&output_instruction, &access, error)) {
                        memory_ssa_instruction_free(&output_instruction);
                        return 0;
                    }
                } else if (writes_global) {
                    MemorySsaAccess access;
                    size_t version_id = memory_ssa_function_allocate_version(state->output_function);

                    if (version_id == (size_t)-1 ||
                        !memory_ssa_push_history(state, input_block->id, slot_id, version_id, error)) {
                        memory_ssa_instruction_free(&output_instruction);
                        return 0;
                    }

                    memset(&access, 0, sizeof(access));
                    access.slot_id = slot_id;
                    access.has_def_version = 1;
                    access.def_version_id = version_id;
                    if (!memory_ssa_instruction_append_access(&output_instruction, &access, error)) {
                        memory_ssa_instruction_free(&output_instruction);
                        return 0;
                    }
                }
            }
        }

        if (!memory_ssa_block_append_instruction(output_block, &output_instruction, error)) {
            memory_ssa_instruction_free(&output_instruction);
            return 0;
        }
        memory_ssa_instruction_free(&output_instruction);
    }

    switch (input_block->terminator.kind) {
    case VALUE_SSA_TERM_JUMP:
        if (!memory_ssa_fill_successor_phi_inputs(
                state, input_block->id, input_block->terminator.as.jump_target, error)) {
            return 0;
        }
        break;
    case VALUE_SSA_TERM_BRANCH:
        if (!memory_ssa_fill_successor_phi_inputs(
                state, input_block->id, input_block->terminator.as.branch.then_target, error) ||
            !memory_ssa_fill_successor_phi_inputs(
                state, input_block->id, input_block->terminator.as.branch.else_target, error)) {
            return 0;
        }
        break;
    case VALUE_SSA_TERM_RETURN:
        break;
    default:
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-074: unsupported terminator in block processing");
        return 0;
    }

    if (!memory_ssa_copy_terminator(output_block, input_block, error)) {
        return 0;
    }
    return 1;
}

static int memory_ssa_bridge_enter_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *bridge_error) {
    MemorySsaBuildState *state = (MemorySsaBuildState *)user_data;
    const ValueSsaBasicBlock *input_block = &function->blocks[block_id];

    (void)analysis;
    (void)bridge_error;
    if (!state || !state->output_function) {
        return 0;
    }
    return memory_ssa_bind_block_phi_versions(state, block_id, (MemorySsaError *)bridge_error) &&
        memory_ssa_process_block(input_block, state, (MemorySsaError *)bridge_error);
}

static int memory_ssa_bridge_leave_block(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id,
    void *user_data,
    ValueSsaError *bridge_error) {
    MemorySsaBuildState *state = (MemorySsaBuildState *)user_data;
    size_t count;

    (void)function;
    (void)analysis;
    (void)bridge_error;
    if (!state || !state->output_function || state->output_function->tracked_slot_count == 0) {
        return 1;
    }

    count = state->entry_history_counts[block_id];

    while (count > 0) {
        size_t history_index = state->block_history_offsets[block_id] + (--count);
        size_t slot_id = state->history_slots[history_index];

        state->current_versions[slot_id] = state->history_previous_versions[history_index];
    }
    state->entry_history_counts[block_id] = 0;
    return 1;
}

int memory_ssa_build_from_value_ssa(const ValueSsaProgram *program,
    MemorySsaProgram *out_program,
    MemorySsaError *error) {
    unsigned char *call_global_reads = NULL;
    unsigned char *call_global_writes = NULL;
    size_t function_index;

    if (!program || !out_program) {
        memory_ssa_set_error(error, 0, 0, "MEMORY-SSA-064: invalid bridge contract");
        return 0;
    }

    memory_ssa_program_free(out_program);
    memory_ssa_program_init(out_program);
    if (!memory_ssa_compute_call_global_effects(program, &call_global_reads, &call_global_writes, error)) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *input_function = &program->functions[function_index];
        MemorySsaFunction *output_function = NULL;
        ValueSsaCfgAnalysis cfg_analysis;
        MemorySsaBuildState state;
        size_t block_index;

        if (!memory_ssa_program_append_function(out_program,
                input_function->name ? input_function->name : "<anon>",
                input_function->has_body,
                &output_function,
                error)) {
            memory_ssa_free_call_global_effects(&call_global_reads, &call_global_writes);
            memory_ssa_program_free(out_program);
            return 0;
        }
        output_function->parameter_count = input_function->parameter_count;
        if (!input_function->has_body) {
            continue;
        }

        value_ssa_cfg_analysis_init(&cfg_analysis);
        if (!memory_ssa_collect_tracked_slots(program, input_function, output_function, error) ||
            !memory_ssa_create_entry_seed_versions(input_function, output_function, error) ||
            !value_ssa_compute_cfg_analysis(input_function, &cfg_analysis, (ValueSsaError *)error)) {
            value_ssa_cfg_analysis_free(&cfg_analysis);
            memory_ssa_free_call_global_effects(&call_global_reads, &call_global_writes);
            memory_ssa_program_free(out_program);
            return 0;
        }

        for (block_index = 0; block_index < input_function->block_count; ++block_index) {
            if (!memory_ssa_function_append_block(output_function, NULL, NULL, error)) {
                value_ssa_cfg_analysis_free(&cfg_analysis);
                memory_ssa_free_call_global_effects(&call_global_reads, &call_global_writes);
                memory_ssa_program_free(out_program);
                return 0;
            }
        }

        if (!memory_ssa_prepare_build_state(
                program,
                input_function,
                output_function,
                &cfg_analysis,
                call_global_reads,
                call_global_writes,
                &state,
                error) ||
            !memory_ssa_mark_phi_blocks(input_function, &cfg_analysis, &state, error) ||
            !memory_ssa_precreate_all_phis(&state, error) ||
            !value_ssa_walk_dominator_tree(input_function,
                &cfg_analysis,
                memory_ssa_bridge_enter_block,
                memory_ssa_bridge_leave_block,
                &state,
                (ValueSsaError *)error)) {
            memory_ssa_free_build_state(&state);
            value_ssa_cfg_analysis_free(&cfg_analysis);
            memory_ssa_free_call_global_effects(&call_global_reads, &call_global_writes);
            memory_ssa_program_free(out_program);
            return 0;
        }

        memory_ssa_free_build_state(&state);
        value_ssa_cfg_analysis_free(&cfg_analysis);
    }

    if (!memory_ssa_verify_program(out_program, error)) {
        memory_ssa_free_call_global_effects(&call_global_reads, &call_global_writes);
        memory_ssa_program_free(out_program);
        return 0;
    }
    memory_ssa_free_call_global_effects(&call_global_reads, &call_global_writes);
    return 1;
}

#endif
