#if defined(__INTELLISENSE__) && !defined(MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ENCODE_SPLIT_AGGREGATOR)
#define MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_encode.c"
#else

static int machine_encode_verify_target_index(size_t target_index, size_t block_count) {
    return target_index < block_count;
}

static int machine_encode_verify_operand(MachineEmitOperand operand,
    const MachineEncodeProgram *program,
    size_t spill_slot_count,
    const char *context,
    MachineEncodeError *error) {
    if (!program || !context) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-149: invalid operand verifier contract");
        return 0;
    }
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return 1;
        case MACHINE_SELECT_OPERAND_REGISTER:
            if (operand.machine_register_id >= program->register_bank.register_count ||
                !program->register_bank.registers) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-150: %s uses invalid machine register", context);
                return 0;
            }
            return 1;
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            if (operand.spill_slot >= spill_slot_count) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-151: %s uses invalid spill slot", context);
                return 0;
            }
            return 1;
        default:
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-152: %s uses invalid operand kind", context);
            return 0;
    }
}

static int machine_encode_verify_slot_ref(const MachineEncodeProgram *program,
    const MachineEncodeFunction *function,
    MachineEmitSlotRef slot,
    const char *context,
    MachineEncodeError *error) {
    if (!program || !function || !context) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-153: invalid slot verifier contract");
        return 0;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_LOCAL) {
        if (slot.id >= function->local_count) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-154: %s references invalid local slot", context);
            return 0;
        }
        return 1;
    }
    if (slot.kind == MACHINE_SELECT_SLOT_GLOBAL) {
        if (slot.id >= program->global_count) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-155: %s references invalid global slot", context);
            return 0;
        }
        return 1;
    }
    machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-156: %s uses invalid slot kind", context);
    return 0;
}

static int machine_encode_verify_return_operand(const MachineEncodeProgram *program,
    const MachineEncodeFunction *function,
    const MachineEncodeBlock *block,
    MachineEncodeError *error) {
    const MachineEmitOperand *return_value;

    if (!program || !function || !block) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-140: invalid return verifier contract");
        return 0;
    }

    return_value = &block->block.terminator.as.return_value;
    switch (block->block.terminator.kind) {
        case MACHINE_LAYOUT_TERM_RETURN:
            if (return_value->kind == MACHINE_SELECT_OPERAND_NONE) {
                return 1;
            }
            if (return_value->kind != MACHINE_SELECT_OPERAND_REGISTER ||
                return_value->machine_register_id >= program->register_bank.register_count) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-141: invalid register return");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_RETURN_IMM:
            if (return_value->kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-142: invalid immediate return");
                return 0;
            }
            return 1;
        case MACHINE_LAYOUT_TERM_RETURN_SPILL:
            if (return_value->kind != MACHINE_SELECT_OPERAND_SPILL_SLOT ||
                return_value->spill_slot >= function->spill_slot_count) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-143: invalid spill return");
                return 0;
            }
            return 1;
        default:
            return 1;
    }
}

int machine_encode_verify_program(const MachineEncodeProgram *program, MachineEncodeError *error) {
    size_t function_index;

    if (!program) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-100: invalid verifier contract");
        return 0;
    }
    if (program->register_bank.register_count > 0 && !program->register_bank.registers) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-101: register table missing");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-102: global table missing");
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-103: function table missing");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEncodeFunction *function = &program->functions[function_index];
        size_t block_index;
        size_t running_offset = 0;

        if (!function->name || function->name[0] == '\0') {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-104: function requires name");
            return 0;
        }
        if (function->block_count > 0 && !function->blocks) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-105: block table missing");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineEncodeBlock *block = &function->blocks[block_index];
            size_t op_index;

            if (block->emit_index != block_index) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-106: emit indices must stay dense");
                return 0;
            }
            if (!block->label_name || block->label_name[0] == '\0') {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-107: encoded block requires label");
                return 0;
            }
            if (!block->block.has_terminator) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-109: encoded block missing terminator");
                return 0;
            }
            if (block->block.op_count > 0 && !block->block.ops) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-157: encoded block op table missing");
                return 0;
            }
            if (block->start_offset != running_offset) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-110: block offsets must stay contiguous");
                return 0;
            }
            if (block->terminator_offset != block->start_offset + block->block.op_count) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-111: terminator offset mismatch");
                return 0;
            }
            if (block->end_offset != block->terminator_offset + 1 ||
                block->span.start_offset != block->start_offset ||
                block->span.terminator_offset != block->terminator_offset ||
                block->span.end_offset != block->end_offset) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-112: block span mismatch");
                return 0;
            }
            for (op_index = 0; op_index < block->block.op_count; ++op_index) {
                const MachineEmitOp *op = &block->block.ops[op_index];

                if (op->has_result &&
                    !machine_encode_verify_operand(
                        op->result, program, function->spill_slot_count, "encoded op result", error)) {
                    return 0;
                }
                switch (op->kind) {
                    case MACHINE_SELECT_OP_COPY:
                    case MACHINE_SELECT_OP_MATERIALIZE_IMM:
                        if (!machine_encode_verify_operand(
                                op->as.copy_value, program, function->spill_slot_count, "copy operand", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_ALU:
                    case MACHINE_SELECT_OP_ALU_IMM:
                    case MACHINE_SELECT_OP_CMP:
                    case MACHINE_SELECT_OP_CMP_IMM:
                        if (!machine_encode_verify_operand(
                                op->as.binary.lhs, program, function->spill_slot_count, "binary lhs", error) ||
                            !machine_encode_verify_operand(
                                op->as.binary.rhs, program, function->spill_slot_count, "binary rhs", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_ALU_IMM || op->kind == MACHINE_SELECT_OP_CMP_IMM) &&
                            (op->as.binary.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                                op->as.binary.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                            machine_encode_set_error(
                                error, 0, 0, "MACHINE-ENCODE-158: immediate binary families require rhs immediate only");
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
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-159: calls require callee name");
                            return 0;
                        }
                        if (op->as.call.arg_count > 0 && !op->as.call.args) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-160: call args missing");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            !op->has_result) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-161: value call requires result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_VOID || op->kind == MACHINE_SELECT_OP_CALL_VOID_IMM) &&
                            op->has_result) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-162: void call must not have result");
                            return 0;
                        }
                        for (arg_index = 0; arg_index < op->as.call.arg_count; ++arg_index) {
                            if (!machine_encode_verify_operand(
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
                            machine_encode_set_error(
                                error, 0, 0, "MACHINE-ENCODE-163: immediate call family requires immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_VOID) &&
                            has_immediate_arg) {
                            machine_encode_set_error(
                                error, 0, 0, "MACHINE-ENCODE-164: generic call family must not hide immediate arg");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL_SPILL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM_SPILL) &&
                            op->result.kind != MACHINE_SELECT_OPERAND_SPILL_SLOT) {
                            machine_encode_set_error(
                                error, 0, 0, "MACHINE-ENCODE-165: spill-result call family requires spill result");
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_CALL ||
                                op->kind == MACHINE_SELECT_OP_CALL_IMM) &&
                            op->result.kind != MACHINE_SELECT_OPERAND_REGISTER) {
                            machine_encode_set_error(
                                error, 0, 0, "MACHINE-ENCODE-166: register-result call family requires register result");
                            return 0;
                        }
                        break;
                    }
                    case MACHINE_SELECT_OP_ADDR_LOCAL:
                    case MACHINE_SELECT_OP_ADDR_GLOBAL:
                        if (!op->has_result ||
                            !machine_encode_verify_slot_ref(program, function, op->as.addr_slot, "addr slot", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_ADDR_LOCAL &&
                                op->as.addr_slot.kind != MACHINE_SELECT_SLOT_LOCAL) ||
                            (op->kind == MACHINE_SELECT_OP_ADDR_GLOBAL &&
                                op->as.addr_slot.kind != MACHINE_SELECT_SLOT_GLOBAL)) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-167: addr op uses wrong slot kind");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_LOCAL:
                    case MACHINE_SELECT_OP_LOAD_GLOBAL:
                        if (!op->has_result ||
                            !machine_encode_verify_slot_ref(program, function, op->as.load_slot, "load slot", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_LOAD_LOCAL &&
                                op->as.load_slot.kind != MACHINE_SELECT_SLOT_LOCAL) ||
                            (op->kind == MACHINE_SELECT_OP_LOAD_GLOBAL &&
                                op->as.load_slot.kind != MACHINE_SELECT_SLOT_GLOBAL)) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-168: load op uses wrong slot kind");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_LOCAL:
                    case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                    case MACHINE_SELECT_OP_STORE_GLOBAL:
                    case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                        if (!machine_encode_verify_slot_ref(program, function, op->as.store.slot, "store slot", error) ||
                            !machine_encode_verify_operand(
                                op->as.store.value, program, function->spill_slot_count, "store value", error)) {
                            return 0;
                        }
                        if ((op->kind == MACHINE_SELECT_OP_STORE_LOCAL_IMM ||
                                op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) &&
                            op->as.store.value.kind != MACHINE_SELECT_OPERAND_IMMEDIATE) {
                            machine_encode_set_error(
                                error, 0, 0, "MACHINE-ENCODE-169: immediate store family requires immediate value");
                            return 0;
                        }
                        if (((op->kind == MACHINE_SELECT_OP_STORE_LOCAL ||
                                    op->kind == MACHINE_SELECT_OP_STORE_LOCAL_IMM) &&
                                op->as.store.slot.kind != MACHINE_SELECT_SLOT_LOCAL) ||
                            ((op->kind == MACHINE_SELECT_OP_STORE_GLOBAL ||
                                    op->kind == MACHINE_SELECT_OP_STORE_GLOBAL_IMM) &&
                                op->as.store.slot.kind != MACHINE_SELECT_SLOT_GLOBAL)) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-170: store op uses wrong slot kind");
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_LOAD_INDIRECT:
                        if (!op->has_result ||
                            !machine_encode_verify_operand(
                                op->as.load_indirect_addr, program, function->spill_slot_count, "load_indirect addr", error)) {
                            return 0;
                        }
                        break;
                    case MACHINE_SELECT_OP_STORE_INDIRECT:
                        if (op->has_result ||
                            !machine_encode_verify_operand(
                                op->as.store_indirect.addr, program, function->spill_slot_count, "store_indirect addr", error) ||
                            !machine_encode_verify_operand(
                                op->as.store_indirect.value, program, function->spill_slot_count, "store_indirect value", error)) {
                            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-171: invalid store_indirect op");
                            return 0;
                        }
                        break;
                    default:
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-172: unknown selected op kind");
                        return 0;
                }
            }
            switch (block->block.terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (!machine_encode_verify_return_operand(program, function, block, error)) {
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                    if (!machine_encode_verify_target_index(
                            block->block.terminator.as.fallthrough_target, function->block_count)) {
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-113: fallthrough target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (!machine_encode_verify_target_index(
                            block->block.terminator.as.jump_target, function->block_count)) {
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-114: jump target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_encode_verify_operand(
                            block->block.terminator.as.branch.condition,
                            program,
                            function->spill_slot_count,
                            "branch condition",
                            error) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.branch.then_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.branch.else_target, function->block_count)) {
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-115: branch target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_encode_verify_operand(
                            block->block.terminator.as.branch_fallthrough.condition,
                            program,
                            function->spill_slot_count,
                            "branch-fallthrough condition",
                            error) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.branch_fallthrough.taken_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.branch_fallthrough.fallthrough_target, function->block_count)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-116: branch-fallthrough target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_encode_verify_operand(
                            block->block.terminator.as.compare_branch.lhs,
                            program,
                            function->spill_slot_count,
                            "compare lhs",
                            error) ||
                        !machine_encode_verify_operand(
                            block->block.terminator.as.compare_branch.rhs,
                            program,
                            function->spill_slot_count,
                            "compare rhs",
                            error) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch.then_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch.else_target, function->block_count)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-117: compare-branch target out of range");
                        return 0;
                    }
                    if (block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM &&
                        (block->block.terminator.as.compare_branch.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                            block->block.terminator.as.compare_branch.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-173: compare-branch-imm requires rhs immediate only");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_encode_verify_operand(
                            block->block.terminator.as.compare_branch_fallthrough.lhs,
                            program,
                            function->spill_slot_count,
                            "compare-ft lhs",
                            error) ||
                        !machine_encode_verify_operand(
                            block->block.terminator.as.compare_branch_fallthrough.rhs,
                            program,
                            function->spill_slot_count,
                            "compare-ft rhs",
                            error) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch_fallthrough.taken_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch_fallthrough.fallthrough_target,
                            function->block_count)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-118: compare-branch-fallthrough target out of range");
                        return 0;
                    }
                    if (block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH &&
                        (block->block.terminator.as.compare_branch_fallthrough.lhs.kind == MACHINE_SELECT_OPERAND_IMMEDIATE ||
                            block->block.terminator.as.compare_branch_fallthrough.rhs.kind != MACHINE_SELECT_OPERAND_IMMEDIATE)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-174: compare-branch-imm-fallthrough requires rhs immediate only");
                        return 0;
                    }
                    break;
                default:
                    machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-119: unknown terminator kind");
                    return 0;
            }
            running_offset = block->end_offset;
        }
    }

    return 1;
}

int machine_encode_verify_current_riscv32_preview_compatibility(const MachineEncodeProgram *program,
    MachineEncodeError *error) {
    MachineEncodeTargetPolicySummary policy_summary;

    if (!program) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-120: invalid riscv32-preview compatibility contract");
        return 0;
    }
    if (!machine_encode_verify_program(program, error)) {
        return 0;
    }
    if (!machine_encode_program_get_target_policy_summary(program, &policy_summary)) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-121: missing encode target policy summary");
        return 0;
    }
    if (program->register_bank.register_count >
        policy_summary.emit_policy.select_policy.current_riscv32_preview_logical_register_cap) {
        machine_encode_set_error(
            error,
            0,
            0,
            "MACHINE-ENCODE-122: current riscv32-preview lane supports at most %zu logical machine registers",
            policy_summary.emit_policy.select_policy.current_riscv32_preview_logical_register_cap);
        return 0;
    }
    return 1;
}

int machine_encode_verify_current_riscv32_preview_bytes_compatibility(const MachineEncodeProgram *program,
    MachineEncodeError *error) {
    MachineBytesProgram bytes_program;
    MachineBytesError bytes_error;
    int ok;

    if (!program) {
        machine_encode_set_error(
            error, 0, 0, "MACHINE-ENCODE-124: invalid riscv32-preview bytes-compatibility contract");
        return 0;
    }
    if (!machine_encode_verify_current_riscv32_preview_compatibility(program, error)) {
        return 0;
    }

    memset(&bytes_error, 0, sizeof(bytes_error));
    machine_bytes_program_init(&bytes_program);
    ok = machine_bytes_lower_program_from_machine_encode_with_profile(
        program,
        MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
        &bytes_program,
        &bytes_error);
    machine_bytes_program_free(&bytes_program);
    if (!ok) {
        machine_encode_set_error(
            error,
            bytes_error.line,
            bytes_error.column,
            "MACHINE-ENCODE-125: current riscv32-preview bytes bridge rejected encoded program: %s",
            bytes_error.message);
        return 0;
    }
    return 1;
}

#endif
