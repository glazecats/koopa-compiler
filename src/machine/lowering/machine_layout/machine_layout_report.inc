#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

static void machine_layout_report_clear_shape(MachineLayoutReport *report) {
    if (!report) {
        return;
    }
    free(report->function_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_summaries);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_summaries = NULL;
    report->function_summary_count = 0;
    report->function_block_summary_offsets = NULL;
    report->block_summaries = NULL;
    report->total_block_summary_count = 0;
    report->functions_with_fallthrough_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_fallthrough = NULL;
    report->function_indices_with_branches = NULL;
}

int machine_layout_report_refresh(MachineLayoutReport *report, MachineLayoutError *error) {
    size_t total_block_summary_count = 0;
    size_t function_index;
    size_t function_count_with_fallthrough = 0;
    size_t function_count_with_branches = 0;

    if (!report) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-400: invalid report refresh contract");
        return 0;
    }

    machine_layout_report_clear_shape(report);
    if (!machine_layout_program_get_target_policy_summary(&report->program, &report->target_policy_summary)) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-401: failed to compute layout target policy");
        machine_layout_report_clear_shape(report);
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
    }

    if (report->program.function_count > 0) {
        report->function_summaries =
            (MachineLayoutFunctionSummary *)calloc(report->program.function_count, sizeof(MachineLayoutFunctionSummary));
        report->function_block_summary_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->block_summaries = total_block_summary_count > 0
            ? (MachineLayoutBlockSummary *)calloc(total_block_summary_count, sizeof(MachineLayoutBlockSummary))
            : NULL;
        report->function_indices_with_fallthrough = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_branches = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->total_block_summary_count = total_block_summary_count;
        if (!report->function_summaries || !report->function_block_summary_offsets ||
            (total_block_summary_count > 0 && !report->block_summaries) ||
            !report->function_indices_with_fallthrough || !report->function_indices_with_branches) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-402: out of memory building layout report");
            machine_layout_report_clear_shape(report);
            return 0;
        }
    }
    report->function_summary_count = report->program.function_count;

    total_block_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineLayoutFunction *function = &report->program.functions[function_index];
        MachineLayoutFunctionSummary *summary = &report->function_summaries[function_index];
        size_t block_index;
        int has_fallthrough = 0;
        int has_branches = 0;

        report->function_block_summary_offsets[function_index] = total_block_summary_count;
        if (!machine_layout_function_compute_summary(function, summary)) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-403: failed to compute function summary");
            machine_layout_report_clear_shape(report);
            return 0;
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineLayoutBlock *block = &function->blocks[block_index];
            MachineLayoutBlockSummary *block_summary = &report->block_summaries[total_block_summary_count++];

            if (!machine_layout_block_get_summary(block, block_summary)) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-404: failed to compute block summary");
                machine_layout_report_clear_shape(report);
                return 0;
            }
            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    has_fallthrough = 1;
                    break;
                default:
                    break;
            }
            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_BRANCH:
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    has_branches = 1;
                    break;
                default:
                    break;
            }
        }
        if (has_fallthrough) {
            report->function_indices_with_fallthrough[function_count_with_fallthrough++] = function_index;
        }
        if (has_branches) {
            report->function_indices_with_branches[function_count_with_branches++] = function_index;
        }
    }
    report->functions_with_fallthrough_count = function_count_with_fallthrough;
    report->functions_with_branches_count = function_count_with_branches;
    return 1;
}

int machine_layout_report_get_summary(const MachineLayoutReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count,
    size_t *out_functions_with_fallthrough_count,
    size_t *out_functions_with_branches_count) {
    if (!report) {
        return 0;
    }
    if (!machine_layout_program_get_summary(
            &report->program, out_register_count, out_global_count, out_function_count)) {
        return 0;
    }
    if (out_total_block_summary_count) {
        *out_total_block_summary_count = report->total_block_summary_count;
    }
    if (out_functions_with_fallthrough_count) {
        *out_functions_with_fallthrough_count = report->functions_with_fallthrough_count;
    }
    if (out_functions_with_branches_count) {
        *out_functions_with_branches_count = report->functions_with_branches_count;
    }
    return 1;
}

int machine_layout_report_get_target_policy_summary_artifact(const MachineLayoutReport *report,
    const MachineLayoutTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_layout_report_verify_current_riscv32_preview_compatibility(const MachineLayoutReport *report,
    MachineLayoutError *error) {
    if (!report) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-410: invalid layout-report compatibility contract");
        return 0;
    }
    return machine_layout_verify_current_riscv32_preview_compatibility(&report->program, error);
}

int machine_layout_report_verify_current_riscv32_preview_bytes_compatibility(const MachineLayoutReport *report,
    MachineLayoutError *error) {
    if (!report) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-411: invalid layout-report bytes-compatibility contract");
        return 0;
    }
    return machine_layout_verify_current_riscv32_preview_bytes_compatibility(&report->program, error);
}

int machine_layout_report_get_program(const MachineLayoutReport *report,
    const MachineLayoutProgram **out_program) {
    if (!report || !out_program) {
        return 0;
    }
    *out_program = &report->program;
    return 1;
}

int machine_layout_report_get_function(const MachineLayoutReport *report,
    size_t function_index,
    const MachineLayoutFunction **out_function) {
    if (!report) {
        return 0;
    }
    return machine_layout_program_get_function(&report->program, function_index, out_function);
}

int machine_layout_report_get_function_by_name(const MachineLayoutReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineLayoutFunction **out_function) {
    if (!report) {
        return 0;
    }
    return machine_layout_program_get_function_by_name(&report->program, function_name, out_function_index, out_function);
}

int machine_layout_report_get_function_summary_artifact(const MachineLayoutReport *report,
    size_t function_index,
    const MachineLayoutFunctionSummary **out_summary) {
    if (!report || !out_summary || function_index >= report->function_summary_count || !report->function_summaries) {
        return 0;
    }
    *out_summary = &report->function_summaries[function_index];
    return 1;
}

int machine_layout_report_get_block_summary(const MachineLayoutReport *report,
    size_t function_index,
    size_t block_index,
    const MachineLayoutBlockSummary **out_summary) {
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

int machine_layout_report_get_functions_with_fallthrough(const MachineLayoutReport *report,
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

int machine_layout_report_get_functions_with_branches(const MachineLayoutReport *report,
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

int machine_layout_build_report_from_program(const MachineLayoutProgram *source,
    MachineLayoutReport *out_report,
    MachineLayoutError *error) {
    if (!source || !out_report) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-405: invalid program report build contract");
        return 0;
    }

    machine_layout_report_free(out_report);
    machine_layout_report_init(out_report);
    if (!machine_layout_clone_program(source, &out_report->program, error)) {
        return 0;
    }
    return machine_layout_report_refresh(out_report, error);
}

int machine_layout_build_report_from_machine_select_program(const MachineSelectProgram *source,
    MachineLayoutReport *out_report,
    MachineLayoutError *error) {
    if (!source || !out_report) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-406: invalid machine-select report build contract");
        return 0;
    }

    machine_layout_report_free(out_report);
    machine_layout_report_init(out_report);
    if (!machine_layout_lower_program_from_machine_select(source, &out_report->program, error)) {
        return 0;
    }
    return machine_layout_report_refresh(out_report, error);
}

int machine_layout_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineLayoutReport *out_report,
    MachineLayoutError *error) {
    if (!source || !out_report) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-407: invalid machine-ir report build contract");
        return 0;
    }

    machine_layout_report_free(out_report);
    machine_layout_report_init(out_report);
    if (!machine_layout_lower_program_from_machine_ir(source, &out_report->program, error)) {
        return 0;
    }
    return machine_layout_report_refresh(out_report, error);
}

int machine_layout_build_report_from_program_dump(const MachineLayoutProgram *source,
    char **out_text,
    MachineLayoutError *error) {
    MachineLayoutReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-408: invalid program report dump contract");
        return 0;
    }

    machine_layout_report_init(&report);
    ok = machine_layout_build_report_from_program(source, &report, error) &&
        machine_layout_dump_report(&report, out_text, error);
    machine_layout_report_free(&report);
    return ok;
}

int machine_layout_build_report_from_machine_select_program_dump(const MachineSelectProgram *source,
    char **out_text,
    MachineLayoutError *error) {
    MachineLayoutReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-409: invalid machine-select report dump contract");
        return 0;
    }

    machine_layout_report_init(&report);
    ok = machine_layout_build_report_from_machine_select_program(source, &report, error) &&
        machine_layout_dump_report(&report, out_text, error);
    machine_layout_report_free(&report);
    return ok;
}

int machine_layout_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineLayoutError *error) {
    MachineLayoutReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-410: invalid machine-ir report dump contract");
        return 0;
    }

    machine_layout_report_init(&report);
    ok = machine_layout_build_report_from_machine_ir_program(source, &report, error) &&
        machine_layout_dump_report(&report, out_text, error);
    machine_layout_report_free(&report);
    return ok;
}

#endif
