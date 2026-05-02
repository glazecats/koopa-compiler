#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_lower_memory_instruction(const MachineIrInstruction *src_op,
    MachineSelectOp *dest_op,
    int *out_handled) {
    if (out_handled) {
        *out_handled = 1;
    }
    if (!src_op || !dest_op || !out_handled) {
        return 0;
    }

    switch (src_op->kind) {
        case MACHINE_IR_INSTR_LOAD_LOCAL:
            dest_op->kind = MACHINE_SELECT_OP_LOAD_LOCAL;
            dest_op->as.load_slot = machine_select_lower_slot(src_op->as.load_slot);
            return 1;
        case MACHINE_IR_INSTR_STORE_LOCAL:
            dest_op->kind = src_op->as.store.value.kind == MACHINE_IR_OPERAND_IMMEDIATE
                ? MACHINE_SELECT_OP_STORE_LOCAL_IMM
                : MACHINE_SELECT_OP_STORE_LOCAL;
            dest_op->as.store.slot = machine_select_lower_slot(src_op->as.store.slot);
            dest_op->as.store.value = machine_select_lower_operand(src_op->as.store.value);
            return 1;
        case MACHINE_IR_INSTR_LOAD_GLOBAL:
            dest_op->kind = MACHINE_SELECT_OP_LOAD_GLOBAL;
            dest_op->as.load_slot = machine_select_lower_slot(src_op->as.load_slot);
            return 1;
        case MACHINE_IR_INSTR_STORE_GLOBAL:
            dest_op->kind = src_op->as.store.value.kind == MACHINE_IR_OPERAND_IMMEDIATE
                ? MACHINE_SELECT_OP_STORE_GLOBAL_IMM
                : MACHINE_SELECT_OP_STORE_GLOBAL;
            dest_op->as.store.slot = machine_select_lower_slot(src_op->as.store.slot);
            dest_op->as.store.value = machine_select_lower_operand(src_op->as.store.value);
            return 1;
        default:
            *out_handled = 0;
            return 1;
    }
}

#endif
