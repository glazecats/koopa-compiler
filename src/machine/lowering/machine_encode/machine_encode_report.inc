#if defined(__INTELLISENSE__) && !defined(MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ENCODE_SPLIT_AGGREGATOR)
#define MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_encode.c"
#else

static void machine_encode_report_clear_shape(MachineEncodeReport *report) {
    if (!report) {
        return;
    }
    free(report->function_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_summaries = NULL;
    report->function_block_summary_offsets = NULL;
    report->block_summaries = NULL;
    report->total_block_summary_count = 0;
    report->functions_with_calls_count = 0;
    report->functions_with_fallthrough_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_calls = NULL;
    report->function_indices_with_fallthrough = NULL;
    report->function_indices_with_branches = NULL;
}

int machine_encode_report_refresh(MachineEncodeReport *report, MachineEncodeError *error) {
    size_t total_block_summary_count = 0;
    size_t function_index;

    if (!report) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-400: invalid report refresh contract");
        return 0;
    }
    if (!machine_encode_verify_program(&report->program, error)) {
        return 0;
    }

    machine_encode_report_clear_shape(report);
    if (!machine_encode_program_get_target_policy_summary(&report->program, &report->target_policy_summary)) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-414: failed to compute encode target policy");
        machine_encode_report_clear_shape(report);
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
    }

    if (report->program.function_count > 0) {
        report->function_summaries =
            (MachineEncodeFunctionSummary *)calloc(report->program.function_count, sizeof(MachineEncodeFunctionSummary));
        report->function_block_summary_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->block_summaries = total_block_summary_count > 0
            ? (MachineEncodeBlockSummary *)calloc(total_block_summary_count, sizeof(MachineEncodeBlockSummary))
            : NULL;
        report->total_block_summary_count = total_block_summary_count;
        report->function_indices_with_calls = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_fallthrough = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_branches = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        if (!report->function_summaries || !report->function_block_summary_offsets ||
            (total_block_summary_count > 0 && !report->block_summaries) ||
            !report->function_indices_with_calls || !report->function_indices_with_fallthrough ||
            !report->function_indices_with_branches) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-401: out of memory building encode report");
            machine_encode_report_clear_shape(report);
            return 0;
        }
    }

    total_block_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineEncodeFunction *function = &report->program.functions[function_index];
        MachineEncodeFunctionSummary *summary = &report->function_summaries[function_index];
        size_t block_index;

        report->function_block_summary_offsets[function_index] = total_block_summary_count;
        if (!machine_encode_function_compute_summary(function, summary)) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-402: failed to compute function summary");
            machine_encode_report_clear_shape(report);
            return 0;
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            if (!machine_encode_block_get_summary(
                    &function->blocks[block_index], &report->block_summaries[total_block_summary_count++])) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-403: failed to compute block summary");
                machine_encode_report_clear_shape(report);
                return 0;
            }
        }
        if (summary->call_count > 0) {
            report->function_indices_with_calls[report->functions_with_calls_count++] = function_index;
        }
        if (summary->fallthrough_count > 0) {
            report->function_indices_with_fallthrough[report->functions_with_fallthrough_count++] = function_index;
        }
        if (summary->branch_count > 0 || summary->compare_branch_count > 0) {
            report->function_indices_with_branches[report->functions_with_branches_count++] = function_index;
        }
    }

    return 1;
}

int machine_encode_report_get_summary(const MachineEncodeReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count) {
    if (!report) {
        return 0;
    }
    if (!machine_encode_program_get_summary(
            &report->program, out_register_count, out_global_count, out_function_count)) {
        return 0;
    }
    if (out_total_block_summary_count) {
        *out_total_block_summary_count = report->total_block_summary_count;
    }
    return 1;
}

int machine_encode_report_get_program(const MachineEncodeReport *report,
    const MachineEncodeProgram **out_program) {
    if (!report || !out_program) {
        return 0;
    }
    *out_program = &report->program;
    return 1;
}

int machine_encode_report_get_function(const MachineEncodeReport *report,
    size_t function_index,
    const MachineEncodeFunction **out_function) {
    if (!report) {
        return 0;
    }
    return machine_encode_program_get_function(&report->program, function_index, out_function);
}

int machine_encode_report_get_function_by_name(const MachineEncodeReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineEncodeFunction **out_function) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (!report || !function_name) {
        return 0;
    }
    return machine_encode_program_get_function_by_name(
        &report->program, function_name, out_function_index, out_function);
}

int machine_encode_report_get_function_summary_artifact(const MachineEncodeReport *report,
    size_t function_index,
    const MachineEncodeFunctionSummary **out_summary) {
    if (!report || !out_summary || function_index >= report->program.function_count || !report->function_summaries) {
        return 0;
    }
    *out_summary = &report->function_summaries[function_index];
    return 1;
}

int machine_encode_report_find_block_summary_by_label(const MachineEncodeReport *report,
    const char *label_name,
    size_t *out_function_index,
    const MachineEncodeBlockSummary **out_summary) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !label_name || !out_summary || !report->function_block_summary_offsets ||
        !report->block_summaries) {
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        size_t block_offset = report->function_block_summary_offsets[function_index];
        size_t next_block_offset = function_index + 1 < report->program.function_count
            ? report->function_block_summary_offsets[function_index + 1]
            : report->total_block_summary_count;
        size_t block_summary_index;

        for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
            const MachineEncodeBlockSummary *summary = &report->block_summaries[block_summary_index];

            if (summary->label_name && strcmp(summary->label_name, label_name) == 0) {
                if (out_function_index) {
                    *out_function_index = function_index;
                }
                *out_summary = summary;
                return 1;
            }
        }
    }
    return 0;
}

int machine_encode_report_find_block_summary_covering_offset(const MachineEncodeReport *report,
    size_t function_index,
    size_t offset,
    const MachineEncodeBlockSummary **out_summary) {
    size_t block_offset;
    size_t next_block_offset;
    size_t block_summary_index;

    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }
    block_offset = report->function_block_summary_offsets[function_index];
    next_block_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
        const MachineEncodeBlockSummary *summary = &report->block_summaries[block_summary_index];

        if (offset >= summary->start_offset && offset < summary->end_offset) {
            *out_summary = summary;
            return 1;
        }
    }
    return 0;
}

int machine_encode_report_find_block_summary_by_function_name_and_offset(const MachineEncodeReport *report,
    const char *function_name,
    size_t offset,
    size_t *out_function_index,
    const MachineEncodeBlockSummary **out_summary) {
    size_t function_index = 0;

    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_encode_report_get_function_by_name(report, function_name, &function_index, NULL)) {
        return 0;
    }
    if (!machine_encode_report_find_block_summary_covering_offset(report, function_index, offset, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_encode_report_get_block_summary(const MachineEncodeReport *report,
    size_t function_index,
    size_t block_index,
    const MachineEncodeBlockSummary **out_summary) {
    size_t offset;
    size_t next_offset;

    if (out_summary) {
        *out_summary = NULL;
    }
    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }
    offset = report->function_block_summary_offsets[function_index];
    next_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    if (block_index >= next_offset - offset) {
        return 0;
    }
    *out_summary = &report->block_summaries[offset + block_index];
    return 1;
}

int machine_encode_report_get_block_terminator_summary(const MachineEncodeReport *report,
    size_t function_index,
    size_t block_index,
    MachineEncodeTerminatorSummary *out_summary) {
    const MachineEncodeFunction *function;

    if (!report || !out_summary || !machine_encode_report_get_function(report, function_index, &function)) {
        return 0;
    }
    return machine_encode_function_get_block_terminator_summary(function, block_index, out_summary);
}

int machine_encode_report_get_functions_with_calls(const MachineEncodeReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }
    if (out_count) {
        *out_count = report->functions_with_calls_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_calls;
    }
    return 1;
}

int machine_encode_report_get_functions_with_fallthrough(const MachineEncodeReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }
    if (out_count) {
        *out_count = report->functions_with_fallthrough_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_fallthrough;
    }
    return 1;
}

int machine_encode_report_get_functions_with_branches(const MachineEncodeReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }
    if (out_count) {
        *out_count = report->functions_with_branches_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_branches;
    }
    return 1;
}

int machine_encode_build_report_from_machine_emit_program(const MachineEmitProgram *source,
    MachineEncodeReport *out_report,
    MachineEncodeError *error) {
    if (!source || !out_report) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-404: invalid emit report build contract");
        return 0;
    }
    machine_encode_report_free(out_report);
    machine_encode_report_init(out_report);
    if (!machine_encode_lower_program_from_machine_emit(source, &out_report->program, error)) {
        return 0;
    }
    return machine_encode_report_refresh(out_report, error);
}

int machine_encode_build_report_from_machine_emit_program_dump(const MachineEmitProgram *source,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-405: invalid emit report dump contract");
        return 0;
    }
    machine_encode_report_init(&report);
    ok = machine_encode_build_report_from_machine_emit_program(source, &report, error) &&
        machine_encode_dump_report(&report, out_text, error);
    machine_encode_report_free(&report);
    return ok;
}

int machine_encode_build_report_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineEncodeReport *out_report,
    MachineEncodeError *error) {
    if (!report || !out_report) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-410: invalid emit-report build contract");
        return 0;
    }
    return machine_encode_build_report_from_machine_emit_program(&report->program, out_report, error);
}

int machine_encode_build_report_from_machine_emit_report_dump(const MachineEmitLowerReport *report,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeReport encode_report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-411: invalid emit-report dump contract");
        return 0;
    }
    machine_encode_report_init(&encode_report);
    ok = machine_encode_build_report_from_machine_emit_report(report, &encode_report, error) &&
        machine_encode_dump_report(&encode_report, out_text, error);
    machine_encode_report_free(&encode_report);
    return ok;
}

int machine_encode_build_report_from_program(const MachineEncodeProgram *source,
    MachineEncodeReport *out_report,
    MachineEncodeError *error) {
    if (!source || !out_report) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-406: invalid program report build contract");
        return 0;
    }
    machine_encode_report_free(out_report);
    machine_encode_report_init(out_report);
    if (!machine_encode_clone_program(source, &out_report->program, error)) {
        machine_encode_report_free(out_report);
        return 0;
    }
    return machine_encode_report_refresh(out_report, error);
}

int machine_encode_build_report_from_program_dump(const MachineEncodeProgram *source,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-407: invalid program report dump contract");
        return 0;
    }
    machine_encode_report_init(&report);
    ok = machine_encode_build_report_from_program(source, &report, error) &&
        machine_encode_dump_report(&report, out_text, error);
    machine_encode_report_free(&report);
    return ok;
}

int machine_encode_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineEncodeReport *out_report,
    MachineEncodeError *error) {
    if (!source || !out_report) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-408: invalid machine-ir report build contract");
        return 0;
    }
    machine_encode_report_free(out_report);
    machine_encode_report_init(out_report);
    if (!machine_encode_lower_program_from_machine_ir(source, &out_report->program, error)) {
        return 0;
    }
    return machine_encode_report_refresh(out_report, error);
}

int machine_encode_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEncodeReport *out_report,
    MachineEncodeError *error) {
    if (!report || !out_report) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-412: invalid machine-ir report build contract");
        return 0;
    }
    machine_encode_report_free(out_report);
    machine_encode_report_init(out_report);
    if (!machine_encode_lower_program_from_machine_ir_report(report, &out_report->program, error)) {
        return 0;
    }
    return machine_encode_report_refresh(out_report, error);
}

int machine_encode_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-409: invalid machine-ir report dump contract");
        return 0;
    }
    machine_encode_report_init(&report);
    ok = machine_encode_build_report_from_machine_ir_program(source, &report, error) &&
        machine_encode_dump_report(&report, out_text, error);
    machine_encode_report_free(&report);
    return ok;
}

int machine_encode_build_report_from_machine_ir_report_dump(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeReport encode_report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-413: invalid machine-ir report dump contract");
        return 0;
    }
    machine_encode_report_init(&encode_report);
    ok = machine_encode_build_report_from_machine_ir_report(report, &encode_report, error) &&
        machine_encode_dump_report(&encode_report, out_text, error);
    machine_encode_report_free(&encode_report);
    return ok;
}

#endif
