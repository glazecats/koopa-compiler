#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_try_fold_binary_immediates(ValueSsaBinaryOp op,
    long long lhs,
    long long rhs,
    long long *out_value) {
    long long result = 0;

    if (!out_value) {
        return 0;
    }

    switch (op) {
    case VALUE_SSA_BINARY_ADD:
        if (__builtin_add_overflow(lhs, rhs, &result)) {
            return 0;
        }
        break;
    case VALUE_SSA_BINARY_SUB:
        if (__builtin_sub_overflow(lhs, rhs, &result)) {
            return 0;
        }
        break;
    case VALUE_SSA_BINARY_MUL:
        if (__builtin_mul_overflow(lhs, rhs, &result)) {
            return 0;
        }
        break;
    case VALUE_SSA_BINARY_BIT_AND:
        result = lhs & rhs;
        break;
    case VALUE_SSA_BINARY_BIT_XOR:
        result = lhs ^ rhs;
        break;
    case VALUE_SSA_BINARY_BIT_OR:
        result = lhs | rhs;
        break;
    case VALUE_SSA_BINARY_EQ:
        result = lhs == rhs ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_NE:
        result = lhs != rhs ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_LT:
        result = lhs < rhs ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_LE:
        result = lhs <= rhs ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_GT:
        result = lhs > rhs ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_GE:
        result = lhs >= rhs ? 1 : 0;
        break;
    case VALUE_SSA_BINARY_DIV:
    case VALUE_SSA_BINARY_MOD:
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
    default:
        return 0;
    }

    *out_value = result;
    return 1;
}

int value_ssa_fold_constants(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-207: invalid constant-fold contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            ValueSsaBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                ValueSsaInstruction *instruction = &block->instructions[instruction_index];
                long long folded_value;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    instruction->as.binary.lhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
                    instruction->as.binary.rhs.kind != VALUE_SSA_VALUE_IMMEDIATE ||
                    !value_ssa_try_fold_binary_immediates(instruction->as.binary.op,
                        instruction->as.binary.lhs.immediate,
                        instruction->as.binary.rhs.immediate,
                        &folded_value)) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = value_ssa_value_immediate(folded_value);
            }
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
