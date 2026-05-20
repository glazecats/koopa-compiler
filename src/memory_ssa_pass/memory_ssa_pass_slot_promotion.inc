#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static int memory_ssa_pass_try_promote_dominating_memory_phi(ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    const ValueSsaCfgAnalysis *cfg_analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_promoted,
    ValueSsaError *error) {
    ValueSsaBasicBlock *phi_block;
    ValueSsaInstruction *instruction;
    const MemorySsaInstruction *memory_instruction;
    const MemorySsaPhi *memory_phi;
    MemorySsaError memory_error;
    ValueSsaPhiInput *phi_inputs = NULL;
    ValueSsaValueRef existing_value;
    size_t phi_block_id;
    size_t phi_index;
    size_t result_id;
    size_t input_index;
    size_t existing_phi_index;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !cfg_analysis || !out_promoted) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-025: invalid dominating-memory-phi promotion contract");
        return 0;
    }

    *out_promoted = 0;
    if (block_id >= value_function->block_count || block_id >= memory_function->block_count ||
        block_id >= cfg_analysis->block_count ||
        instruction_index >= value_function->blocks[block_id].instruction_count ||
        instruction_index >= memory_function->blocks[block_id].instruction_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-026: invalid dominating-memory-phi promotion site");
        return 0;
    }

    instruction = &value_function->blocks[block_id].instructions[instruction_index];
    memory_instruction = &memory_function->blocks[block_id].instructions[instruction_index];
    if ((instruction->kind != VALUE_SSA_INSTR_LOAD_LOCAL && instruction->kind != VALUE_SSA_INSTR_LOAD_GLOBAL) ||
        !instruction->has_result || memory_instruction->access_count == 0 ||
        !memory_instruction->accesses[0].has_use_version ||
        memory_instruction->accesses[0].use_version_id >= memory_def_use->version_count ||
        !memory_def_use->has_def || !memory_def_use->has_def[memory_instruction->accesses[0].use_version_id] ||
        !memory_def_use->def_kinds ||
        memory_def_use->def_kinds[memory_instruction->accesses[0].use_version_id] != MEMORY_SSA_VERSION_DEF_PHI) {
        return 1;
    }
    if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
        !memory_ssa_pass_local_slot_is_scalar_replaceable(value_function, instruction->as.load_slot)) {
        return 1;
    }

    phi_block_id = memory_def_use->def_block_ids[memory_instruction->accesses[0].use_version_id];
    phi_index = memory_def_use->def_phi_indices[memory_instruction->accesses[0].use_version_id];
    if (phi_block_id >= value_function->block_count || phi_block_id >= memory_function->block_count ||
        phi_block_id >= cfg_analysis->block_count || phi_index >= memory_function->blocks[phi_block_id].phi_count) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-027: invalid memory-phi metadata while promoting load");
        return 0;
    }
    if (phi_block_id != block_id &&
        !cfg_analysis->dominates[phi_block_id * cfg_analysis->block_count + block_id]) {
        return 1;
    }

    /*
     * Be conservative for loop headers: if the load itself sits in a block with
     * a backedge predecessor, only promote from a dominating memory phi when we
     * can later resolve each phi input to a concrete predecessor-carried value.
     * Otherwise we may manufacture a self-referential scalar carrier that does
     * not actually correspond to the slot state along the loop backedge.
     */
    if (phi_block_id == block_id && memory_ssa_pass_block_has_backedge_predecessor(cfg_analysis, block_id)) {
        return 1;
    }

    phi_block = &value_function->blocks[phi_block_id];
    if (phi_block_id == block_id) {
        if (!memory_ssa_pass_find_prior_equivalent_value_in_block(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                phi_block_id,
                instruction_index,
                instruction->as.load_slot,
                memory_instruction->accesses[0].use_version_id,
                out_promoted,
                &existing_value,
                error)) {
            return 0;
        }
    } else {
        if (!memory_ssa_find_block_equivalent_value(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                phi_block_id,
                instruction->as.load_slot,
                memory_instruction->accesses[0].use_version_id,
                out_promoted,
                &existing_value,
                &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            return 0;
        }
    }
    if (*out_promoted) {
        instruction->kind = VALUE_SSA_INSTR_MOV;
        instruction->as.mov_value = existing_value;
        return 1;
    }

    memory_phi = &memory_function->blocks[phi_block_id].phis[phi_index];
    phi_inputs = (ValueSsaPhiInput *)calloc(memory_phi->input_count, sizeof(ValueSsaPhiInput));
    if (!phi_inputs) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-028: out of memory while promoting memory phi");
        return 0;
    }

    for (input_index = 0; input_index < memory_phi->input_count; ++input_index) {
        int has_input_value = 0;
        ValueSsaValueRef input_value;

        if (!memory_ssa_resolve_version_value(memory_function,
                memory_def_use,
                memory_phi->inputs[input_index].version_id,
                &has_input_value,
                &input_value,
                &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            free(phi_inputs);
            return 0;
        }
        if (!has_input_value) {
            if (!memory_ssa_find_block_equivalent_value(value_function,
                    memory_function,
                    memory_def_use,
                    value_def_use,
                    memory_phi->inputs[input_index].predecessor_block_id,
                    instruction->as.load_slot,
                    memory_phi->inputs[input_index].version_id,
                    &has_input_value,
                    &input_value,
                    &memory_error)) {
                memory_ssa_pass_copy_memory_error(error, &memory_error);
                free(phi_inputs);
                return 0;
            }
        }
        if (!has_input_value) {
            size_t predecessor_block_id = memory_phi->inputs[input_index].predecessor_block_id;

            if (phi_block_id < cfg_analysis->block_count &&
                predecessor_block_id < cfg_analysis->block_count &&
                phi_block_id != predecessor_block_id &&
                cfg_analysis->dominates[phi_block_id * cfg_analysis->block_count + predecessor_block_id]) {
                free(phi_inputs);
                return 1;
            }
            if (!memory_ssa_pass_find_dominating_equivalent_value(value_function,
                    memory_function,
                    memory_def_use,
                    value_def_use,
                    cfg_analysis,
                    predecessor_block_id,
                    value_function->blocks[predecessor_block_id].instruction_count,
                    instruction->as.load_slot,
                    memory_phi->inputs[input_index].version_id,
                    &has_input_value,
                    &input_value,
                    error)) {
                free(phi_inputs);
                return 0;
            }
        }
        if (!has_input_value) {
            free(phi_inputs);
            return 1;
        }

        {
            size_t predecessor_block_id = memory_phi->inputs[input_index].predecessor_block_id;
            int predecessor_is_backedge =
                phi_block_id < cfg_analysis->block_count &&
                predecessor_block_id < cfg_analysis->block_count &&
                phi_block_id != predecessor_block_id &&
                cfg_analysis->dominates[phi_block_id * cfg_analysis->block_count + predecessor_block_id];

            if (predecessor_is_backedge &&
                input_value.kind == VALUE_SSA_VALUE_ID &&
                input_value.value_id < value_def_use->value_count &&
                value_def_use->has_def[input_value.value_id] &&
                value_def_use->def_block_ids[input_value.value_id] != predecessor_block_id) {
                free(phi_inputs);
                return 1;
            }
        }

        phi_inputs[input_index].predecessor_block_id = memory_phi->inputs[input_index].predecessor_block_id;
        phi_inputs[input_index].value = input_value;
    }

    if (memory_phi->input_count > 0) {
        int all_inputs_equal = 1;
        ValueSsaValueRef common_input_value = phi_inputs[0].value;

        for (input_index = 1; input_index < memory_phi->input_count; ++input_index) {
            if (!memory_ssa_pass_value_refs_equal(common_input_value, phi_inputs[input_index].value)) {
                all_inputs_equal = 0;
                break;
            }
        }
        if (all_inputs_equal) {
            free(phi_inputs);
            instruction->kind = VALUE_SSA_INSTR_MOV;
            instruction->as.mov_value = common_input_value;
            *out_promoted = 1;
            return 1;
        }
    }

    for (existing_phi_index = 0; existing_phi_index < phi_block->phi_count; ++existing_phi_index) {
        const ValueSsaPhi *existing_phi = &phi_block->phis[existing_phi_index];
        int same_phi = 1;

        if (existing_phi->input_count != memory_phi->input_count ||
            existing_phi->result_id >= value_function->next_value_id) {
            continue;
        }
        for (input_index = 0; input_index < memory_phi->input_count; ++input_index) {
            if (existing_phi->inputs[input_index].predecessor_block_id !=
                    memory_phi->inputs[input_index].predecessor_block_id ||
                !memory_ssa_pass_value_refs_equal(
                    existing_phi->inputs[input_index].value, phi_inputs[input_index].value)) {
                same_phi = 0;
                break;
            }
        }
        if (!same_phi) {
            continue;
        }

        free(phi_inputs);
        instruction->kind = VALUE_SSA_INSTR_MOV;
        instruction->as.mov_value = value_ssa_value_id(existing_phi->result_id);
        *out_promoted = 1;
        return 1;
    }

    result_id = value_ssa_function_allocate_value(value_function);
    if (result_id == (size_t)-1 || !memory_ssa_pass_append_phi(phi_block, result_id, phi_inputs, memory_phi->input_count, error)) {
        free(phi_inputs);
        return 0;
    }
    free(phi_inputs);

    instruction->kind = VALUE_SSA_INSTR_MOV;
    instruction->as.mov_value = value_ssa_value_id(result_id);
    *out_promoted = 1;
    return 1;
}

static int memory_ssa_pass_promote_slots(ValueSsaProgram *program,
    ValueSsaInstructionKind load_kind,
    const char *invalid_error_code,
    const char *nonconverged_error_code,
    ValueSsaError *error) {
    size_t iteration;

    if (!program) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "%s",
            invalid_error_code ? invalid_error_code : "MEMORY-SSA-PASS-052: invalid slot-promotion contract");
        return 0;
    }
    if (load_kind != VALUE_SSA_INSTR_LOAD_LOCAL && load_kind != VALUE_SSA_INSTR_LOAD_GLOBAL) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-054: invalid slot-promotion load kind");
        return 0;
    }

    for (iteration = 0; iteration < 8; ++iteration) {
        MemorySsaProgram memory_program;
        MemorySsaError memory_error;
        size_t function_index;
        int changed = 0;

        memory_ssa_program_init(&memory_program);
        if (!memory_ssa_build_from_value_ssa(program, &memory_program, &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            memory_ssa_program_free(&memory_program);
            return 0;
        }
        if (memory_program.function_count != program->function_count) {
            memory_ssa_pass_set_error(
                error, 0, 0, "MEMORY-SSA-PASS-005: function-count mismatch after memory_ssa build");
            memory_ssa_program_free(&memory_program);
            return 0;
        }

        for (function_index = 0; function_index < program->function_count; ++function_index) {
            ValueSsaFunction *value_function = &program->functions[function_index];
            const MemorySsaFunction *memory_function = &memory_program.functions[function_index];
            MemorySsaDefUseAnalysis def_use;
            ValueSsaDefUseAnalysis value_def_use;
            ValueSsaCfgAnalysis cfg_analysis;
            size_t block_index;
            int ok = 1;

            if (!value_function->has_body) {
                continue;
            }
            if (!memory_ssa_pass_validate_alignment(value_function, memory_function, error)) {
                memory_ssa_program_free(&memory_program);
                return 0;
            }

            memory_ssa_def_use_analysis_init(&def_use);
            value_ssa_def_use_analysis_init(&value_def_use);
            value_ssa_cfg_analysis_init(&cfg_analysis);
            if (!memory_ssa_compute_def_use_analysis(memory_function, &def_use, &memory_error)) {
                memory_ssa_pass_copy_memory_error(error, &memory_error);
                memory_ssa_def_use_analysis_free(&def_use);
                value_ssa_def_use_analysis_free(&value_def_use);
                value_ssa_cfg_analysis_free(&cfg_analysis);
                memory_ssa_program_free(&memory_program);
                return 0;
            }
            if (!value_ssa_compute_def_use_analysis(value_function, &value_def_use, error) ||
                !value_ssa_compute_cfg_analysis(value_function, &cfg_analysis, error)) {
                memory_ssa_def_use_analysis_free(&def_use);
                value_ssa_def_use_analysis_free(&value_def_use);
                value_ssa_cfg_analysis_free(&cfg_analysis);
                memory_ssa_program_free(&memory_program);
                return 0;
            }

            for (block_index = 0; block_index < value_function->block_count && ok; ++block_index) {
                ValueSsaBasicBlock *value_block = &value_function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < value_block->instruction_count; ++instruction_index) {
                    ValueSsaInstruction *instruction = &value_block->instructions[instruction_index];
                    int promoted = 0;

                    if (instruction->kind != load_kind) {
                        continue;
                    }
                    if (load_kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                        !memory_ssa_pass_local_slot_is_scalar_replaceable(value_function, instruction->as.load_slot)) {
                        continue;
                    }
                    ok = memory_ssa_pass_try_promote_dominating_memory_phi(value_function,
                        memory_function,
                        &def_use,
                        &value_def_use,
                        &cfg_analysis,
                        block_index,
                        instruction_index,
                        &promoted,
                        error);
                    if (!ok) {
                        break;
                    }
                    if (promoted) {
                        changed = 1;
                    }
                }
            }

            memory_ssa_def_use_analysis_free(&def_use);
            value_ssa_def_use_analysis_free(&value_def_use);
            value_ssa_cfg_analysis_free(&cfg_analysis);
            if (!ok) {
                memory_ssa_program_free(&memory_program);
                return 0;
            }
        }

        memory_ssa_program_free(&memory_program);
        if (!changed) {
            return value_ssa_verify_program(program, error);
        }
    }

    memory_ssa_pass_set_error(
        error,
        0,
        0,
        "%s",
        nonconverged_error_code
            ? nonconverged_error_code
            : "MEMORY-SSA-PASS-053: slot promotion did not converge within 8 iterations");
    return 0;
}

int memory_ssa_pass_promote_local_slots(ValueSsaProgram *program, ValueSsaError *error) {
    return memory_ssa_pass_promote_slots(program,
        VALUE_SSA_INSTR_LOAD_LOCAL,
        "MEMORY-SSA-PASS-052: invalid local-slot-promotion contract",
        "MEMORY-SSA-PASS-053: local-slot promotion did not converge within 8 iterations",
        error);
}

int memory_ssa_pass_promote_global_slots(ValueSsaProgram *program, ValueSsaError *error) {
    return memory_ssa_pass_promote_slots(program,
        VALUE_SSA_INSTR_LOAD_GLOBAL,
        "MEMORY-SSA-PASS-055: invalid global-slot-promotion contract",
        "MEMORY-SSA-PASS-056: global-slot promotion did not converge within 8 iterations",
        error);
}

#endif
