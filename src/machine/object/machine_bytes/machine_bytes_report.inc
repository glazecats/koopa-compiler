#if defined(__INTELLISENSE__) && !defined(MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_BYTES_SPLIT_AGGREGATOR)
#define MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT
#include "machine_bytes.c"
#else

int machine_bytes_report_refresh(MachineBytesReport *report, MachineBytesError *error) {
    size_t total_block_summary_count = 0;
    size_t total_program_byte_count = 0;
    size_t total_reference_summary_count = 0;
    size_t total_symbol_summary_capacity = 0;
    size_t total_section_summary_capacity = 0;
    size_t function_index;

    if (!report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-400: invalid report refresh contract");
        return 0;
    }

    machine_bytes_report_clear_shape(report);
    if (!machine_bytes_program_get_target_policy_summary(&report->program, &report->target_policy_summary)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-408: failed to compute target policy summary");
        machine_bytes_report_clear_shape(report);
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
        total_reference_summary_count += machine_bytes_function_reference_count(&report->program.functions[function_index]);
    }
    total_symbol_summary_capacity =
        report->program.function_count + total_block_summary_count + report->program.global_count +
        total_reference_summary_count;
    if (report->program.function_count > 0u || report->program.global_count > 0u) {
        total_section_summary_capacity = 3u;
    }

    if (report->program.function_count > 0) {
        report->function_summaries =
            (MachineBytesFunctionSummary *)calloc(report->program.function_count, sizeof(MachineBytesFunctionSummary));
        report->function_byte_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_block_summary_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_reference_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_fixup_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->block_summaries = total_block_summary_count > 0
            ? (MachineBytesBlockSummary *)calloc(total_block_summary_count, sizeof(MachineBytesBlockSummary))
            : NULL;
        report->reference_summaries = total_reference_summary_count > 0
            ? (MachineBytesReferenceSummary *)calloc(total_reference_summary_count, sizeof(MachineBytesReferenceSummary))
            : NULL;
        report->fixup_summaries = total_reference_summary_count > 0
            ? (MachineBytesFixupSummary *)calloc(total_reference_summary_count, sizeof(MachineBytesFixupSummary))
            : NULL;
        report->function_indices_with_calls = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_fallthrough = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_branches = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->total_block_summary_count = total_block_summary_count;
        report->total_reference_summary_count = total_reference_summary_count;
        report->total_fixup_summary_count = total_reference_summary_count;
        if (!report->function_summaries || !report->function_byte_offsets || !report->function_block_summary_offsets ||
            !report->function_reference_offsets || !report->function_fixup_offsets ||
            (total_block_summary_count > 0 && !report->block_summaries) ||
            (total_reference_summary_count > 0 && (!report->reference_summaries || !report->fixup_summaries)) ||
            !report->function_indices_with_calls || !report->function_indices_with_fallthrough ||
            !report->function_indices_with_branches) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-401: out of memory building bytes report");
            machine_bytes_report_clear_shape(report);
            return 0;
        }
    }
    if (total_symbol_summary_capacity > 0u) {
        report->symbol_summaries =
            (MachineBytesSymbolSummary *)calloc(total_symbol_summary_capacity, sizeof(MachineBytesSymbolSummary));
        if (!report->symbol_summaries) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-401: out of memory building bytes report");
            machine_bytes_report_clear_shape(report);
            return 0;
        }
    }
    if (total_section_summary_capacity > 0u) {
        report->section_summaries =
            (MachineBytesSectionSummary *)calloc(total_section_summary_capacity, sizeof(MachineBytesSectionSummary));
        if (!report->section_summaries) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-401: out of memory building bytes report");
            machine_bytes_report_clear_shape(report);
            return 0;
        }
    }
    report->total_symbol_summary_count = total_symbol_summary_capacity;

    total_block_summary_count = 0;
    total_reference_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineBytesFunction *function = &report->program.functions[function_index];
        MachineBytesFunctionSummary *summary = &report->function_summaries[function_index];
        size_t block_index;

        report->function_byte_offsets[function_index] = total_program_byte_count;
        report->function_block_summary_offsets[function_index] = total_block_summary_count;
        report->function_reference_offsets[function_index] = total_reference_summary_count;
        if (!machine_bytes_function_compute_summary(function, summary)) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-402: failed to compute function summary");
            machine_bytes_report_clear_shape(report);
            return 0;
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            if (!machine_bytes_block_get_summary(
                    &function->blocks[block_index], &report->block_summaries[total_block_summary_count++])) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-403: failed to compute block summary");
                machine_bytes_report_clear_shape(report);
                return 0;
            }
        }
        total_reference_summary_count += machine_bytes_function_reference_count(function);
        if (summary->call_count > 0) {
            report->function_indices_with_calls[report->functions_with_calls_count++] = function_index;
        }
        if (summary->fallthrough_count > 0) {
            report->function_indices_with_fallthrough[report->functions_with_fallthrough_count++] = function_index;
        }
        if (summary->branch_count > 0 || summary->compare_branch_count > 0) {
            report->function_indices_with_branches[report->functions_with_branches_count++] = function_index;
        }
        total_program_byte_count += summary->byte_count;
    }
    report->total_program_byte_count = total_program_byte_count;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        if (!machine_bytes_report_populate_function_references(
                report, function_index, report->function_reference_offsets[function_index])) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-404: failed to populate reference summaries");
            machine_bytes_report_clear_shape(report);
            return 0;
        }
    }
    if (!machine_bytes_report_populate_symbols(report) || !machine_bytes_report_populate_fixups(report)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-406: failed to populate symbol/fixup summaries");
        machine_bytes_report_clear_shape(report);
        return 0;
    }
    if (!machine_bytes_report_populate_sections(report)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-407: failed to populate section summaries");
        machine_bytes_report_clear_shape(report);
        return 0;
    }
    return 1;
}

#endif

int machine_bytes_report_get_summary(const MachineBytesReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count) {
    if (!report) {
        return 0;
    }
    if (!machine_bytes_program_get_summary(
            &report->program, out_register_count, out_global_count, out_function_count)) {
        return 0;
    }
    if (out_total_block_summary_count) {
        *out_total_block_summary_count = report->total_block_summary_count;
    }
    return 1;
}

int machine_bytes_report_get_target_policy_summary_artifact(const MachineBytesReport *report,
    const MachineBytesTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_bytes_report_verify_current_riscv32_preview_compatibility(const MachineBytesReport *report,
    MachineBytesError *error) {
    if (!report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-135: invalid bytes-report compatibility contract");
        return 0;
    }
    return machine_bytes_verify_current_riscv32_preview_compatibility(&report->program, error);
}

int machine_bytes_report_get_program(const MachineBytesReport *report,
    const MachineBytesProgram **out_program) {
    if (!report || !out_program) {
        return 0;
    }
    *out_program = &report->program;
    return 1;
}

int machine_bytes_report_get_function(const MachineBytesReport *report,
    size_t function_index,
    const MachineBytesFunction **out_function) {
    if (!report) {
        return 0;
    }
    return machine_bytes_program_get_function(&report->program, function_index, out_function);
}

int machine_bytes_report_get_function_by_name(const MachineBytesReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineBytesFunction **out_function) {
    if (!report || !function_name) {
        return 0;
    }
    return machine_bytes_program_get_function_by_name(
        &report->program, function_name, out_function_index, out_function);
}

int machine_bytes_report_get_function_summary_artifact(const MachineBytesReport *report,
    size_t function_index,
    const MachineBytesFunctionSummary **out_summary) {
    if (!report || !out_summary || function_index >= report->program.function_count || !report->function_summaries) {
        return 0;
    }
    *out_summary = &report->function_summaries[function_index];
    return 1;
}

int machine_bytes_report_get_total_program_byte_count(const MachineBytesReport *report,
    size_t *out_total_program_byte_count) {
    if (!report || !out_total_program_byte_count) {
        return 0;
    }
    *out_total_program_byte_count = report->total_program_byte_count;
    return 1;
}

int machine_bytes_report_get_reference_summary_count(const MachineBytesReport *report,
    size_t *out_total_reference_summary_count) {
    if (!report || !out_total_reference_summary_count) {
        return 0;
    }
    *out_total_reference_summary_count = report->total_reference_summary_count;
    return 1;
}

int machine_bytes_report_get_reference_summary(const MachineBytesReport *report,
    size_t reference_index,
    const MachineBytesReferenceSummary **out_summary) {
    if (!report || !out_summary || !report->reference_summaries ||
        reference_index >= report->total_reference_summary_count) {
        return 0;
    }
    *out_summary = &report->reference_summaries[reference_index];
    return 1;
}

int machine_bytes_report_get_function_reference_summaries(const MachineBytesReport *report,
    size_t function_index,
    size_t *out_count,
    const MachineBytesReferenceSummary **out_summaries) {
    size_t start;
    size_t end;

    if (out_count) {
        *out_count = 0;
    }
    if (out_summaries) {
        *out_summaries = NULL;
    }
    if (!report || function_index >= report->program.function_count || !report->function_reference_offsets) {
        return 0;
    }
    start = report->function_reference_offsets[function_index];
    end = function_index + 1 < report->program.function_count
        ? report->function_reference_offsets[function_index + 1]
        : report->total_reference_summary_count;
    if (out_count) {
        *out_count = end - start;
    }
    if (out_summaries && report->reference_summaries) {
        *out_summaries = report->reference_summaries + start;
    }
    return 1;
}

int machine_bytes_report_get_fixup_summary_count(const MachineBytesReport *report,
    size_t *out_total_fixup_summary_count) {
    if (!report || !out_total_fixup_summary_count) {
        return 0;
    }
    *out_total_fixup_summary_count = report->total_fixup_summary_count;
    return 1;
}

int machine_bytes_report_get_fixup_summary(const MachineBytesReport *report,
    size_t fixup_index,
    const MachineBytesFixupSummary **out_summary) {
    if (!report || !out_summary || !report->fixup_summaries ||
        fixup_index >= report->total_fixup_summary_count) {
        return 0;
    }
    *out_summary = &report->fixup_summaries[fixup_index];
    return 1;
}

int machine_bytes_report_get_function_fixup_summaries(const MachineBytesReport *report,
    size_t function_index,
    size_t *out_count,
    const MachineBytesFixupSummary **out_summaries) {
    size_t start;
    size_t end;

    if (out_count) {
        *out_count = 0;
    }
    if (out_summaries) {
        *out_summaries = NULL;
    }
    if (!report || function_index >= report->program.function_count || !report->function_fixup_offsets) {
        return 0;
    }
    start = report->function_fixup_offsets[function_index];
    end = function_index + 1 < report->program.function_count
        ? report->function_fixup_offsets[function_index + 1]
        : report->total_fixup_summary_count;
    if (out_count) {
        *out_count = end - start;
    }
    if (out_summaries && report->fixup_summaries) {
        *out_summaries = report->fixup_summaries + start;
    }
    return 1;
}

int machine_bytes_report_get_symbol_summary_count(const MachineBytesReport *report,
    size_t *out_total_symbol_summary_count) {
    if (!report || !out_total_symbol_summary_count) {
        return 0;
    }
    *out_total_symbol_summary_count = report->total_symbol_summary_count;
    return 1;
}

int machine_bytes_report_get_symbol_summary(const MachineBytesReport *report,
    size_t symbol_index,
    const MachineBytesSymbolSummary **out_summary) {
    if (!report || !out_summary || !report->symbol_summaries ||
        symbol_index >= report->total_symbol_summary_count) {
        return 0;
    }
    *out_summary = &report->symbol_summaries[symbol_index];
    return 1;
}

int machine_bytes_report_find_symbol_summary_by_name(const MachineBytesReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineBytesSymbolSummary **out_summary) {
    size_t symbol_index;

    if (!report || !symbol_name || !report->symbol_summaries) {
        return 0;
    }
    symbol_index = machine_bytes_report_find_symbol_index_by_name(report, symbol_name);
    if (symbol_index == (size_t)-1) {
        return 0;
    }
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    if (out_summary) {
        *out_summary = &report->symbol_summaries[symbol_index];
    }
    return 1;
}

int machine_bytes_report_get_section_summary_count(const MachineBytesReport *report,
    size_t *out_total_section_summary_count) {
    if (!report || !out_total_section_summary_count) {
        return 0;
    }
    *out_total_section_summary_count = report->total_section_summary_count;
    return 1;
}

int machine_bytes_report_get_section_summary(const MachineBytesReport *report,
    size_t section_index,
    const MachineBytesSectionSummary **out_summary) {
    if (!report || !out_summary || !report->section_summaries ||
        section_index >= report->total_section_summary_count) {
        return 0;
    }
    *out_summary = &report->section_summaries[section_index];
    return 1;
}

int machine_bytes_report_find_section_summary_by_name(const MachineBytesReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineBytesSectionSummary **out_summary) {
    size_t section_index;

    if (!report || !section_name || !report->section_summaries) {
        return 0;
    }
    for (section_index = 0; section_index < report->total_section_summary_count; ++section_index) {
        if (report->section_summaries[section_index].name &&
            strcmp(report->section_summaries[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_summary) {
                *out_summary = &report->section_summaries[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_bytes_report_get_section_symbol_summaries(const MachineBytesReport *report,
    size_t section_index,
    size_t *out_count,
    const MachineBytesSymbolSummary **out_summaries) {
    const MachineBytesSectionSummary *section;

    if (out_count) {
        *out_count = 0;
    }
    if (out_summaries) {
        *out_summaries = NULL;
    }
    if (!report || !report->symbol_summaries ||
        !machine_bytes_report_get_section_summary(report, section_index, &section) || !section) {
        return 0;
    }
    if (out_count) {
        *out_count = section->symbol_count;
    }
    if (out_summaries) {
        *out_summaries = report->symbol_summaries + section->symbol_start_index;
    }
    return 1;
}

int machine_bytes_report_get_section_fixup_summaries(const MachineBytesReport *report,
    size_t section_index,
    size_t *out_count,
    const MachineBytesFixupSummary **out_summaries) {
    const MachineBytesSectionSummary *section;

    if (out_count) {
        *out_count = 0;
    }
    if (out_summaries) {
        *out_summaries = NULL;
    }
    if (!report || !report->fixup_summaries ||
        !machine_bytes_report_get_section_summary(report, section_index, &section) || !section) {
        return 0;
    }
    if (out_count) {
        *out_count = section->fixup_count;
    }
    if (out_summaries) {
        *out_summaries = report->fixup_summaries + section->fixup_start_index;
    }
    return 1;
}

int machine_bytes_report_copy_bytes(const MachineBytesReport *report,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-128: invalid report byte-copy contract");
        return 0;
    }
    return machine_bytes_program_copy_bytes(&report->program, out_bytes, out_byte_count, error);
}

int machine_bytes_report_copy_function_bytes(const MachineBytesReport *report,
    size_t function_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-130: invalid report function byte-copy contract");
        return 0;
    }
    return machine_bytes_program_copy_function_bytes(&report->program, function_index, out_bytes, out_byte_count, error);
}

int machine_bytes_report_copy_block_bytes(const MachineBytesReport *report,
    size_t function_index,
    size_t block_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!report) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-131: invalid report block byte-copy contract");
        return 0;
    }
    return machine_bytes_program_copy_block_bytes(&report->program, function_index, block_index, out_bytes, out_byte_count, error);
}

int machine_bytes_report_get_function_byte_span(const MachineBytesReport *report,
    size_t function_index,
    size_t *out_start_byte_offset,
    size_t *out_end_byte_offset) {
    size_t start;
    size_t end;

    if (!report || function_index >= report->program.function_count || !report->function_summaries ||
        !report->function_byte_offsets) {
        return 0;
    }
    start = report->function_byte_offsets[function_index];
    end = start + report->function_summaries[function_index].byte_count;
    if (out_start_byte_offset) {
        *out_start_byte_offset = start;
    }
    if (out_end_byte_offset) {
        *out_end_byte_offset = end;
    }
    return 1;
}

int machine_bytes_report_find_block_summary_by_label(const MachineBytesReport *report,
    const char *label_name,
    size_t *out_function_index,
    const MachineBytesBlockSummary **out_summary) {
    size_t function_index;

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
            const MachineBytesBlockSummary *summary = &report->block_summaries[block_summary_index];

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

int machine_bytes_report_find_block_summary_covering_offset(const MachineBytesReport *report,
    size_t function_index,
    size_t byte_offset,
    const MachineBytesBlockSummary **out_summary) {
    size_t block_offset;
    size_t next_block_offset;
    size_t block_summary_index;

    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_block_summary_offsets || !report->block_summaries) {
        return 0;
    }
    block_offset = report->function_block_summary_offsets[function_index];
    next_block_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
        const MachineBytesBlockSummary *summary = &report->block_summaries[block_summary_index];
        if (byte_offset >= summary->start_byte_offset && byte_offset < summary->end_byte_offset) {
            *out_summary = summary;
            return 1;
        }
    }
    return 0;
}

int machine_bytes_report_find_block_summary_by_function_name_and_offset(const MachineBytesReport *report,
    const char *function_name,
    size_t byte_offset,
    size_t *out_function_index,
    const MachineBytesBlockSummary **out_summary) {
    size_t function_index = 0;

    if (!report || !function_name || !out_summary) {
        return 0;
    }
    if (!machine_bytes_report_get_function_by_name(report, function_name, &function_index, NULL)) {
        return 0;
    }
    if (!machine_bytes_report_find_block_summary_covering_offset(report, function_index, byte_offset, out_summary)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    return 1;
}

int machine_bytes_report_find_block_summary_by_program_byte_offset(const MachineBytesReport *report,
    size_t program_byte_offset,
    size_t *out_function_index,
    const MachineBytesBlockSummary **out_summary) {
    size_t function_index;

    if (!report || !out_summary || !report->function_summaries || !report->function_byte_offsets) {
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        size_t start = report->function_byte_offsets[function_index];
        size_t end = start + report->function_summaries[function_index].byte_count;

        if (program_byte_offset >= start && program_byte_offset < end) {
            if (!machine_bytes_report_find_block_summary_covering_offset(
                    report, function_index, program_byte_offset - start, out_summary)) {
                return 0;
            }
            if (out_function_index) {
                *out_function_index = function_index;
            }
            return 1;
        }
    }
    return 0;
}

int machine_bytes_report_get_block_summary(const MachineBytesReport *report,
    size_t function_index,
    size_t block_index,
    const MachineBytesBlockSummary **out_summary) {
    size_t offset;
    size_t next_offset;

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

int machine_bytes_report_get_block_terminator_summary(const MachineBytesReport *report,
    size_t function_index,
    size_t block_index,
    MachineBytesTerminatorSummary *out_summary) {
    const MachineBytesFunction *function;

    if (!report || !out_summary || !machine_bytes_report_get_function(report, function_index, &function)) {
        return 0;
    }
    return machine_bytes_function_get_block_terminator_summary(function, block_index, out_summary);
}

int machine_bytes_report_get_functions_with_calls(const MachineBytesReport *report,
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

int machine_bytes_report_get_functions_with_fallthrough(const MachineBytesReport *report,
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

int machine_bytes_report_get_functions_with_branches(const MachineBytesReport *report,
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
