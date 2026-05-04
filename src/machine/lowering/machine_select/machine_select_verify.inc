#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_verify_operand(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectOperand operand,
    const char *context,
    MachineSelectError *error) {
    if (!program || !function || !context) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-100: invalid operand verifier contract");
        return 0;
    }

    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return 1;
        case MACHINE_SELECT_OPERAND_REGISTER:
            if (operand.machine_register_id >= program->register_bank.register_count ||
                !program->register_bank.registers) {
                machine_select_set_error(
                    error, 0, 0, "MACHINE-SELECT-101: %s uses out-of-range machine register", context);
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            if (function->spill_slot_count == 0 || operand.spill_slot >= function->spill_slot_count) {
                machine_select_set_error(
                    error, 0, 0, "MACHINE-SELECT-102: %s uses invalid spill slot", context);
                return 0;
            }
            return 1;
        default:
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-103: %s uses invalid operand kind", context);
            return 0;
    }
}

static int machine_select_verify_result_operand(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectOperand operand,
    const char *context,
    MachineSelectError *error) {
    if (operand.kind != MACHINE_SELECT_OPERAND_REGISTER &&
        operand.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-104: %s result must be register or spill slot", context);
        return 0;
    }
    return machine_select_verify_operand(program, function, operand, context, error);
}

static int machine_select_verify_slot_ref(const MachineSelectProgram *program,
    const MachineSelectFunction *function,
    MachineSelectSlotRef slot,
    const char *context,
    MachineSelectError *error) {
    if (!program || !function || !context) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-105: invalid slot verifier contract");
        return 0;
    }

    if (slot.kind == MACHINE_SELECT_SLOT_LOCAL) {
        if (slot.id >= function->local_count || !function->locals) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-106: %s references invalid local slot", context);
            return 0;
        }
        return 1;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_GLOBAL) {
        if (slot.id >= program->global_count || !program->globals) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-107: %s references invalid global slot", context);
            return 0;
        }
        return 1;
    }

    machine_select_set_error(error, 0, 0, "MACHINE-SELECT-108: %s uses invalid slot kind", context);
    return 0;
}

int machine_select_verify_program(const MachineSelectProgram *program, MachineSelectError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-109: invalid program verifier contract");
        return 0;
    }
    if (program->register_bank.register_count > 0 && !program->register_bank.registers) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-110: register table missing");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-111: global table missing");
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-112: function table missing");
        return 0;
    }

    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        if (program->register_bank.registers[register_index].register_id != register_index) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-113: register ids must stay dense");
            return 0;
        }
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        if (program->globals[global_index].id != global_index) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-114: global ids must stay dense");
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineSelectFunction *function = &program->functions[function_index];
        size_t local_index;
        size_t block_index;

        if (!function->name || function->name[0] == '\0') {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-115: functions must have names");
            return 0;
        }
        if (function->has_body && ((function->local_count > 0 && !function->locals) ||
                (function->block_count > 0 && !function->blocks))) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-116: malformed function body tables");
            return 0;
        }

        for (local_index = 0; local_index < function->local_count; ++local_index) {
            if (function->locals[local_index].id != local_index) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-117: local ids must stay dense");
                return 0;
            }
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineSelectBasicBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (block->id != block_index) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-118: block ids must stay dense");
                return 0;
            }
            if (!block->has_terminator) {
                machine_select_set_error(error, 0, 0, "MACHINE-SELECT-119: block is missing terminator");
                return 0;
            }

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineSelectOp *op = &block->ops[op_index];

                if (op->has_result &&
                    !machine_select_verify_result_operand(program, function, op->result, "selected op result", error)) {
                    return 0;
                }

                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_select_verify_operand(program, function, op->as.copy_value, "copy operand", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                    case MACHINE_SELECT_OP_ALU_IMM:
                    case MACHINE_SELECT_OP_CMP:
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_select_verify_operand(program, function, op->as.binary.lhs, "binary lhs", error) ||
                            !machine_select_verify_operand(program, function, op->as.binary.rhs, "binary rhs", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_ALU_IMM || op->kind == MACHINE_SELECT_OP_CMP_IMM) &&
                            (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                                op->as.binary.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                            machine_select_set_error(
                                error, 0, 0, "MACHINE-SELECT-133: immediate binary families require rhs immediate only");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_CALL:
                    case MACHINE_SELECT_OP_CALL_IMM:
                    case MACHINE_SELECT_OP_CALL_SPILL:
                    case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                    case MACHINE_SELECT_OP_CALL_VOID:
                    case MACHINE_SELECT_OP_CALL_VOID_IMM: {
                        size_t arg_index;
                        int has_immediate_arg = 0;

                        if (!op->as.call.callee_name || op->as.call.callee_name[0] == '\0') {
                            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-120: calls require callee name");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            !op->has_result) {
                            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-127: value calls require result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_VOID || op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) &&
                            op->has_result) {
                            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-128: void calls must not have result");
                            return 0;
                        }
                        if (op->as.call.arg_count > 0 && !op->as.call.args) {
                            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-121: call args missing");
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (!machine_select_verify_operand(
                                    program, function, op->as.call.args[arg_index], "call arg", error)) {
                                return 0;
                            }
                            if (op->as.call.args[arg_index].kind == MACHINE_SELECT_OPERAND_IMMEDIATE) {
                                has_immediate_arg = 1;
                            }
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) &&
                            !has_immediate_arg) {
                            machine_select_set_error(
                                error, 0, 0, "MACHINE-SELECT-130: immediate call family requires immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_VOID) &&
                            has_immediate_arg) {
                            machine_select_set_error(
                                error, 0, 0, "MACHINE-SELECT-131: generic call family must not hide immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            op->result.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                            machine_select_set_error(
                                error, 0, 0, "MACHINE-SELECT-132: spill-result call family requires spill result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM) &&
                            op->result.kind != MACHINE_SELECT_OPERAND_REGISTER) {
                            machine_select_set_error(
                                error, 0, 0, "MACHINE-SELECT-133: register-result call family requires register result");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!op->has_result ||
                            !machine_select_verify_slot_ref(program, function, op->as.load_slot, "load slot", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_select_verify_slot_ref(program, function, op->as.store.slot, "store slot", error) ||
                            !machine_select_verify_operand(program, function, op->as.store.value, "store value", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_STORE_LOCAL_IMM ||
                                op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) &&
                            op->as.store.value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                            machine_select_set_error(
                                error, 0, 0, "MACHINE-SELECT-129: immediate store family requires immediate value");
                            return 0;
                        }
                        break;
                    default:
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-122: unknown selected op kind");
                        return 0;
                }
            }

            switch (block->terminator.kind) {
                case MACHINE_SELECT_TERM_RETURN:
                case MACHINE_SELECT_TERM_RETURN_IMM:
                case MACHINE_SELECT_TERM_RETURN_SPILL:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_NONE &&
                        !machine_select_verify_operand(program, function, block->terminator.as.return_value, "return value", error)) {
                        return 0;
                    }
                    if (block->terminator.kind == MACHINE_SELECT_TERM_RETURN_IMM &&
                        block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                        machine_select_set_error(
                            error, 0, 0, "MACHINE-SELECT-134: immediate return family requires immediate value");
                        return 0;
                    }
                    if (block->terminator.kind == MACHINE_SELECT_TERM_RETURN_SPILL &&
                        block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                        machine_select_set_error(
                            error, 0, 0, "MACHINE-SELECT-135: spill return family requires spill value");
                        return 0;
                    }
                    if (block->terminator.kind == MACHINE_SELECT_TERM_RETURN &&
                        block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_NONE &&
                        block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_REGISTER) {
                        machine_select_set_error(
                            error, 0, 0, "MACHINE-SELECT-136: register return family requires register value");
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_JUMP:
                    if (block->terminator.as.jump_target >= function->block_count) {
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-123: jump target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_BRANCH:
                    if (!machine_select_verify_operand(
                            program, function, block->terminator.as.branch.condition, "branch condition", error) ||
                        block->terminator.as.branch.then_target >= function->block_count ||
                        block->terminator.as.branch.else_target >= function->block_count) {
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-124: invalid branch terminator");
                        return 0;
                    }
                    break;
                case MACHINE_SELECT_TERM_COMPARE_BRANCH:
                case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_select_verify_operand(
                            program, function, block->terminator.as.compare_branch.lhs, "cmp-branch lhs", error) ||
                        !machine_select_verify_operand(
                            program, function, block->terminator.as.compare_branch.rhs, "cmp-branch rhs", error) ||
                        block->terminator.as.compare_branch.then_target >= function->block_count ||
                        block->terminator.as.compare_branch.else_target >= function->block_count ||
                        !machine_select_binary_is_compare(block->terminator.as.compare_branch.op)) {
                        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-126: invalid compare-branch terminator");
                        return 0;
                    }
                    if (block->terminator.kind == MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM &&
                        (block->terminator.as.compare_branch.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                            block->terminator.as.compare_branch.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                        machine_select_set_error(
                            error, 0, 0, "MACHINE-SELECT-134: compare-branch immediate family requires rhs immediate only");
                        return 0;
                    }
                    break;
                default:
                    machine_select_set_error(error, 0, 0, "MACHINE-SELECT-125: unknown terminator kind");
                    return 0;
            }
        }
    }

    return 1;
}

#endif
