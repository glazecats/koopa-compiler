#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

int value_ssa_build_program_machine_allocation_view_from_report(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error) {
    const ValueSsaProgramAllocationLayout *layout = NULL;

    if (!report || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-021: invalid report-to-machine-view contract");
        return 0;
    }

    if (!value_ssa_allocate_rewrite_layout_report_get_program_layout(report, &layout) || !layout) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-022: invalid report program-layout query");
        return 0;
    }

    return value_ssa_build_program_machine_allocation_view(layout, bank, out_view, error);
}

int value_ssa_allocate_function_machine_view(const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error) {
    ValueSsaFunctionAllocationLayout layout;
    int ok = 0;

    if (!function || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-058: invalid function machine-view entry contract");
        return 0;
    }

    value_ssa_function_allocation_layout_init(&layout);
    value_ssa_function_machine_allocation_view_free(out_view);
    if (value_ssa_allocate_function_layout(function, color_budget, &layout, error) &&
        value_ssa_build_function_machine_allocation_view(&layout, bank, out_view, error)) {
        ok = 1;
    }
    value_ssa_function_allocation_layout_free(&layout);
    return ok;
}

int value_ssa_allocate_program_machine_view(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error) {
    ValueSsaProgramAllocationLayout layout;
    int ok = 0;

    if (!program || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-059: invalid program machine-view entry contract");
        return 0;
    }

    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_program_machine_allocation_view_free(out_view);
    if (value_ssa_allocate_program_layout(program, color_budget, &layout, error) &&
        value_ssa_build_program_machine_allocation_view(&layout, bank, out_view, error)) {
        ok = 1;
    }
    value_ssa_program_allocation_layout_free(&layout);
    return ok;
}

int value_ssa_build_allocate_rewrite_machine_report(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaAllocateRewriteMachineReport *out_report,
    ValueSsaError *error) {
    size_t allocation_rounds = 0;
    size_t rewrite_rounds = 0;

    if (!report || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-023: invalid allocate+rewrite machine report build contract");
        return 0;
    }

    value_ssa_allocate_rewrite_machine_report_free(out_report);
    if (!value_ssa_allocate_rewrite_layout_report_get_stats(report, &allocation_rounds, &rewrite_rounds) ||
        !value_ssa_build_program_machine_allocation_view_from_report(report, bank, &out_report->view, error)) {
        value_ssa_allocate_rewrite_machine_report_free(out_report);
        return 0;
    }

    out_report->stats.allocation_rounds = allocation_rounds;
    out_report->stats.rewrite_rounds = rewrite_rounds;
    return 1;
}

int value_ssa_dump_allocate_rewrite_machine_report_artifact(
    const ValueSsaAllocateRewriteMachineReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    char *machine_text = NULL;
    size_t allocation_rounds = 0;
    size_t rewrite_rounds = 0;
    const ValueSsaProgramMachineAllocationView *view = NULL;
    int ok = 0;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-024: invalid allocate+rewrite machine report artifact contract");
        return 0;
    }
    if (!value_ssa_allocate_rewrite_machine_report_get_stats(report, &allocation_rounds, &rewrite_rounds) ||
        !value_ssa_allocate_rewrite_machine_report_get_program_view(report, &view) || !view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-025: invalid machine report query contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_machine_append_format(&builder,
            "allocate+rewrite machine-report allocation_rounds=%zu rewrite_rounds=%zu\n",
            allocation_rounds,
            rewrite_rounds) ||
        !value_ssa_dump_program_machine_allocation_view(view, &machine_text, error) ||
        !value_ssa_machine_append_format(&builder, "%s", machine_text ? machine_text : "")) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-026: out of memory building allocate+rewrite machine report");
        goto cleanup;
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(machine_text);
    free(builder.data);
    return ok;
}

int value_ssa_dump_allocate_rewrite_machine_report(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocateRewriteMachineReport machine_report;
    int ok = 0;

    if (!report || !bank || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-027: invalid allocate+rewrite machine report dump contract");
        return 0;
    }

    value_ssa_allocate_rewrite_machine_report_init(&machine_report);
    if (value_ssa_build_allocate_rewrite_machine_report(report, bank, &machine_report, error)) {
        ok = value_ssa_dump_allocate_rewrite_machine_report_artifact(&machine_report, out_text, error);
    }
    value_ssa_allocate_rewrite_machine_report_free(&machine_report);
    return ok;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report(
    ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaAllocateRewriteMachineReport *out_report,
    ValueSsaError *error) {
    ValueSsaAllocateRewriteLayoutReport layout_report;
    int ok = 0;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-054: invalid allocate+rewrite machine report entry contract");
        return 0;
    }

    value_ssa_allocate_rewrite_layout_report_init(&layout_report);
    value_ssa_allocate_rewrite_machine_report_free(out_report);
    if (value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(
            program, color_budget, &layout_report, error) &&
        value_ssa_build_allocate_rewrite_machine_report(&layout_report, bank, out_report, error)) {
        ok = 1;
    }
    value_ssa_allocate_rewrite_layout_report_free(&layout_report);
    return ok;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report_dump(
    ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocateRewriteMachineReport machine_report;
    int ok = 0;

    if (!program || !bank || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-055: invalid allocate+rewrite machine report dump entry contract");
        return 0;
    }

    value_ssa_allocate_rewrite_machine_report_init(&machine_report);
    if (value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report(
            program, color_budget, bank, &machine_report, error)) {
        ok = value_ssa_dump_allocate_rewrite_machine_report_artifact(&machine_report, out_text, error);
    }
    value_ssa_allocate_rewrite_machine_report_free(&machine_report);
    return ok;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_flat_machine_report(
    ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaAllocateRewriteMachineReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-056: invalid flat-machine report entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report(
            program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_allocate_function_flat_machine_view(const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-060: invalid flat function machine-view entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_allocate_function_machine_view(function, color_budget, &bank, out_view, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_allocate_program_flat_machine_view(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-061: invalid flat program machine-view entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_allocate_program_machine_view(program, color_budget, &bank, out_view, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_allocate_function_machine_view_dump(const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaFunctionMachineAllocationView view;
    int ok = 0;

    if (!function || !bank || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-062: invalid function machine-view dump entry contract");
        return 0;
    }

    value_ssa_function_machine_allocation_view_init(&view);
    if (value_ssa_allocate_function_machine_view(function, color_budget, bank, &view, error)) {
        ok = value_ssa_dump_function_machine_allocation_view(&view, out_text, error);
    }
    value_ssa_function_machine_allocation_view_free(&view);
    return ok;
}

int value_ssa_allocate_program_machine_view_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaProgramMachineAllocationView view;
    int ok = 0;

    if (!program || !bank || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-063: invalid program machine-view dump entry contract");
        return 0;
    }

    value_ssa_program_machine_allocation_view_init(&view);
    if (value_ssa_allocate_program_machine_view(program, color_budget, bank, &view, error)) {
        ok = value_ssa_dump_program_machine_allocation_view(&view, out_text, error);
    }
    value_ssa_program_machine_allocation_view_free(&view);
    return ok;
}

int value_ssa_allocate_function_flat_machine_view_dump(const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-064: invalid flat function machine-view dump entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_allocate_function_machine_view_dump(function, color_budget, &bank, out_text, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_allocate_program_flat_machine_view_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-065: invalid flat program machine-view dump entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_allocate_program_machine_view_dump(program, color_budget, &bank, out_text, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_flat_machine_report_dump(
    ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-057: invalid flat-machine report dump entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_allocate_and_rewrite_program_single_block_spills_machine_report_dump(
            program, color_budget, &bank, out_text, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

#endif
