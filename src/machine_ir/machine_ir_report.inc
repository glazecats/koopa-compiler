#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

static void machine_ir_allocate_rewrite_report_clear_shape(MachineIrAllocateRewriteReport *report) {
    if (!report) {
        return;
    }

    free(report->function_shape_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_shape_summaries);
    free(report->function_phi_counts);
    free(report->function_call_counts);
    free(report->function_spill_slot_counts);
    free(report->function_load_local_counts);
    free(report->function_store_local_counts);
    free(report->function_load_global_counts);
    free(report->function_store_global_counts);
    free(report->function_memory_op_counts);
    free(report->function_indices_with_phi);
    free(report->function_indices_with_call);
    free(report->function_indices_with_spills);
    free(report->function_indices_with_memory_ops);
    free(report->function_indices_register_only);

    report->function_shape_summaries = NULL;
    report->function_block_summary_offsets = NULL;
    report->block_shape_summaries = NULL;
    report->total_block_summary_count = 0;
    report->function_phi_counts = NULL;
    report->function_call_counts = NULL;
    report->function_spill_slot_counts = NULL;
    report->function_load_local_counts = NULL;
    report->function_store_local_counts = NULL;
    report->function_load_global_counts = NULL;
    report->function_store_global_counts = NULL;
    report->function_memory_op_counts = NULL;
    report->functions_with_phi_count = 0;
    report->functions_with_call_count = 0;
    report->functions_with_spills_count = 0;
    report->functions_with_memory_ops_count = 0;
    report->functions_register_only_count = 0;
    report->function_indices_with_phi = NULL;
    report->function_indices_with_call = NULL;
    report->function_indices_with_spills = NULL;
    report->function_indices_with_memory_ops = NULL;
    report->function_indices_register_only = NULL;
}

int machine_ir_allocate_rewrite_report_refresh_shape(MachineIrAllocateRewriteReport *report,
    MachineIrError *error) {
    size_t total_block_summary_count = 0;
    size_t function_index;

    if (!report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-412: invalid machine-ir report refresh contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_clear_shape(report);
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        total_block_summary_count += report->program.functions[function_index].block_count;
    }

    if (report->program.function_count > 0) {
        report->function_shape_summaries =
            (MachineIrFunctionShapeSummary *)calloc(report->program.function_count, sizeof(MachineIrFunctionShapeSummary));
        report->function_block_summary_offsets =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_phi_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_call_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_spill_slot_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_load_local_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_store_local_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_load_global_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_store_global_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->function_memory_op_counts =
            (size_t *)calloc(report->program.function_count, sizeof(size_t));
        report->block_shape_summaries =
            total_block_summary_count > 0
                ? (MachineIrBlockShapeSummary *)calloc(total_block_summary_count, sizeof(MachineIrBlockShapeSummary))
                : NULL;
        report->total_block_summary_count = total_block_summary_count;
        report->function_indices_with_phi =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_call =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_spills =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_with_memory_ops =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        report->function_indices_register_only =
            (size_t *)malloc(report->program.function_count * sizeof(size_t));
        if (!report->function_shape_summaries || !report->function_block_summary_offsets ||
            !report->function_phi_counts || !report->function_call_counts ||
            !report->function_spill_slot_counts || !report->function_load_local_counts ||
            !report->function_store_local_counts || !report->function_load_global_counts ||
            !report->function_store_global_counts || !report->function_memory_op_counts ||
            (total_block_summary_count > 0 && !report->block_shape_summaries) ||
            !report->function_indices_with_phi || !report->function_indices_with_call ||
            !report->function_indices_with_spills || !report->function_indices_with_memory_ops ||
            !report->function_indices_register_only) {
            machine_ir_set_error(error, 0, 0, "MACHINE-IR-411: out of memory building machine-ir report indices");
            machine_ir_allocate_rewrite_report_clear_shape(report);
            return 0;
        }
    }

    total_block_summary_count = 0;
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        const MachineIrFunction *function = &report->program.functions[function_index];
        MachineIrFunctionShapeSummary *summary = &report->function_shape_summaries[function_index];
        size_t block_index;
        int has_phi = 0;
        int has_call = 0;

        report->function_block_summary_offsets[function_index] = total_block_summary_count;
        summary->block_count = function->block_count;
        report->function_spill_slot_counts[function_index] = function->spill_slot_count;
        summary->spill_slot_count = function->spill_slot_count;
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineIrBasicBlock *block = &function->blocks[block_index];
            MachineIrBlockShapeSummary *block_summary = &report->block_shape_summaries[total_block_summary_count++];
            size_t instruction_index;

            block_summary->block_id = block->id;
            block_summary->phi_count = block->phi_count;
            block_summary->instruction_count = block->instruction_count;
            summary->instruction_count += block->instruction_count;
            if (block->phi_count > 0) {
                has_phi = 1;
                report->function_phi_counts[function_index] += block->phi_count;
                summary->phi_count += block->phi_count;
                ++summary->blocks_with_phi_count;
            }
            for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
                switch (block->instructions[instruction_index].kind) {
                    case MACHINE_IR_INSTR_CALL:
                        has_call = 1;
                        ++report->function_call_counts[function_index];
                        ++summary->call_count;
                        ++block_summary->call_count;
                        break;
                    case MACHINE_IR_INSTR_LOAD_LOCAL:
                        ++report->function_load_local_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->load_local_count;
                        ++summary->memory_op_count;
                        ++block_summary->load_local_count;
                        ++block_summary->memory_op_count;
                        break;
                    case MACHINE_IR_INSTR_STORE_LOCAL:
                        ++report->function_store_local_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->store_local_count;
                        ++summary->memory_op_count;
                        ++block_summary->store_local_count;
                        ++block_summary->memory_op_count;
                        break;
                    case MACHINE_IR_INSTR_LOAD_GLOBAL:
                        ++report->function_load_global_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->load_global_count;
                        ++summary->memory_op_count;
                        ++block_summary->load_global_count;
                        ++block_summary->memory_op_count;
                        break;
                    case MACHINE_IR_INSTR_STORE_GLOBAL:
                        ++report->function_store_global_counts[function_index];
                        ++report->function_memory_op_counts[function_index];
                        ++summary->store_global_count;
                        ++summary->memory_op_count;
                        ++block_summary->store_global_count;
                        ++block_summary->memory_op_count;
                        break;
                    default:
                        break;
                }
            }
            if (block_summary->call_count > 0) {
                ++summary->blocks_with_call_count;
            }
            if (block_summary->memory_op_count > 0) {
                ++summary->blocks_with_memory_ops_count;
            }
            if (block->has_terminator) {
                switch (block->terminator.kind) {
                    case MACHINE_IR_TERM_RETURN:
                        block_summary->has_return_terminator = 1;
                        ++summary->return_block_count;
                        break;
                    case MACHINE_IR_TERM_JUMP:
                        block_summary->has_jump_terminator = 1;
                        ++summary->jump_block_count;
                        break;
                    case MACHINE_IR_TERM_BRANCH:
                        block_summary->has_branch_terminator = 1;
                        ++summary->branch_block_count;
                        break;
                    default:
                        break;
                }
            }
        }

        if (has_phi) {
            report->function_indices_with_phi[report->functions_with_phi_count++] = function_index;
        }
        if (has_call) {
            report->function_indices_with_call[report->functions_with_call_count++] = function_index;
        }
        if (function->spill_slot_count > 0) {
            report->function_indices_with_spills[report->functions_with_spills_count++] = function_index;
        }
        if (summary->memory_op_count > 0) {
            report->function_indices_with_memory_ops[report->functions_with_memory_ops_count++] = function_index;
        }
        if (function->spill_slot_count == 0) {
            report->function_indices_register_only[report->functions_register_only_count++] = function_index;
        }
    }

    return 1;
}

int machine_ir_lower_program_from_value_ssa_dump(const ValueSsaProgram *program,
    const ValueSsaProgramMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !machine_view || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-400: invalid Value-SSA machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_lower_program_from_value_ssa(program, machine_view, bank, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_lower_program_from_value_ssa_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-401: invalid allocate+lower machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_lower_program_from_value_ssa(program, color_budget, bank, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_lower_program_from_value_ssa_flat_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-402: invalid flat allocate+lower machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_lower_program_from_value_ssa_flat(
             program, color_budget, machine_register_count, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    ValueSsaProgram rewritten_program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !bank || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-403: invalid allocate+rewrite machine-IR contract");
        return 0;
    }

    value_ssa_program_init(&rewritten_program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_program_machine_allocation_view_init(&machine_view);
    memset(&value_error, 0, sizeof(value_error));

    if (!machine_ir_clone_value_ssa_program(program, &rewritten_program, error)) {
        goto cleanup;
    }
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills(
            &rewritten_program, color_budget, &result, &value_error) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &value_error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &rewritten_program, &value_error) ||
        !value_ssa_build_program_machine_allocation_view(&layout, bank, &machine_view, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    ok = machine_ir_lower_program_from_value_ssa(&rewritten_program, &machine_view, bank, out_program, error);

cleanup:
    value_ssa_program_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&rewritten_program);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills_flat(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrProgram *out_program,
    MachineIrError *error) {
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !out_program) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-404: invalid flat allocate+rewrite machine-IR contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    memset(&value_error, 0, sizeof(value_error));
    if (!value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }
    ok = machine_ir_allocate_and_rewrite_program_single_block_spills(
        program, color_budget, &bank, out_program, error);
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-405: invalid allocate+rewrite machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_and_rewrite_program_single_block_spills(
             program, color_budget, bank, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_allocate_and_rewrite_program_single_block_spills_flat_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrProgram machine_program;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-406: invalid flat allocate+rewrite machine-IR dump contract");
        return 0;
    }

    machine_ir_program_init(&machine_program);
    ok = machine_ir_allocate_and_rewrite_program_single_block_spills_flat(
             program, color_budget, machine_register_count, &machine_program, error) &&
        machine_ir_dump_program(&machine_program, out_text, error);
    machine_ir_program_free(&machine_program);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    ValueSsaProgram rewritten_program;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaProgramMachineAllocationView machine_view;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !bank || !out_report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-407: invalid allocate+rewrite machine-ir report contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_free(out_report);
    value_ssa_program_init(&rewritten_program);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_program_machine_allocation_view_init(&machine_view);
    memset(&value_error, 0, sizeof(value_error));

    if (!machine_ir_clone_value_ssa_program(program, &rewritten_program, error)) {
        goto cleanup;
    }
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
            &rewritten_program, color_budget, &result, &stats, &value_error) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &value_error) ||
        !value_ssa_program_allocation_layout_attach_program_context(&layout, &rewritten_program, &value_error) ||
        !value_ssa_build_program_machine_allocation_view(&layout, bank, &machine_view, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        goto cleanup;
    }
    out_report->allocation_rounds = stats.allocation_rounds;
    out_report->rewrite_rounds = stats.rewrite_rounds;
    ok = machine_ir_lower_program_from_value_ssa(
        &rewritten_program, &machine_view, bank, &out_report->program, error);
    if (!ok) {
        goto cleanup;
    }

    if (!machine_ir_allocate_rewrite_report_refresh_shape(out_report, error)) {
        ok = 0;
        goto cleanup;
    }

cleanup:
    if (!ok) {
        machine_ir_allocate_rewrite_report_free(out_report);
    }
    value_ssa_program_machine_allocation_view_free(&machine_view);
    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&rewritten_program);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    ValueSsaMachineRegisterBank bank;
    ValueSsaError value_error;
    int ok = 0;

    if (!program || !out_report) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-408: invalid flat allocate+rewrite machine-ir report contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    memset(&value_error, 0, sizeof(value_error));
    if (!value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, &value_error)) {
        machine_ir_set_error_from_value_ssa(error, &value_error);
        return 0;
    }
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
        program, color_budget, &bank, out_report, error);
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !bank || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-409: invalid allocate+rewrite machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
             program, color_budget, bank, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-410: invalid flat allocate+rewrite machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    if (!machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            program, color_budget, machine_register_count, out_report, error)) {
        return 0;
    }
    if (!machine_ir_eliminate_phi_nodes_in_report(out_report, error)) {
        machine_ir_allocate_rewrite_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-413: invalid phi-eliminated machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    if (!machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
            program, color_budget, machine_register_count, out_report, error)) {
        return 0;
    }
    if (!machine_ir_canonicalize_program_in_report(out_report, error)) {
        machine_ir_allocate_rewrite_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-519: invalid cleaned machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error) {
    return machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
        program, color_budget, machine_register_count, out_report, error);
}

int machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error) {
    MachineIrAllocateRewriteReport report;
    int ok;

    if (out_text) {
        *out_text = NULL;
    }
    if (!program || !out_text) {
        machine_ir_set_error(error, 0, 0, "MACHINE-IR-530: invalid canonicalized machine-ir report dump contract");
        return 0;
    }

    machine_ir_allocate_rewrite_report_init(&report);
    ok = machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
             program, color_budget, machine_register_count, &report, error) &&
        machine_ir_dump_allocate_rewrite_report(&report, out_text, error);
    machine_ir_allocate_rewrite_report_free(&report);
    return ok;
}

#endif
