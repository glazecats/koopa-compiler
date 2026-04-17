#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static int memory_ssa_pass_try_phi_forward_join_load(ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    const ValueSsaCfgAnalysis *cfg_analysis,
    size_t block_id,
    size_t instruction_index,
    int *out_forwarded,
    int *out_restart_required,
    ValueSsaError *error) {
    ValueSsaBasicBlock *value_block;
    ValueSsaInstruction *instruction;
    const MemorySsaInstruction *memory_instruction;
    MemorySsaError memory_error;
    ValueSsaPhiInput *phi_inputs = NULL;
    size_t predecessor_count;
    size_t predecessor_block_id;
    size_t phi_result_id;
    size_t input_index = 0;
    int has_backedge_predecessor;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !cfg_analysis || !out_forwarded ||
        !out_restart_required) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-031: invalid phi-load-forward contract");
        return 0;
    }

    *out_forwarded = 0;
    *out_restart_required = 0;
    if (block_id >= value_function->block_count || block_id >= memory_function->block_count ||
        block_id >= cfg_analysis->block_count || instruction_index >= value_function->blocks[block_id].instruction_count ||
        instruction_index >= memory_function->blocks[block_id].instruction_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-031: invalid phi-load-forward site");
        return 0;
    }

    predecessor_count = cfg_analysis->predecessor_counts[block_id];
    if (predecessor_count < 2) {
        return 1;
    }
    has_backedge_predecessor = memory_ssa_pass_block_has_backedge_predecessor(cfg_analysis, block_id);

    value_block = &value_function->blocks[block_id];
    instruction = &value_block->instructions[instruction_index];
    memory_instruction = &memory_function->blocks[block_id].instructions[instruction_index];
    if ((instruction->kind != VALUE_SSA_INSTR_LOAD_LOCAL && instruction->kind != VALUE_SSA_INSTR_LOAD_GLOBAL) ||
        !instruction->has_result || memory_instruction->access_count == 0 ||
        !memory_instruction->accesses[0].has_use_version) {
        return 1;
    }

    phi_inputs = (ValueSsaPhiInput *)calloc(predecessor_count, sizeof(ValueSsaPhiInput));
    if (!phi_inputs) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-032: out of memory while preparing load phi");
        return 0;
    }

    for (predecessor_block_id = 0; predecessor_block_id < cfg_analysis->block_count; ++predecessor_block_id) {
        int has_value = 0;
        ValueSsaValueRef input_value;

        if (!cfg_analysis->predecessor_matrix[predecessor_block_id * cfg_analysis->block_count + block_id]) {
            continue;
        }
        if (!memory_ssa_find_block_equivalent_value(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                predecessor_block_id,
                instruction->as.load_slot,
                memory_instruction->accesses[0].use_version_id,
                &has_value,
                &input_value,
                &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            free(phi_inputs);
            return 0;
        }
        if (!has_value) {
            if (!has_backedge_predecessor) {
                int has_dominating_value = 0;
                ValueSsaValueRef dominating_value;
                ValueSsaBasicBlock *predecessor_block = &value_function->blocks[predecessor_block_id];
                ValueSsaInstruction inserted_load;
                size_t inserted_value_id;

                if (!memory_ssa_pass_find_dominating_equivalent_value(value_function,
                        memory_function,
                        memory_def_use,
                        value_def_use,
                        cfg_analysis,
                        predecessor_block_id,
                        predecessor_block->instruction_count,
                        instruction->as.load_slot,
                        memory_instruction->accesses[0].use_version_id,
                        &has_dominating_value,
                        &dominating_value,
                        error)) {
                    free(phi_inputs);
                    return 0;
                }
                if (has_dominating_value) {
                    input_value = dominating_value;
                    has_value = 1;
                } else {
                    inserted_value_id = value_ssa_function_allocate_value(value_function);
                    if (inserted_value_id == (size_t)-1) {
                        memory_ssa_pass_set_error(
                            error, 0, 0, "MEMORY-SSA-PASS-032: failed to allocate value for PRE load");
                        free(phi_inputs);
                        return 0;
                    }

                    memset(&inserted_load, 0, sizeof(inserted_load));
                    inserted_load.kind = instruction->kind;
                    inserted_load.has_result = 1;
                    inserted_load.result = value_ssa_value_id(inserted_value_id);
                    inserted_load.as.load_slot = instruction->as.load_slot;

                    if (!memory_ssa_pass_append_instruction_ignoring_terminator(
                            predecessor_block, &inserted_load, error)) {
                        free(phi_inputs);
                        return 0;
                    }

                    input_value = value_ssa_value_id(inserted_value_id);
                    has_value = 1;
                    *out_restart_required = 1;
                }
            } else {
                free(phi_inputs);
                return 1;
            }
        }

        phi_inputs[input_index].predecessor_block_id = predecessor_block_id;
        phi_inputs[input_index].value = input_value;
        ++input_index;
    }

    if (input_index != predecessor_count) {
        free(phi_inputs);
        return 1;
    }

    phi_result_id = value_ssa_function_allocate_value(value_function);
    if (phi_result_id == (size_t)-1 ||
        !memory_ssa_pass_append_phi(value_block, phi_result_id, phi_inputs, predecessor_count, error)) {
        free(phi_inputs);
        return 0;
    }
    free(phi_inputs);

    instruction->kind = VALUE_SSA_INSTR_MOV;
    instruction->as.mov_value = value_ssa_value_id(phi_result_id);
    *out_forwarded = 1;
    return 1;
}

static int memory_ssa_pass_find_prior_equivalent_value_in_block(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_id,
    size_t before_instruction_index,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    const ValueSsaBasicBlock *block;
    MemorySsaError memory_error;
    size_t instruction_index;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !out_has_value || !out_value) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-022: invalid block-equivalent lookup contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (block_id >= value_function->block_count || block_id >= memory_function->block_count ||
        before_instruction_index > value_function->blocks[block_id].instruction_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-022: invalid block-equivalent lookup site");
        return 0;
    }

    block = &value_function->blocks[block_id];
    for (instruction_index = before_instruction_index; instruction_index > 0; --instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index - 1];
        int matches = 0;

        if (!instruction->has_result) {
            continue;
        }
        if (!memory_ssa_value_matches_slot_version(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                instruction->result,
                expected_slot,
                expected_version_id,
                &matches,
                &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            return 0;
        }
        if (matches) {
            *out_has_value = 1;
            *out_value = instruction->result;
            return 1;
        }
    }

    for (instruction_index = block->phi_count; instruction_index > 0; --instruction_index) {
        ValueSsaValueRef phi_value = value_ssa_value_id(block->phis[instruction_index - 1].result_id);
        int matches = 0;

        if (!memory_ssa_value_matches_slot_version(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                phi_value,
                expected_slot,
                expected_version_id,
                &matches,
                &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            return 0;
        }
        if (matches) {
            *out_has_value = 1;
            *out_value = phi_value;
            return 1;
        }
    }

    return 1;
}

static int memory_ssa_pass_block_has_backedge_predecessor(const ValueSsaCfgAnalysis *cfg_analysis, size_t block_id) {
    size_t predecessor_block_id;

    if (!cfg_analysis || !cfg_analysis->predecessor_matrix || !cfg_analysis->dominates ||
        block_id >= cfg_analysis->block_count) {
        return 0;
    }

    for (predecessor_block_id = 0; predecessor_block_id < cfg_analysis->block_count; ++predecessor_block_id) {
        if (!cfg_analysis->predecessor_matrix[predecessor_block_id * cfg_analysis->block_count + block_id]) {
            continue;
        }
        if (cfg_analysis->dominates[block_id * cfg_analysis->block_count + predecessor_block_id]) {
            return 1;
        }
    }

    return 0;
}

static int memory_ssa_pass_find_dominating_equivalent_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    const ValueSsaCfgAnalysis *cfg_analysis,
    size_t block_id,
    size_t instruction_index,
    ValueSsaSlotRef expected_slot,
    size_t expected_version_id,
    int *out_has_value,
    ValueSsaValueRef *out_value,
    ValueSsaError *error) {
    MemorySsaError memory_error;
    size_t current_block_id;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !cfg_analysis ||
        !out_has_value || !out_value) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-023: invalid dominating-equivalent forwarding contract");
        return 0;
    }

    *out_has_value = 0;
    *out_value = value_ssa_value_immediate(0);
    if (block_id >= value_function->block_count || block_id >= memory_function->block_count ||
        block_id >= cfg_analysis->block_count ||
        instruction_index > value_function->blocks[block_id].instruction_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-024: invalid dominating-equivalent site");
        return 0;
    }

    current_block_id = block_id;
    while (current_block_id != (size_t)-1) {
        int ok;

        if (current_block_id == block_id) {
            ok = memory_ssa_pass_find_prior_equivalent_value_in_block(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                current_block_id,
                instruction_index,
                expected_slot,
                expected_version_id,
                out_has_value,
                out_value,
                error);
        } else {
            ok = memory_ssa_find_block_equivalent_value(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                current_block_id,
                expected_slot,
                expected_version_id,
                out_has_value,
                out_value,
                &memory_error);
            if (!ok) {
                memory_ssa_pass_copy_memory_error(error, &memory_error);
                return 0;
            }
        }

        if (!ok || *out_has_value) {
            return ok;
        }
        if (current_block_id == block_id && memory_ssa_pass_block_has_backedge_predecessor(cfg_analysis, block_id)) {
            return 1;
        }
        current_block_id = cfg_analysis->immediate_dominator[current_block_id];
    }

    return 1;
}

static int memory_ssa_pass_forward_loads(ValueSsaProgram *program,
    ValueSsaInstructionKind load_kind,
    const char *invalid_error_code,
    ValueSsaError *error) {
    size_t iteration;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "%s", invalid_error_code ? invalid_error_code : "MEMORY-SSA-PASS");
        return 0;
    }

    for (iteration = 0; iteration < 8; ++iteration) {
        MemorySsaProgram memory_program;
        MemorySsaError memory_error;
        size_t function_index;
        int changed = 0;
        int restart_required = 0;

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

        for (function_index = 0; function_index < program->function_count && !restart_required; ++function_index) {
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

            for (block_index = 0; block_index < value_function->block_count && ok && !restart_required; ++block_index) {
                ValueSsaBasicBlock *value_block = &value_function->blocks[block_index];
                size_t instruction_index;

                for (instruction_index = 0; instruction_index < value_block->instruction_count; ++instruction_index) {
                    ValueSsaInstruction *instruction = &value_block->instructions[instruction_index];
                    int has_value = 0;
                    ValueSsaValueRef value;

                    if (instruction->kind != load_kind) {
                        continue;
                    }

                    ok = memory_ssa_resolve_load_store_value(
                        memory_function, &def_use, block_index, instruction_index, &has_value, &value, &memory_error);
                    if (!ok) {
                        memory_ssa_pass_copy_memory_error(error, &memory_error);
                        break;
                    }
                    if (!has_value) {
                        ok = memory_ssa_pass_find_dominating_equivalent_value(value_function,
                            memory_function,
                            &def_use,
                            &value_def_use,
                            &cfg_analysis,
                            block_index,
                            instruction_index,
                            instruction->as.load_slot,
                            memory_function->blocks[block_index].instructions[instruction_index].accesses[0].use_version_id,
                            &has_value,
                            &value,
                            error);
                        if (!ok || !has_value) {
                            int forwarded_via_phi = 0;

                            ok = memory_ssa_pass_try_phi_forward_join_load(
                                value_function,
                                memory_function,
                                &def_use,
                                &value_def_use,
                                &cfg_analysis,
                                block_index,
                                instruction_index,
                                &forwarded_via_phi,
                                &restart_required,
                                error);
                            if (!ok) {
                                break;
                            }
                            if (forwarded_via_phi) {
                                changed = 1;
                            }
                            if (forwarded_via_phi || restart_required) {
                                if (restart_required) {
                                    break;
                                }
                                continue;
                            }
                            {
                                int promoted_via_phi = 0;

                                ok = memory_ssa_pass_try_promote_dominating_memory_phi(value_function,
                                    memory_function,
                                    &def_use,
                                    &value_def_use,
                                    &cfg_analysis,
                                    block_index,
                                    instruction_index,
                                    &promoted_via_phi,
                                    error);
                                if (!ok) {
                                    break;
                                }
                                if (promoted_via_phi) {
                                    changed = 1;
                                    continue;
                                }
                            }
                            continue;
                        }
                    }

                    instruction->kind = VALUE_SSA_INSTR_MOV;
                    instruction->as.mov_value = value;
                    changed = 1;
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
        if (!restart_required) {
            if (!changed) {
                return value_ssa_verify_program(program, error);
            }
        }
    }

    memory_ssa_pass_set_error(error,
        0,
        0,
        "MEMORY-SSA-PASS-051: load forwarding did not converge within %d iterations",
        8);
    return 0;
}

int memory_ssa_pass_forward_local_loads(ValueSsaProgram *program, ValueSsaError *error) {
    return memory_ssa_pass_forward_loads(program,
        VALUE_SSA_INSTR_LOAD_LOCAL,
        "MEMORY-SSA-PASS-004: invalid local-load-forward contract",
        error);
}

int memory_ssa_pass_forward_global_loads(ValueSsaProgram *program, ValueSsaError *error) {
    return memory_ssa_pass_forward_loads(program,
        VALUE_SSA_INSTR_LOAD_GLOBAL,
        "MEMORY-SSA-PASS-009: invalid global-load-forward contract",
        error);
}

#endif
