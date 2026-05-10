#if defined(__INTELLISENSE__) && !defined(MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ENCODE_SPLIT_AGGREGATOR)
#define MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_encode.c"
#else

static int machine_encode_dump_operand(MachineEncodeStringBuilder *builder, MachineEmitOperand operand) {
    switch (operand.kind) {
        case MACHINE_SELECT_OPERAND_IMMEDIATE:
            return machine_encode_append_format(builder, "%lld", operand.immediate);
        case MACHINE_SELECT_OPERAND_REGISTER:
            return machine_encode_append_format(builder, "reg.%zu", operand.machine_register_id);
        case MACHINE_SELECT_OPERAND_SPILL_SLOT:
            return machine_encode_append_format(builder, "spill.%zu", operand.spill_slot);
        default:
            return machine_encode_append_format(builder, "<none>");
    }
}

int machine_encode_dump_program(const MachineEncodeProgram *program,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeStringBuilder builder;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-200: invalid dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_encode_append_format(&builder, "machine_encode\n")) {
        free(builder.data);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEncodeFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!machine_encode_append_format(
                &builder,
                "function %s params=%zu locals=%zu spills=%zu\n",
                function->name ? function->name : "<unnamed>",
                function->parameter_count,
                function->local_count,
                function->spill_slot_count)) {
            free(builder.data);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineEncodeBlock *block = &function->blocks[block_index];
            MachineEncodeTerminatorSummary terminator_summary;

            if (!machine_encode_append_format(
                    &builder,
                    "  %s @%zu..%zu term@%zu ; emit.%zu -> layout.%zu -> bb.%zu\n",
                    block->label_name ? block->label_name : "<null>",
                    block->start_offset,
                    block->end_offset,
                    block->terminator_offset,
                    block->emit_index,
                    block->original_layout_index,
                    block->original_block_id)) {
                free(builder.data);
                return 0;
            }

            if (!machine_encode_function_get_block_terminator_summary(function, block_index, &terminator_summary)) {
                free(builder.data);
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-201: invalid encoded terminator summary");
                return 0;
            }

            switch (block->block.terminator.kind) {
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                    if (!terminator_summary.has_primary_target ||
                        !machine_encode_append_format(
                            &builder,
                            "    fallthrough %s @%zu\n",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_offset)) {
                        free(builder.data);
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-202: invalid fallthrough target");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (!terminator_summary.has_primary_target ||
                        !machine_encode_append_format(
                            &builder,
                            "    jmp %s @%zu\n",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_offset)) {
                        free(builder.data);
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-203: invalid jump target");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!terminator_summary.has_primary_target || !terminator_summary.has_secondary_target ||
                        !machine_encode_append_format(
                            &builder,
                            "    br then=%s @%zu else=%s @%zu\n",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_offset)) {
                        free(builder.data);
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-204: invalid branch target");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!terminator_summary.has_primary_target || !terminator_summary.has_secondary_target ||
                        !machine_encode_append_format(
                            &builder,
                            "    brft.%s taken=%s @%zu fallthrough=%s @%zu\n",
                            terminator_summary.branch_on_true ? "t" : "f",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_offset)) {
                        free(builder.data);
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-205: invalid branch-ft target");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!terminator_summary.has_primary_target || !terminator_summary.has_secondary_target ||
                        !machine_encode_append_format(
                            &builder,
                            "    %s.%d then=%s @%zu else=%s @%zu\n",
                            block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM ? "cmpbri" : "cmpbr",
                            (int)terminator_summary.compare_op,
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_offset)) {
                        free(builder.data);
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: invalid compare-branch target");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!terminator_summary.has_primary_target || !terminator_summary.has_secondary_target ||
                        !machine_encode_append_format(
                            &builder,
                            "    %s.%d taken=%s @%zu fallthrough=%s @%zu\n",
                            block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH
                                ? "cmpbrift"
                                : "cmpbrft",
                            (int)terminator_summary.compare_op,
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_offset)) {
                        free(builder.data);
                        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-207: invalid compare-ft target");
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN:
                    if (!machine_encode_append_format(&builder, "    ret")) {
                        free(builder.data);
                        return 0;
                    }
                    if (terminator_summary.has_return_value &&
                        (!machine_encode_append_format(&builder, " ") ||
                            !machine_encode_dump_operand(&builder, terminator_summary.return_value) ||
                            !machine_encode_append_format(&builder, "\n"))) {
                        free(builder.data);
                        return 0;
                    }
                    if (!terminator_summary.has_return_value &&
                        !machine_encode_append_format(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                    if (!machine_encode_append_format(&builder, "    reti ") ||
                        !machine_encode_dump_operand(&builder, terminator_summary.return_value) ||
                        !machine_encode_append_format(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (!machine_encode_append_format(&builder, "    retspill ") ||
                        !machine_encode_dump_operand(&builder, terminator_summary.return_value) ||
                        !machine_encode_append_format(&builder, "\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    if (!machine_encode_append_format(&builder, "    term=%d\n", (int)block->block.terminator.kind)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
            }
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_encode_dump_report(const MachineEncodeReport *report,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeStringBuilder builder;
    char *program_text = NULL;
    size_t function_index;
    size_t index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-205: invalid report dump contract");
        return 0;
    }
    if (report->program.function_count > 0 &&
        (!report->program.functions || !report->function_summaries || !report->function_block_summary_offsets)) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-208: report function tables missing");
        return 0;
    }
    if (report->total_block_summary_count > 0 && !report->block_summaries) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-209: report block summary table missing");
        return 0;
    }
    if ((report->functions_with_calls_count > 0 && !report->function_indices_with_calls) ||
        (report->functions_with_fallthrough_count > 0 && !report->function_indices_with_fallthrough) ||
        (report->functions_with_branches_count > 0 && !report->function_indices_with_branches)) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-210: report filter tables missing");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_encode_append_format(
            &builder,
            "machine_encode-report call_funcs=%zu fallthrough_funcs=%zu branch_funcs=%zu total_block_summaries=%zu\n",
            report->functions_with_calls_count,
            report->functions_with_fallthrough_count,
            report->functions_with_branches_count,
            report->total_block_summary_count)) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
        goto cleanup;
    }
    if (!machine_encode_append_format(
            &builder,
            "target_policy preview_reg_cap=%zu preserves_spills=%d preserves_global_slots=%d preserves_fallthrough=%d preserves_offsets=%d\n",
            report->target_policy_summary.emit_policy.select_policy.current_riscv32_preview_logical_register_cap,
            report->target_policy_summary.emit_policy.preserves_spill_operands_for_later_materialization,
            report->target_policy_summary.emit_policy.preserves_global_slot_ops_for_later_address_formation,
            report->target_policy_summary.emit_policy.preserves_fallthrough_terminator_shapes,
            report->target_policy_summary.preserves_block_unit_offsets)) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
        goto cleanup;
    }

    if (!machine_encode_append_format(&builder, "functions-with-calls:")) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_calls_count; ++index) {
        if (!machine_encode_append_format(&builder, " %zu", report->function_indices_with_calls[index])) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
            goto cleanup;
        }
    }
    if (!machine_encode_append_format(&builder, "\nfunctions-with-fallthrough:")) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_fallthrough_count; ++index) {
        if (!machine_encode_append_format(&builder, " %zu", report->function_indices_with_fallthrough[index])) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
            goto cleanup;
        }
    }
    if (!machine_encode_append_format(&builder, "\nfunctions-with-branches:")) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_branches_count; ++index) {
        if (!machine_encode_append_format(&builder, " %zu", report->function_indices_with_branches[index])) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
            goto cleanup;
        }
    }
    if (!machine_encode_append_format(&builder, "\nfunction-summaries:\n")) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
        goto cleanup;
    }

    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineEncodeFunction *function = &report->program.functions[function_index];
        const MachineEncodeFunctionSummary *summary =
            report->function_summaries ? &report->function_summaries[function_index] : NULL;
        size_t block_offset =
            report->function_block_summary_offsets ? report->function_block_summary_offsets[function_index] : 0;
        size_t next_offset = function_index + 1 < report->program.function_count && report->function_block_summary_offsets
            ? report->function_block_summary_offsets[function_index + 1]
            : report->total_block_summary_count;
        size_t block_summary_index;

        if (next_offset < block_offset || next_offset > report->total_block_summary_count) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-211: report block summary offsets drift");
            goto cleanup;
        }

        if (!machine_encode_append_format(
                &builder,
                "  fn.%zu %s blocks=%zu units=%zu op_units=%zu calls=%zu blocks_with_calls=%zu term_units=%zu jumps=%zu fallthrough=%zu branches=%zu cmp_branches=%zu returns=%zu\n",
                function_index,
                function->name ? function->name : "<unnamed>",
                summary ? summary->block_count : 0,
                summary ? summary->unit_count : 0,
                summary ? summary->op_unit_count : 0,
                summary ? summary->call_count : 0,
                summary ? summary->blocks_with_calls_count : 0,
                summary ? summary->terminator_unit_count : 0,
                summary ? summary->jump_count : 0,
                summary ? summary->fallthrough_count : 0,
                summary ? summary->branch_count : 0,
                summary ? summary->compare_branch_count : 0,
                summary ? summary->return_count : 0)) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
            goto cleanup;
        }

        for (block_summary_index = block_offset; block_summary_index < next_offset; ++block_summary_index) {
            const MachineEncodeBlockSummary *summary_block = &report->block_summaries[block_summary_index];

            if (!machine_encode_append_format(
                    &builder,
                    "    emit.%zu label=%s layout.%zu bb.%zu start=%zu term=%zu end=%zu\n",
                    summary_block->emit_index,
                    summary_block->label_name ? summary_block->label_name : "<null>",
                    summary_block->original_layout_index,
                    summary_block->original_block_id,
                    summary_block->start_offset,
                    summary_block->terminator_offset,
                    summary_block->end_offset)) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-206: out of memory while dumping encode report");
                goto cleanup;
            }
        }
    }

    if (!machine_encode_append_format(&builder, "\n") ||
        !machine_encode_dump_program(&report->program, &program_text, error) ||
        !machine_encode_append_format(&builder, "%s", program_text ? program_text : "")) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-207: out of memory while dumping encode report");
        goto cleanup;
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(program_text);
    free(builder.data);
    return ok;
}

#endif
