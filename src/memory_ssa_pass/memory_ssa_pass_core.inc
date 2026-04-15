#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static void memory_ssa_pass_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...) {
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

static void memory_ssa_pass_copy_memory_error(ValueSsaError *error, const MemorySsaError *memory_error) {
    if (!error || !memory_error) {
        return;
    }

    error->line = memory_error->line;
    error->column = memory_error->column;
    snprintf(error->message, sizeof(error->message), "%s", memory_error->message);
}

static int memory_ssa_pass_compact_removed_instructions(ValueSsaBasicBlock *block,
    const unsigned char *remove_flags) {
    size_t read_index;
    size_t write_index = 0;

    if (!block || !remove_flags) {
        return 0;
    }

    for (read_index = 0; read_index < block->instruction_count; ++read_index) {
        if (remove_flags[read_index]) {
            continue;
        }

        if (write_index != read_index) {
            block->instructions[write_index] = block->instructions[read_index];
        }
        ++write_index;
    }

    block->instruction_count = write_index;
    return 1;
}

static int memory_ssa_pass_validate_alignment(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    ValueSsaError *error) {
    size_t block_index;

    if (!value_function || !memory_function) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-001: invalid pass alignment contract");
        return 0;
    }
    if (value_function->block_count != memory_function->block_count) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-002: function '%s' block-count mismatch between value_ssa and memory_ssa",
            value_function->name ? value_function->name : "<anon>");
        return 0;
    }

    for (block_index = 0; block_index < value_function->block_count; ++block_index) {
        if (value_function->blocks[block_index].instruction_count !=
            memory_function->blocks[block_index].instruction_count) {
            memory_ssa_pass_set_error(error,
                0,
                0,
                "MEMORY-SSA-PASS-003: function '%s' bb.%zu instruction-count mismatch between value_ssa and memory_ssa",
                value_function->name ? value_function->name : "<anon>",
                block_index);
            return 0;
        }
    }

    return 1;
}

static int memory_ssa_pass_instruction_site_dominates(const ValueSsaCfgAnalysis *cfg_analysis,
    size_t candidate_block_id,
    size_t candidate_instruction_index,
    size_t use_block_id,
    size_t use_instruction_index,
    ValueSsaError *error) {
    if (!cfg_analysis || !cfg_analysis->dominates || candidate_block_id >= cfg_analysis->block_count ||
        use_block_id >= cfg_analysis->block_count) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-021: invalid CFG analysis while comparing instruction dominance");
        return -1;
    }

    if (candidate_block_id == use_block_id) {
        return candidate_instruction_index < use_instruction_index;
    }

    return cfg_analysis->dominates[(candidate_block_id * cfg_analysis->block_count) + use_block_id] ? 1 : 0;
}

static int memory_ssa_pass_append_phi(ValueSsaBasicBlock *block,
    size_t result_id,
    const ValueSsaPhiInput *inputs,
    size_t input_count,
    ValueSsaError *error) {
    ValueSsaPhi *phis;
    ValueSsaPhi *phi;
    size_t next_capacity;

    if (!block || (input_count > 0 && !inputs)) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-033: invalid phi append contract");
        return 0;
    }

    if (block->phi_count == block->phi_capacity) {
        next_capacity = block->phi_capacity ? (block->phi_capacity * 2) : 4;
        if (next_capacity < block->phi_count || next_capacity > ((size_t)-1) / sizeof(ValueSsaPhi)) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-034: phi table capacity overflow");
            return 0;
        }
        phis = (ValueSsaPhi *)realloc(block->phis, next_capacity * sizeof(ValueSsaPhi));
        if (!phis) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-035: out of memory growing phi table");
            return 0;
        }
        block->phis = phis;
        block->phi_capacity = next_capacity;
    }

    phi = &block->phis[block->phi_count];
    phi->result_id = result_id;
    phi->inputs = NULL;
    phi->input_count = input_count;
    phi->input_capacity = input_count;
    if (input_count > 0) {
        phi->inputs = (ValueSsaPhiInput *)malloc(input_count * sizeof(ValueSsaPhiInput));
        if (!phi->inputs) {
            memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-036: out of memory copying phi inputs");
            return 0;
        }
        memcpy(phi->inputs, inputs, input_count * sizeof(ValueSsaPhiInput));
    }

    ++block->phi_count;
    return 1;
}

static int memory_ssa_pass_value_matches_slot_version(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    ValueSsaValueRef value,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    unsigned char *visit_state,
    int *out_matches,
    ValueSsaError *error) {
    size_t def_block_id;
    size_t def_phi_index;
    size_t def_instruction_index;
    const ValueSsaInstruction *def_instruction;
    const MemorySsaInstruction *memory_instruction;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !visit_state || !out_matches) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-025: invalid slot-version value match contract");
        return 0;
    }

    *out_matches = 0;
    if (value.kind != VALUE_SSA_VALUE_ID || value.value_id >= value_def_use->value_count ||
        !value_def_use->has_def || !value_def_use->has_def[value.value_id]) {
        return 1;
    }
    if (visit_state[value.value_id] == 1) {
        *out_matches = 1;
        return 1;
    }

    visit_state[value.value_id] = 1;
    def_block_id = value_def_use->def_block_ids[value.value_id];
    def_phi_index = value_def_use->def_phi_indices[value.value_id];
    def_instruction_index = value_def_use->def_instruction_indices[value.value_id];

    if (def_phi_index != (size_t)-1) {
        const ValueSsaPhi *phi;
        size_t input_index;
        const MemorySsaPhi *expected_phi = NULL;
        size_t expected_phi_block_id = 0;
        size_t expected_phi_index = 0;
        int expected_is_phi = 0;

        if (def_block_id >= value_function->block_count || def_phi_index >= value_function->blocks[def_block_id].phi_count) {
            visit_state[value.value_id] = 2;
            return 1;
        }
        if (expected_version_id < memory_def_use->version_count &&
            memory_def_use->has_def &&
            memory_def_use->has_def[expected_version_id] &&
            memory_def_use->def_kinds &&
            memory_def_use->def_kinds[expected_version_id] == MEMORY_SSA_VERSION_DEF_PHI) {
            expected_phi_block_id = memory_def_use->def_block_ids[expected_version_id];
            expected_phi_index = memory_def_use->def_phi_indices[expected_version_id];
            if (expected_phi_block_id < memory_function->block_count &&
                expected_phi_index < memory_function->blocks[expected_phi_block_id].phi_count) {
                expected_phi = &memory_function->blocks[expected_phi_block_id].phis[expected_phi_index];
                expected_is_phi = 1;
            }
        }

        phi = &value_function->blocks[def_block_id].phis[def_phi_index];
        for (input_index = 0; input_index < phi->input_count; ++input_index) {
            int input_matches = 0;
            size_t input_expected_version_id = expected_version_id;

            if (expected_is_phi) {
                size_t expected_input_index;
                int found_expected_input = 0;

                for (expected_input_index = 0; expected_input_index < expected_phi->input_count; ++expected_input_index) {
                    if (expected_phi->inputs[expected_input_index].predecessor_block_id ==
                        phi->inputs[input_index].predecessor_block_id) {
                        input_expected_version_id = expected_phi->inputs[expected_input_index].version_id;
                        found_expected_input = 1;
                        break;
                    }
                }
                if (!found_expected_input) {
                    visit_state[value.value_id] = 2;
                    return 1;
                }
            }

            if (!memory_ssa_pass_value_matches_slot_version(value_function,
                    memory_function,
                    memory_def_use,
                    value_def_use,
                    phi->inputs[input_index].value,
                    expected_slot,
                    input_expected_version_id,
                    visit_state,
                    &input_matches,
                    error)) {
                visit_state[value.value_id] = 2;
                return 0;
            }
            if (!input_matches) {
                visit_state[value.value_id] = 2;
                return 1;
            }
        }

        *out_matches = 1;
        visit_state[value.value_id] = 2;
        return 1;
    }

    if (def_instruction_index == (size_t)-1 || def_block_id >= value_function->block_count ||
        def_instruction_index >= value_function->blocks[def_block_id].instruction_count ||
        def_block_id >= memory_function->block_count ||
        def_instruction_index >= memory_function->blocks[def_block_id].instruction_count) {
        visit_state[value.value_id] = 2;
        return 1;
    }

    def_instruction = &value_function->blocks[def_block_id].instructions[def_instruction_index];
    memory_instruction = &memory_function->blocks[def_block_id].instructions[def_instruction_index];
    switch (def_instruction->kind) {
    case VALUE_SSA_INSTR_MOV:
        if (!memory_ssa_pass_value_matches_slot_version(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                def_instruction->as.mov_value,
                expected_slot,
                expected_version_id,
                visit_state,
                out_matches,
                error)) {
            visit_state[value.value_id] = 2;
            return 0;
        }
        break;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        if (memory_instruction->access_count > 0 && memory_instruction->accesses[0].has_use_version &&
            memory_instruction->accesses[0].use_version_id == expected_version_id &&
            memory_ssa_pass_slot_refs_equal(def_instruction->as.load_slot, expected_slot)) {
            *out_matches = 1;
        }
        break;
    default:
        break;
    }

    visit_state[value.value_id] = 2;
    return 1;
}

static int memory_ssa_pass_find_block_equivalent_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_id,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    unsigned char *visit_state = NULL;
    const ValueSsaBasicBlock *block;
    size_t instruction_index;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !out_has_value || !out_value ||
        block_id >= value_function->block_count || block_id >= memory_function->block_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-037: invalid block-equivalent value lookup");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    visit_state = (unsigned char *)calloc(value_def_use->value_count ? value_def_use->value_count : 1, sizeof(unsigned char));
    if (!visit_state) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-038: out of memory while searching block value");
        return 0;
    }

    block = &value_function->blocks[block_id];
    for (instruction_index = block->instruction_count; instruction_index > 0; --instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];
        int matches = 0;

        if (!instruction->has_result) {
            continue;
        }
        memset(visit_state, 0, value_def_use->value_count * sizeof(unsigned char));
        if (!memory_ssa_pass_value_matches_slot_version(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                instruction->result,
                expected_slot,
                expected_version_id,
                visit_state,
                &matches,
                error)) {
            free(visit_state);
            return 0;
        }
        if (matches) {
            *out_has_value = 1;
            *out_value = instruction->result;
            free(visit_state);
            return 1;
        }
    }

    for (instruction_index = block->phi_count; instruction_index > 0; --instruction_index) {
        ValueSsaValueRef phi_value = value_ssa_value_id(block->phis[instruction_index - 1].result_id);
        int matches = 0;

        memset(visit_state, 0, value_def_use->value_count * sizeof(unsigned char));
        if (!memory_ssa_pass_value_matches_slot_version(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                phi_value,
                expected_slot,
                expected_version_id,
                visit_state,
                &matches,
                error)) {
            free(visit_state);
            return 0;
        }
        if (matches) {
            *out_has_value = 1;
            *out_value = phi_value;
            free(visit_state);
            return 1;
        }
    }

    free(visit_state);
    return 1;
}

static int memory_ssa_pass_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }

    switch (lhs.kind) {
    case VALUE_SSA_VALUE_IMMEDIATE:
        return lhs.immediate == rhs.immediate;
    case VALUE_SSA_VALUE_ID:
        return lhs.value_id == rhs.value_id;
    default:
        return 0;
    }
}

static int memory_ssa_pass_slot_refs_equal(ValueSsaSlotRef lhs, ValueSsaSlotRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }
    return lhs.id == rhs.id;
}

static int memory_ssa_pass_dump_program_copy(const ValueSsaProgram *program,
    char **out_text,
    ValueSsaError *error) {
    if (!out_text) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-015: invalid dump-copy contract");
        return 0;
    }

    *out_text = NULL;
    if (!value_ssa_dump_program(program, out_text)) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-016: failed to dump Value-SSA program");
        return 0;
    }

    return 1;
}

static int memory_ssa_pass_run_one_step(const char *step_name,
    int (*pass_fn)(ValueSsaProgram *, ValueSsaError *),
    ValueSsaProgram *program,
    ValueSsaError *error) {
    if (!step_name || !pass_fn || !program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-017: invalid pipeline-step contract");
        return 0;
    }
    if (!pass_fn(program, error)) {
        return 0;
    }
    if (!value_ssa_verify_program(program, error)) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-018: pipeline step '%s' produced verifier-invalid Value-SSA: %s",
            step_name,
            error ? error->message : "");
        return 0;
    }

    return 1;
}

#endif
