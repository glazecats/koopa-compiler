#if defined(__INTELLISENSE__) && !defined(MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ENCODE_SPLIT_AGGREGATOR)
#define MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_encode.c"
#else

static int machine_encode_verify_target_index(size_t target_index, size_t block_count) {
    return target_index < block_count;
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
                    if (!machine_encode_verify_target_index(
                            block->block.terminator.as.branch.then_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.branch.else_target, function->block_count)) {
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-115: branch target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_encode_verify_target_index(
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
                    if (!machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch.then_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch.else_target, function->block_count)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-117: compare-branch target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch_fallthrough.taken_target, function->block_count) ||
                        !machine_encode_verify_target_index(
                            block->block.terminator.as.compare_branch_fallthrough.fallthrough_target,
                            function->block_count)) {
                        machine_encode_set_error(
                            error, 0, 0, "MACHINE-ENCODE-118: compare-branch-fallthrough target out of range");
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
