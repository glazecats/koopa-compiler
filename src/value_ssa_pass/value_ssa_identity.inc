#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_pass.c"
#else

static int value_ssa_try_simplify_binary_identity(ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    ValueSsaValueRef *out_replacement) {
    if (!out_replacement) {
        return 0;
    }

    switch (op) {
    case VALUE_SSA_BINARY_ADD:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 0) {
            *out_replacement = lhs;
            return 1;
        }
        if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == 0) {
            *out_replacement = rhs;
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_SUB:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 0) {
            *out_replacement = lhs;
            return 1;
        }
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = value_ssa_value_immediate(0);
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_MUL:
        if ((rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 0) ||
            (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == 0)) {
            *out_replacement = value_ssa_value_immediate(0);
            return 1;
        }
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 1) {
            *out_replacement = lhs;
            return 1;
        }
        if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == 1) {
            *out_replacement = rhs;
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_DIV:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 1) {
            *out_replacement = lhs;
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_MOD:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 1) {
            *out_replacement = value_ssa_value_immediate(0);
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_BIT_AND:
        if ((rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 0) ||
            (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == 0)) {
            *out_replacement = value_ssa_value_immediate(0);
            return 1;
        }
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == -1) {
            *out_replacement = lhs;
            return 1;
        }
        if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == -1) {
            *out_replacement = rhs;
            return 1;
        }
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = lhs;
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_BIT_XOR:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 0) {
            *out_replacement = lhs;
            return 1;
        }
        if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == 0) {
            *out_replacement = rhs;
            return 1;
        }
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = value_ssa_value_immediate(0);
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_BIT_OR:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE && rhs.immediate == 0) {
            *out_replacement = lhs;
            return 1;
        }
        if (lhs.kind == VALUE_SSA_VALUE_IMMEDIATE && lhs.immediate == 0) {
            *out_replacement = rhs;
            return 1;
        }
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = lhs;
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_EQ:
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = value_ssa_value_immediate(1);
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_NE:
    case VALUE_SSA_BINARY_LT:
    case VALUE_SSA_BINARY_GT:
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = value_ssa_value_immediate(0);
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_LE:
    case VALUE_SSA_BINARY_GE:
        if (value_ssa_value_refs_equal(lhs, rhs)) {
            *out_replacement = value_ssa_value_immediate(1);
            return 1;
        }
        return 0;
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
    default:
        return 0;
    }
}

int value_ssa_simplify_algebraic_identities(ValueSsaProgram *program, ValueSsaError *error) {
    size_t function_index;

    if (!program) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-208: invalid identity-simplify contract");
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
                ValueSsaValueRef replacement;

                if (instruction->kind != VALUE_SSA_INSTR_BINARY ||
                    !value_ssa_try_simplify_binary_identity(instruction->as.binary.op,
                        instruction->as.binary.lhs,
                        instruction->as.binary.rhs,
                        &replacement)) {
                    continue;
                }

                instruction->kind = VALUE_SSA_INSTR_MOV;
                instruction->as.mov_value = replacement;
            }
        }
    }

    return value_ssa_verify_program(program, error);
}

#endif
