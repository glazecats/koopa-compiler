#if defined(__INTELLISENSE__) && !defined(MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_BYTES_SPLIT_AGGREGATOR)
#define MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT
#include "machine_bytes.c"
#else

int machine_bytes_dump_program(const MachineBytesProgram *program,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesStringBuilder builder;
    size_t function_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-200: invalid dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_bytes_append_format(&builder, "machine_bytes\n")) {
        free(builder.data);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = &program->functions[function_index];
        size_t block_index;
        size_t function_byte_count = function->block_count > 0 ? function->blocks[function->block_count - 1].end_byte_offset : 0;

        if (!machine_bytes_append_format(
                &builder,
                "function %s params=%zu locals=%zu spills=%zu bytes=%zu\n",
                function->name ? function->name : "<unnamed>",
                function->parameter_count,
                function->local_count,
                function->spill_slot_count,
                function_byte_count)) {
            free(builder.data);
            return 0;
        }

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineBytesBlock *block = &function->blocks[block_index];
            MachineBytesTerminatorSummary terminator_summary;
            size_t byte_index;

            if (!machine_bytes_append_format(
                    &builder,
                    "  %s @%zu..%zu term@%zu bytes=",
                    block->label_name ? block->label_name : "<null>",
                    block->start_byte_offset,
                    block->end_byte_offset,
                    block->terminator_byte_offset)) {
                free(builder.data);
                return 0;
            }
            for (byte_index = 0; byte_index < block->byte_count; ++byte_index) {
                if (!machine_bytes_append_format(&builder, "%s%02X", byte_index == 0 ? "" : " ", block->bytes[byte_index])) {
                    free(builder.data);
                    return 0;
                }
            }
            if (!machine_bytes_append_format(
                    &builder,
                    " ; emit.%zu -> layout.%zu -> bb.%zu\n",
                    block->emit_index,
                    block->original_layout_index,
                    block->original_block_id)) {
                free(builder.data);
                return 0;
            }

            if (!machine_bytes_function_get_block_terminator_summary(function, block_index, &terminator_summary)) {
                free(builder.data);
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-201: invalid byte terminator summary");
                return 0;
            }
            switch (block->block.terminator.kind) {
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                    if (!machine_bytes_append_format(
                            &builder,
                            "    fallthrough %s @%zu\n",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_byte_offset)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_JUMP:
                    if (!machine_bytes_append_format(
                            &builder,
                            "    jmp %s @%zu\n",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_byte_offset)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    if (!machine_bytes_append_format(
                            &builder,
                            "    br then=%s @%zu else=%s @%zu\n",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_byte_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_byte_offset)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    if (!machine_bytes_append_format(
                            &builder,
                            "    brft.%s taken=%s @%zu fallthrough=%s @%zu\n",
                            terminator_summary.branch_on_true ? "t" : "f",
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_byte_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_byte_offset)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    if (!machine_bytes_append_format(
                            &builder,
                            "    %s.%d then=%s @%zu else=%s @%zu\n",
                            block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM ? "cmpbri" : "cmpbr",
                            (int)terminator_summary.compare_op,
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_byte_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_byte_offset)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    if (!machine_bytes_append_format(
                            &builder,
                            "    %s.%d taken=%s @%zu fallthrough=%s @%zu\n",
                            block->block.terminator.kind == MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH
                                ? "cmpbrift"
                                : "cmpbrft",
                            (int)terminator_summary.compare_op,
                            terminator_summary.primary_target_label_name,
                            terminator_summary.primary_target_byte_offset,
                            terminator_summary.secondary_target_label_name,
                            terminator_summary.secondary_target_byte_offset)) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                case MACHINE_LAYOUT_TERM_RETURN:
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    if (!machine_bytes_append_format(&builder, "    return\n")) {
                        free(builder.data);
                        return 0;
                    }
                    break;
                default:
                    if (!machine_bytes_append_format(&builder, "    term=%d\n", (int)block->block.terminator.kind)) {
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

#endif

int machine_bytes_dump_report(const MachineBytesReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesStringBuilder builder;
    char *program_text = NULL;
    size_t function_index;
    size_t index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-202: invalid report dump contract");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_bytes_append_format(
            &builder,
            "machine_bytes-report total_bytes=%zu call_funcs=%zu fallthrough_funcs=%zu branch_funcs=%zu total_block_summaries=%zu\n",
            report->total_program_byte_count,
            report->functions_with_calls_count,
            report->functions_with_fallthrough_count,
            report->functions_with_branches_count,
            report->total_block_summary_count)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    if (!machine_bytes_append_format(&builder, "functions-with-calls:")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_calls_count; ++index) {
        if (!machine_bytes_append_format(&builder, " %zu", report->function_indices_with_calls[index])) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }
    }
    if (!machine_bytes_append_format(&builder, "\nfunctions-with-fallthrough:")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_fallthrough_count; ++index) {
        if (!machine_bytes_append_format(&builder, " %zu", report->function_indices_with_fallthrough[index])) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }
    }
    if (!machine_bytes_append_format(&builder, "\nfunctions-with-branches:")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    for (index = 0; index < report->functions_with_branches_count; ++index) {
        if (!machine_bytes_append_format(&builder, " %zu", report->function_indices_with_branches[index])) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }
    }
    if (!machine_bytes_append_format(&builder, "\nsections:\n")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    for (index = 0; index < report->total_section_summary_count; ++index) {
        const MachineBytesSectionSummary *section = &report->section_summaries[index];

        if (!machine_bytes_append_format(
                &builder,
                "  sec.%zu %s span=%zu..%zu bytes=%zu funcs=%zu blocks=%zu symbols=%zu fixups=%zu\n",
                index,
                section->name ? section->name : "<null>",
                section->start_byte_offset,
                section->end_byte_offset,
                section->byte_count,
                section->function_count,
                section->block_count,
                section->symbol_count,
                section->fixup_count)) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }
    }
    if (!machine_bytes_append_format(&builder, "symbols:\n")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    for (index = 0; index < report->total_symbol_summary_count; ++index) {
        const MachineBytesSymbolSummary *symbol = &report->symbol_summaries[index];

        if (!machine_bytes_append_format(
                &builder,
                "  sym.%zu %s %s defined=%d fn=%s emit=%s offset=%s bytes=%zu incoming_fixups=%zu\n",
                index,
                symbol->kind == MACHINE_BYTES_SYMBOL_FUNCTION ? "function"
                    : (symbol->kind == MACHINE_BYTES_SYMBOL_BLOCK ? "block" : "external"),
                symbol->name ? symbol->name : "<null>",
                symbol->is_defined,
                symbol->has_function_index ? "yes" : "no",
                symbol->has_emit_index ? "yes" : "no",
                symbol->has_byte_offset ? "yes" : "no",
                symbol->byte_count,
                symbol->incoming_fixup_count)) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }
    }
    if (!machine_bytes_append_format(&builder, "fixups:\n")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }
    for (index = 0; index < report->total_fixup_summary_count; ++index) {
        const MachineBytesFixupSummary *fixup = &report->fixup_summaries[index];

        if (!machine_bytes_append_format(
                &builder,
                "  fixup.%zu %s src=%s patch@%zu bytes=%zu target=%s sym=%s\n",
                index,
                fixup->kind == MACHINE_BYTES_FIXUP_CALL_TARGET ? "call"
                    : (fixup->kind == MACHINE_BYTES_FIXUP_CONTROL_PRIMARY ? "ctrl-primary" : "ctrl-secondary"),
                fixup->source_label_name ? fixup->source_label_name : "<null>",
                fixup->patch_byte_offset,
                fixup->patch_byte_count,
                fixup->target_name ? fixup->target_name : "<null>",
                fixup->has_target_symbol_index ? "yes" : "no")) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }
    }
    if (!machine_bytes_append_format(&builder, "\nfunction-summaries:\n")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
        goto cleanup;
    }

    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineBytesFunction *function = &report->program.functions[function_index];
        const MachineBytesFunctionSummary *summary =
            report->function_summaries ? &report->function_summaries[function_index] : NULL;
        size_t function_start = report->function_byte_offsets ? report->function_byte_offsets[function_index] : 0;
        size_t function_end = function_start + (summary ? summary->byte_count : 0);
        size_t block_offset =
            report->function_block_summary_offsets ? report->function_block_summary_offsets[function_index] : 0;
        size_t next_offset = function_index + 1 < report->program.function_count && report->function_block_summary_offsets
            ? report->function_block_summary_offsets[function_index + 1]
            : report->total_block_summary_count;
        size_t block_summary_index;

        if (!machine_bytes_append_format(
                &builder,
                "  fn.%zu %s span=%zu..%zu blocks=%zu bytes=%zu op_bytes=%zu calls=%zu blocks_with_calls=%zu term_bytes=%zu jumps=%zu fallthrough=%zu branches=%zu cmp_branches=%zu returns=%zu\n",
                function_index,
                function->name ? function->name : "<unnamed>",
                function_start,
                function_end,
                summary ? summary->block_count : 0,
                summary ? summary->byte_count : 0,
                summary ? summary->op_byte_count : 0,
                summary ? summary->call_count : 0,
                summary ? summary->blocks_with_calls_count : 0,
                summary ? summary->terminator_byte_count : 0,
                summary ? summary->jump_count : 0,
                summary ? summary->fallthrough_count : 0,
                summary ? summary->branch_count : 0,
                summary ? summary->compare_branch_count : 0,
                summary ? summary->return_count : 0)) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
            goto cleanup;
        }

        for (block_summary_index = block_offset; block_summary_index < next_offset; ++block_summary_index) {
            const MachineBytesBlockSummary *summary_block = &report->block_summaries[block_summary_index];

            if (!machine_bytes_append_format(
                    &builder,
                    "    emit.%zu label=%s layout.%zu bb.%zu start=%zu term=%zu end=%zu bytes=%zu\n",
                    summary_block->emit_index,
                    summary_block->label_name ? summary_block->label_name : "<null>",
                    summary_block->original_layout_index,
                    summary_block->original_block_id,
                    summary_block->start_byte_offset,
                    summary_block->terminator_byte_offset,
                    summary_block->end_byte_offset,
                    summary_block->byte_count)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-203: out of memory while dumping bytes report");
                goto cleanup;
            }
        }
    }

    if (!machine_bytes_append_format(&builder, "\n") ||
        !machine_bytes_dump_program(&report->program, &program_text, error) ||
        !machine_bytes_append_format(&builder, "%s", program_text ? program_text : "")) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-204: out of memory while dumping bytes report");
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

int machine_bytes_lower_dump_from_machine_encode(const MachineEncodeProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesProgram program;
    int ok;

    machine_bytes_program_init(&program);
    ok = machine_bytes_lower_program_from_machine_encode(source, &program, error) &&
        machine_bytes_dump_program(&program, out_text, error);
    machine_bytes_program_free(&program);
    return ok;
}

int machine_bytes_lower_dump_from_machine_encode_report(const MachineEncodeReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesProgram program;
    int ok;

    machine_bytes_program_init(&program);
    ok = machine_bytes_lower_program_from_machine_encode_report(report, &program, error) &&
        machine_bytes_dump_program(&program, out_text, error);
    machine_bytes_program_free(&program);
    return ok;
}

int machine_bytes_lower_dump_from_machine_emit(const MachineEmitProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesProgram program;
    int ok;

    machine_bytes_program_init(&program);
    ok = machine_bytes_lower_program_from_machine_emit(source, &program, error) &&
        machine_bytes_dump_program(&program, out_text, error);
    machine_bytes_program_free(&program);
    return ok;
}

int machine_bytes_lower_dump_from_machine_emit_report(const MachineEmitLowerReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesProgram program;
    int ok;

    machine_bytes_program_init(&program);
    ok = machine_bytes_lower_program_from_machine_emit_report(report, &program, error) &&
        machine_bytes_dump_program(&program, out_text, error);
    machine_bytes_program_free(&program);
    return ok;
}

int machine_bytes_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesProgram program;
    int ok;

    machine_bytes_program_init(&program);
    ok = machine_bytes_lower_program_from_machine_ir(source, &program, error) &&
        machine_bytes_dump_program(&program, out_text, error);
    machine_bytes_program_free(&program);
    return ok;
}

int machine_bytes_lower_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesProgram program;
    int ok;

    machine_bytes_program_init(&program);
    ok = machine_bytes_lower_program_from_machine_ir_report(report, &program, error) &&
        machine_bytes_dump_program(&program, out_text, error);
    machine_bytes_program_free(&program);
    return ok;
}

int machine_bytes_build_report_from_machine_encode_program_with_profile(const MachineEncodeProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!source || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-404: invalid encode-program report build contract");
        return 0;
    }
    machine_bytes_report_free(out_report);
    machine_bytes_report_init(out_report);
    if (!machine_bytes_lower_program_from_machine_encode_with_profile(source, profile, &out_report->program, error)) {
        return 0;
    }
    return machine_bytes_report_refresh(out_report, error);
}

int machine_bytes_build_report_from_machine_encode_program(const MachineEncodeProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    return machine_bytes_build_report_from_machine_encode_program_with_profile(
        source,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_report,
        error);
}

int machine_bytes_build_report_from_machine_encode_report_with_profile(const MachineEncodeReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!report || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-405: invalid encode-report report build contract");
        return 0;
    }
    return machine_bytes_build_report_from_machine_encode_program_with_profile(&report->program, profile, out_report, error);
}

int machine_bytes_build_report_from_machine_encode_report(const MachineEncodeReport *report,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    return machine_bytes_build_report_from_machine_encode_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_report,
        error);
}

int machine_bytes_build_report_from_machine_emit_program_with_profile(const MachineEmitProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!source || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-414: invalid emit-program report build contract");
        return 0;
    }
    machine_bytes_report_free(out_report);
    machine_bytes_report_init(out_report);
    if (!machine_bytes_lower_program_from_machine_emit_with_profile(source, profile, &out_report->program, error)) {
        return 0;
    }
    return machine_bytes_report_refresh(out_report, error);
}

int machine_bytes_build_report_from_machine_emit_program(const MachineEmitProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    return machine_bytes_build_report_from_machine_emit_program_with_profile(
        source,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_report,
        error);
}

int machine_bytes_build_report_from_machine_emit_report_with_profile(const MachineEmitLowerReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!report || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-415: invalid emit-report report build contract");
        return 0;
    }
    machine_bytes_report_free(out_report);
    machine_bytes_report_init(out_report);
    if (!machine_bytes_lower_program_from_machine_emit_report_with_profile(report, profile, &out_report->program, error)) {
        return 0;
    }
    return machine_bytes_report_refresh(out_report, error);
}

int machine_bytes_build_report_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    return machine_bytes_build_report_from_machine_emit_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_report,
        error);
}

int machine_bytes_build_report_from_program(const MachineBytesProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!source || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-406: invalid program report build contract");
        return 0;
    }
    machine_bytes_report_free(out_report);
    machine_bytes_report_init(out_report);
    if (!machine_bytes_clone_program(source, &out_report->program, error)) {
        machine_bytes_report_free(out_report);
        return 0;
    }
    return machine_bytes_report_refresh(out_report, error);
}

int machine_bytes_build_report_from_machine_ir_program_with_profile(const MachineIrProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!source || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-407: invalid machine-ir report build contract");
        return 0;
    }
    machine_bytes_report_free(out_report);
    machine_bytes_report_init(out_report);
    if (!machine_bytes_lower_program_from_machine_ir_with_profile(source, profile, &out_report->program, error)) {
        return 0;
    }
    return machine_bytes_report_refresh(out_report, error);
}

int machine_bytes_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    return machine_bytes_build_report_from_machine_ir_program_with_profile(
        source,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_report,
        error);
}

int machine_bytes_build_report_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    if (!report || !out_report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-408: invalid machine-ir-report build contract");
        return 0;
    }
    machine_bytes_report_free(out_report);
    machine_bytes_report_init(out_report);
    if (!machine_bytes_lower_program_from_machine_ir_report_with_profile(report, profile, &out_report->program, error)) {
        return 0;
    }
    return machine_bytes_report_refresh(out_report, error);
}

int machine_bytes_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineBytesReport *out_report,
    MachineBytesError *error) {
    return machine_bytes_build_report_from_machine_ir_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_report,
        error);
}

int machine_bytes_build_report_from_machine_encode_program_dump(const MachineEncodeProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-409: invalid encode-program report dump contract");
        return 0;
    }
    machine_bytes_report_init(&report);
    ok = machine_bytes_build_report_from_machine_encode_program(source, &report, error) &&
        machine_bytes_dump_report(&report, out_text, error);
    machine_bytes_report_free(&report);
    return ok;
}

int machine_bytes_build_report_from_machine_encode_report_dump(const MachineEncodeReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport bytes_report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-410: invalid encode-report report dump contract");
        return 0;
    }
    machine_bytes_report_init(&bytes_report);
    ok = machine_bytes_build_report_from_machine_encode_report(report, &bytes_report, error) &&
        machine_bytes_dump_report(&bytes_report, out_text, error);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

int machine_bytes_build_report_from_machine_emit_program_dump(const MachineEmitProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-416: invalid emit-program report dump contract");
        return 0;
    }
    machine_bytes_report_init(&report);
    ok = machine_bytes_build_report_from_machine_emit_program(source, &report, error) &&
        machine_bytes_dump_report(&report, out_text, error);
    machine_bytes_report_free(&report);
    return ok;
}

int machine_bytes_build_report_from_machine_emit_report_dump(const MachineEmitLowerReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport bytes_report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-417: invalid emit-report report dump contract");
        return 0;
    }
    machine_bytes_report_init(&bytes_report);
    ok = machine_bytes_build_report_from_machine_emit_report(report, &bytes_report, error) &&
        machine_bytes_dump_report(&bytes_report, out_text, error);
    machine_bytes_report_free(&bytes_report);
    return ok;
}

int machine_bytes_build_report_from_program_dump(const MachineBytesProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-411: invalid program report dump contract");
        return 0;
    }
    machine_bytes_report_init(&report);
    ok = machine_bytes_build_report_from_program(source, &report, error) &&
        machine_bytes_dump_report(&report, out_text, error);
    machine_bytes_report_free(&report);
    return ok;
}

int machine_bytes_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-412: invalid machine-ir program dump contract");
        return 0;
    }
    machine_bytes_report_init(&report);
    ok = machine_bytes_build_report_from_machine_ir_program(source, &report, error) &&
        machine_bytes_dump_report(&report, out_text, error);
    machine_bytes_report_free(&report);
    return ok;
}

int machine_bytes_build_report_from_machine_ir_report_dump(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineBytesError *error) {
    MachineBytesReport bytes_report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-413: invalid machine-ir report dump contract");
        return 0;
    }
    machine_bytes_report_init(&bytes_report);
    ok = machine_bytes_build_report_from_machine_ir_report(report, &bytes_report, error) &&
        machine_bytes_dump_report(&bytes_report, out_text, error);
    machine_bytes_report_free(&bytes_report);
    return ok;
}
