#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_try_lower_constant_branch(const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block) {
    long long condition_value;

    if (!src_block || !dest_block ||
        !src_block->has_terminator ||
        src_block->terminator.kind != MACHINE_IR_TERM_BRANCH ||
        src_block->terminator.as.branch.condition.kind != MACHINE_IR_OPERAND_IMMEDIATE) {
        return 0;
    }

    condition_value = src_block->terminator.as.branch.condition.immediate;
    dest_block->terminator.kind = MACHINE_SELECT_TERM_JUMP;
    dest_block->terminator.as.jump_target = condition_value != 0
        ? src_block->terminator.as.branch.then_target
        : src_block->terminator.as.branch.else_target;
    dest_block->has_terminator = 1;
    return 1;
}

static int machine_select_machine_ir_operand_matches_result(MachineIrOperand operand, MachineIrOperand result) {
    if (operand.kind != result.kind) {
        return 0;
    }
    switch (operand.kind) {
        case MACHINE_IR_OPERAND_REGISTER:
            return operand.machine_register_id == result.machine_register_id;
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            return operand.spill_slot == result.spill_slot;
        default:
            return 0;
    }
}

static int machine_select_machine_ir_instruction_uses_result(const MachineIrInstruction *instruction,
    MachineIrOperand result) {
    size_t arg_index;

    if (!instruction) {
        return 0;
    }
    switch (instruction->kind) {
        case MACHINE_IR_INSTR_MOV:
            return machine_select_machine_ir_operand_matches_result(instruction->as.mov_value, result);
        case MACHINE_IR_INSTR_BINARY:
            return machine_select_machine_ir_operand_matches_result(instruction->as.binary.lhs, result) ||
                machine_select_machine_ir_operand_matches_result(instruction->as.binary.rhs, result);
        case MACHINE_IR_INSTR_CALL:
            for (arg_index = 0; arg_index < instruction->as.call.arg_count; ++arg_index) {
                if (machine_select_machine_ir_operand_matches_result(instruction->as.call.args[arg_index], result)) {
                    return 1;
                }
            }
            return 0;
        case MACHINE_IR_INSTR_STORE_LOCAL:
        case MACHINE_IR_INSTR_STORE_GLOBAL:
            return machine_select_machine_ir_operand_matches_result(instruction->as.store.value, result);
        default:
            return 0;
    }
}

static int machine_select_machine_ir_terminator_uses_result(const MachineIrTerminator *terminator,
    MachineIrOperand result) {
    if (!terminator) {
        return 0;
    }
    switch (terminator->kind) {
        case MACHINE_IR_TERM_RETURN:
            return machine_select_machine_ir_operand_matches_result(terminator->as.return_value, result);
        case MACHINE_IR_TERM_BRANCH:
            return machine_select_machine_ir_operand_matches_result(terminator->as.branch.condition, result);
        default:
            return 0;
    }
}

static int machine_select_machine_ir_op_redefines_result(const MachineIrInstruction *instruction,
    MachineIrOperand result) {
    if (!instruction || !instruction->has_result) {
        return 0;
    }
    return machine_select_machine_ir_operand_matches_result(instruction->result, result);
}

static int machine_select_machine_ir_block_result_live_out(const MachineIrFunction *function,
    size_t block_id,
    MachineIrOperand result) {
    unsigned char *visited;
    size_t *stack;
    size_t stack_size = 0;
    size_t block_count;
    size_t successor_index;
    const MachineIrBasicBlock *source_block;

    if (!function || block_id >= function->block_count ||
        (result.kind != MACHINE_IR_OPERAND_REGISTER && result.kind != MACHINE_IR_OPERAND_SPILL_SLOT)) {
        return 0;
    }

    block_count = function->block_count;
    visited = block_count > 0 ? (unsigned char *)calloc(block_count, sizeof(unsigned char)) : NULL;
    stack = block_count > 0 ? (size_t *)malloc(block_count * sizeof(size_t)) : NULL;
    if ((block_count > 0 && !visited) || (block_count > 0 && !stack)) {
        free(stack);
        free(visited);
        return 1;
    }

    source_block = &function->blocks[block_id];
    switch (source_block->terminator.kind) {
        case MACHINE_IR_TERM_JUMP:
            if (source_block->terminator.as.jump_target < block_count) {
                stack[stack_size++] = source_block->terminator.as.jump_target;
                visited[source_block->terminator.as.jump_target] = 1u;
            }
            break;
        case MACHINE_IR_TERM_BRANCH:
            if (source_block->terminator.as.branch.then_target < block_count) {
                stack[stack_size++] = source_block->terminator.as.branch.then_target;
                visited[source_block->terminator.as.branch.then_target] = 1u;
            }
            if (source_block->terminator.as.branch.else_target < block_count &&
                !visited[source_block->terminator.as.branch.else_target]) {
                stack[stack_size++] = source_block->terminator.as.branch.else_target;
                visited[source_block->terminator.as.branch.else_target] = 1u;
            }
            break;
        default:
            break;
    }

    while (stack_size > 0) {
        const MachineIrBasicBlock *block = &function->blocks[stack[--stack_size]];
        int blocked = 0;
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const MachineIrInstruction *instruction = &block->instructions[instruction_index];

            if (machine_select_machine_ir_instruction_uses_result(instruction, result)) {
                free(stack);
                free(visited);
                return 1;
            }
            if (machine_select_machine_ir_op_redefines_result(instruction, result)) {
                blocked = 1;
                break;
            }
        }
        if (!blocked && machine_select_machine_ir_terminator_uses_result(&block->terminator, result)) {
            free(stack);
            free(visited);
            return 1;
        }
        if (blocked) {
            continue;
        }

        switch (block->terminator.kind) {
            case MACHINE_IR_TERM_JUMP:
                successor_index = block->terminator.as.jump_target;
                if (successor_index < block_count && !visited[successor_index]) {
                    visited[successor_index] = 1u;
                    stack[stack_size++] = successor_index;
                }
                break;
            case MACHINE_IR_TERM_BRANCH:
                successor_index = block->terminator.as.branch.then_target;
                if (successor_index < block_count && !visited[successor_index]) {
                    visited[successor_index] = 1u;
                    stack[stack_size++] = successor_index;
                }
                successor_index = block->terminator.as.branch.else_target;
                if (successor_index < block_count && !visited[successor_index]) {
                    visited[successor_index] = 1u;
                    stack[stack_size++] = successor_index;
                }
                break;
            default:
                break;
        }
    }

    free(stack);
    free(visited);
    return 0;
}

static int machine_select_try_lower_materialized_boolean_branch(const MachineIrFunction *src_function,
    const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block) {
    const MachineIrInstruction *tail_op;
    long long condition_value;

    if (!src_function || !src_block || !dest_block ||
        !src_block->has_terminator ||
        src_block->terminator.kind != MACHINE_IR_TERM_BRANCH ||
        src_block->instruction_count == 0) {
        return 0;
    }

    tail_op = &src_block->instructions[src_block->instruction_count - 1];
    if (tail_op->kind != MACHINE_IR_INSTR_MOV ||
        !tail_op->has_result ||
        tail_op->as.mov_value.kind != MACHINE_IR_OPERAND_IMMEDIATE ||
        src_block->terminator.as.branch.condition.kind != MACHINE_IR_OPERAND_REGISTER ||
        tail_op->result.kind != MACHINE_IR_OPERAND_REGISTER ||
        src_block->terminator.as.branch.condition.machine_register_id != tail_op->result.machine_register_id) {
        return 0;
    }
    if (machine_select_machine_ir_block_result_live_out(src_function, src_block->id, tail_op->result)) {
        return 0;
    }

    condition_value = tail_op->as.mov_value.immediate;
    machine_select_op_free(&dest_block->ops[dest_block->op_count - 1]);
    dest_block->op_count -= 1;
    if (dest_block->op_count == 0) {
        free(dest_block->ops);
        dest_block->ops = NULL;
        dest_block->op_capacity = 0;
    }

    dest_block->terminator.kind = MACHINE_SELECT_TERM_JUMP;
    dest_block->terminator.as.jump_target = condition_value != 0
        ? src_block->terminator.as.branch.then_target
        : src_block->terminator.as.branch.else_target;
    dest_block->has_terminator = 1;
    return 1;
}

static int machine_select_try_lower_compare_branch(const MachineIrFunction *src_function,
    const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block) {
    const MachineIrInstruction *tail_op;
    long long folded_value;
    MachineIrBinaryOp compare_op;
    MachineIrOperand lhs;
    MachineIrOperand rhs;

    if (!src_function || !src_block || !dest_block ||
        !src_block->has_terminator ||
        src_block->terminator.kind != MACHINE_IR_TERM_BRANCH ||
        src_block->instruction_count == 0) {
        return 0;
    }

    tail_op = &src_block->instructions[src_block->instruction_count - 1];
    if (tail_op->kind != MACHINE_IR_INSTR_BINARY ||
        !tail_op->has_result ||
        !machine_select_binary_is_compare(tail_op->as.binary.op) ||
        src_block->terminator.as.branch.condition.kind != MACHINE_IR_OPERAND_REGISTER ||
        tail_op->result.kind != MACHINE_IR_OPERAND_REGISTER ||
        src_block->terminator.as.branch.condition.machine_register_id != tail_op->result.machine_register_id) {
        return 0;
    }
    if (machine_select_machine_ir_block_result_live_out(src_function, src_block->id, tail_op->result)) {
        return 0;
    }

    compare_op = tail_op->as.binary.op;
    lhs = tail_op->as.binary.lhs;
    rhs = tail_op->as.binary.rhs;
    machine_select_canonicalize_binary_immediate_side(&compare_op, &lhs, &rhs);

    if (machine_select_try_eval_binary_immediate(
            compare_op,
            lhs,
            rhs,
            &folded_value)) {
        machine_select_op_free(&dest_block->ops[dest_block->op_count - 1]);
        dest_block->op_count -= 1;
        if (dest_block->op_count == 0) {
            free(dest_block->ops);
            dest_block->ops = NULL;
            dest_block->op_capacity = 0;
        }
        dest_block->terminator.kind = MACHINE_SELECT_TERM_JUMP;
        dest_block->terminator.as.jump_target = folded_value != 0
            ? src_block->terminator.as.branch.then_target
            : src_block->terminator.as.branch.else_target;
        dest_block->has_terminator = 1;
        return 1;
    }

    machine_select_op_free(&dest_block->ops[dest_block->op_count - 1]);
    dest_block->op_count -= 1;
    if (dest_block->op_count == 0) {
        free(dest_block->ops);
        dest_block->ops = NULL;
        dest_block->op_capacity = 0;
    }

    dest_block->terminator.kind =
        machine_select_binary_uses_immediate_form(
            compare_op,
            lhs,
            rhs)
        ? MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM
        : MACHINE_SELECT_TERM_COMPARE_BRANCH;
    dest_block->terminator.as.compare_branch.op = compare_op;
    dest_block->terminator.as.compare_branch.lhs = machine_select_lower_operand(lhs);
    dest_block->terminator.as.compare_branch.rhs = machine_select_lower_operand(rhs);
    dest_block->terminator.as.compare_branch.then_target = src_block->terminator.as.branch.then_target;
    dest_block->terminator.as.compare_branch.else_target = src_block->terminator.as.branch.else_target;
    dest_block->has_terminator = 1;
    return 1;
}

static int machine_select_lower_terminator(const MachineIrFunction *src_function,
    const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block,
    MachineSelectError *error) {
    MachineSelectOperand return_value;

    if (!src_block || !dest_block) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-314: invalid terminator lowering contract");
        return 0;
    }

    switch (src_block->terminator.kind) {
        case MACHINE_IR_TERM_RETURN:
            return_value = machine_select_lower_operand(src_block->terminator.as.return_value);
            if (return_value.kind == MACHINE_SELECT_OPERAND_NONE) {
                dest_block->terminator.kind = MACHINE_SELECT_TERM_RETURN;
            } else if (return_value.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                dest_block->terminator.kind = MACHINE_SELECT_TERM_RETURN_IMM;
            } else if (return_value.kind == MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                dest_block->terminator.kind = MACHINE_SELECT_TERM_RETURN_SPILL;
            } else {
                dest_block->terminator.kind = MACHINE_SELECT_TERM_RETURN;
            }
            dest_block->terminator.as.return_value = return_value;
            return 1;
        case MACHINE_IR_TERM_JUMP:
            dest_block->terminator.kind = MACHINE_SELECT_TERM_JUMP;
            dest_block->terminator.as.jump_target = src_block->terminator.as.jump_target;
            return 1;
        case MACHINE_IR_TERM_BRANCH:
            if (machine_select_try_lower_constant_branch(src_block, dest_block) ||
                machine_select_try_lower_materialized_boolean_branch(src_function, src_block, dest_block) ||
                machine_select_try_lower_compare_branch(src_function, src_block, dest_block)) {
                return 1;
            }
            dest_block->terminator.kind = MACHINE_SELECT_TERM_BRANCH;
            dest_block->terminator.as.branch.condition =
                machine_select_lower_operand(src_block->terminator.as.branch.condition);
            dest_block->terminator.as.branch.then_target = src_block->terminator.as.branch.then_target;
            dest_block->terminator.as.branch.else_target = src_block->terminator.as.branch.else_target;
            return 1;
        default:
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-314: unknown machine-ir terminator kind");
            return 0;
    }
}

#endif
