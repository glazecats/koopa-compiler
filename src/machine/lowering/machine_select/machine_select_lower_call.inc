#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_lower_call_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    MachineSelectError *error) {
    size_t arg_index;
    int has_immediate_arg = 0;
    int has_spill_result = 0;

    if (!src_op || !dest_op) {
        return 0;
    }
    if (src_op->kind != MACHINE_IR_INSTR_CALL) {
        return 1;
    }

    for (arg_index = 0; arg_index < src_op->as.call.arg_count; ++arg_index) {
        if (src_op->as.call.args[arg_index].kind == MACHINE_IR_OPERAND_IMMEDIATE) {
            has_immediate_arg = 1;
            break;
        }
    }
    has_spill_result = src_op->has_result && src_op->result.kind == MACHINE_IR_OPERAND_SPILL_SLOT;

    if (src_op->has_result) {
        if (has_spill_result) {
            dest_op->kind = has_immediate_arg ? MACHINE_SELECT_OP_CALL_IMM_SPILL : MACHINE_SELECT_OP_CALL_SPILL;
        } else {
            dest_op->kind = has_immediate_arg ? MACHINE_SELECT_OP_CALL_IMM : MACHINE_SELECT_OP_CALL;
        }
    } else {
        dest_op->kind = has_immediate_arg ? MACHINE_SELECT_OP_CALL_VOID_IMM : MACHINE_SELECT_OP_CALL_VOID;
    }
    dest_op->has_result = src_op->has_result;
    dest_op->as.call.callee_name = machine_select_strdup(src_op->as.call.callee_name);
    if (src_op->as.call.callee_name && !dest_op->as.call.callee_name) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-311: out of memory copying callee");
        return 0;
    }
    dest_op->as.call.arg_count = src_op->as.call.arg_count;
    if (src_op->as.call.arg_count > 0) {
        dest_op->as.call.args =
            (MachineSelectOperand *)calloc(src_op->as.call.arg_count, sizeof(MachineSelectOperand));
        if (!dest_op->as.call.args) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-312: out of memory copying call args");
            return 0;
        }
    }
    for (arg_index = 0; arg_index < src_op->as.call.arg_count; ++arg_index) {
        dest_op->as.call.args[arg_index] = machine_select_lower_operand(src_op->as.call.args[arg_index]);
    }
    return 1;
}

#endif
