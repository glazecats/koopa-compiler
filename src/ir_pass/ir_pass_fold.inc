#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <limits.h>
#endif

static int ir_pass_try_fold_binary_immediate(IrBinaryOp op,
    long long lhs,
    long long rhs,
    long long *out_value) {
    const int shift_width = (int)(sizeof(long long) * 8u);

    if (!out_value) {
        return 0;
    }

    switch (op) {
    case IR_BINARY_ADD:
        return !__builtin_add_overflow(lhs, rhs, out_value);
    case IR_BINARY_SUB:
        return !__builtin_sub_overflow(lhs, rhs, out_value);
    case IR_BINARY_MUL:
        return !__builtin_mul_overflow(lhs, rhs, out_value);
    case IR_BINARY_DIV:
        if (rhs == 0) {
            return 0;
        }
        if (lhs == LLONG_MIN && rhs == -1) {
            return 0;
        }
        *out_value = lhs / rhs;
        return 1;
    case IR_BINARY_MOD:
        if (rhs == 0) {
            return 0;
        }
        if (lhs == LLONG_MIN && rhs == -1) {
            return 0;
        }
        *out_value = lhs % rhs;
        return 1;
    case IR_BINARY_BIT_AND:
        *out_value = lhs & rhs;
        return 1;
    case IR_BINARY_BIT_XOR:
        *out_value = lhs ^ rhs;
        return 1;
    case IR_BINARY_BIT_OR:
        *out_value = lhs | rhs;
        return 1;
    case IR_BINARY_SHIFT_LEFT:
        if (rhs < 0 || rhs >= shift_width || lhs < 0) {
            return 0;
        }
        if ((unsigned long long)lhs > ((unsigned long long)LLONG_MAX >> (unsigned long long)rhs)) {
            return 0;
        }
        *out_value = lhs << rhs;
        return 1;
    case IR_BINARY_SHIFT_RIGHT:
        if (rhs < 0 || rhs >= shift_width || lhs < 0) {
            return 0;
        }
        *out_value = lhs >> rhs;
        return 1;
    case IR_BINARY_EQ:
        *out_value = (lhs == rhs) ? 1LL : 0LL;
        return 1;
    case IR_BINARY_NE:
        *out_value = (lhs != rhs) ? 1LL : 0LL;
        return 1;
    case IR_BINARY_LT:
        *out_value = (lhs < rhs) ? 1LL : 0LL;
        return 1;
    case IR_BINARY_LE:
        *out_value = (lhs <= rhs) ? 1LL : 0LL;
        return 1;
    case IR_BINARY_GT:
        *out_value = (lhs > rhs) ? 1LL : 0LL;
        return 1;
    case IR_BINARY_GE:
        *out_value = (lhs >= rhs) ? 1LL : 0LL;
        return 1;
    default:
        return 0;
    }
}

static int ir_pass_is_immediate_value(IrValueRef value, long long expected) {
    return value.kind == IR_VALUE_IMMEDIATE && value.immediate == expected;
}

static int ir_pass_try_simplify_binary_identity(IrBinaryOp op,
    IrValueRef lhs,
    IrValueRef rhs,
    IrValueRef *out_value) {
    if (!out_value) {
        return 0;
    }

    switch (op) {
    case IR_BINARY_ADD:
        if (ir_pass_is_immediate_value(lhs, 0)) {
            *out_value = rhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_SUB:
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = ir_pass_value_immediate(0);
            return 1;
        }
        if (ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_MUL:
        if (ir_pass_is_immediate_value(lhs, 0) || ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = ir_pass_value_immediate(0);
            return 1;
        }
        if (ir_pass_is_immediate_value(lhs, 1)) {
            *out_value = rhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(rhs, 1)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_DIV:
        if (ir_pass_is_immediate_value(rhs, 1)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_MOD:
        if (ir_pass_is_immediate_value(rhs, 1)) {
            *out_value = ir_pass_value_immediate(0);
            return 1;
        }
        return 0;
    case IR_BINARY_BIT_AND:
        if (ir_pass_is_immediate_value(lhs, 0) || ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = ir_pass_value_immediate(0);
            return 1;
        }
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = lhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(lhs, -1)) {
            *out_value = rhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(rhs, -1)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_BIT_XOR:
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = ir_pass_value_immediate(0);
            return 1;
        }
        if (ir_pass_is_immediate_value(lhs, 0)) {
            *out_value = rhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_BIT_OR:
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = lhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(lhs, 0)) {
            *out_value = rhs;
            return 1;
        }
        if (ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_SHIFT_LEFT:
    case IR_BINARY_SHIFT_RIGHT:
        if (ir_pass_is_immediate_value(rhs, 0)) {
            *out_value = lhs;
            return 1;
        }
        return 0;
    case IR_BINARY_EQ:
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = ir_pass_value_immediate(1);
            return 1;
        }
        return 0;
    case IR_BINARY_NE:
    case IR_BINARY_LT:
    case IR_BINARY_GT:
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = ir_pass_value_immediate(0);
            return 1;
        }
        return 0;
    case IR_BINARY_LE:
    case IR_BINARY_GE:
        if (ir_pass_value_ref_equals(lhs, rhs)) {
            *out_value = ir_pass_value_immediate(1);
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

int ir_pass_fold_immediate_binary(IrProgram *program, IrError *error) {
    size_t function_index;

    if (!program) {
        ir_pass_set_error(error, 0, 0, "IR-PASS-005: null program for fold-immediate-binary");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        IrFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->has_body) {
            continue;
        }

        if (function->block_count > 0 && !function->blocks) {
            ir_pass_set_error(error,
                0,
                0,
                "IR-PASS-006: malformed function '%s' block table",
                function->name ? function->name : "<unnamed>");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            IrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                IrInstruction *instruction = &block->instructions[instruction_index];
                IrValueRef simplified_value;
                long long folded_value;

                if (instruction->kind != IR_INSTR_BINARY) {
                    continue;
                }

                if (instruction->as.binary.lhs.kind == IR_VALUE_IMMEDIATE
                    && instruction->as.binary.rhs.kind == IR_VALUE_IMMEDIATE) {
                    if (!ir_pass_try_fold_binary_immediate(instruction->as.binary.op,
                            instruction->as.binary.lhs.immediate,
                            instruction->as.binary.rhs.immediate,
                            &folded_value)) {
                        continue;
                    }

                    instruction->kind = IR_INSTR_MOV;
                    instruction->as.mov_value = ir_pass_value_immediate(folded_value);
                    continue;
                }

                if (!ir_pass_try_simplify_binary_identity(instruction->as.binary.op,
                        instruction->as.binary.lhs,
                        instruction->as.binary.rhs,
                        &simplified_value)) {
                    continue;
                }

                instruction->kind = IR_INSTR_MOV;
                instruction->as.mov_value = simplified_value;
            }
        }
    }

    return 1;
}

#endif
