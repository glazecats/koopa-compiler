#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_lower_value_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    int *out_handled) {
    long long folded_value;
    MachineIrBinaryOp binary_op;
    MachineIrOperand lhs;
    MachineIrOperand rhs;

    if (out_handled) {
        *out_handled = 1;
    }
    if (!src_op || !dest_op || !out_handled) {
        return 0;
    }

    switch (src_op->kind) {
        case MACHINE_IR_INSTR_MOV:
            dest_op->kind = src_op->as.mov_value.kind == MACHINE_IR_OPERAND_IMMEDIATE
                ? MACHINE_SELECT_OP_MATERIALIZE_IMM
                : MACHINE_SELECT_OP_COPY;
            dest_op->as.copy_value = machine_select_lower_operand(src_op->as.mov_value);
            return 1;
        case MACHINE_IR_INSTR_BINARY:
            binary_op = src_op->as.binary.op;
            lhs = src_op->as.binary.lhs;
            rhs = src_op->as.binary.rhs;
            machine_select_canonicalize_binary_immediate_side(&binary_op, &lhs, &rhs);
            if (machine_select_try_eval_binary_immediate(
                    binary_op,
                    lhs,
                    rhs,
                    &folded_value)) {
                dest_op->kind = MACHINE_SELECT_OP_MATERIALIZE_IMM;
                dest_op->as.copy_value = machine_select_operand_immediate(folded_value);
                return 1;
            }
            if (machine_select_binary_is_compare(binary_op)) {
                dest_op->kind = machine_select_binary_uses_immediate_form(
                    binary_op,
                    lhs,
                    rhs)
                    ? MACHINE_SELECT_OP_CMP_IMM
                    : MACHINE_SELECT_OP_CMP;
            } else {
                dest_op->kind = machine_select_binary_uses_immediate_form(
                    binary_op,
                    lhs,
                    rhs)
                    ? MACHINE_SELECT_OP_ALU_IMM
                    : MACHINE_SELECT_OP_ALU;
            }
            dest_op->as.binary.op = binary_op;
            dest_op->as.binary.lhs = machine_select_lower_operand(lhs);
            dest_op->as.binary.rhs = machine_select_lower_operand(rhs);
            return 1;
        default:
            *out_handled = 0;
            return 1;
    }
}

#endif
