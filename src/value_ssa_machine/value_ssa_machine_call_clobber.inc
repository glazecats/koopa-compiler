#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

static ValueSsaMachineCallClobberRiskClass value_ssa_machine_classify_call_clobber_risk(
    size_t call_crossing_count,
    size_t call_density_sum,
    size_t use_call_density_sum) {
    if (call_crossing_count == 0) {
        return VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_NONE;
    }
    if (call_crossing_count >= 2 || use_call_density_sum >= 2) {
        return VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT;
    }
    if (call_density_sum >= 2 || use_call_density_sum >= 1) {
        return VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED;
    }
    return VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
}

const char *value_ssa_machine_call_clobber_risk_class_name(
    ValueSsaMachineCallClobberRiskClass risk_class) {
    switch (risk_class) {
    case VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_NONE:
        return "none";
    case VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING:
        return "crossing";
    case VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED:
        return "repeated";
    case VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT:
        return "hot";
    default:
        return "unknown";
    }
}

void value_ssa_machine_call_clobber_risk_report_init(ValueSsaMachineCallClobberRiskReport *report) {
    size_t risk_class;

    if (!report) {
        return;
    }

    report->value_count = 0;
    report->item_count = 0;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        report->risk_class_counts[risk_class] = 0;
    }
    report->items = NULL;
}

void value_ssa_machine_call_clobber_risk_report_free(ValueSsaMachineCallClobberRiskReport *report) {
    if (!report) {
        return;
    }

    free(report->items);
    value_ssa_machine_call_clobber_risk_report_init(report);
}

void value_ssa_function_machine_call_clobber_risk_view_init(ValueSsaFunctionMachineCallClobberRiskView *view) {
    if (!view) {
        return;
    }

    view->function_name = NULL;
    value_ssa_machine_call_clobber_risk_report_init(&view->risk_report);
}

void value_ssa_function_machine_call_clobber_risk_view_free(ValueSsaFunctionMachineCallClobberRiskView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    value_ssa_machine_call_clobber_risk_report_free(&view->risk_report);
    value_ssa_function_machine_call_clobber_risk_view_init(view);
}

void value_ssa_program_machine_call_clobber_risk_report_init(ValueSsaProgramMachineCallClobberRiskReport *report) {
    size_t risk_class;

    if (!report) {
        return;
    }

    report->function_count = 0;
    report->function_views = NULL;
    report->total_value_count = 0;
    report->total_risky_value_count = 0;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        report->total_risk_class_counts[risk_class] = 0;
        report->functions_with_risk_class_counts[risk_class] = 0;
        report->function_indices_with_risk_class[risk_class] = NULL;
    }
    report->functions_with_risky_values_count = 0;
    report->function_indices_with_risky_values = NULL;
}

void value_ssa_program_machine_call_clobber_risk_report_free(ValueSsaProgramMachineCallClobberRiskReport *report) {
    size_t function_index;
    size_t risk_class;

    if (!report) {
        return;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        value_ssa_function_machine_call_clobber_risk_view_free(&report->function_views[function_index]);
    }
    free(report->function_views);
    free(report->function_indices_with_risky_values);
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        free(report->function_indices_with_risk_class[risk_class]);
    }
    value_ssa_program_machine_call_clobber_risk_report_init(report);
}

int value_ssa_build_machine_call_clobber_risk_report(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineCallClobberRiskReport *out_report,
    ValueSsaError *error) {
    size_t value_id;

    if (!machine_view || !allocation_prep || !out_report || machine_view->value_count != allocation_prep->value_count) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-066: invalid machine call-clobber risk build contract");
        return 0;
    }

    value_ssa_machine_call_clobber_risk_report_free(out_report);
    out_report->value_count = machine_view->value_count;
    if (machine_view->caller_clobbered_value_count == 0) {
        return 1;
    }

    out_report->items = (ValueSsaMachineCallClobberRiskItem *)calloc(
        machine_view->caller_clobbered_value_count, sizeof(ValueSsaMachineCallClobberRiskItem));
    if (!out_report->items) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-067: out of memory building machine call-clobber risk report");
        value_ssa_machine_call_clobber_risk_report_free(out_report);
        return 0;
    }

    for (value_id = 0; value_id < machine_view->caller_clobbered_value_count; ++value_id) {
        size_t risky_value_id = machine_view->caller_clobbered_value_ids[value_id];
        const ValueSsaMachineAllocatedValuePlacement *placement = &machine_view->placements[risky_value_id];
        ValueSsaMachineCallClobberRiskItem *item;
        size_t crossing_count;

        if (!allocation_prep->call_crossing_counts || risky_value_id >= allocation_prep->value_count) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-068: invalid allocation prep for machine call-clobber risk report");
            value_ssa_machine_call_clobber_risk_report_free(out_report);
            return 0;
        }

        crossing_count = allocation_prep->call_crossing_counts[risky_value_id];
        if (crossing_count == 0) {
            continue;
        }

        item = &out_report->items[out_report->item_count++];
        item->value_id = risky_value_id;
        item->machine_register_id = placement->machine_register_id;
        item->machine_register_name = placement->machine_register_name;
        item->call_crossing_count = crossing_count;
        item->call_density_sum =
            allocation_prep->call_density_sums ? allocation_prep->call_density_sums[risky_value_id] : 0;
        item->use_call_density_sum =
            allocation_prep->use_call_density_sums ? allocation_prep->use_call_density_sums[risky_value_id] : 0;
        item->risk_class = value_ssa_machine_classify_call_clobber_risk(
            item->call_crossing_count, item->call_density_sum, item->use_call_density_sum);
        if (item->risk_class > VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_NONE &&
            item->risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT) {
            ++out_report->risk_class_counts[item->risk_class];
        }
    }

    return 1;
}

int value_ssa_compute_function_machine_call_clobber_risk(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineCallClobberRiskReport *out_report,
    ValueSsaError *error) {
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaAllocationPrep owned_prep;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    int ok = 0;

    if (!function || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-069: invalid function machine call-clobber risk entry contract");
        return 0;
    }

    value_ssa_function_machine_allocation_view_init(&machine_view);
    value_ssa_allocation_prep_init(&owned_prep);
    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    }
    if (value_ssa_allocate_function_machine_view(function, color_budget, bank, &machine_view, error) &&
        value_ssa_build_machine_call_clobber_risk_report(&machine_view, active_prep, out_report, error)) {
        ok = 1;
    }

cleanup:
    value_ssa_allocation_prep_free(&owned_prep);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    return ok;
}

int value_ssa_compute_function_flat_machine_call_clobber_risk(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineCallClobberRiskReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-070: invalid flat machine call-clobber risk entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_function_machine_call_clobber_risk(
            function, color_budget, &bank, allocation_prep, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_program_machine_call_clobber_risk(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineCallClobberRiskReport *out_report,
    ValueSsaError *error) {
    size_t function_index;
    size_t risk_class;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-074: invalid program machine call-clobber risk entry contract");
        return 0;
    }

    value_ssa_program_machine_call_clobber_risk_report_free(out_report);
    out_report->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_report->function_views =
        (ValueSsaFunctionMachineCallClobberRiskView *)calloc(program->function_count, sizeof(ValueSsaFunctionMachineCallClobberRiskView));
    out_report->function_indices_with_risky_values = (size_t *)malloc(program->function_count * sizeof(size_t));
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        out_report->function_indices_with_risk_class[risk_class] =
            (size_t *)malloc(program->function_count * sizeof(size_t));
    }
    if (!out_report->function_views || !out_report->function_indices_with_risky_values) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-075: out of memory building program machine call-clobber risk report");
        value_ssa_program_machine_call_clobber_risk_report_free(out_report);
        return 0;
    }
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        if (!out_report->function_indices_with_risk_class[risk_class]) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-084: out of memory building risk-class function filters");
            value_ssa_program_machine_call_clobber_risk_report_free(out_report);
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunctionMachineCallClobberRiskView *function_view = &out_report->function_views[function_index];

        value_ssa_function_machine_call_clobber_risk_view_init(function_view);
        if (program->functions[function_index].name) {
            function_view->function_name = value_ssa_machine_strdup(program->functions[function_index].name);
            if (!function_view->function_name) {
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-076: out of memory copying risk-view function name");
                value_ssa_program_machine_call_clobber_risk_report_free(out_report);
                return 0;
            }
        }
        if (!value_ssa_compute_function_machine_call_clobber_risk(
                &program->functions[function_index],
                color_budget,
                bank,
                NULL,
                &function_view->risk_report,
                error)) {
            value_ssa_program_machine_call_clobber_risk_report_free(out_report);
            return 0;
        }

        out_report->total_value_count += function_view->risk_report.value_count;
        out_report->total_risky_value_count += function_view->risk_report.item_count;
        if (function_view->risk_report.item_count > 0) {
            out_report->function_indices_with_risky_values[out_report->functions_with_risky_values_count++] = function_index;
        }
        for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
             risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
             ++risk_class) {
            out_report->total_risk_class_counts[risk_class] += function_view->risk_report.risk_class_counts[risk_class];
            if (function_view->risk_report.risk_class_counts[risk_class] > 0) {
                out_report->function_indices_with_risk_class[risk_class]
                    [out_report->functions_with_risk_class_counts[risk_class]++] = function_index;
            }
        }
    }

    return 1;
}

int value_ssa_compute_program_flat_machine_call_clobber_risk(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineCallClobberRiskReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-077: invalid flat program machine call-clobber risk entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_program_machine_call_clobber_risk(program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_machine_call_clobber_risk_report_get_summary(
    const ValueSsaMachineCallClobberRiskReport *report,
    size_t *out_value_count,
    size_t *out_total_risky_value_count) {
    if (!report) {
        return 0;
    }

    if (out_value_count) {
        *out_value_count = report->value_count;
    }
    if (out_total_risky_value_count) {
        *out_total_risky_value_count = report->item_count;
    }
    return 1;
}

int value_ssa_machine_call_clobber_risk_report_get_class_count(
    const ValueSsaMachineCallClobberRiskReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0;
    }
    if (!report || risk_class >= VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT) {
        return 0;
    }

    if (out_count) {
        *out_count = report->risk_class_counts[risk_class];
    }
    return 1;
}

int value_ssa_machine_call_clobber_risk_report_find_value(
    const ValueSsaMachineCallClobberRiskReport *report,
    size_t value_id,
    size_t *out_index,
    const ValueSsaMachineCallClobberRiskItem **out_item) {
    size_t item_index;

    if (out_index) {
        *out_index = 0;
    }
    if (out_item) {
        *out_item = NULL;
    }
    if (!report) {
        return 0;
    }

    for (item_index = 0; item_index < report->item_count; ++item_index) {
        if (report->items[item_index].value_id == value_id) {
            if (out_index) {
                *out_index = item_index;
            }
            if (out_item) {
                *out_item = &report->items[item_index];
            }
            return 1;
        }
    }
    return 0;
}

int value_ssa_program_machine_call_clobber_risk_report_get_class_count(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0;
    }
    if (!report || risk_class >= VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT) {
        return 0;
    }

    if (out_count) {
        *out_count = report->total_risk_class_counts[risk_class];
    }
    return 1;
}

int value_ssa_program_machine_call_clobber_risk_report_get_summary(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_risky_value_count) {
    if (!report) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = report->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = report->total_value_count;
    }
    if (out_total_risky_value_count) {
        *out_total_risky_value_count = report->total_risky_value_count;
    }
    return 1;
}

int value_ssa_program_machine_call_clobber_risk_report_get_function_view(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineCallClobberRiskView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || function_index >= report->function_count || !report->function_views) {
        return 0;
    }

    *out_view = &report->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_call_clobber_risk_report_get_function_view_by_name(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineCallClobberRiskView **out_view) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !function_name || !report->function_views) {
        return 0;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        const ValueSsaFunctionMachineCallClobberRiskView *function_view = &report->function_views[function_index];

        if (function_view->function_name && strcmp(function_view->function_name, function_name) == 0) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_view) {
                *out_view = function_view;
            }
            return 1;
        }
    }
    return 0;
}

int value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risky_values(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
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
        *out_count = report->functions_with_risky_values_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_risky_values;
    }
    return 1;
}

int value_ssa_program_machine_call_clobber_risk_report_get_functions_with_risk_class(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count,
    const size_t **out_function_indices) {
    size_t count;

    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report || risk_class >= VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT ||
        risk_class == VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_NONE) {
        return 0;
    }

    count = report->functions_with_risk_class_counts[risk_class];
    if (out_count) {
        *out_count = count;
    }
    if (out_function_indices && count > 0) {
        *out_function_indices = report->function_indices_with_risk_class[risk_class];
    }
    return 1;
}

int value_ssa_dump_machine_call_clobber_risk_report(
    const ValueSsaMachineCallClobberRiskReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t item_index;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-071: invalid machine call-clobber risk dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "machine-call-clobber-risk values=%zu risky=%zu crossing=%zu repeated=%zu hot=%zu\n",
            report->value_count,
            report->item_count,
            report->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
            report->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
            report->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-072: out of memory building machine call-clobber risk header");
        return 0;
    }

    for (item_index = 0; item_index < report->item_count; ++item_index) {
        const ValueSsaMachineCallClobberRiskItem *item = &report->items[item_index];

        if (!value_ssa_machine_append_format(&builder,
                "ssa.%zu reg=%s class=%s call_cross=%zu calls=%zu use_calls=%zu\n",
                item->value_id,
                item->machine_register_name ? item->machine_register_name : "<none>",
                value_ssa_machine_call_clobber_risk_class_name(item->risk_class),
                item->call_crossing_count,
                item->call_density_sum,
                item->use_call_density_sum)) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-073: out of memory appending machine call-clobber item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_call_clobber_risk_report(
    const ValueSsaProgramMachineCallClobberRiskReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    size_t risk_class;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-078: invalid program machine call-clobber risk dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "program-machine-call-clobber-risk functions=%zu values=%zu risky=%zu risky_funcs=%zu crossing=%zu repeated=%zu hot=%zu\n",
            report->function_count,
            report->total_value_count,
            report->total_risky_value_count,
            report->functions_with_risky_values_count,
            report->total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
            report->total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
            report->total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-079: out of memory building program machine call-clobber risk header");
        return 0;
    }

    if (report->functions_with_risky_values_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-risk:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-080: out of memory appending risk-filter header");
            return 0;
        }
        for (function_index = 0; function_index < report->functions_with_risky_values_count; ++function_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", report->function_indices_with_risky_values[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-081: out of memory appending risk-filter values");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-082: out of memory finishing risk-filter values");
            return 0;
        }
    }
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        size_t member_index;
        size_t member_count = report->functions_with_risk_class_counts[risk_class];

        if (member_count == 0) {
            continue;
        }
        if (!value_ssa_machine_append_format(
                &builder,
                "functions-with-risk-class %s:",
                value_ssa_machine_call_clobber_risk_class_name((ValueSsaMachineCallClobberRiskClass)risk_class))) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-085: out of memory appending risk-class filter header");
            return 0;
        }
        for (member_index = 0; member_index < member_count; ++member_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", report->function_indices_with_risk_class[risk_class][member_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-086: out of memory appending risk-class filter values");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-087: out of memory finishing risk-class filter values");
            return 0;
        }
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        const ValueSsaFunctionMachineCallClobberRiskView *function_view = &report->function_views[function_index];
        char *function_text = NULL;

        if (!value_ssa_machine_append_format(
                &builder,
                "function %s risky=%zu values=%zu crossing=%zu repeated=%zu hot=%zu\n",
                function_view->function_name ? function_view->function_name : "<anon>",
                function_view->risk_report.item_count,
                function_view->risk_report.value_count,
                function_view->risk_report.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
                function_view->risk_report.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
                function_view->risk_report.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-083: out of memory appending function risk header");
            return 0;
        }
        if (!value_ssa_dump_machine_call_clobber_risk_report(&function_view->risk_report, &function_text, error) ||
            !value_ssa_machine_append_format(&builder, "%s", function_text ? function_text : "")) {
            free(function_text);
            free(builder.data);
            return 0;
        }
        free(function_text);
    }

    *out_text = builder.data;
    return 1;
}

#endif
