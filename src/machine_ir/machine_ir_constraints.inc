#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static int machine_ir_operand_get_resource_index(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrOperand operand,
    size_t *out_index) {
    size_t register_count;

    if (out_index) {
        *out_index = (size_t)-1;
    }
    if (!program || !function || !out_index) {
        return 0;
    }

    register_count = program->register_bank.register_count;
    switch (operand.kind) {
        case MACHINE_IR_OPERAND_REGISTER:
            if (operand.machine_register_id >= register_count) {
                return 0;
            }
            *out_index = operand.machine_register_id;
            return 1;
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            if (operand.spill_slot >= function->spill_slot_count) {
                return 0;
            }
            *out_index = register_count + operand.spill_slot;
            return 1;
        default:
            return 0;
    }
}

static size_t machine_ir_operand_resource_slot(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrOperand operand) {
    size_t resource_index;

    if (machine_ir_operand_get_resource_index(program, function, operand, &resource_index)) {
        return resource_index;
    }
    return (size_t)-1;
}

static int machine_ir_register_operands_have_compatible_constraints(const MachineIrProgram *program,
    MachineIrOperand lhs,
    MachineIrOperand rhs) {
    const MachineIrRegisterDesc *lhs_desc;
    const MachineIrRegisterDesc *rhs_desc;

    if (!program || lhs.kind != MACHINE_IR_OPERAND_REGISTER || rhs.kind != MACHINE_IR_OPERAND_REGISTER ||
        lhs.machine_register_id >= program->register_bank.register_count ||
        rhs.machine_register_id >= program->register_bank.register_count ||
        !program->register_bank.registers) {
        return 0;
    }

    lhs_desc = &program->register_bank.registers[lhs.machine_register_id];
    rhs_desc = &program->register_bank.registers[rhs.machine_register_id];
    return lhs_desc->register_class == rhs_desc->register_class &&
        lhs_desc->allocatable == rhs_desc->allocatable &&
        lhs_desc->caller_clobbered == rhs_desc->caller_clobbered &&
        lhs_desc->callee_preserved == rhs_desc->callee_preserved;
}

static int machine_ir_result_operands_have_compatible_constraints(const MachineIrProgram *program,
    const MachineIrFunction *function,
    MachineIrOperand lhs,
    MachineIrOperand rhs) {
    if (!program || !function || lhs.kind != rhs.kind) {
        return 0;
    }

    switch (lhs.kind) {
        case MACHINE_IR_OPERAND_REGISTER:
            return machine_ir_register_operands_have_compatible_constraints(program, lhs, rhs);
        case MACHINE_IR_OPERAND_SPILL_SLOT:
            if (lhs.spill_slot >= function->spill_slot_count || rhs.spill_slot >= function->spill_slot_count) {
                return 0;
            }
            return lhs.spill_slot == rhs.spill_slot;
        default:
            return machine_ir_operands_equal(lhs, rhs);
    }
}

#endif
