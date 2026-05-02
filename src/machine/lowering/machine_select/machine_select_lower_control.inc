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

static int machine_select_try_lower_materialized_boolean_branch(const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block) {
    const MachineIrInstruction *tail_op;
    long long condition_value;

    if (!src_block || !dest_block ||
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

static int machine_select_try_lower_compare_branch(const MachineIrBasicBlock *src_block,
    MachineSelectBasicBlock *dest_block) {
    const MachineIrInstruction *tail_op;
    long long folded_value;

    if (!src_block || !dest_block ||
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

    if (machine_select_try_eval_binary_immediate(
            tail_op->as.binary.op,
            tail_op->as.binary.lhs,
            tail_op->as.binary.rhs,
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
            tail_op->as.binary.op,
            tail_op->as.binary.lhs,
            tail_op->as.binary.rhs)
        ? MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM
        : MACHINE_SELECT_TERM_COMPARE_BRANCH;
    dest_block->terminator.as.compare_branch.op = tail_op->as.binary.op;
    dest_block->terminator.as.compare_branch.lhs = machine_select_lower_operand(tail_op->as.binary.lhs);
    dest_block->terminator.as.compare_branch.rhs = machine_select_lower_operand(tail_op->as.binary.rhs);
    dest_block->terminator.as.compare_branch.then_target = src_block->terminator.as.branch.then_target;
    dest_block->terminator.as.compare_branch.else_target = src_block->terminator.as.branch.else_target;
    dest_block->has_terminator = 1;
    return 1;
}

static int machine_select_lower_terminator(const MachineIrBasicBlock *src_block,
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
            if (return_value.kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
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
                machine_select_try_lower_materialized_boolean_branch(src_block, dest_block) ||
                machine_select_try_lower_compare_branch(src_block, dest_block)) {
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
