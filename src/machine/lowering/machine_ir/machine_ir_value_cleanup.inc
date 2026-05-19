#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static int machine_ir_try_fold_binary_immediates(MachineIrBinaryOp op,
    long long lhs,
    long long rhs,
    long long *out_value) {
    int32_t lhs32;
    int32_t rhs32;
    long long math_result;

    if (!out_value) {
        return 0;
    }

    lhs32 = (int32_t)machine_ir_normalize_sysy_int_value(lhs);
    rhs32 = (int32_t)machine_ir_normalize_sysy_int_value(rhs);
    switch (op) {
        case MACHINE_IR_BINARY_ADD:
            math_result = (long long)lhs32 + (long long)rhs32;
            if (math_result < INT32_MIN || math_result > INT32_MAX) {
                return 0;
            }
            *out_value = machine_ir_normalize_sysy_int_value((long long)lhs32 + (long long)rhs32);
            return 1;
        case MACHINE_IR_BINARY_SUB:
            math_result = (long long)lhs32 - (long long)rhs32;
            if (math_result < INT32_MIN || math_result > INT32_MAX) {
                return 0;
            }
            *out_value = machine_ir_normalize_sysy_int_value((long long)lhs32 - (long long)rhs32);
            return 1;
        case MACHINE_IR_BINARY_MUL:
            math_result = (long long)lhs32 * (long long)rhs32;
            if (math_result < INT32_MIN || math_result > INT32_MAX) {
                return 0;
            }
            *out_value = machine_ir_normalize_sysy_int_value((long long)lhs32 * (long long)rhs32);
            return 1;
        case MACHINE_IR_BINARY_DIV:
        case MACHINE_IR_BINARY_MOD:
        case MACHINE_IR_BINARY_SHIFT_LEFT:
        case MACHINE_IR_BINARY_SHIFT_RIGHT:
            return 0;
        case MACHINE_IR_BINARY_BIT_AND:
            *out_value = machine_ir_normalize_sysy_int_value((long long)(lhs32 & rhs32));
            return 1;
        case MACHINE_IR_BINARY_BIT_XOR:
            *out_value = machine_ir_normalize_sysy_int_value((long long)(lhs32 ^ rhs32));
            return 1;
        case MACHINE_IR_BINARY_BIT_OR:
            *out_value = machine_ir_normalize_sysy_int_value((long long)(lhs32 | rhs32));
            return 1;
        case MACHINE_IR_BINARY_EQ:
            *out_value = lhs32 == rhs32 ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_NE:
            *out_value = lhs32 != rhs32 ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_LT:
            *out_value = lhs32 < rhs32 ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_LE:
            *out_value = lhs32 <= rhs32 ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_GT:
            *out_value = lhs32 > rhs32 ? 1 : 0;
            return 1;
        case MACHINE_IR_BINARY_GE:
            *out_value = lhs32 >= rhs32 ? 1 : 0;
            return 1;
        default:
            return 0;
    }
}

static void machine_ir_rewrite_binary_instruction_to_mov(MachineIrInstruction *instruction,
    MachineIrOperand mov_value) {
    if (!instruction) {
        return;
    }

    instruction->kind = MACHINE_IR_INSTR_MOV;
    instruction->as.mov_value = mov_value;
}

static int machine_ir_cleanup_machine_values(MachineIrProgram *program,
    int *out_changed,
    MachineIrError *error) {
    size_t function_index;

    if (out_changed) {
        *out_changed = 0;
    }
    if (!program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-550: invalid machine-value cleanup contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            MachineIrBasicBlock *block = &function->blocks[block_index];
            size_t instruction_index;

            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                MachineIrInstruction *instruction = &block->instructions[instruction_index];
                MachineIrOperand lhs;
                MachineIrOperand rhs;
                long long folded_value;

                if (instruction->kind != MACHINE_IR_INSTR_BINARY || !instruction->has_result) {
                    continue;
                }

                lhs = instruction->as.binary.lhs;
                rhs = instruction->as.binary.rhs;
                if (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE &&
                    rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE &&
                    machine_ir_try_fold_binary_immediates(
                        instruction->as.binary.op,
                        lhs.immediate,
                        rhs.immediate,
                        &folded_value)) {
                    machine_ir_rewrite_binary_instruction_to_mov(
                        instruction,
                        machine_ir_operand_immediate(folded_value));
                    if (out_changed) {
                        *out_changed = 1;
                    }
                    continue;
                }

                switch (instruction->as.binary.op) {
                    case MACHINE_IR_BINARY_ADD:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, rhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_SUB:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_MUL:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 1) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == 1) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, rhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if ((rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 0) ||
                            (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == 0)) {
                            machine_ir_rewrite_binary_instruction_to_mov(
                                instruction,
                                machine_ir_operand_immediate(0));
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_DIV:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 1) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_MOD:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 1) {
                            machine_ir_rewrite_binary_instruction_to_mov(
                                instruction,
                                machine_ir_operand_immediate(0));
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_BIT_AND:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == -1) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == -1) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, rhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if ((rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 0) ||
                            (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == 0)) {
                            machine_ir_rewrite_binary_instruction_to_mov(
                                instruction,
                                machine_ir_operand_immediate(0));
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_BIT_OR:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, rhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    case MACHINE_IR_BINARY_BIT_XOR:
                        if (rhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && rhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, lhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if (lhs.kind == MACHINE_IR_OPERAND_IMMEDIATE && lhs.immediate == 0) {
                            machine_ir_rewrite_binary_instruction_to_mov(instruction, rhs);
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        } else if (machine_ir_operands_equal(lhs, rhs)) {
                            machine_ir_rewrite_binary_instruction_to_mov(
                                instruction,
                                machine_ir_operand_immediate(0));
                            if (out_changed) {
                                *out_changed = 1;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }

    return 1;
}

#endif
