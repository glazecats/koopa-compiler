#if defined(__INTELLISENSE__) && !defined(MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MEMORY_SSA_PASS_SPLIT_AGGREGATOR)
#define MEMORY_SSA_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "memory_ssa_pass.c"
#else

static int memory_ssa_pass_store_matches_previous_loaded_value(const ValueSsaFunction *value_function,
    const MemorySsaFunction *memory_function,
    const MemorySsaDefUseAnalysis *memory_def_use,
    const ValueSsaDefUseAnalysis *value_def_use,
    size_t block_index,
    size_t instruction_index,
    int *out_matches,
    ValueSsaError *error) {
    const ValueSsaInstruction *store_instruction;
    const MemorySsaInstruction *memory_store_instruction;
    unsigned char *visit_state = NULL;
    int ok = 1;

    if (!value_function || !memory_function || !memory_def_use || !value_def_use || !out_matches) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-026: invalid redundant-store load-match contract");
        return 0;
    }

    *out_matches = 0;
    if (block_index >= value_function->block_count || instruction_index >= value_function->blocks[block_index].instruction_count ||
        block_index >= memory_function->block_count ||
        instruction_index >= memory_function->blocks[block_index].instruction_count) {
        memory_ssa_pass_set_error(
            error, 0, 0, "MEMORY-SSA-PASS-026: invalid redundant-store load-match site");
        return 0;
    }

    store_instruction = &value_function->blocks[block_index].instructions[instruction_index];
    memory_store_instruction = &memory_function->blocks[block_index].instructions[instruction_index];
    if ((store_instruction->kind != VALUE_SSA_INSTR_STORE_LOCAL &&
            store_instruction->kind != VALUE_SSA_INSTR_STORE_GLOBAL) ||
        memory_store_instruction->access_count == 0 || !memory_store_instruction->accesses[0].has_use_version ||
        store_instruction->as.store.value.kind != VALUE_SSA_VALUE_ID) {
        return 1;
    }

    visit_state = (unsigned char *)calloc(value_def_use->value_count, sizeof(unsigned char));
    if (!visit_state) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            "MEMORY-SSA-PASS-027: out of memory while matching redundant store to prior load");
        return 0;
    }

    ok = memory_ssa_pass_value_matches_slot_version(value_function,
        memory_function,
        memory_def_use,
        value_def_use,
        store_instruction->as.store.value,
        store_instruction->as.store.slot,
        memory_store_instruction->accesses[0].use_version_id,
        visit_state,
        out_matches,
        error);
    free(visit_state);
    return ok;
}

int memory_ssa_pass_eliminate_redundant_stores(ValueSsaProgram *program, ValueSsaError *error) {
    MemorySsaProgram memory_program;
    MemorySsaError memory_error;
    size_t function_index;

    if (!program) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-010: invalid redundant-store contract");
        return 0;
    }

    memory_ssa_program_init(&memory_program);
    if (!memory_ssa_build_from_value_ssa(program, &memory_program, &memory_error)) {
        memory_ssa_pass_copy_memory_error(error, &memory_error);
        memory_ssa_program_free(&memory_program);
        return 0;
    }
    if (memory_program.function_count != program->function_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-005: function-count mismatch after memory_ssa build");
        memory_ssa_program_free(&memory_program);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *value_function = &program->functions[function_index];
        const MemorySsaFunction *memory_function = &memory_program.functions[function_index];
        MemorySsaDefUseAnalysis def_use;
        ValueSsaDefUseAnalysis value_def_use;
        unsigned char **remove_flags = NULL;
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
        if (!memory_ssa_compute_def_use_analysis(memory_function, &def_use, &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            memory_ssa_def_use_analysis_free(&def_use);
            value_ssa_def_use_analysis_free(&value_def_use);
            memory_ssa_program_free(&memory_program);
            return 0;
        }
        if (!value_ssa_compute_def_use_analysis(value_function, &value_def_use, error)) {
            memory_ssa_def_use_analysis_free(&def_use);
            value_ssa_def_use_analysis_free(&value_def_use);
            memory_ssa_program_free(&memory_program);
            return 0;
        }

        remove_flags = (unsigned char **)calloc(value_function->block_count, sizeof(unsigned char *));
        if (!remove_flags) {
            memory_ssa_pass_set_error(error,
                0,
                0,
                "MEMORY-SSA-PASS-011: out of memory while preparing redundant-store removal in function '%s'",
                value_function->name ? value_function->name : "<anon>");
            memory_ssa_def_use_analysis_free(&def_use);
            memory_ssa_program_free(&memory_program);
            return 0;
        }

        for (block_index = 0; block_index < value_function->block_count && ok; ++block_index) {
            ValueSsaBasicBlock *value_block = &value_function->blocks[block_index];
            size_t instruction_index;

            if (value_block->instruction_count == 0) {
                continue;
            }

            remove_flags[block_index] = (unsigned char *)calloc(value_block->instruction_count, sizeof(unsigned char));
            if (!remove_flags[block_index]) {
                memory_ssa_pass_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-PASS-011: out of memory while preparing redundant-store removal in function '%s'",
                    value_function->name ? value_function->name : "<anon>");
                ok = 0;
                break;
            }

            for (instruction_index = 0; instruction_index < value_block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &value_block->instructions[instruction_index];
                int has_previous_value = 0;
                ValueSsaValueRef previous_value;

                if (instruction->kind != VALUE_SSA_INSTR_STORE_LOCAL &&
                    instruction->kind != VALUE_SSA_INSTR_STORE_GLOBAL) {
                    continue;
                }

                ok = memory_ssa_resolve_store_previous_value(memory_function,
                    &def_use,
                    block_index,
                    instruction_index,
                    &has_previous_value,
                    &previous_value,
                    &memory_error);
                if (!ok) {
                    memory_ssa_pass_copy_memory_error(error, &memory_error);
                    break;
                }
                if (!has_previous_value) {
                    int matches_previous_load = 0;

                    ok = memory_ssa_pass_store_matches_previous_loaded_value(value_function,
                        memory_function,
                        &def_use,
                        &value_def_use,
                        block_index,
                        instruction_index,
                        &matches_previous_load,
                        error);
                    if (!ok) {
                        break;
                    }
                    if (matches_previous_load) {
                        remove_flags[block_index][instruction_index] = 1;
                    }
                    continue;
                }
                if (memory_ssa_pass_value_refs_equal(previous_value, instruction->as.store.value)) {
                    remove_flags[block_index][instruction_index] = 1;
                }
            }
        }

        if (ok) {
            for (block_index = 0; block_index < value_function->block_count; ++block_index) {
                if (!remove_flags[block_index]) {
                    continue;
                }
                if (!memory_ssa_pass_compact_removed_instructions(&value_function->blocks[block_index],
                        remove_flags[block_index])) {
                    memory_ssa_pass_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-PASS-012: failed to compact redundant-store results");
                    ok = 0;
                    break;
                }
            }
        }

        for (block_index = 0; block_index < value_function->block_count; ++block_index) {
            free(remove_flags[block_index]);
        }
        free(remove_flags);
        memory_ssa_def_use_analysis_free(&def_use);
        value_ssa_def_use_analysis_free(&value_def_use);
        if (!ok) {
            memory_ssa_program_free(&memory_program);
            return 0;
        }
    }

    memory_ssa_program_free(&memory_program);
    return value_ssa_verify_program(program, error);
}

static int memory_ssa_pass_eliminate_dead_stores_impl(ValueSsaProgram *program,
    int include_globals,
    ValueSsaError *error) {
    MemorySsaProgram memory_program;
    MemorySsaError memory_error;
    size_t function_index;

    if (!program) {
        memory_ssa_pass_set_error(error,
            0,
            0,
            include_globals ? "MEMORY-SSA-PASS-013: invalid dead-store contract"
                            : "MEMORY-SSA-PASS-006: invalid dead-local-store contract");
        return 0;
    }

    memory_ssa_program_init(&memory_program);
    if (!memory_ssa_build_from_value_ssa(program, &memory_program, &memory_error)) {
        memory_ssa_pass_copy_memory_error(error, &memory_error);
        memory_ssa_program_free(&memory_program);
        return 0;
    }
    if (memory_program.function_count != program->function_count) {
        memory_ssa_pass_set_error(error, 0, 0, "MEMORY-SSA-PASS-005: function-count mismatch after memory_ssa build");
        memory_ssa_program_free(&memory_program);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *value_function = &program->functions[function_index];
        const MemorySsaFunction *memory_function = &memory_program.functions[function_index];
        MemorySsaDefUseAnalysis def_use;
        unsigned char **remove_flags = NULL;
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
        if (!memory_ssa_compute_def_use_analysis(memory_function, &def_use, &memory_error)) {
            memory_ssa_pass_copy_memory_error(error, &memory_error);
            memory_ssa_def_use_analysis_free(&def_use);
            memory_ssa_program_free(&memory_program);
            return 0;
        }

        remove_flags = (unsigned char **)calloc(value_function->block_count, sizeof(unsigned char *));
        if (!remove_flags) {
            memory_ssa_pass_set_error(error,
                0,
                0,
                "MEMORY-SSA-PASS-007: out of memory while preparing dead-local-store removal in function '%s'",
                value_function->name ? value_function->name : "<anon>");
            memory_ssa_def_use_analysis_free(&def_use);
            memory_ssa_program_free(&memory_program);
            return 0;
        }

        for (block_index = 0; block_index < value_function->block_count && ok; ++block_index) {
            ValueSsaBasicBlock *value_block = &value_function->blocks[block_index];
            size_t instruction_index;

            if (value_block->instruction_count == 0) {
                continue;
            }

            remove_flags[block_index] = (unsigned char *)calloc(value_block->instruction_count, sizeof(unsigned char));
            if (!remove_flags[block_index]) {
                memory_ssa_pass_set_error(error,
                    0,
                    0,
                    "MEMORY-SSA-PASS-007: out of memory while preparing dead-local-store removal in function '%s'",
                    value_function->name ? value_function->name : "<anon>");
                ok = 0;
                break;
            }

            for (instruction_index = 0; instruction_index < value_block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &value_block->instructions[instruction_index];
                int is_dead = 0;

                if (instruction->kind != VALUE_SSA_INSTR_STORE_LOCAL &&
                    instruction->kind != VALUE_SSA_INSTR_STORE_GLOBAL) {
                    continue;
                }
                if (!include_globals && instruction->kind == VALUE_SSA_INSTR_STORE_GLOBAL) {
                    continue;
                }

                ok = memory_ssa_is_trivially_dead_store(
                    memory_function, &def_use, block_index, instruction_index, &is_dead, &memory_error);
                if (!ok) {
                    memory_ssa_pass_copy_memory_error(error, &memory_error);
                    break;
                }
                if (is_dead) {
                    remove_flags[block_index][instruction_index] = 1;
                }
            }
        }

        if (ok) {
            for (block_index = 0; block_index < value_function->block_count; ++block_index) {
                if (!remove_flags[block_index]) {
                    continue;
                }
                if (!memory_ssa_pass_compact_removed_instructions(&value_function->blocks[block_index],
                        remove_flags[block_index])) {
                    memory_ssa_pass_set_error(error,
                        0,
                        0,
                        "MEMORY-SSA-PASS-008: failed to compact dead-local-store results");
                    ok = 0;
                    break;
                }
            }
        }

        for (block_index = 0; block_index < value_function->block_count; ++block_index) {
            free(remove_flags[block_index]);
        }
        free(remove_flags);
        memory_ssa_def_use_analysis_free(&def_use);
        if (!ok) {
            memory_ssa_program_free(&memory_program);
            return 0;
        }
    }

    memory_ssa_program_free(&memory_program);
    return value_ssa_verify_program(program, error);
}

int memory_ssa_pass_eliminate_dead_stores(ValueSsaProgram *program, ValueSsaError *error) {
    return memory_ssa_pass_eliminate_dead_stores_impl(program, 1, error);
}

int memory_ssa_pass_eliminate_dead_local_stores(ValueSsaProgram *program, ValueSsaError *error) {
    return memory_ssa_pass_eliminate_dead_stores_impl(program, 0, error);
}

#endif
