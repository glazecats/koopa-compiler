#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_sweep_dead_defs(ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    int *out_changed,
    ValueSsaError *error) {
    size_t block_index;

    if (!function || !analysis || !out_changed) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-130: invalid dead-def cleanup contract");
        return 0;
    }

    *out_changed = 0;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_read_index;
        size_t phi_write_index = 0;
        size_t instruction_read_index;
        size_t instruction_write_index = 0;

        for (phi_read_index = 0; phi_read_index < block->phi_count; ++phi_read_index) {
            ValueSsaPhi *phi = &block->phis[phi_read_index];

            if (analysis->use_counts[phi->result_id] == 0) {
                value_ssa_phi_free(phi);
                *out_changed = 1;
                continue;
            }

            if (phi_write_index != phi_read_index) {
                block->phis[phi_write_index] = block->phis[phi_read_index];
            }
            ++phi_write_index;
        }
        block->phi_count = phi_write_index;

        for (instruction_read_index = 0; instruction_read_index < block->instruction_count; ++instruction_read_index) {
            ValueSsaInstruction *instruction = &block->instructions[instruction_read_index];

            if (value_ssa_instruction_is_removable_dead_def(instruction) &&
                analysis->use_counts[instruction->result.value_id] == 0) {
                value_ssa_instruction_free(instruction);
                *out_changed = 1;
                continue;
            }

            if (instruction_write_index != instruction_read_index) {
                block->instructions[instruction_write_index] = block->instructions[instruction_read_index];
            }
            ++instruction_write_index;
        }
        block->instruction_count = instruction_write_index;
    }

    return 1;
}

int value_ssa_eliminate_dead_value_defs(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-130: invalid dead-def cleanup contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        ValueSsaDefUseAnalysis analysis;
        int changed = 0;

        if (!function->has_body || function->next_value_id == 0) {
            continue;
        }

        value_ssa_def_use_analysis_init(&analysis);
        do {
            changed = 0;
            value_ssa_def_use_analysis_free(&analysis);
            if (!value_ssa_compute_def_use_analysis(function, &analysis, error) ||
                !value_ssa_sweep_dead_defs(function, &analysis, &changed, error)) {
                value_ssa_def_use_analysis_free(&analysis);
                return 0;
            }
        } while (changed);
        value_ssa_def_use_analysis_free(&analysis);

        if (!value_ssa_compact_value_ids(function, error)) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
