#if defined(__INTELLISENSE__) && !defined(MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EMIT_SPLIT_AGGREGATOR)
#define MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_emit.c"
#else

static int machine_emit_verify_operand(MachineEmitOperand operand,
    const MachineEmitProgram *program,
    size_t spill_slot_count,
    const char *context,
    MachineEmitError *error) {
    if (!program || !context) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-100: invalid operand verifier contract");
        return 0;
    }
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return 1;
        case MACHINE_SELECT_OPERAND_REGISTER:
            if (operand.machine_register_id >= program->register_bank.register_count ||
                !program->register_bank.registers) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-101: %s uses invalid machine register", context);
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            if (operand.spill_slot >= spill_slot_count) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-102: %s uses invalid spill slot", context);
                return 0;
            }
            return 1;
        default:
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-103: %s uses invalid operand kind", context);
            return 0;
    }
}

static int machine_emit_verify_slot_ref(const MachineEmitProgram *program,
    const MachineEmitFunction *function,
    MachineEmitSlotRef slot,
    const char *context,
    MachineEmitError *error) {
    if (!program || !function || !context) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-104: invalid slot verifier contract");
        return 0;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_LOCAL) {
        if (slot.id >= function->local_count) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-105: %s references invalid local slot", context);
            return 0;
        }
        return 1;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_GLOBAL) {
        if (slot.id >= program->global_count) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-106: %s references invalid global slot", context);
            return 0;
        }
        return 1;
    }

    machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-107: %s uses invalid slot kind", context);
    return 0;
}

int machine_emit_verify_program(const MachineEmitProgram *program, MachineEmitError *error) {
    size_t function_index;

    if (!program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-108: invalid program verifier contract");
        return 0;
    }
    if (program->register_bank.register_count > 0 && !program->register_bank.registers) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-109: register table missing");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-110: global table missing");
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-111: function table missing");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEmitFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->name || function->name[0] == '\0') {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-112: function requires name");
            return 0;
        }
        if (function->block_count > 0 && !function->blocks) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-113: block table missing");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineEmitBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (block->emit_index != block_index) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-114: emit indices must stay dense");
                return 0;
            }
            if (!block->label_name || block->label_name[0] == '\0') {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-115: block requires emitted label");
                return 0;
            }
            {
                size_t prior_function_index;

                for (prior_function_index = 0; prior_function_index <= function_index; ++prior_function_index) {
                    const MachineEmitFunction *prior_function = &program->functions[prior_function_index];
                    size_t prior_block_limit = prior_function_index == function_index ? block_index : prior_function->block_count;
                    size_t prior_block_index;

                    for (prior_block_index = 0; prior_block_index < prior_block_limit; ++prior_block_index) {
                        if (strcmp(block->label_name, prior_function->blocks[prior_block_index].label_name) == 0) {
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-116: emitted labels must be globally unique");
                            return 0;
                        }
                    }
                }
            }
            if (block->op_count > 0 && !block->ops) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-117: block op table missing");
                return 0;
            }
            if (!block->has_terminator) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-118: block missing terminator");
                return 0;
            }

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                const MachineEmitOp *op = &block->ops[op_index];

                if (op->has_result &&
                    !machine_emit_verify_operand(
                        op->result, program, function->spill_slot_count, "selected op result", error)) {
                    return 0;
                }

                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_emit_verify_operand(
                                op->as.copy_value, program, function->spill_slot_count, "copy operand", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                    case MACHINE_SELECT_OP_ALU_IMM:
                    case MACHINE_SELECT_OP_CMP:
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_emit_verify_operand(
                                op->as.binary.lhs, program, function->spill_slot_count, "binary lhs", error) ||
                            !machine_emit_verify_operand(
                                op->as.binary.rhs, program, function->spill_slot_count, "binary rhs", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_ALU_IMM || op->kind == MACHINE_SELECT_OP_CMP_IMM) &&
                            (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                                op->as.binary.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-142: immediate binary families require rhs immediate only");
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
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-119: calls require callee name");
                            return 0;
                        }
                        if (op->as.call.arg_count > 0 && !op->as.call.args) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-120: call args missing");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            !op->has_result) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-121: value call requires result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_VOID || op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) &&
                            op->has_result) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-122: void call must not have result");
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (!machine_emit_verify_operand(
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
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-123: immediate call family requires immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_VOID) &&
                            has_immediate_arg) {
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-124: generic call family must not hide immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            op->result.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-143: spill-result call family requires spill result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM) &&
                            op->result.kind != MACHINE_SELECT_OPERAND_REGISTER) {
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-144: register-result call family requires register result");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_ADDR_LOCAL:
                    case MACHINE_SELECT_OP_ADDR_GLOBAL:
                        if (!op->has_result ||
                            !machine_emit_verify_slot_ref(program, function, op->as.addr_slot, "addr slot", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_ADDR_LOCAL &&
                                op->as.addr_slot.kind != MACHINE_SELECT_SLOT_LOCAL) ||
                            (op->kind == MACHINE_SELECT_OP_ADDR_GLOBAL &&
                                op->as.addr_slot.kind != MACHINE_SELECT_SLOT_GLOBAL)) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-130: addr op uses wrong slot kind");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!op->has_result ||
                            !machine_emit_verify_slot_ref(program, function, op->as.load_slot, "load slot", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_LOAD_LOCAL &&
                                op->as.load_slot.kind != MACHINE_SELECT_SLOT_LOCAL) ||
                            (op->kind == MACHINE_SELECT_OP_LOAD_GLOBAL &&
                                op->as.load_slot.kind != MACHINE_SELECT_SLOT_GLOBAL)) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-145: load op uses wrong slot kind");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_emit_verify_slot_ref(program, function, op->as.store.slot, "store slot", error) ||
                            !machine_emit_verify_operand(
                                op->as.store.value, program, function->spill_slot_count, "store value", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_STORE_LOCAL_IMM ||
                                op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) &&
                            op->as.store.value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                            machine_emit_set_error(
                                error, 0, 0, "MACHINE-EMIT-125: immediate store family requires immediate value");
                            return 0;
                        }
                        if (((op->kind == MACHINE_SELECT_OP_STORE_LOCAL ||
                                    op->kind == MACHINE_SELECT_OP_STORE_LOCAL_IMM) &&
                                op->as.store.slot.kind != MACHINE_SELECT_SLOT_LOCAL) ||
                            ((op->kind == MACHINE_SELECT_OP_STORE_GLOBAL ||
                                    op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) &&
                                op->as.store.slot.kind != MACHINE_SELECT_SLOT_GLOBAL)) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-146: store op uses wrong slot kind");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_INDIRECT:
                        if (!op->has_result ||
                            !machine_emit_verify_operand(
                                op->as.load_indirect_addr, program, function->spill_slot_count, "load_indirect addr", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_INDIRECT:
                        if (op->has_result ||
                            !machine_emit_verify_operand(
                                op->as.store_indirect.addr, program, function->spill_slot_count, "store_indirect addr", error) ||
                            !machine_emit_verify_operand(
                                op->as.store_indirect.value, program, function->spill_slot_count, "store_indirect value", error)) {
                            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-131: invalid store_indirect op");
                            return 0;
                        }
                        break;
                    default:
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-126: unknown selected op kind");
                        return 0;
                }
            }

            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_NONE &&
                        (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_REGISTER ||
                            !machine_emit_verify_operand(
                                block->terminator.as.return_value, program, function->spill_slot_count, "return", error))) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-127: invalid register return");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-128: invalid immediate return");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (block->terminator.as.return_value.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT ||
                        !machine_emit_verify_operand(
                            block->terminator.as.return_value,
                            program,
                            function->spill_slot_count,
                            "spill return",
                            error)) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-129: invalid spill return");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (block->terminator.as.jump_target >= function->block_count) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-130: target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_emit_verify_operand(
                            block->terminator.as.branch.condition,
                            program,
                            function->spill_slot_count,
                            "branch condition",
                            error) ||
                        block->terminator.as.branch.then_target >= function->block_count ||
                        block->terminator.as.branch.else_target >= function->block_count) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-131: invalid branch");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_emit_verify_operand(
                            block->terminator.as.branch_fallthrough.condition,
                            program,
                            function->spill_slot_count,
                            "branch-fallthrough condition",
                            error) ||
                        block->terminator.as.branch_fallthrough.taken_target >= function->block_count ||
                        block->terminator.as.branch_fallthrough.fallthrough_target >= function->block_count) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-132: invalid branch-fallthrough");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_emit_verify_operand(
                            block->terminator.as.compare_branch.lhs,
                            program,
                            function->spill_slot_count,
                            "compare lhs",
                            error) ||
                        !machine_emit_verify_operand(
                            block->terminator.as.compare_branch.rhs,
                            program,
                            function->spill_slot_count,
                            "compare rhs",
                            error) ||
                        block->terminator.as.compare_branch.then_target >= function->block_count ||
                        block->terminator.as.compare_branch.else_target >= function->block_count) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-133: invalid compare-branch");
                        return 0;
                    }
                    if (block->terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM &&
                        (block->terminator.as.compare_branch.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                            block->terminator.as.compare_branch.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                        machine_emit_set_error(
                            error, 0, 0, "MACHINE-EMIT-147: compare-branch-imm requires rhs immediate only");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_emit_verify_operand(
                            block->terminator.as.compare_branch_fallthrough.lhs,
                            program,
                            function->spill_slot_count,
                            "compare-ft lhs",
                            error) ||
                        !machine_emit_verify_operand(
                            block->terminator.as.compare_branch_fallthrough.rhs,
                            program,
                            function->spill_slot_count,
                            "compare-ft rhs",
                            error) ||
                        block->terminator.as.compare_branch_fallthrough.taken_target >= function->block_count ||
                        block->terminator.as.compare_branch_fallthrough.fallthrough_target >= function->block_count) {
                        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-134: invalid compare-branch-fallthrough");
                        return 0;
                    }
                    if (block->terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH &&
                        (block->terminator.as.compare_branch_fallthrough.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                            block->terminator.as.compare_branch_fallthrough.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                        machine_emit_set_error(
                            error, 0, 0, "MACHINE-EMIT-148: compare-branch-imm-fallthrough requires rhs immediate only");
                        return 0;
                    }
                    break;
                default:
                    machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-135: unknown terminator kind");
                    return 0;
            }
        }
    }

    return 1;
}

int machine_emit_verify_current_riscv32_preview_compatibility(const MachineEmitProgram *program,
    MachineEmitError *error) {
    MachineEmitTargetPolicySummary policy_summary;

    if (!program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-135: invalid riscv32-preview compatibility contract");
        return 0;
    }
    if (!machine_emit_verify_program(program, error)) {
        return 0;
    }
    if (!machine_emit_program_get_target_policy_summary(program, &policy_summary)) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-136: missing emit target policy summary");
        return 0;
    }
    if (program->register_bank.register_count >
        policy_summary.select_policy.current_riscv32_preview_logical_register_cap) {
        machine_emit_set_error(
            error,
            0,
            0,
            "MACHINE-EMIT-137: current riscv32-preview lane supports at most %zu logical machine registers",
            policy_summary.select_policy.current_riscv32_preview_logical_register_cap);
        return 0;
    }
    return 1;
}

int machine_emit_verify_current_riscv32_preview_bytes_compatibility(const MachineEmitProgram *program,
    MachineEmitError *error) {
    MachineEncodeProgram encode_program;
    MachineEncodeError encode_error;
    int ok;

    if (!program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-139: invalid riscv32-preview bytes-compatibility contract");
        return 0;
    }
    if (!machine_emit_verify_current_riscv32_preview_compatibility(program, error)) {
        return 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    machine_encode_program_init(&encode_program);
    ok = machine_encode_lower_program_from_machine_emit(program, &encode_program, &encode_error) &&
        machine_encode_verify_current_riscv32_preview_bytes_compatibility(&encode_program, &encode_error);
    machine_encode_program_free(&encode_program);
    if (!ok) {
        machine_emit_set_error(
            error,
            encode_error.line,
            encode_error.column,
            "MACHINE-EMIT-140: current riscv32-preview bytes bridge rejected emitted program: %s",
            encode_error.message);
        return 0;
    }
    return 1;
}

#endif
