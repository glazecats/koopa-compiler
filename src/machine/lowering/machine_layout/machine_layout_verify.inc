#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

static int machine_layout_verify_operand(MachineLayoutOperand operand,
    const MachineLayoutProgram *program,
    size_t spill_slot_count,
    const char *context,
    MachineLayoutError *error) {
    if (!program || !context) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-100: invalid operand verifier contract");
        return 0;
    }
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return 1;
        case MACHINE_SELECT_OPERAND_REGISTER:
            if (operand.machine_register_id >= program->register_bank.register_count ||
                !program->register_bank.registers) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-101: %s uses invalid machine register", context);
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            if (operand.spill_slot >= spill_slot_count) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-102: %s uses invalid spill slot", context);
                return 0;
            }
            return 1;
        default:
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-103: %s uses invalid operand kind", context);
            return 0;
    }
}

static int machine_layout_verify_slot_ref(const MachineLayoutProgram *program,
    const MachineLayoutFunction *function,
    MachineLayoutSlotRef slot,
    const char *context,
    MachineLayoutError *error) {
    if (!program || !function || !context) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-104: invalid slot verifier contract");
        return 0;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_LOCAL) {
        if (slot.id >= function->local_count) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-105: %s references invalid local slot", context);
            return 0;
        }
        return 1;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_GLOBAL) {
        if (slot.id >= program->global_count) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-106: %s references invalid global slot", context);
            return 0;
        }
        return 1;
    }

    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-107: %s uses invalid slot kind", context);
    return 0;
}

int machine_layout_verify_program(const MachineLayoutProgram *program, MachineLayoutError *error) {
    size_t function_index;

    if (!program) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-108: invalid program verifier contract");
        return 0;
    }
    if (program->register_bank.register_count > 0 && !program->register_bank.registers) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-109: register table missing");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-110: global table missing");
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-111: function table missing");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineLayoutFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->name || function->name[0] == '\0') {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-112: function requires name");
            return 0;
        }
        if (function->block_count > 0 && !function->blocks) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-113: block table missing");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineLayoutBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (block->layout_index != block_index) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-114: layout indices must stay dense");
                return 0;
            }
            if (block->op_count > 0 && !block->ops) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-115: block op table missing");
                return 0;
            }
            if (!block->has_terminator) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-116: block missing terminator");
                return 0;
            }

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineLayoutOp *op = &block->ops[op_index];

                if (op->has_result &&
                    !machine_layout_verify_operand(
                        op->result, program, function->spill_slot_count, "selected op result", error)) {
                    return 0;
                }

                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_layout_verify_operand(
                                op->as.copy_value, program, function->spill_slot_count, "copy operand", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                    case MACHINE_SELECT_OP_ALU_IMM:
                    case MACHINE_SELECT_OP_CMP:
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_layout_verify_operand(
                                op->as.binary.lhs, program, function->spill_slot_count, "binary lhs", error) ||
                            !machine_layout_verify_operand(
                                op->as.binary.rhs, program, function->spill_slot_count, "binary rhs", error)) {
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
                            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-117: calls require callee name");
                            return 0;
                        }
                        if (op->as.call.arg_count > 0 && !op->as.call.args) {
                            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-118: call args missing");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            !op->has_result) {
                            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-119: value call requires result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_VOID || op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) &&
                            op->has_result) {
                            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-120: void call must not have result");
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (!machine_layout_verify_operand(
                                    op->as.call.args[arg_index],
                                    program,
                                    function->spill_slot_count,
                                    "call arg",
                                    error)) {
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
                            machine_layout_set_error(
                                error, 0, 0, "MACHINE-LAYOUT-121: immediate call family requires immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_VOID) &&
                            has_immediate_arg) {
                            machine_layout_set_error(
                                error, 0, 0, "MACHINE-LAYOUT-122: generic call family must not hide immediate arg");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!op->has_result ||
                            !machine_layout_verify_slot_ref(program, function, op->as.load_slot, "load slot", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_layout_verify_slot_ref(program, function, op->as.store.slot, "store slot", error) ||
                            !machine_layout_verify_operand(
                                op->as.store.value, program, function->spill_slot_count, "store value", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_STORE_LOCAL_IMM ||
                                op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) &&
                            op->as.store.value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                            machine_layout_set_error(
                                error, 0, 0, "MACHINE-LAYOUT-123: immediate store family requires immediate value");
                            return 0;
                        }
                        break;
                    default:
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-124: unknown selected op kind");
                        return 0;
                }
            }

            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_REGISTER ||
                        !machine_layout_verify_operand(
                            block->terminator.as.return_value, program, function->spill_slot_count, "return", error)) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-125: invalid register return");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-126: invalid immediate return");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT ||
                        !machine_layout_verify_operand(
                            block->terminator.as.return_value,
                            program,
                            function->spill_slot_count,
                            "spill return",
                            error)) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-127: invalid spill return");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (block->terminator.as.jump_target >= function->block_count) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-128: target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_layout_verify_operand(
                            block->terminator.as.branch.condition,
                            program,
                            function->spill_slot_count,
                            "branch condition",
                            error) ||
                        block->terminator.as.branch.then_target >= function->block_count ||
                        block->terminator.as.branch.else_target >= function->block_count) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-129: invalid branch");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_layout_verify_operand(
                            block->terminator.as.branch_fallthrough.condition,
                            program,
                            function->spill_slot_count,
                            "branch-fallthrough condition",
                            error) ||
                        block->terminator.as.branch_fallthrough.taken_target >= function->block_count ||
                        block->terminator.as.branch_fallthrough.fallthrough_target >= function->block_count) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-130: invalid branch-fallthrough");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_layout_verify_operand(
                            block->terminator.as.compare_branch.lhs,
                            program,
                            function->spill_slot_count,
                            "compare lhs",
                            error) ||
                        !machine_layout_verify_operand(
                            block->terminator.as.compare_branch.rhs,
                            program,
                            function->spill_slot_count,
                            "compare rhs",
                            error) ||
                        block->terminator.as.compare_branch.then_target >= function->block_count ||
                        block->terminator.as.compare_branch.else_target >= function->block_count) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-131: invalid compare-branch");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_layout_verify_operand(
                            block->terminator.as.compare_branch_fallthrough.lhs,
                            program,
                            function->spill_slot_count,
                            "compare-ft lhs",
                            error) ||
                        !machine_layout_verify_operand(
                            block->terminator.as.compare_branch_fallthrough.rhs,
                            program,
                            function->spill_slot_count,
                            "compare-ft rhs",
                            error) ||
                        block->terminator.as.compare_branch_fallthrough.taken_target >= function->block_count ||
                        block->terminator.as.compare_branch_fallthrough.fallthrough_target >= function->block_count) {
                        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-132: invalid compare-branch-fallthrough");
                        return 0;
                    }
                    break;
                default:
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-133: unknown terminator kind");
                    return 0;
            }
        }
    }

    return 1;
}

int machine_layout_verify_current_riscv32_preview_compatibility(const MachineLayoutProgram *program,
    MachineLayoutError *error) {
    MachineLayoutTargetPolicySummary policy_summary;

    if (!program) {
        machine_layout_set_error(
            error, 0, 0, "MACHINE-LAYOUT-136: invalid riscv32-preview compatibility contract");
        return 0;
    }
    if (!machine_layout_verify_program(program, error)) {
        return 0;
    }
    if (!machine_layout_program_get_target_policy_summary(program, &policy_summary)) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-137: missing layout target policy summary");
        return 0;
    }
    if (program->register_bank.register_count >
        policy_summary.select_policy.current_riscv32_preview_logical_register_cap) {
        machine_layout_set_error(
            error,
            0,
            0,
            "MACHINE-LAYOUT-138: current riscv32-preview lane supports at most %zu logical machine registers",
            policy_summary.select_policy.current_riscv32_preview_logical_register_cap);
        return 0;
    }
    return 1;
}

int machine_layout_verify_current_riscv32_preview_bytes_compatibility(const MachineLayoutProgram *program,
    MachineLayoutError *error) {
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    int ok;

    if (!program) {
        machine_layout_set_error(
            error, 0, 0, "MACHINE-LAYOUT-139: invalid riscv32-preview bytes-compatibility contract");
        return 0;
    }
    if (!machine_layout_verify_current_riscv32_preview_compatibility(program, error)) {
        return 0;
    }

    memset(&emit_error, 0, sizeof(emit_error));
    machine_emit_program_init(&emit_program);
    ok = machine_emit_lower_program_from_machine_layout(program, &emit_program, &emit_error) &&
        machine_emit_verify_current_riscv32_preview_bytes_compatibility(&emit_program, &emit_error);
    machine_emit_program_free(&emit_program);
    if (!ok) {
        machine_layout_set_error(
            error,
            emit_error.line,
            emit_error.column,
            "MACHINE-LAYOUT-140: current riscv32-preview bytes bridge rejected layout program: %s",
            emit_error.message);
        return 0;
    }
    return 1;
}

#endif
