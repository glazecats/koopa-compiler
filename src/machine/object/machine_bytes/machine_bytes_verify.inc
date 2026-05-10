#if defined(__INTELLISENSE__) && !defined(MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_BYTES_SPLIT_AGGREGATOR)
#define MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT
#include "machine_bytes.c"
#else

int machine_bytes_verify_program(const MachineBytesProgram *program, MachineBytesError *error) {
    size_t function_index;
    size_t *function_byte_offsets = NULL;

    if (!program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-100: invalid verifier contract");
        return 0;
    }
    if (!machine_bytes_target_profile_is_valid(program->target_profile)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-122: invalid target profile");
        return 0;
    }
    if (!machine_bytes_profile_supports_register_bank(
            program->target_profile, program->register_bank.register_count, error)) {
        return 0;
    }
    if (program->register_bank.register_count > 0 && !program->register_bank.registers) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-101: register table missing");
        return 0;
    }
    if (program->global_count > 0 && !program->globals) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-102: global table missing");
        return 0;
    }
    if (program->function_count > 0 && !program->functions) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-103: function table missing");
        return 0;
    }
    if (program->function_count > 0) {
        size_t running_function_offset = 0u;

        function_byte_offsets = (size_t *)calloc(program->function_count, sizeof(size_t));
        if (!function_byte_offsets) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-123: out of memory verifying function offsets");
            return 0;
        }
        for (function_index = 0u; function_index < program->function_count; ++function_index) {
            function_byte_offsets[function_index] = running_function_offset;
            running_function_offset += machine_bytes_function_total_byte_count(&program->functions[function_index]);
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = &program->functions[function_index];
        size_t block_index;
        size_t running_offset = 0;

        if (!function->name || function->name[0] == '\0') {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-104: function requires name");
            return 0;
        }
        if (function->block_count > 0 && !function->blocks) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-105: block table missing");
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineBytesBlock *block = &function->blocks[block_index];

            if (block->emit_index != block_index) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-106: emit indices must stay dense");
                return 0;
            }
            if (!block->label_name || block->label_name[0] == '\0') {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-107: byte block requires label");
                return 0;
            }
            if (!block->block.has_terminator) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-108: byte block missing terminator");
                return 0;
            }
            if (block->start_byte_offset != running_offset) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-109: block byte offsets must stay contiguous");
                return 0;
            }
            if (block->end_byte_offset != block->start_byte_offset + block->byte_count) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-111: end byte offset mismatch");
                return 0;
            }
            if (block->terminator_byte_offset != block->end_byte_offset -
                    machine_bytes_terminator_encoded_size_for_profile(
                        program->target_profile, function, &block->block.terminator)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-112: terminator byte offset mismatch");
                return 0;
            }
            if (block->byte_count != block->end_byte_offset - block->start_byte_offset ||
                (block->byte_count > 0 && !block->bytes)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-112: byte array length mismatch");
                return 0;
            }
            for (size_t op_index = 0u; op_index < block->block.op_count; ++op_index) {
                if (!machine_bytes_verify_op_encoding_for_profile(
                        program->target_profile,
                        &block->block.ops[op_index],
                        error)) {
                    return 0;
                }
            }
            if (!machine_bytes_verify_terminator_encoding_for_profile(
                    program->target_profile,
                    &block->block.terminator,
                    error)) {
                return 0;
            }
            if (!machine_bytes_verify_control_target_encoding_for_profile(
                    program->target_profile,
                    &block->block.terminator,
                    error)) {
                return 0;
            }
            {
                MachineBytesBlock expected_block = *block;
                unsigned char *expected_bytes = NULL;

                if (block->byte_count > 0) {
                    expected_bytes = (unsigned char *)calloc(block->byte_count, sizeof(unsigned char));
                }
                if (block->byte_count > 0 && !expected_bytes) {
                    free(function_byte_offsets);
                    machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-123: out of memory verifying bytes");
                    return 0;
                }
                expected_block.bytes = expected_bytes;
                if (!machine_bytes_write_block_bytes_for_profile(
                        program->target_profile,
                        program,
                        function,
                        function_byte_offsets ? function_byte_offsets[function_index] : 0u,
                        function_byte_offsets,
                        &expected_block,
                        error) ||
                    (block->byte_count > 0 && memcmp(block->bytes, expected_bytes, block->byte_count) != 0)) {
                    free(expected_bytes);
                    free(function_byte_offsets);
                    if (!error || error->message[0] == '\0') {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-113: op byte mismatch");
                    }
                    return 0;
                }
                free(expected_bytes);
            }
            switch (block->block.terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                    if (!machine_bytes_verify_target_index(block->block.terminator.as.fallthrough_target, function->block_count)) {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-115: fallthrough target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (!machine_bytes_verify_target_index(block->block.terminator.as.jump_target, function->block_count)) {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-116: jump target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_bytes_verify_target_index(block->block.terminator.as.branch.then_target, function->block_count) ||
                        !machine_bytes_verify_target_index(block->block.terminator.as.branch.else_target, function->block_count)) {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-117: branch target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_bytes_verify_target_index(
                            block->block.terminator.as.branch_fallthrough.taken_target, function->block_count) ||
                        !machine_bytes_verify_target_index(
                            block->block.terminator.as.branch_fallthrough.fallthrough_target, function->block_count)) {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-118: branch-ft target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_bytes_verify_target_index(
                            block->block.terminator.as.compare_branch.then_target, function->block_count) ||
                        !machine_bytes_verify_target_index(
                            block->block.terminator.as.compare_branch.else_target, function->block_count)) {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-119: compare-branch target out of range");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_bytes_verify_target_index(
                            block->block.terminator.as.compare_branch_fallthrough.taken_target, function->block_count) ||
                        !machine_bytes_verify_target_index(
                            block->block.terminator.as.compare_branch_fallthrough.fallthrough_target,
                            function->block_count)) {
                        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-120: compare-ft target out of range");
                        return 0;
                    }
                    break;
                default:
                    machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-121: unknown terminator kind");
                    return 0;
            }

            running_offset = block->end_byte_offset;
        }
    }
    free(function_byte_offsets);
    return 1;
}

int machine_bytes_verify_current_riscv32_preview_compatibility(const MachineBytesProgram *program,
    MachineBytesError *error) {
    MachineBytesTargetPolicySummary policy_summary;

    if (!program) {
        machine_bytes_set_error(
            error, 0, 0, "MACHINE-BYTES-132: invalid riscv32-preview compatibility contract");
        return 0;
    }
    if (!machine_bytes_verify_program(program, error)) {
        return 0;
    }
    if (!machine_bytes_program_get_target_policy_summary(program, &policy_summary)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-133: missing bytes target policy summary");
        return 0;
    }
    if (program->register_bank.register_count > policy_summary.max_logical_machine_register_count &&
        policy_summary.max_logical_machine_register_count > 0u) {
        machine_bytes_set_error(
            error,
            0,
            0,
            "MACHINE-BYTES-134: current riscv32-preview lane supports at most %zu logical machine registers",
            policy_summary.max_logical_machine_register_count);
        return 0;
    }
    return 1;
}

#endif
