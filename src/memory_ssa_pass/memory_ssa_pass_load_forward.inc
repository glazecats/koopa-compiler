#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static size_t memory_ssa_pass_load_forward_iteration_limit(const ValueSsaProgram *program) {
    size_t function_index;
    size_t total_blocks = 0u;
    size_t total_instructions = 0u;
    size_t limit;

    if (!program) {
        return 8u;
    }

    for (function_index = 0u; function_index < program->function_count; ++function_index) {
        const ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }
        total_blocks += function->block_count;
        for (block_index = 0u; block_index < function->block_count; ++block_index) {
            total_instructions += function->blocks[block_index].instruction_count;
        }
    }

    /*
     * Global/local load PRE is monotone here: each restart inserts one more
     * predecessor load, and each successful forward removes at least one load.
     * Deep short-circuit CFGs legitimately need far more than a flat constant
     * 8 restarts, so derive a program-shaped budget instead of using a fixed
     * small cap.
     */
    limit = total_instructions + (total_blocks * 2u) + 8u;
    if (limit < 256u) {
        limit = 256u;
    }
    return limit;
}

static int memory_ssa_pass_map_predecessor_expected_version(const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    size_t block_id,
    size_t predecessor_block_id,
    size_t expected_version_id,
    size_t *out_expected_version_id,
    ValueSsaError *error) {
    size_t phi_block_id;
    size_t phi_index;
    const MemorySsaPhi *phi;
    size_t input_index;

    if (!memory_function || !memory_def_use || !out_expected_version_id) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-066: invalid predecessor-version mapping contract");
        return 0;
    }

    *out_expected_version_id = expected_version_id;
    if (expected_version_id >= memory_def_use->version_count || !memory_def_use->has_def || !memory_def_use->has_def[expected_version_id] ||
        !memory_def_use->def_kinds || memory_def_use->def_kinds[expected_version_id] != MEMORY_SSA_VERSION_DEF_PHI) {
        return 1;
    }

    phi_block_id = memory_def_use->def_block_ids[expected_version_id];
    phi_index = memory_def_use->def_phi_indices[expected_version_id];
    if (phi_block_id != block_id) {
        return 1;
    }
    if (phi_block_id >= memory_function->block_count || phi_index >= memory_function->blocks[phi_block_id].phi_count) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-067: invalid memory phi def for mem.%zu during predecessor mapping",
            expected_version_id);
        return 0;
    }

    phi = &memory_function->blocks[phi_block_id].phis[phi_index];
    for (input_index = 0; input_index < phi->input_count; ++input_index) {
        if (phi->inputs[input_index].predecessor_block_id == predecessor_block_id) {
            *out_expected_version_id = phi->inputs[input_index].version_id;
            return 1;
        }
    }

    memory_ssa_pass_set_error(error,
        0,
        0,
        "MEMORY-SSA-PASS-068: memory phi mem.%zu is missing predecessor bb.%zu",
        expected_version_id,
        predecessor_block_id);
    return 0;
}

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
    size_t *pending_insert_input_indices = NULL;
    size_t predecessor_count;
    size_t predecessor_block_id;
    size_t phi_result_id;
    size_t input_index = 0;
    int has_backedge_predecessor;
    size_t pending_insert_count = 0;
    size_t join_expected_version_id;

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
    join_expected_version_id = memory_instruction->accesses[0].use_version_id;

    phi_inputs = (ValueSsaPhiInput *)calloc(predecessor_count, sizeof(ValueSsaPhiInput));
    pending_insert_input_indices = (size_t *)calloc(predecessor_count ? predecessor_count : 1u, sizeof(size_t));
    if (!phi_inputs || !pending_insert_input_indices) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-032: out of memory while preparing load phi");
        free(pending_insert_input_indices);
        free(phi_inputs);
        return 0;
    }

    for (predecessor_block_id = 0; predecessor_block_id < cfg_analysis->block_count; ++predecessor_block_id) {
        int has_value = 0;
        ValueSsaValueRef input_value = value_ssa_value_immediate(0);
        size_t predecessor_expected_version_id = join_expected_version_id;

        if (!cfg_analysis->predecessor_matrix[predecessor_block_id * cfg_analysis->block_count + block_id]) {
            continue;
        }
        if (!memory_ssa_pass_map_predecessor_expected_version(memory_function,
                memory_def_use,
                block_id,
                predecessor_block_id,
                join_expected_version_id,
                &predecessor_expected_version_id,
                error)) {
            free(pending_insert_input_indices);
            free(phi_inputs);
            return 0;
        }
        if (!memory_ssa_find_block_equivalent_value(value_function,
                memory_function,
                memory_def_use,
                value_def_use,
                predecessor_block_id,
                instruction->as.load_slot,
                predecessor_expected_version_id,
                &has_value,
                &input_value,
                &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            free(pending_insert_input_indices);
            free(phi_inputs);
            return 0;
        }
        if (!has_value) {
            if (!has_backedge_predecessor) {
                int has_dominating_value = 0;
                ValueSsaValueRef dominating_value;
                const ValueSsaBasicBlock *predecessor_block = &value_function->blocks[predecessor_block_id];

                if (!memory_ssa_pass_find_dominating_equivalent_value(value_function,
                        memory_function,
                        memory_def_use,
                        value_def_use,
                        cfg_analysis,
                        predecessor_block_id,
                        predecessor_block->instruction_count,
                        instruction->as.load_slot,
                        predecessor_expected_version_id,
                        &has_dominating_value,
                        &dominating_value,
                        error)) {
                    free(pending_insert_input_indices);
                    free(phi_inputs);
                    return 0;
                }
                if (has_dominating_value) {
                    input_value = dominating_value;
                    has_value = 1;
                } else {
                    if (predecessor_block->terminator.kind != VALUE_SSA_TERM_JUMP ||
                        predecessor_block->terminator.as.jump_target != block_id) {
                        free(pending_insert_input_indices);
                        free(phi_inputs);
                        return 1;
                    }

                    pending_insert_input_indices[pending_insert_count++] = input_index;
                }
            } else {
                free(pending_insert_input_indices);
                free(phi_inputs);
                return 1;
            }
        }

        phi_inputs[input_index].predecessor_block_id = predecessor_block_id;
        phi_inputs[input_index].value = input_value;
        ++input_index;
    }

    if (input_index != predecessor_count) {
        free(pending_insert_input_indices);
        free(phi_inputs);
        return 1;
    }

    if (pending_insert_count > 0) {
        size_t pending_index;

        for (pending_index = 0; pending_index < pending_insert_count; ++pending_index) {
            ValueSsaInstruction inserted_load;
            size_t inserted_value_id;
            size_t pending_input_index = pending_insert_input_indices[pending_index];
            size_t pending_predecessor_block_id = phi_inputs[pending_input_index].predecessor_block_id;
            inserted_value_id = value_ssa_function_allocate_value(value_function);
            if (inserted_value_id == (size_t)-1) {
                memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-032: failed to allocate value for PRE load");
                free(pending_insert_input_indices);
                free(phi_inputs);
                return 0;
            }

            memset(&inserted_load, 0, sizeof(inserted_load));
            inserted_load.kind = instruction->kind;
            inserted_load.has_result = 1;
            inserted_load.result = value_ssa_value_id(inserted_value_id);
            inserted_load.as.load_slot = instruction->as.load_slot;
            phi_inputs[pending_input_index].value = value_ssa_value_id(inserted_value_id);

            if (!memory_ssa_pass_append_instruction_ignoring_terminator(
                    &value_function->blocks[pending_predecessor_block_id], &inserted_load, error)) {
                free(pending_insert_input_indices);
                free(phi_inputs);
                return 0;
            }
        }

        *out_restart_required = 1;
    }

    free(pending_insert_input_indices);

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
    size_t iteration_limit;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "%s", invalid_error_code ? invalid_error_code : "MEMORY-SSA-PASS");
        return 0;
    }

    iteration_limit = memory_ssa_pass_load_forward_iteration_limit(program);
    for (iteration = 0; iteration < iteration_limit; ++iteration) {
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
        "MEMORY-SSA-PASS-051: load forwarding did not converge within %zu iterations",
        iteration_limit);
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
