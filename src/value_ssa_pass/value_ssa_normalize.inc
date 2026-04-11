#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_binary_op_is_commutative(ValueSsaBinaryOp op) {
    switch (op) {
    case VALUE_SSA_BINARY_ADD:
    case VALUE_SSA_BINARY_MUL:
    case VALUE_SSA_BINARY_BIT_AND:
    case VALUE_SSA_BINARY_BIT_XOR:
    case VALUE_SSA_BINARY_BIT_OR:
    case VALUE_SSA_BINARY_EQ:
    case VALUE_SSA_BINARY_NE:
        return 1;
    default:
        return 0;
    }
}

static int value_ssa_binary_op_swap_operands(ValueSsaBinaryOp op, ValueSsaBinaryOp *out_swapped) {
    if (!out_swapped) {
        return 0;
    }

    switch (op) {
    case VALUE_SSA_BINARY_LT:
        *out_swapped = VALUE_SSA_BINARY_GT;
        return 1;
    case VALUE_SSA_BINARY_LE:
        *out_swapped = VALUE_SSA_BINARY_GE;
        return 1;
    case VALUE_SSA_BINARY_GT:
        *out_swapped = VALUE_SSA_BINARY_LT;
        return 1;
    case VALUE_SSA_BINARY_GE:
        *out_swapped = VALUE_SSA_BINARY_LE;
        return 1;
    default:
        if (value_ssa_binary_op_is_commutative(op)) {
            *out_swapped = op;
            return 1;
        }
        return 0;
    }
}

static int value_ssa_value_ref_is_immediate(ValueSsaValueRef value) {
    return value.kind == VALUE_SSA_VALUE_IMMEDIATE;
}

static int value_ssa_should_normalize_binary_operands(ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaBinaryOp *out_swapped_op) {
    if (!out_swapped_op) {
        return 0;
    }

    if (!value_ssa_binary_op_swap_operands(op, out_swapped_op)) {
        return 0;
    }

    if (value_ssa_value_ref_is_immediate(lhs) && !value_ssa_value_ref_is_immediate(rhs)) {
        return 1;
    }

    if (value_ssa_binary_op_is_commutative(op) &&
        lhs.kind == VALUE_SSA_VALUE_ID &&
        rhs.kind == VALUE_SSA_VALUE_ID &&
        lhs.value_id > rhs.value_id) {
        return 1;
    }

    return 0;
}

int value_ssa_normalize_binary_operands(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-209: invalid binary-normalize contract");
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
                ValueSsaBinaryOp swapped_op;
                ValueSsaValueRef old_lhs;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    !value_ssa_should_normalize_binary_operands(instruction->as.binary.op,
                        instruction->as.binary.lhs,
                        instruction->as.binary.rhs,
                        &swapped_op)) {
                    continue;
                }

                old_lhs = instruction->as.binary.lhs;
                instruction->as.binary.lhs = instruction->as.binary.rhs;
                instruction->as.binary.rhs = old_lhs;
                instruction->as.binary.op = swapped_op;
            }
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
