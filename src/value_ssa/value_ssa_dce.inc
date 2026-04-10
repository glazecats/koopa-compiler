#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

static void value_ssa_increment_use_count(ValueSsaValueRef value, size_t *use_counts) {
    if (!use_counts || value.kind != VALUE_SSA_VALUE_ID) {
        return;
    }

    ++use_counts[value.value_id];
}

static int value_ssa_compute_use_counts(const ValueSsaFunction *function,
    size_t *use_counts,
    ValueSsaError *error) {
    size_t block_index;

    if (!function || !use_counts) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-130: invalid dead-def cleanup contract");
        return 0;
    }

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t phi_index;
        size_t instruction_index;

        for (phi_index = 0; phi_index < block->phi_count; ++phi_index) {
            const ValueSsaPhi *phi = &block->phis[phi_index];
            size_t input_index;

            for (input_index = 0; input_index < phi->input_count; ++input_index) {
                value_ssa_increment_use_count(phi->inputs[input_index].value, use_counts);
            }
        }

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];
            size_t arg_index;

            switch (instruction->kind) {
            case VALUE_SSA_INSTR_MOV:
                value_ssa_increment_use_count(instruction->as.mov_value, use_counts);
                break;
            case VALUE_SSA_INSTR_BINARY:
                value_ssa_increment_use_count(instruction->as.binary.lhs, use_counts);
                value_ssa_increment_use_count(instruction->as.binary.rhs, use_counts);
                break;
            case VALUE_SSA_INSTR_CALL:
                for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                    value_ssa_increment_use_count(instruction->as.call.args[arg_index], use_counts);
                }
                break;
            case VALUE_SSA_INSTR_STORE_LOCAL:
            case VALUE_SSA_INSTR_STORE_GLOBAL:
                value_ssa_increment_use_count(instruction->as.store.value, use_counts);
                break;
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
            default:
                break;
            }
        }

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            value_ssa_increment_use_count(block->terminator.as.return_value, use_counts);
            break;
        case VALUE_SSA_TERM_BRANCH:
            value_ssa_increment_use_count(block->terminator.as.branch.condition, use_counts);
            break;
        case VALUE_SSA_TERM_JUMP:
        default:
            break;
        }
    }

    return 1;
}

static int value_ssa_sweep_dead_defs(ValueSsaFunction *function,
    const size_t *use_counts,
    int *out_changed,
    ValueSsaError *error) {
    size_t block_index;

    if (!function || !use_counts || !out_changed) {
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

            if (use_counts[phi->result_id] == 0) {
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
                use_counts[instruction->result.value_id] == 0) {
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
        size_t *use_counts = NULL;
        int changed = 0;

        if (!function->has_body || function->next_value_id == 0) {
            continue;
        }

        do {
            changed = 0;
            use_counts = (size_t *)calloc(function->next_value_id, sizeof(size_t));
            if (!use_counts) {
                value_ssa_set_error(error,
                    0,
                    0,
                    "VALUE-SSA-131: out of memory while computing dead defs for function '%s'",
                    function->name ? function->name : "<anon>");
                return 0;
            }

            if (!value_ssa_compute_use_counts(function, use_counts, error) ||
                !value_ssa_sweep_dead_defs(function, use_counts, &changed, error)) {
                free(use_counts);
                return 0;
            }

            free(use_counts);
        } while (changed);

        if (!value_ssa_compact_value_ids(function, error)) {
            return 0;
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
