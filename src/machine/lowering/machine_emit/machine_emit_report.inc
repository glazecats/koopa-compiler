#if defined(__INTELLISENSE__) && !defined(MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EMIT_SPLIT_AGGREGATOR)
#define MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_emit.c"
#else

static void machine_emit_lower_report_clear_shape(MachineEmitLowerReport *report) {
    if (!report) {
        return;
    }
    free(report->function_shape_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_shape_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_shape_summaries = NULL;
    report->function_block_summary_offsets = NULL;
    report->block_shape_summaries = NULL;
    report->total_block_summary_count = 0;
    report->functions_with_calls_count = 0;
    report->functions_with_fallthrough_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_calls = NULL;
    report->function_indices_with_fallthrough = NULL;
    report->function_indices_with_branches = NULL;
}

int machine_emit_lower_report_refresh_shape(MachineEmitLowerReport *report, MachineEmitError *error) {
    size_t total_block_summary_count = 0;
    size_t function_index;

    if (!report) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-500: invalid report refresh contract");
        return 0;
    }

    machine_emit_lower_report_clear_shape(report);
    if (!machine_emit_program_get_target_policy_summary(&report->program, &report->target_policy_summary)) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-513: failed to compute emit target policy");
        machine_emit_lower_report_clear_shape(report);
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
    }

    if (report->program.function_count > 0) {
        report->function_shape_summaries = (MachineEmitFunctionShapeSummary *)calloc(
            report->program.function_count, sizeof(MachineEmitFunctionShapeSummary));
        report->function_block_summary_offsets = (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->block_shape_summaries = total_block_summary_count > 0
            ? (MachineEmitBlockShapeSummary *)calloc(total_block_summary_count, sizeof(MachineEmitBlockShapeSummary))
            : NULL;
        report->total_block_summary_count = total_block_summary_count;
        report->function_indices_with_calls = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_fallthrough = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_branches = (size_t *)malloc(report->program.function_count * sizeof(size_t));
        if (!report->function_shape_summaries || !report->function_block_summary_offsets ||
            (total_block_summary_count > 0 && !report->block_shape_summaries) ||
            !report->function_indices_with_calls || !report->function_indices_with_fallthrough ||
            !report->function_indices_with_branches) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-501: out of memory building emit report shape");
            machine_emit_lower_report_clear_shape(report);
            return 0;
        }
    }

    total_block_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineEmitFunction *function = &report->program.functions[function_index];
        MachineEmitFunctionShapeSummary *summary = &report->function_shape_summaries[function_index];
        size_t block_index;
        int has_calls = 0;
        int has_fallthrough = 0;
        int has_branches = 0;

        summary->block_count = function->block_count;
        report->function_block_summary_offsets[function_index] = total_block_summary_count;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineEmitBlock *block = &function->blocks[block_index];
            MachineEmitBlockShapeSummary *block_summary = &report->block_shape_summaries[total_block_summary_count++];
            size_t op_index;

            block_summary->emit_index = block->emit_index;
            block_summary->original_layout_index = block->original_layout_index;
            block_summary->original_block_id = block->original_block_id;
            block_summary->label_name = block->label_name;
            block_summary->op_count = block->op_count;
            block_summary->has_terminator = block->has_terminator;
            block_summary->terminator_kind = block->terminator.kind;
            summary->op_count += block->op_count;

            for (op_index = 0; op_index < block->op_count; ++op_index) {
                if (machine_emit_op_has_call_payload(block->ops[op_index].kind)) {
                    has_calls = 1;
                    ++block_summary->call_count;
                    ++summary->call_count;
                }
            }
            if (block_summary->call_count > 0) {
                ++summary->blocks_with_calls_count;
            }

            switch (block->terminator.kind) {
                case MACHINE_LAYOUT_TERM_RETURN:
                    ++summary->return_count;
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_IMM:
                    ++summary->return_imm_count;
                    break;
                case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                    ++summary->return_spill_count;
                    break;
                case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                    has_fallthrough = 1;
                    ++summary->fallthrough_count;
                    break;
                case MACHINE_LAYOUT_TERM_JUMP:
                    ++summary->jump_count;
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH:
                    has_branches = 1;
                    ++summary->branch_count;
                    break;
                case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                    has_branches = 1;
                    has_fallthrough = 1;
                    ++summary->branch_fallthrough_count;
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                    has_branches = 1;
                    ++summary->compare_branch_count;
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                    has_branches = 1;
                    ++summary->compare_branch_imm_count;
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                    has_branches = 1;
                    has_fallthrough = 1;
                    ++summary->compare_branch_fallthrough_count;
                    break;
                case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                    has_branches = 1;
                    has_fallthrough = 1;
                    ++summary->compare_branch_imm_fallthrough_count;
                    break;
                default:
                    break;
            }
        }

        if (has_calls) {
            report->function_indices_with_calls[report->functions_with_calls_count++] = function_index;
        }
        if (has_fallthrough) {
            report->function_indices_with_fallthrough[report->functions_with_fallthrough_count++] = function_index;
        }
        if (has_branches) {
            report->function_indices_with_branches[report->functions_with_branches_count++] = function_index;
        }
    }

    return 1;
}

int machine_emit_build_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEmitProgram *out_program,
    MachineEmitError *error) {
    const MachineIrProgram *program = NULL;

    if (!report || !out_program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-502: invalid report lowering contract");
        return 0;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(report, &program) || !program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-503: machine-ir report does not expose program");
        return 0;
    }
    return machine_emit_lower_program_from_machine_ir(program, out_program, error);
}

int machine_emit_build_report_from_machine_layout_program(const MachineLayoutProgram *source,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error) {
    if (!source || !out_report) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-504: invalid layout report build contract");
        return 0;
    }

    machine_emit_lower_report_free(out_report);
    machine_emit_lower_report_init(out_report);
    if (!machine_emit_lower_program_from_machine_layout(source, &out_report->program, error)) {
        return 0;
    }
    return machine_emit_lower_report_refresh_shape(out_report, error);
}

int machine_emit_build_report_from_program(const MachineEmitProgram *source,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error) {
    if (!source || !out_report) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-511: invalid emit-program report build contract");
        return 0;
    }

    machine_emit_lower_report_free(out_report);
    machine_emit_lower_report_init(out_report);
    if (!machine_emit_clone_program(source, &out_report->program, error)) {
        machine_emit_lower_report_free(out_report);
        return 0;
    }
    return machine_emit_lower_report_refresh_shape(out_report, error);
}

int machine_emit_build_report_from_machine_layout_program_dump(const MachineLayoutProgram *source,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitLowerReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-508: invalid layout report dump contract");
        return 0;
    }

    machine_emit_lower_report_init(&report);
    ok = machine_emit_build_report_from_machine_layout_program(source, &report, error) &&
        machine_emit_dump_lower_report(&report, out_text, error);
    machine_emit_lower_report_free(&report);
    return ok;
}

int machine_emit_build_report_from_program_dump(const MachineEmitProgram *source,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitLowerReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-512: invalid emit-program report dump contract");
        return 0;
    }

    machine_emit_lower_report_init(&report);
    ok = machine_emit_build_report_from_program(source, &report, error) &&
        machine_emit_dump_lower_report(&report, out_text, error);
    machine_emit_lower_report_free(&report);
    return ok;
}

int machine_emit_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error) {
    if (!source || !out_report) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-505: invalid machine-ir report build contract");
        return 0;
    }

    machine_emit_lower_report_free(out_report);
    machine_emit_lower_report_init(out_report);
    if (!machine_emit_lower_program_from_machine_ir(source, &out_report->program, error)) {
        return 0;
    }
    return machine_emit_lower_report_refresh_shape(out_report, error);
}

int machine_emit_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitLowerReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-509: invalid machine-ir report dump contract");
        return 0;
    }

    machine_emit_lower_report_init(&report);
    ok = machine_emit_build_report_from_machine_ir_program(source, &report, error) &&
        machine_emit_dump_lower_report(&report, out_text, error);
    machine_emit_lower_report_free(&report);
    return ok;
}

int machine_emit_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error) {
    const MachineIrProgram *program = NULL;

    if (!report || !out_report) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-506: invalid report-to-report bridge contract");
        return 0;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(report, &program) || !program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-507: machine-ir report does not expose program");
        return 0;
    }
    return machine_emit_build_report_from_machine_ir_program(program, out_report, error);
}

int machine_emit_build_report_from_machine_ir_report_dump(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitLowerReport emit_report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-510: invalid report-to-report dump contract");
        return 0;
    }

    machine_emit_lower_report_init(&emit_report);
    ok = machine_emit_build_report_from_machine_ir_report(report, &emit_report, error) &&
        machine_emit_dump_lower_report(&emit_report, out_text, error);
    machine_emit_lower_report_free(&emit_report);
    return ok;
}

#endif
