#if defined(__INTELLISENSE__) && !defined(MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_SELECT_SPLIT_AGGREGATOR)
#define MACHINE_SELECT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_select.c"
#else

static int machine_select_program_has_phi_nodes(const MachineIrProgram *program) {
    size_t function_index;

    if (!program) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineIrFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            if (function->blocks[block_index].phi_count != 0) {
                return 1;
            }
        }
    }

    return 0;
}

static void machine_select_lower_report_clear_shape(MachineSelectLowerReport *report) {
    if (!report) {
        return;
    }
    free(report->function_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_spills);
    free(report->function_indices_with_memory_ops);
    free(report->function_indices_with_branches);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_summaries = NULL;
    report->function_summary_count = 0;
    report->function_block_summary_offsets = NULL;
    report->block_summaries = NULL;
    report->total_block_summary_count = 0;
    report->functions_with_calls_count = 0;
    report->functions_with_spills_count = 0;
    report->functions_with_memory_ops_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_calls = NULL;
    report->function_indices_with_spills = NULL;
    report->function_indices_with_memory_ops = NULL;
    report->function_indices_with_branches = NULL;
}

int machine_select_lower_report_refresh(MachineSelectLowerReport *report, MachineSelectError *error) {
    size_t function_index;
    size_t total_block_summary_count = 0;
    size_t function_count_with_calls = 0;
    size_t function_count_with_spills = 0;
    size_t function_count_with_memory_ops = 0;
    size_t function_count_with_branches = 0;

    if (!report) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-402: invalid report refresh contract");
        return 0;
    }
    if (!machine_select_verify_program(&report->program, error)) {
        return 0;
    }

    machine_select_lower_report_clear_shape(report);
    if (!machine_select_program_get_target_policy_summary(&report->program, &report->target_policy_summary)) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-417: failed to compute target policy summary");
        machine_select_lower_report_clear_shape(report);
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
    }

    report->function_summary_count = report->program.function_count;
    if (report->function_summary_count > 0) {
        report->function_summaries =
            (MachineSelectFunctionSummary *)calloc(report->function_summary_count, sizeof(MachineSelectFunctionSummary));
        report->function_block_summary_offsets = (size_t *)calloc(report->function_summary_count, sizeof(size_t));
        report->block_summaries = total_block_summary_count > 0
            ? (MachineSelectBlockShapeSummary *)calloc(total_block_summary_count, sizeof(MachineSelectBlockShapeSummary))
            : NULL;
        report->total_block_summary_count = total_block_summary_count;
        report->function_indices_with_calls = (size_t *)calloc(report->function_summary_count, sizeof(size_t));
        report->function_indices_with_spills = (size_t *)calloc(report->function_summary_count, sizeof(size_t));
        report->function_indices_with_memory_ops = (size_t *)calloc(report->function_summary_count, sizeof(size_t));
        report->function_indices_with_branches = (size_t *)calloc(report->function_summary_count, sizeof(size_t));
        if (!report->function_summaries || !report->function_block_summary_offsets ||
            (total_block_summary_count > 0 && !report->block_summaries) || !report->function_indices_with_calls ||
            !report->function_indices_with_spills || !report->function_indices_with_memory_ops ||
            !report->function_indices_with_branches) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-403: out of memory building function summaries");
            machine_select_lower_report_clear_shape(report);
            return 0;
        }
    }

    total_block_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineSelectFunction *function = &report->program.functions[function_index];
        if (!machine_select_function_compute_summary(function, &report->function_summaries[function_index])) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-404: failed to compute function summary");
            machine_select_lower_report_clear_shape(report);
            return 0;
        }

        {
            const MachineSelectFunctionSummary *summary = &report->function_summaries[function_index];
            size_t block_index;
            size_t call_count = summary->call_count + summary->call_imm_count + summary->call_spill_count +
                summary->call_imm_spill_count + summary->call_void_count + summary->call_void_imm_count;
            size_t memory_op_count = summary->load_local_count + summary->store_local_count +
                summary->store_local_imm_count + summary->load_global_count + summary->store_global_count +
                summary->store_global_imm_count;
            size_t branch_count =
                summary->plain_branch_count + summary->compare_branch_count + summary->compare_branch_imm_count;

            report->function_block_summary_offsets[function_index] = total_block_summary_count;
            for (block_index = 0; block_index < function->block_count; ++block_index) {
                const MachineSelectBasicBlock *block = &function->blocks[block_index];
                MachineSelectBlockShapeSummary *block_summary = &report->block_summaries[total_block_summary_count++];
                size_t op_index;

                block_summary->block_id = block->id;
                block_summary->op_count = block->op_count;
                for (op_index = 0; op_index < block->op_count; ++op_index) {
                    switch (block->ops[op_index].kind) {
                        case MACHINE_SELECT_OP_CALL:
                        case MACHINE_SELECT_OP_CALL_IMM:
                        case MACHINE_SELECT_OP_CALL_SPILL:
                        case MACHINE_SELECT_OP_CALL_IMM_SPILL:
                        case MACHINE_SELECT_OP_CALL_VOID:
                        case MACHINE_SELECT_OP_CALL_VOID_IMM:
                            ++block_summary->call_count;
                            break;
                        case MACHINE_SELECT_OP_LOAD_LOCAL:
                            ++block_summary->load_local_count;
                            break;
                        case MACHINE_SELECT_OP_STORE_LOCAL:
                            ++block_summary->store_local_count;
                            break;
                        case MACHINE_SELECT_OP_STORE_LOCAL_IMM:
                            ++block_summary->store_local_imm_count;
                            break;
                        case MACHINE_SELECT_OP_LOAD_GLOBAL:
                            ++block_summary->load_global_count;
                            break;
                        case MACHINE_SELECT_OP_STORE_GLOBAL:
                            ++block_summary->store_global_count;
                            break;
                        case MACHINE_SELECT_OP_STORE_GLOBAL_IMM:
                            ++block_summary->store_global_imm_count;
                            break;
                        default:
                            break;
                    }
                }

                switch (block->terminator.kind) {
                    case MACHINE_SELECT_TERM_RETURN:
                    case MACHINE_SELECT_TERM_RETURN_IMM:
                    case MACHINE_SELECT_TERM_RETURN_SPILL:
                        block_summary->has_return_terminator = 1;
                        break;
                    case MACHINE_SELECT_TERM_JUMP:
                        block_summary->has_jump_terminator = 1;
                        break;
                    case MACHINE_SELECT_TERM_BRANCH:
                        block_summary->has_branch_terminator = 1;
                        break;
                    case MACHINE_SELECT_TERM_COMPARE_BRANCH:
                    case MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM:
                        block_summary->has_compare_branch_terminator = 1;
                        break;
                    default:
                        break;
                }
            }

            if (call_count > 0) {
                report->function_indices_with_calls[function_count_with_calls++] = function_index;
            }
            if (summary->has_spills) {
                report->function_indices_with_spills[function_count_with_spills++] = function_index;
            }
            if (memory_op_count > 0) {
                report->function_indices_with_memory_ops[function_count_with_memory_ops++] = function_index;
            }
            if (branch_count > 0) {
                report->function_indices_with_branches[function_count_with_branches++] = function_index;
            }
        }
    }

    report->functions_with_calls_count = function_count_with_calls;
    report->functions_with_spills_count = function_count_with_spills;
    report->functions_with_memory_ops_count = function_count_with_memory_ops;
    report->functions_with_branches_count = function_count_with_branches;
    return 1;
}

int machine_select_build_report_from_program(const MachineSelectProgram *source,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error) {
    if (!source || !out_report) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-414: invalid selected-program report build contract");
        return 0;
    }

    machine_select_lower_report_free(out_report);
    machine_select_lower_report_init(out_report);
    if (!machine_select_clone_program(source, &out_report->program, error)) {
        return 0;
    }
    return machine_select_lower_report_refresh(out_report, error);
}

int machine_select_build_report_from_program_dump(const MachineSelectProgram *source,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectLowerReport report;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!source || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-415: invalid selected-program report dump contract");
        return 0;
    }

    machine_select_lower_report_init(&report);
    if (machine_select_build_report_from_program(source, &report, error)) {
        ok = machine_select_dump_lower_report_artifact(&report, out_text, error);
    }
    machine_select_lower_report_free(&report);
    return ok;
}

int machine_select_build_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    const MachineIrProgram *program = NULL;

    if (!report || !out_program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-400: invalid report lowering contract");
        return 0;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(report, &program) || !program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-401: machine-ir report does not expose program");
        return 0;
    }
    if (machine_select_program_has_phi_nodes(program)) {
        return machine_select_lower_canonicalized_program_from_machine_ir(program, out_program, error);
    }
    return machine_select_lower_program_from_machine_ir(program, out_program, error);
}

int machine_select_build_program_from_machine_ir_report_conservative(const MachineIrAllocateRewriteReport *report,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    const MachineIrProgram *program = NULL;

    if (!report || !out_program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-421: invalid conservative report lowering contract");
        return 0;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(report, &program) || !program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-422: machine-ir report does not expose program");
        return 0;
    }
    if (machine_select_program_has_phi_nodes(program)) {
        return machine_select_lower_canonicalized_program_from_machine_ir_conservative(program, out_program, error);
    }
    return machine_select_lower_program_from_machine_ir_conservative(program, out_program, error);
}

int machine_select_build_program_from_machine_ir_report_conservative_no_phi(
    const MachineIrAllocateRewriteReport *report,
    MachineSelectProgram *out_program,
    MachineSelectError *error) {
    const MachineIrProgram *program = NULL;

    if (!report || !out_program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-423: invalid conservative no-phi report lowering contract");
        return 0;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(report, &program) || !program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-424: machine-ir report does not expose program");
        return 0;
    }
    if (machine_select_program_has_phi_nodes(program)) {
        machine_select_set_error(error,
            0,
            0,
            "MACHINE-SELECT-425: conservative no-phi report lowering requires phi-free machine-ir input");
        return 0;
    }
    return machine_select_lower_program_from_machine_ir_conservative(program, out_program, error);
}

int machine_select_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error) {
    if (!source || !out_report) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-416: invalid report build contract");
        return 0;
    }

    machine_select_lower_report_free(out_report);
    machine_select_lower_report_init(out_report);
    if (!machine_select_lower_program_from_machine_ir(source, &out_report->program, error)) {
        return 0;
    }
    return machine_select_lower_report_refresh(out_report, error);
}

int machine_select_build_report_from_canonicalized_machine_ir_program(const MachineIrProgram *source,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error) {
    MachineIrProgram canonicalized;
    MachineIrError machine_error;
    int ok;

    if (!source || !out_report) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-410: invalid canonicalized report build contract");
        return 0;
    }

    memset(&machine_error, 0, sizeof(machine_error));
    machine_ir_program_init(&canonicalized);
    ok = machine_ir_build_canonicalized_program(source, &canonicalized, &machine_error);
    if (!ok) {
        machine_select_set_error(error, machine_error.line, machine_error.column, "%s", machine_error.message);
        machine_ir_program_free(&canonicalized);
        return 0;
    }

    ok = machine_select_build_report_from_machine_ir_program(&canonicalized, out_report, error);
    machine_ir_program_free(&canonicalized);
    return ok;
}

int machine_select_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error) {
    const MachineIrProgram *program = NULL;

    if (!report || !out_report) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-405: invalid report-to-report bridge contract");
        return 0;
    }
    if (!machine_ir_allocate_rewrite_report_get_program(report, &program) || !program) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-406: machine-ir report does not expose program");
        return 0;
    }
    return machine_select_build_report_from_machine_ir_program(program, out_report, error);
}

int machine_select_dump_lower_report_artifact(const MachineSelectLowerReport *report,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectStringBuilder builder;
    char *program_text = NULL;
    size_t function_index;
    size_t filter_index;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-407: invalid lower-report dump contract");
        return 0;
    }
    if (report->function_summary_count != report->program.function_count) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-430: report function-summary count drift");
        return 0;
    }
    if (report->program.function_count > 0 &&
        (!report->program.functions || !report->function_summaries || !report->function_block_summary_offsets)) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-431: report function tables missing");
        return 0;
    }
    if (report->total_block_summary_count > 0 && !report->block_summaries) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-432: report block summary table missing");
        return 0;
    }
    if ((report->functions_with_calls_count > 0 && !report->function_indices_with_calls) ||
        (report->functions_with_spills_count > 0 && !report->function_indices_with_spills) ||
        (report->functions_with_memory_ops_count > 0 && !report->function_indices_with_memory_ops) ||
        (report->functions_with_branches_count > 0 && !report->function_indices_with_branches)) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-433: report filter tables missing");
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_select_append_format(&builder,
            "machine_select report registers=%zu globals=%zu functions=%zu with_calls=%zu with_spills=%zu with_memory_ops=%zu with_branches=%zu\n",
            report->program.register_bank.register_count,
            report->program.global_count,
            report->program.function_count,
            report->functions_with_calls_count,
            report->functions_with_spills_count,
            report->functions_with_memory_ops_count,
            report->functions_with_branches_count)) {
        free(builder.data);
        return 0;
    }
    if (!machine_select_append_format(
            &builder,
            "target_policy preview_reg_cap=%zu imm_legalization=%d cmpbr_fusion=%d preserves_spills=%d preserves_global_slots=%d\n",
            report->target_policy_summary.current_riscv32_preview_logical_register_cap,
            report->target_policy_summary.supports_early_immediate_legalization,
            report->target_policy_summary.supports_compare_branch_fusion,
            report->target_policy_summary.preserves_spill_operands_for_later_materialization,
            report->target_policy_summary.preserves_global_slot_ops_for_later_address_formation)) {
        free(builder.data);
        return 0;
    }

    if (!machine_select_append_format(&builder, "functions_with_calls:")) {
        free(builder.data);
        return 0;
    }
    for (filter_index = 0; filter_index < report->functions_with_calls_count; ++filter_index) {
        if (!machine_select_append_format(&builder, " %zu", report->function_indices_with_calls[filter_index])) {
            free(builder.data);
            return 0;
        }
    }
    if (!machine_select_append_format(&builder, "\nfunctions_with_spills:")) {
        free(builder.data);
        return 0;
    }
    for (filter_index = 0; filter_index < report->functions_with_spills_count; ++filter_index) {
        if (!machine_select_append_format(&builder, " %zu", report->function_indices_with_spills[filter_index])) {
            free(builder.data);
            return 0;
        }
    }
    if (!machine_select_append_format(&builder, "\nfunctions_with_memory_ops:")) {
        free(builder.data);
        return 0;
    }
    for (filter_index = 0; filter_index < report->functions_with_memory_ops_count; ++filter_index) {
        if (!machine_select_append_format(&builder, " %zu", report->function_indices_with_memory_ops[filter_index])) {
            free(builder.data);
            return 0;
        }
    }
    if (!machine_select_append_format(&builder, "\nfunctions_with_branches:")) {
        free(builder.data);
        return 0;
    }
    for (filter_index = 0; filter_index < report->functions_with_branches_count; ++filter_index) {
        if (!machine_select_append_format(&builder, " %zu", report->function_indices_with_branches[filter_index])) {
            free(builder.data);
            return 0;
        }
    }

    if (!machine_select_append_format(&builder, "\nfunction_summaries:\n")) {
        free(builder.data);
        return 0;
    }
    for (function_index = 0; function_index < report->function_summary_count; ++function_index) {
        const MachineSelectFunction *function = &report->program.functions[function_index];
        const MachineSelectFunctionSummary *summary = &report->function_summaries[function_index];
        size_t block_offset = report->function_block_summary_offsets ? report->function_block_summary_offsets[function_index] : 0;
        size_t next_block_offset = function_index + 1 < report->program.function_count && report->function_block_summary_offsets
            ? report->function_block_summary_offsets[function_index + 1]
            : report->total_block_summary_count;
        size_t block_summary_index;

        if (next_block_offset < block_offset || next_block_offset > report->total_block_summary_count) {
            machine_select_set_error(error, 0, 0, "MACHINE-SELECT-434: report block summary offsets drift");
            free(builder.data);
            return 0;
        }

        if (!machine_select_append_format(&builder,
                "  fn.%zu %s blocks=%zu ops=%zu calls=%zu spills=%d load_local=%zu store_local=%zu store_locali=%zu load_global=%zu store_global=%zu store_globali=%zu ret=%zu reti=%zu retspill=%zu br=%zu cmpbr=%zu cmpbri=%zu blocks_with_calls=%zu blocks_with_memory_ops=%zu branch_blocks=%zu jump_blocks=%zu return_blocks=%zu\n",
                function_index,
                function->name ? function->name : "<unnamed>",
                summary->block_count,
                summary->op_count,
                summary->call_count + summary->call_imm_count + summary->call_spill_count +
                    summary->call_imm_spill_count + summary->call_void_count + summary->call_void_imm_count,
                summary->has_spills,
                summary->load_local_count,
                summary->store_local_count,
                summary->store_local_imm_count,
                summary->load_global_count,
                summary->store_global_count,
                summary->store_global_imm_count,
                summary->return_count,
                summary->return_imm_count,
                summary->return_spill_count,
                summary->plain_branch_count,
                summary->compare_branch_count,
                summary->compare_branch_imm_count,
                summary->blocks_with_calls_count,
                summary->blocks_with_memory_ops_count,
                summary->branch_block_count,
                summary->jump_block_count,
                summary->return_block_count)) {
            free(builder.data);
            return 0;
        }

        for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
            const MachineSelectBlockShapeSummary *block_summary = &report->block_summaries[block_summary_index];

            if (!machine_select_append_format(&builder,
                    "    bb.%zu ops=%zu calls=%zu load_local=%zu store_local=%zu store_locali=%zu load_global=%zu store_global=%zu store_globali=%zu term_ret=%d term_jmp=%d term_br=%d term_cmpbr=%d\n",
                    block_summary->block_id,
                    block_summary->op_count,
                    block_summary->call_count,
                    block_summary->load_local_count,
                    block_summary->store_local_count,
                    block_summary->store_local_imm_count,
                    block_summary->load_global_count,
                    block_summary->store_global_count,
                    block_summary->store_global_imm_count,
                    block_summary->has_return_terminator,
                    block_summary->has_jump_terminator,
                    block_summary->has_branch_terminator,
                    block_summary->has_compare_branch_terminator)) {
                free(builder.data);
                return 0;
            }
        }
    }

    if (!machine_select_append_format(&builder, "\n") ||
        !machine_select_dump_program(&report->program, &program_text, error) ||
        !machine_select_append_format(&builder, "%s", program_text ? program_text : "")) {
        free(program_text);
        free(builder.data);
        return 0;
    }

    *out_text = builder.data;
    free(program_text);
    return 1;
}

int machine_select_dump_report_from_machine_ir_program(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectLowerReport report;
    int ok = 0;

    if (!source || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-408: invalid report dump build contract");
        return 0;
    }

    machine_select_lower_report_init(&report);
    if (machine_select_build_report_from_machine_ir_program(source, &report, error)) {
        ok = machine_select_dump_lower_report_artifact(&report, out_text, error);
    }
    machine_select_lower_report_free(&report);
    return ok;
}

int machine_select_dump_report_from_canonicalized_machine_ir_program(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectLowerReport report;
    int ok = 0;

    if (!source || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-411: invalid canonicalized report dump build contract");
        return 0;
    }

    machine_select_lower_report_init(&report);
    if (machine_select_build_report_from_canonicalized_machine_ir_program(source, &report, error)) {
        ok = machine_select_dump_lower_report_artifact(&report, out_text, error);
    }
    machine_select_lower_report_free(&report);
    return ok;
}

int machine_select_dump_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectLowerReport lower_report;
    int ok = 0;

    if (!report || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-409: invalid report-to-report dump contract");
        return 0;
    }

    machine_select_lower_report_init(&lower_report);
    if (machine_select_build_report_from_machine_ir_report(report, &lower_report, error)) {
        ok = machine_select_dump_lower_report_artifact(&lower_report, out_text, error);
    }
    machine_select_lower_report_free(&lower_report);
    return ok;
}

int machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error) {
    MachineIrAllocateRewriteReport report;
    MachineIrError machine_error;
    int ok = 0;

    if (!program || !out_report) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-412: invalid canonicalized flat report entry contract");
        return 0;
    }

    memset(&machine_error, 0, sizeof(machine_error));
    machine_ir_allocate_rewrite_report_init(&report);
    if (machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            program, color_budget, machine_register_count, &report, &machine_error)) {
        ok = machine_select_build_report_from_machine_ir_report(&report, out_report, error);
    } else {
        machine_select_set_error(error, machine_error.line, machine_error.column, "%s", machine_error.message);
    }
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineSelectError *error) {
    MachineSelectLowerReport report;
    int ok = 0;

    if (!program || !out_text) {
        machine_select_set_error(error, 0, 0, "MACHINE-SELECT-413: invalid canonicalized flat report dump contract");
        return 0;
    }

    machine_select_lower_report_init(&report);
    if (machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
            program, color_budget, machine_register_count, &report, error)) {
        ok = machine_select_dump_lower_report_artifact(&report, out_text, error);
    }
    machine_select_lower_report_free(&report);
    return ok;
}

#endif
