#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

static int value_ssa_machine_compare_protection_items(
    const ValueSsaMachineProtectionAgendaItem *lhs,
    const ValueSsaMachineProtectionAgendaItem *rhs) {
    if (lhs->protection_priority > rhs->protection_priority) {
        return -1;
    }
    if (lhs->protection_priority < rhs->protection_priority) {
        return 1;
    }
    if ((int)lhs->risk_class > (int)rhs->risk_class) {
        return -1;
    }
    if ((int)lhs->risk_class < (int)rhs->risk_class) {
        return 1;
    }
    if ((int)lhs->rematerializable < (int)rhs->rematerializable) {
        return -1;
    }
    if ((int)lhs->rematerializable > (int)rhs->rematerializable) {
        return 1;
    }
    if ((int)lhs->single_block_live_range < (int)rhs->single_block_live_range) {
        return -1;
    }
    if ((int)lhs->single_block_live_range > (int)rhs->single_block_live_range) {
        return 1;
    }
    if (lhs->value_id < rhs->value_id) {
        return -1;
    }
    if (lhs->value_id > rhs->value_id) {
        return 1;
    }
    return 0;
}

static void value_ssa_machine_sort_protection_agenda(ValueSsaMachineProtectionAgenda *agenda) {
    size_t i;
    size_t j;

    if (!agenda || agenda->item_count < 2 || !agenda->items) {
        return;
    }

    for (i = 1; i < agenda->item_count; ++i) {
        ValueSsaMachineProtectionAgendaItem current = agenda->items[i];
        j = i;
        while (j > 0 && value_ssa_machine_compare_protection_items(&current, &agenda->items[j - 1]) < 0) {
            agenda->items[j] = agenda->items[j - 1];
            --j;
        }
        agenda->items[j] = current;
    }
}

static int value_ssa_machine_compare_register_pressure_items(
    const ValueSsaMachineRegisterProtectionPressureItem *lhs,
    const ValueSsaMachineRegisterProtectionPressureItem *rhs) {
    if (lhs->total_protection_priority > rhs->total_protection_priority) {
        return -1;
    }
    if (lhs->total_protection_priority < rhs->total_protection_priority) {
        return 1;
    }
    if (lhs->max_protection_priority > rhs->max_protection_priority) {
        return -1;
    }
    if (lhs->max_protection_priority < rhs->max_protection_priority) {
        return 1;
    }
    if (lhs->item_count > rhs->item_count) {
        return -1;
    }
    if (lhs->item_count < rhs->item_count) {
        return 1;
    }
    if (lhs->machine_register_id < rhs->machine_register_id) {
        return -1;
    }
    if (lhs->machine_register_id > rhs->machine_register_id) {
        return 1;
    }
    return 0;
}

static void value_ssa_machine_sort_register_pressure_view(
    ValueSsaMachineRegisterProtectionPressureView *view) {
    size_t i;
    size_t j;

    if (!view || view->pressured_register_count < 2 || !view->items) {
        return;
    }

    for (i = 1; i < view->pressured_register_count; ++i) {
        ValueSsaMachineRegisterProtectionPressureItem current = view->items[i];
        j = i;
        while (j > 0 && value_ssa_machine_compare_register_pressure_items(&current, &view->items[j - 1]) < 0) {
            view->items[j] = view->items[j - 1];
            --j;
        }
        view->items[j] = current;
    }
}

static int value_ssa_machine_compare_preservation_hint_candidates(
    const ValueSsaMachinePreservationHintCandidate *lhs,
    const ValueSsaMachinePreservationHintCandidate *rhs) {
    if (lhs->suggested_register_total_priority < rhs->suggested_register_total_priority) {
        return -1;
    }
    if (lhs->suggested_register_total_priority > rhs->suggested_register_total_priority) {
        return 1;
    }
    if (lhs->suggested_register_item_count < rhs->suggested_register_item_count) {
        return -1;
    }
    if (lhs->suggested_register_item_count > rhs->suggested_register_item_count) {
        return 1;
    }
    if (lhs->suggested_register_max_priority < rhs->suggested_register_max_priority) {
        return -1;
    }
    if (lhs->suggested_register_max_priority > rhs->suggested_register_max_priority) {
        return 1;
    }
    if (lhs->suggested_machine_register_id < rhs->suggested_machine_register_id) {
        return -1;
    }
    if (lhs->suggested_machine_register_id > rhs->suggested_machine_register_id) {
        return 1;
    }
    return 0;
}

const char *value_ssa_machine_preservation_reason_name(
    ValueSsaMachinePreservationReason reason) {
    switch (reason) {
    case VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_EMPTY:
        return "target-empty";
    case VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED:
        return "target-less-occupied";
    case VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LOWER_PROTECTION_PRESSURE:
        return "target-lower-protection-pressure";
    case VALUE_SSA_MACHINE_PRESERVATION_REASON_UNKNOWN:
    default:
        return "unknown";
    }
}

static ValueSsaMachinePreservationReason value_ssa_machine_preservation_hint_candidate_reason_code(
    const ValueSsaMachinePreservationHintCandidate *candidate) {
    if (!candidate) {
        return VALUE_SSA_MACHINE_PRESERVATION_REASON_UNKNOWN;
    }
    if (candidate->suggested_register_total_priority > 0) {
        return VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LOWER_PROTECTION_PRESSURE;
    }
    if (candidate->suggested_register_item_count > 0) {
        return VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_LESS_OCCUPIED;
    }
    return VALUE_SSA_MACHINE_PRESERVATION_REASON_TARGET_EMPTY;
}

static void value_ssa_machine_sort_preservation_hint_candidates(
    ValueSsaFunctionMachinePreservationHintView *view) {
    size_t i;
    size_t j;

    if (!view || view->candidate_count < 2 || !view->candidates) {
        return;
    }

    for (i = 1; i < view->candidate_count; ++i) {
        ValueSsaMachinePreservationHintCandidate current = view->candidates[i];
        j = i;
        while (j > 0 && value_ssa_machine_compare_preservation_hint_candidates(&current, &view->candidates[j - 1]) < 0) {
            view->candidates[j] = view->candidates[j - 1];
            --j;
        }
        view->candidates[j] = current;
    }
}

void value_ssa_machine_protection_agenda_init(ValueSsaMachineProtectionAgenda *agenda) {
    size_t risk_class;

    if (!agenda) {
        return;
    }

    agenda->value_count = 0;
    agenda->item_count = 0;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        agenda->risk_class_counts[risk_class] = 0;
    }
    agenda->items = NULL;
}

void value_ssa_machine_protection_agenda_free(ValueSsaMachineProtectionAgenda *agenda) {
    if (!agenda) {
        return;
    }

    free(agenda->items);
    value_ssa_machine_protection_agenda_init(agenda);
}

void value_ssa_function_machine_protection_view_init(ValueSsaFunctionMachineProtectionView *view) {
    if (!view) {
        return;
    }

    view->function_name = NULL;
    value_ssa_machine_protection_agenda_init(&view->agenda);
}

void value_ssa_function_machine_protection_view_free(ValueSsaFunctionMachineProtectionView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    value_ssa_machine_protection_agenda_free(&view->agenda);
    value_ssa_function_machine_protection_view_init(view);
}

void value_ssa_program_machine_protection_report_init(ValueSsaProgramMachineProtectionReport *report) {
    size_t risk_class;

    if (!report) {
        return;
    }

    report->function_count = 0;
    report->function_views = NULL;
    report->total_value_count = 0;
    report->total_item_count = 0;
    report->functions_with_items_count = 0;
    report->function_indices_with_items = NULL;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        report->total_risk_class_counts[risk_class] = 0;
        report->functions_with_risk_class_counts[risk_class] = 0;
        report->function_indices_with_risk_class[risk_class] = NULL;
    }
}

void value_ssa_program_machine_protection_report_free(ValueSsaProgramMachineProtectionReport *report) {
    size_t function_index;
    size_t risk_class;

    if (!report) {
        return;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        value_ssa_function_machine_protection_view_free(&report->function_views[function_index]);
    }
    free(report->function_views);
    free(report->function_indices_with_items);
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        free(report->function_indices_with_risk_class[risk_class]);
    }
    value_ssa_program_machine_protection_report_init(report);
}

void value_ssa_machine_register_protection_pressure_view_init(
    ValueSsaMachineRegisterProtectionPressureView *view) {
    size_t risk_class;

    if (!view) {
        return;
    }

    view->machine_register_count = 0;
    view->pressured_register_count = 0;
    view->total_value_count = 0;
    view->total_item_count = 0;
    view->total_protection_priority = 0;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        view->risk_class_counts[risk_class] = 0;
    }
    view->items = NULL;
}

void value_ssa_machine_register_protection_pressure_view_free(
    ValueSsaMachineRegisterProtectionPressureView *view) {
    size_t item_index;

    if (!view) {
        return;
    }

    if (view->items) {
        for (item_index = 0; item_index < view->pressured_register_count; ++item_index) {
            free(view->items[item_index].value_ids);
        }
    }
    free(view->items);
    value_ssa_machine_register_protection_pressure_view_init(view);
}

void value_ssa_function_machine_register_protection_pressure_view_init(
    ValueSsaFunctionMachineRegisterProtectionPressureView *view) {
    if (!view) {
        return;
    }

    view->function_name = NULL;
    value_ssa_machine_register_protection_pressure_view_init(&view->pressure_view);
}

void value_ssa_function_machine_register_protection_pressure_view_free(
    ValueSsaFunctionMachineRegisterProtectionPressureView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    value_ssa_machine_register_protection_pressure_view_free(&view->pressure_view);
    value_ssa_function_machine_register_protection_pressure_view_init(view);
}

void value_ssa_program_machine_register_protection_pressure_report_init(
    ValueSsaProgramMachineRegisterProtectionPressureReport *report) {
    size_t risk_class;

    if (!report) {
        return;
    }

    report->function_count = 0;
    report->function_views = NULL;
    report->total_value_count = 0;
    report->total_item_count = 0;
    report->total_protection_priority = 0;
    report->total_pressured_register_count = 0;
    report->functions_with_pressure_count = 0;
    report->function_indices_with_pressure = NULL;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        report->risk_class_counts[risk_class] = 0;
        report->functions_with_risk_class_counts[risk_class] = 0;
        report->function_indices_with_risk_class[risk_class] = NULL;
    }
}

void value_ssa_program_machine_register_protection_pressure_report_free(
    ValueSsaProgramMachineRegisterProtectionPressureReport *report) {
    size_t function_index;
    size_t risk_class;

    if (!report) {
        return;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        value_ssa_function_machine_register_protection_pressure_view_free(&report->function_views[function_index]);
    }
    free(report->function_views);
    free(report->function_indices_with_pressure);
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        free(report->function_indices_with_risk_class[risk_class]);
    }
    value_ssa_program_machine_register_protection_pressure_report_init(report);
}

void value_ssa_function_machine_register_protection_hotspot_view_init(
    ValueSsaFunctionMachineRegisterProtectionHotspotView *view) {
    size_t risk_class;

    if (!view) {
        return;
    }

    view->function_name = NULL;
    view->has_hotspot = 0u;
    view->hotspot.machine_register_id = 0;
    view->hotspot.machine_register_name = NULL;
    view->hotspot.item_count = 0;
    view->hotspot.total_protection_priority = 0;
    view->hotspot.max_protection_priority = 0;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        view->hotspot.risk_class_counts[risk_class] = 0;
    }
}

void value_ssa_function_machine_register_protection_hotspot_view_free(
    ValueSsaFunctionMachineRegisterProtectionHotspotView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    value_ssa_function_machine_register_protection_hotspot_view_init(view);
}

void value_ssa_program_machine_register_protection_hotspot_report_init(
    ValueSsaProgramMachineRegisterProtectionHotspotReport *report) {
    if (!report) {
        return;
    }

    report->function_count = 0;
    report->function_views = NULL;
    report->total_value_count = 0;
    report->total_item_count = 0;
    report->total_hotspot_priority = 0;
    report->total_hotspot_item_count = 0;
    report->functions_with_hotspots_count = 0;
    report->function_indices_with_hotspots = NULL;
}

void value_ssa_program_machine_register_protection_hotspot_report_free(
    ValueSsaProgramMachineRegisterProtectionHotspotReport *report) {
    size_t function_index;

    if (!report) {
        return;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        value_ssa_function_machine_register_protection_hotspot_view_free(&report->function_views[function_index]);
    }
    free(report->function_views);
    free(report->function_indices_with_hotspots);
    value_ssa_program_machine_register_protection_hotspot_report_init(report);
}

void value_ssa_function_machine_preservation_hint_view_init(
    ValueSsaFunctionMachinePreservationHintView *view) {
    size_t risk_class;

    if (!view) {
        return;
    }

    view->function_name = NULL;
    view->has_hint = 0u;
    view->source_machine_register_id = 0;
    view->source_machine_register_name = NULL;
    view->suggested_machine_register_id = 0;
    view->suggested_machine_register_name = NULL;
    view->protected_item_count = 0;
    view->protected_total_priority = 0;
    view->protected_max_priority = 0;
    view->candidate_count = 0;
    view->candidates = NULL;
    for (risk_class = 0; risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++risk_class) {
        view->source_risk_class_counts[risk_class] = 0;
    }
}

void value_ssa_function_machine_preservation_hint_view_free(
    ValueSsaFunctionMachinePreservationHintView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    free(view->candidates);
    value_ssa_function_machine_preservation_hint_view_init(view);
}

void value_ssa_program_machine_preservation_hint_report_init(
    ValueSsaProgramMachinePreservationHintReport *report) {
    size_t reason_index;

    if (!report) {
        return;
    }

    report->function_count = 0;
    report->function_views = NULL;
    report->total_value_count = 0;
    report->total_item_count = 0;
    report->total_hint_priority = 0;
    report->functions_with_hints_count = 0;
    report->function_indices_with_hints = NULL;
    for (reason_index = 0; reason_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++reason_index) {
        report->reason_counts[reason_index] = 0;
        report->functions_with_reason_counts[reason_index] = 0;
        report->function_indices_with_reason[reason_index] = NULL;
    }
}

void value_ssa_program_machine_preservation_hint_report_free(
    ValueSsaProgramMachinePreservationHintReport *report) {
    size_t function_index;
    size_t reason_index;

    if (!report) {
        return;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        value_ssa_function_machine_preservation_hint_view_free(&report->function_views[function_index]);
    }
    free(report->function_views);
    free(report->function_indices_with_hints);
    for (reason_index = 0; reason_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++reason_index) {
        free(report->function_indices_with_reason[reason_index]);
    }
    value_ssa_program_machine_preservation_hint_report_init(report);
}

void value_ssa_function_machine_planning_view_init(
    ValueSsaFunctionMachinePlanningView *view) {
    if (!view) {
        return;
    }

    view->function_name = NULL;
    value_ssa_machine_register_protection_pressure_view_init(&view->pressure_view);
    value_ssa_function_machine_register_protection_hotspot_view_init(&view->hotspot_view);
    value_ssa_function_machine_preservation_hint_view_init(&view->preservation_hint_view);
}

void value_ssa_function_machine_planning_view_free(
    ValueSsaFunctionMachinePlanningView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    value_ssa_machine_register_protection_pressure_view_free(&view->pressure_view);
    value_ssa_function_machine_register_protection_hotspot_view_free(&view->hotspot_view);
    value_ssa_function_machine_preservation_hint_view_free(&view->preservation_hint_view);
    value_ssa_function_machine_planning_view_init(view);
}

void value_ssa_program_machine_planning_report_init(
    ValueSsaProgramMachinePlanningReport *report) {
    size_t reason_index;

    if (!report) {
        return;
    }

    report->function_count = 0;
    report->function_views = NULL;
    report->total_value_count = 0;
    report->total_pressure_item_count = 0;
    report->total_pressure_priority = 0;
    report->total_hotspot_priority = 0;
    report->total_hint_priority = 0;
    report->functions_with_pressure_count = 0;
    report->function_indices_with_pressure = NULL;
    for (reason_index = 0; reason_index < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++reason_index) {
        report->pressure_reason_counts[reason_index] = 0;
        report->functions_with_pressure_reason_counts[reason_index] = 0;
        report->function_indices_with_pressure_reason[reason_index] = NULL;
    }
    report->functions_with_hotspots_count = 0;
    report->function_indices_with_hotspots = NULL;
    report->functions_with_hints_count = 0;
    report->function_indices_with_hints = NULL;
    for (reason_index = 0; reason_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++reason_index) {
        report->hint_reason_counts[reason_index] = 0;
        report->functions_with_hint_reason_counts[reason_index] = 0;
        report->function_indices_with_hint_reason[reason_index] = NULL;
    }
}

void value_ssa_program_machine_planning_report_free(
    ValueSsaProgramMachinePlanningReport *report) {
    size_t function_index;
    size_t reason_index;

    if (!report) {
        return;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        value_ssa_function_machine_planning_view_free(&report->function_views[function_index]);
    }
    free(report->function_views);
    free(report->function_indices_with_pressure);
    for (reason_index = 0; reason_index < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++reason_index) {
        free(report->function_indices_with_pressure_reason[reason_index]);
    }
    free(report->function_indices_with_hotspots);
    free(report->function_indices_with_hints);
    for (reason_index = 0; reason_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++reason_index) {
        free(report->function_indices_with_hint_reason[reason_index]);
    }
    value_ssa_program_machine_planning_report_init(report);
}

int value_ssa_build_machine_protection_agenda(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineProtectionAgenda *out_agenda,
    ValueSsaError *error) {
    size_t item_index;

    if (!machine_view || !allocation_prep || !out_agenda || machine_view->value_count != allocation_prep->value_count) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-088: invalid machine protection agenda build contract");
        return 0;
    }

    value_ssa_machine_protection_agenda_free(out_agenda);
    out_agenda->value_count = machine_view->value_count;
    if (machine_view->caller_clobbered_value_count == 0) {
        return 1;
    }

    out_agenda->items = (ValueSsaMachineProtectionAgendaItem *)calloc(
        machine_view->caller_clobbered_value_count, sizeof(ValueSsaMachineProtectionAgendaItem));
    if (!out_agenda->items) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-089: out of memory building machine protection agenda");
        value_ssa_machine_protection_agenda_free(out_agenda);
        return 0;
    }

    for (item_index = 0; item_index < machine_view->caller_clobbered_value_count; ++item_index) {
        size_t value_id = machine_view->caller_clobbered_value_ids[item_index];
        const ValueSsaMachineAllocatedValuePlacement *placement = &machine_view->placements[value_id];
        ValueSsaMachineCallClobberRiskClass risk_class;
        ValueSsaMachineProtectionAgendaItem *item;

        if (!allocation_prep->call_crossing_counts || value_id >= allocation_prep->value_count) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-090: invalid allocation prep for machine protection agenda");
            value_ssa_machine_protection_agenda_free(out_agenda);
            return 0;
        }

        risk_class = value_ssa_machine_classify_call_clobber_risk(
            allocation_prep->call_crossing_counts[value_id],
            allocation_prep->call_density_sums ? allocation_prep->call_density_sums[value_id] : 0,
            allocation_prep->use_call_density_sums ? allocation_prep->use_call_density_sums[value_id] : 0);
        if (risk_class == VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_NONE) {
            continue;
        }

        item = &out_agenda->items[out_agenda->item_count++];
        item->value_id = value_id;
        item->machine_register_id = placement->machine_register_id;
        item->machine_register_name = placement->machine_register_name;
        item->risk_class = risk_class;
        item->call_crossing_count = allocation_prep->call_crossing_counts[value_id];
        item->call_density_sum = allocation_prep->call_density_sums ? allocation_prep->call_density_sums[value_id] : 0;
        item->use_call_density_sum =
            allocation_prep->use_call_density_sums ? allocation_prep->use_call_density_sums[value_id] : 0;
        item->loop_depth_sum = allocation_prep->loop_depth_sums ? allocation_prep->loop_depth_sums[value_id] : 0;
        item->use_loop_depth_sum =
            allocation_prep->use_loop_depth_sums ? allocation_prep->use_loop_depth_sums[value_id] : 0;
        item->single_block_live_range =
            allocation_prep->single_block_live_ranges ? allocation_prep->single_block_live_ranges[value_id] : 0u;
        item->rematerializable = allocation_prep->rematerializable ? allocation_prep->rematerializable[value_id] : 0u;
        item->protection_priority =
            item->call_crossing_count * 64u +
            item->use_call_density_sum * 32u +
            item->call_density_sum * 16u +
            item->use_loop_depth_sum * 8u +
            item->loop_depth_sum * 4u +
            (item->single_block_live_range ? 0u : 2u) +
            (item->rematerializable ? 0u : 1u);
        ++out_agenda->risk_class_counts[risk_class];
    }

    value_ssa_machine_sort_protection_agenda(out_agenda);
    return 1;
}

int value_ssa_compute_function_machine_protection_agenda(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineProtectionAgenda *out_agenda,
    ValueSsaError *error) {
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaAllocationPrep owned_prep;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    int ok = 0;

    if (!function || !bank || !out_agenda) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-091: invalid function machine protection agenda entry contract");
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
        value_ssa_build_machine_protection_agenda(&machine_view, active_prep, out_agenda, error)) {
        ok = 1;
    }

cleanup:
    value_ssa_allocation_prep_free(&owned_prep);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    return ok;
}

int value_ssa_compute_function_flat_machine_protection_agenda(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineProtectionAgenda *out_agenda,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_agenda) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-092: invalid flat function machine protection agenda entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_function_machine_protection_agenda(
            function, color_budget, &bank, allocation_prep, out_agenda, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_program_machine_protection_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineProtectionReport *out_report,
    ValueSsaError *error) {
    size_t function_index;
    size_t risk_class;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-093: invalid program machine protection report entry contract");
        return 0;
    }

    value_ssa_program_machine_protection_report_free(out_report);
    out_report->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_report->function_views =
        (ValueSsaFunctionMachineProtectionView *)calloc(program->function_count, sizeof(ValueSsaFunctionMachineProtectionView));
    out_report->function_indices_with_items = (size_t *)malloc(program->function_count * sizeof(size_t));
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        out_report->function_indices_with_risk_class[risk_class] =
            (size_t *)malloc(program->function_count * sizeof(size_t));
    }
    if (!out_report->function_views || !out_report->function_indices_with_items) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-094: out of memory building program machine protection report");
        value_ssa_program_machine_protection_report_free(out_report);
        return 0;
    }
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        if (!out_report->function_indices_with_risk_class[risk_class]) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-095: out of memory building protection risk-class filters");
            value_ssa_program_machine_protection_report_free(out_report);
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunctionMachineProtectionView *function_view = &out_report->function_views[function_index];

        value_ssa_function_machine_protection_view_init(function_view);
        if (program->functions[function_index].name) {
            function_view->function_name = value_ssa_machine_strdup(program->functions[function_index].name);
            if (!function_view->function_name) {
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-096: out of memory copying protection-view function name");
                value_ssa_program_machine_protection_report_free(out_report);
                return 0;
            }
        }
        if (!value_ssa_compute_function_machine_protection_agenda(
                &program->functions[function_index], color_budget, bank, NULL, &function_view->agenda, error)) {
            value_ssa_program_machine_protection_report_free(out_report);
            return 0;
        }

        out_report->total_value_count += function_view->agenda.value_count;
        out_report->total_item_count += function_view->agenda.item_count;
        if (function_view->agenda.item_count > 0) {
            out_report->function_indices_with_items[out_report->functions_with_items_count++] = function_index;
        }
        for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
             risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
             ++risk_class) {
            out_report->total_risk_class_counts[risk_class] += function_view->agenda.risk_class_counts[risk_class];
            if (function_view->agenda.risk_class_counts[risk_class] > 0) {
                out_report->function_indices_with_risk_class[risk_class]
                    [out_report->functions_with_risk_class_counts[risk_class]++] = function_index;
            }
        }
    }

    return 1;
}

int value_ssa_compute_program_flat_machine_protection_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineProtectionReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-097: invalid flat program machine protection report entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_program_machine_protection_report(program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_build_machine_register_protection_pressure_view(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineRegisterProtectionPressureView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineProtectionAgenda agenda;
    size_t item_index;
    size_t register_index;
    size_t *register_item_counts = NULL;
    size_t *register_write_offsets = NULL;
    int ok = 0;

    if (!machine_view || !allocation_prep || !out_view || machine_view->value_count != allocation_prep->value_count) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-110: invalid register protection pressure build contract");
        return 0;
    }

    value_ssa_machine_register_protection_pressure_view_free(out_view);
    out_view->machine_register_count = machine_view->machine_register_count;
    out_view->total_value_count = machine_view->value_count;

    value_ssa_machine_protection_agenda_init(&agenda);
    if (!value_ssa_build_machine_protection_agenda(machine_view, allocation_prep, &agenda, error)) {
        goto cleanup;
    }

    out_view->total_item_count = agenda.item_count;
    for (register_index = 0; register_index < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++register_index) {
        out_view->risk_class_counts[register_index] = agenda.risk_class_counts[register_index];
    }
    if (machine_view->machine_register_count == 0 || agenda.item_count == 0) {
        ok = 1;
        goto cleanup;
    }

    register_item_counts = (size_t *)calloc(machine_view->machine_register_count, sizeof(size_t));
    register_write_offsets = (size_t *)calloc(machine_view->machine_register_count, sizeof(size_t));
    out_view->items = (ValueSsaMachineRegisterProtectionPressureItem *)calloc(
        machine_view->machine_register_count, sizeof(ValueSsaMachineRegisterProtectionPressureItem));
    if (!register_item_counts || !register_write_offsets || !out_view->items) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-111: out of memory building register protection pressure view");
        goto cleanup;
    }

    for (item_index = 0; item_index < agenda.item_count; ++item_index) {
        const ValueSsaMachineProtectionAgendaItem *agenda_item = &agenda.items[item_index];
        size_t register_id = agenda_item->machine_register_id;

        if (register_id >= machine_view->machine_register_count) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-112: invalid machine register id in protection agenda");
            goto cleanup;
        }
        ++register_item_counts[register_id];
    }

    for (register_index = 0; register_index < machine_view->machine_register_count; ++register_index) {
        ValueSsaMachineRegisterProtectionPressureItem *item;

        if (register_item_counts[register_index] == 0) {
            continue;
        }
        item = &out_view->items[out_view->pressured_register_count++];
        item->machine_register_id = register_index;
        item->machine_register_name = NULL;
        item->item_count = register_item_counts[register_index];
        item->value_ids = (size_t *)malloc(item->item_count * sizeof(size_t));
        if (!item->value_ids) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-113: out of memory allocating register pressure value list");
            goto cleanup;
        }
    }

    for (item_index = 0; item_index < agenda.item_count; ++item_index) {
        const ValueSsaMachineProtectionAgendaItem *agenda_item = &agenda.items[item_index];
        size_t register_id = agenda_item->machine_register_id;
        size_t pressured_index;

        for (pressured_index = 0; pressured_index < out_view->pressured_register_count; ++pressured_index) {
            ValueSsaMachineRegisterProtectionPressureItem *item = &out_view->items[pressured_index];

            if (item->machine_register_id != register_id) {
                continue;
            }
            if (!item->machine_register_name) {
                item->machine_register_name = agenda_item->machine_register_name;
            }
            item->value_ids[register_write_offsets[register_id]++] = agenda_item->value_id;
            ++item->risk_class_counts[agenda_item->risk_class];
            item->total_protection_priority += agenda_item->protection_priority;
            if (agenda_item->protection_priority > item->max_protection_priority) {
                item->max_protection_priority = agenda_item->protection_priority;
            }
            out_view->total_protection_priority += agenda_item->protection_priority;
            break;
        }
    }

    value_ssa_machine_sort_register_pressure_view(out_view);
    ok = 1;

cleanup:
    free(register_write_offsets);
    free(register_item_counts);
    value_ssa_machine_protection_agenda_free(&agenda);
    if (!ok) {
        value_ssa_machine_register_protection_pressure_view_free(out_view);
    }
    return ok;
}

int value_ssa_compute_function_machine_register_protection_pressure_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineRegisterProtectionPressureView *out_view,
    ValueSsaError *error) {
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaAllocationPrep owned_prep;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    int ok = 0;

    if (!function || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-114: invalid function register protection pressure entry contract");
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
        value_ssa_build_machine_register_protection_pressure_view(&machine_view, active_prep, out_view, error)) {
        ok = 1;
    }

cleanup:
    value_ssa_allocation_prep_free(&owned_prep);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    return ok;
}

int value_ssa_compute_function_flat_machine_register_protection_pressure_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaMachineRegisterProtectionPressureView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-115: invalid flat function register protection pressure entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_function_machine_register_protection_pressure_view(
            function, color_budget, &bank, allocation_prep, out_view, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_program_machine_register_protection_pressure_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineRegisterProtectionPressureReport *out_report,
    ValueSsaError *error) {
    size_t function_index;
    size_t risk_class;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-116: invalid program register protection pressure report entry contract");
        return 0;
    }

    value_ssa_program_machine_register_protection_pressure_report_free(out_report);
    out_report->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_report->function_views =
        (ValueSsaFunctionMachineRegisterProtectionPressureView *)calloc(
            program->function_count, sizeof(ValueSsaFunctionMachineRegisterProtectionPressureView));
    out_report->function_indices_with_pressure = (size_t *)malloc(program->function_count * sizeof(size_t));
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        out_report->function_indices_with_risk_class[risk_class] =
            (size_t *)malloc(program->function_count * sizeof(size_t));
    }
    if (!out_report->function_views || !out_report->function_indices_with_pressure) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-117: out of memory building program register protection pressure report");
        value_ssa_program_machine_register_protection_pressure_report_free(out_report);
        return 0;
    }
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        if (!out_report->function_indices_with_risk_class[risk_class]) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-118: out of memory building register protection pressure filters");
            value_ssa_program_machine_register_protection_pressure_report_free(out_report);
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunctionMachineRegisterProtectionPressureView *function_view = &out_report->function_views[function_index];

        value_ssa_function_machine_register_protection_pressure_view_init(function_view);
        if (program->functions[function_index].name) {
            function_view->function_name = value_ssa_machine_strdup(program->functions[function_index].name);
            if (!function_view->function_name) {
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-119: out of memory copying pressure-view function name");
                value_ssa_program_machine_register_protection_pressure_report_free(out_report);
                return 0;
            }
        }
        if (!value_ssa_compute_function_machine_register_protection_pressure_view(
                &program->functions[function_index], color_budget, bank, NULL, &function_view->pressure_view, error)) {
            value_ssa_program_machine_register_protection_pressure_report_free(out_report);
            return 0;
        }

        out_report->total_value_count += function_view->pressure_view.total_value_count;
        out_report->total_item_count += function_view->pressure_view.total_item_count;
        out_report->total_protection_priority += function_view->pressure_view.total_protection_priority;
        out_report->total_pressured_register_count += function_view->pressure_view.pressured_register_count;
        if (function_view->pressure_view.pressured_register_count > 0) {
            out_report->function_indices_with_pressure[out_report->functions_with_pressure_count++] = function_index;
        }
        for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
             risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
             ++risk_class) {
            out_report->risk_class_counts[risk_class] += function_view->pressure_view.risk_class_counts[risk_class];
            if (function_view->pressure_view.risk_class_counts[risk_class] > 0) {
                out_report->function_indices_with_risk_class[risk_class]
                    [out_report->functions_with_risk_class_counts[risk_class]++] = function_index;
            }
        }
    }

    return 1;
}

int value_ssa_compute_program_flat_machine_register_protection_pressure_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineRegisterProtectionPressureReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-120: invalid flat program register protection pressure report entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_program_machine_register_protection_pressure_report(
            program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_build_function_machine_register_protection_hotspot_view(
    const char *function_name,
    const ValueSsaMachineRegisterProtectionPressureView *pressure_view,
    ValueSsaFunctionMachineRegisterProtectionHotspotView *out_view,
    ValueSsaError *error) {
    const ValueSsaMachineRegisterProtectionPressureItem *best_item = NULL;

    if (!pressure_view || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-135: invalid function register protection hotspot build contract");
        return 0;
    }

    value_ssa_function_machine_register_protection_hotspot_view_free(out_view);
    if (function_name) {
        out_view->function_name = value_ssa_machine_strdup(function_name);
        if (!out_view->function_name) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-136: out of memory copying hotspot-view function name");
            value_ssa_function_machine_register_protection_hotspot_view_free(out_view);
            return 0;
        }
    }
    if (pressure_view->pressured_register_count == 0 || !pressure_view->items) {
        return 1;
    }

    best_item = &pressure_view->items[0];
    out_view->has_hotspot = 1u;
    out_view->hotspot.machine_register_id = best_item->machine_register_id;
    out_view->hotspot.machine_register_name = best_item->machine_register_name;
    out_view->hotspot.item_count = best_item->item_count;
    out_view->hotspot.total_protection_priority = best_item->total_protection_priority;
    out_view->hotspot.max_protection_priority = best_item->max_protection_priority;
    memcpy(out_view->hotspot.risk_class_counts,
        best_item->risk_class_counts,
        sizeof(out_view->hotspot.risk_class_counts));
    return 1;
}

int value_ssa_build_function_machine_preservation_hint_view(
    const char *function_name,
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterProtectionPressureView *pressure_view,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachinePreservationHintView *out_view,
    ValueSsaError *error) {
    const ValueSsaMachineRegisterProtectionPressureItem *source_item = NULL;
    const ValueSsaMachineRegisterValueGroup *target_group = NULL;
    size_t register_index;
    size_t candidate_count = 0;
    size_t candidate_index = 0;

    if (!machine_view || !pressure_view || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-150: invalid function preservation hint build contract");
        return 0;
    }

    value_ssa_function_machine_preservation_hint_view_free(out_view);
    if (function_name) {
        out_view->function_name = value_ssa_machine_strdup(function_name);
        if (!out_view->function_name) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-151: out of memory copying preservation-hint function name");
            value_ssa_function_machine_preservation_hint_view_free(out_view);
            return 0;
        }
    }
    if (!pressure_view->items || pressure_view->pressured_register_count == 0) {
        return 1;
    }

    source_item = &pressure_view->items[0];
    if (!source_item->machine_register_name || source_item->item_count == 0 || source_item->total_protection_priority == 0) {
        return 1;
    }
    if (source_item->machine_register_id >= bank->register_count || !bank->registers ||
        !bank->registers[source_item->machine_register_id].caller_clobbered) {
        return 1;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const ValueSsaMachineRegisterDesc *desc = &bank->registers[register_index];

        if (!desc->allocatable || !desc->callee_preserved) {
            continue;
        }
        ++candidate_count;
    }
    if (candidate_count == 0) {
        return 1;
    }

    out_view->candidates =
        (ValueSsaMachinePreservationHintCandidate *)calloc(candidate_count, sizeof(ValueSsaMachinePreservationHintCandidate));
    if (!out_view->candidates) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-165: out of memory allocating preservation hint candidates");
        value_ssa_function_machine_preservation_hint_view_free(out_view);
        return 0;
    }
    out_view->candidate_count = candidate_count;

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const ValueSsaMachineRegisterDesc *desc = &bank->registers[register_index];
        ValueSsaMachinePreservationHintCandidate *candidate;
        const ValueSsaMachineRegisterProtectionPressureItem *target_pressure = NULL;

        if (!desc->allocatable || !desc->callee_preserved) {
            continue;
        }
        candidate = &out_view->candidates[candidate_index++];
        candidate->machine_register_id = source_item->machine_register_id;
        candidate->machine_register_name = source_item->machine_register_name;
        candidate->suggested_machine_register_id = desc->register_id;
        candidate->suggested_machine_register_name = desc->name;
        if (value_ssa_machine_register_protection_pressure_view_get_register_item(
                pressure_view, desc->register_id, &target_pressure) &&
            target_pressure) {
            candidate->suggested_register_item_count = target_pressure->item_count;
            candidate->suggested_register_total_priority = target_pressure->total_protection_priority;
            candidate->suggested_register_max_priority = target_pressure->max_protection_priority;
        } else {
            candidate->suggested_register_item_count = 0;
            candidate->suggested_register_total_priority = 0;
            candidate->suggested_register_max_priority = 0;
        }
        if (value_ssa_function_machine_allocation_view_get_machine_register_group(
                machine_view, desc->register_id, &target_group) &&
            target_group) {
            candidate->suggested_register_item_count += target_group->value_count;
        }
        candidate->protected_item_count = source_item->item_count;
        candidate->protected_total_priority = source_item->total_protection_priority;
        candidate->protected_max_priority = source_item->max_protection_priority;
        candidate->primary_reason = value_ssa_machine_preservation_reason_name(
            value_ssa_machine_preservation_hint_candidate_reason_code(candidate));
    }

    value_ssa_machine_sort_preservation_hint_candidates(out_view);

    out_view->has_hint = 1u;
    out_view->source_machine_register_id = source_item->machine_register_id;
    out_view->source_machine_register_name = source_item->machine_register_name;
    out_view->suggested_machine_register_id = out_view->candidates[0].suggested_machine_register_id;
    out_view->suggested_machine_register_name = out_view->candidates[0].suggested_machine_register_name;
    out_view->protected_item_count = source_item->item_count;
    out_view->protected_total_priority = source_item->total_protection_priority;
    out_view->protected_max_priority = source_item->max_protection_priority;
    memcpy(out_view->source_risk_class_counts,
        source_item->risk_class_counts,
        sizeof(out_view->source_risk_class_counts));
    out_view->primary_reason = value_ssa_machine_preservation_hint_candidate_reason_code(&out_view->candidates[0]);
    return 1;
}

int value_ssa_compute_function_machine_register_protection_hotspot_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachineRegisterProtectionHotspotView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
    int ok = 0;

    if (!function || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-137: invalid function register protection hotspot entry contract");
        return 0;
    }

    value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
    if (value_ssa_compute_function_machine_register_protection_pressure_view(
            function, color_budget, bank, allocation_prep, &pressure_view, error) &&
        value_ssa_build_function_machine_register_protection_hotspot_view(
            function->name, &pressure_view, out_view, error)) {
        ok = 1;
    }
    value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    return ok;
}

int value_ssa_compute_function_flat_machine_register_protection_hotspot_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachineRegisterProtectionHotspotView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-138: invalid flat function register protection hotspot entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_function_machine_register_protection_hotspot_view(
            function, color_budget, &bank, allocation_prep, out_view, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_program_machine_register_protection_hotspot_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineRegisterProtectionHotspotReport *out_report,
    ValueSsaError *error) {
    size_t function_index;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-139: invalid program register protection hotspot report entry contract");
        return 0;
    }

    value_ssa_program_machine_register_protection_hotspot_report_free(out_report);
    out_report->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_report->function_views =
        (ValueSsaFunctionMachineRegisterProtectionHotspotView *)calloc(
            program->function_count, sizeof(ValueSsaFunctionMachineRegisterProtectionHotspotView));
    out_report->function_indices_with_hotspots = (size_t *)malloc(program->function_count * sizeof(size_t));
    if (!out_report->function_views || !out_report->function_indices_with_hotspots) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-140: out of memory building program register protection hotspot report");
        value_ssa_program_machine_register_protection_hotspot_report_free(out_report);
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunctionMachineRegisterProtectionHotspotView *function_view = &out_report->function_views[function_index];
        ValueSsaMachineRegisterProtectionPressureView pressure_view;

        value_ssa_function_machine_register_protection_hotspot_view_init(function_view);
        value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
        if (!value_ssa_compute_function_machine_register_protection_pressure_view(
                &program->functions[function_index], color_budget, bank, NULL, &pressure_view, error) ||
            !value_ssa_build_function_machine_register_protection_hotspot_view(
                program->functions[function_index].name, &pressure_view, function_view, error)) {
            value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
            value_ssa_program_machine_register_protection_hotspot_report_free(out_report);
            return 0;
        }

        out_report->total_value_count += pressure_view.total_value_count;
        out_report->total_item_count += pressure_view.total_item_count;
        if (function_view->has_hotspot) {
            out_report->total_hotspot_priority += function_view->hotspot.total_protection_priority;
            out_report->total_hotspot_item_count += function_view->hotspot.item_count;
            out_report->function_indices_with_hotspots[out_report->functions_with_hotspots_count++] = function_index;
        }
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    }

    return 1;
}

int value_ssa_compute_program_flat_machine_register_protection_hotspot_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachineRegisterProtectionHotspotReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-141: invalid flat program register protection hotspot report entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_program_machine_register_protection_hotspot_report(
            program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_function_machine_preservation_hint_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePreservationHintView *out_view,
    ValueSsaError *error) {
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaMachineRegisterProtectionPressureView pressure_view;
    int ok = 0;

    if (!function || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-152: invalid function preservation hint entry contract");
        return 0;
    }

    value_ssa_function_machine_allocation_view_init(&machine_view);
    value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
    if (value_ssa_allocate_function_machine_view(function, color_budget, bank, &machine_view, error) &&
        value_ssa_compute_function_machine_register_protection_pressure_view(
            function, color_budget, bank, allocation_prep, &pressure_view, error) &&
        value_ssa_build_function_machine_preservation_hint_view(
            function->name, &machine_view, &pressure_view, bank, out_view, error)) {
        ok = 1;
    }
    value_ssa_function_machine_allocation_view_free(&machine_view);
    value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    return ok;
}

int value_ssa_compute_function_flat_machine_preservation_hint_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePreservationHintView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-153: invalid flat function preservation hint entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_function_machine_preservation_hint_view(
            function, color_budget, &bank, allocation_prep, out_view, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_program_machine_preservation_hint_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachinePreservationHintReport *out_report,
    ValueSsaError *error) {
    size_t function_index;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-154: invalid program preservation hint report entry contract");
        return 0;
    }

    value_ssa_program_machine_preservation_hint_report_free(out_report);
    out_report->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_report->function_views =
        (ValueSsaFunctionMachinePreservationHintView *)calloc(
            program->function_count, sizeof(ValueSsaFunctionMachinePreservationHintView));
    out_report->function_indices_with_hints = (size_t *)malloc(program->function_count * sizeof(size_t));
    for (function_index = 0; function_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++function_index) {
        out_report->function_indices_with_reason[function_index] =
            (size_t *)malloc(program->function_count * sizeof(size_t));
    }
    if (!out_report->function_views || !out_report->function_indices_with_hints) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-155: out of memory building program preservation hint report");
        value_ssa_program_machine_preservation_hint_report_free(out_report);
        return 0;
    }
    for (function_index = 0; function_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++function_index) {
        if (!out_report->function_indices_with_reason[function_index]) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-179: out of memory building preservation reason filters");
            value_ssa_program_machine_preservation_hint_report_free(out_report);
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunctionMachinePreservationHintView *function_view = &out_report->function_views[function_index];
        ValueSsaFunctionMachineAllocationView machine_view;
        ValueSsaMachineRegisterProtectionPressureView pressure_view;

        value_ssa_function_machine_preservation_hint_view_init(function_view);
        value_ssa_function_machine_allocation_view_init(&machine_view);
        value_ssa_machine_register_protection_pressure_view_init(&pressure_view);
        if (!value_ssa_allocate_function_machine_view(
                &program->functions[function_index], color_budget, bank, &machine_view, error) ||
            !value_ssa_compute_function_machine_register_protection_pressure_view(
                &program->functions[function_index], color_budget, bank, NULL, &pressure_view, error) ||
            !value_ssa_build_function_machine_preservation_hint_view(
                program->functions[function_index].name, &machine_view, &pressure_view, bank, function_view, error)) {
            value_ssa_function_machine_allocation_view_free(&machine_view);
            value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
            value_ssa_program_machine_preservation_hint_report_free(out_report);
            return 0;
        }

        out_report->total_value_count += pressure_view.total_value_count;
        out_report->total_item_count += pressure_view.total_item_count;
        if (function_view->has_hint) {
            out_report->total_hint_priority += function_view->protected_total_priority;
            out_report->function_indices_with_hints[out_report->functions_with_hints_count++] = function_index;
            ++out_report->reason_counts[function_view->primary_reason];
            out_report->function_indices_with_reason[function_view->primary_reason]
                [out_report->functions_with_reason_counts[function_view->primary_reason]++] = function_index;
        }
        value_ssa_function_machine_allocation_view_free(&machine_view);
        value_ssa_machine_register_protection_pressure_view_free(&pressure_view);
    }

    return 1;
}

int value_ssa_compute_program_flat_machine_preservation_hint_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachinePreservationHintReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-156: invalid flat program preservation hint report entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_program_machine_preservation_hint_report(
            program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_build_function_machine_planning_view(
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaAllocationPrep *allocation_prep,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachinePlanningView *out_view,
    ValueSsaError *error) {
    const char *function_name = NULL;
    size_t value_count = 0;
    size_t machine_register_count = 0;
    size_t spill_slot_count = 0;
    size_t machine_colored_count = 0;
    size_t spilled_count = 0;
    size_t caller_clobbered_value_count = 0;
    size_t callee_preserved_value_count = 0;
    size_t used_machine_register_count = 0;
    size_t machine_register_group_count = 0;

    if (!machine_view || !allocation_prep || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-167: invalid function planning view build contract");
        return 0;
    }

    value_ssa_function_machine_planning_view_free(out_view);
    if (!value_ssa_function_machine_allocation_view_get_summary(
            machine_view,
            &function_name,
            &value_count,
            &machine_register_count,
            &spill_slot_count,
            &machine_colored_count,
            &spilled_count,
            &caller_clobbered_value_count,
            &callee_preserved_value_count,
            &used_machine_register_count,
            &machine_register_group_count)) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-168: invalid machine allocation summary for planning view");
        return 0;
    }
    if (function_name) {
        out_view->function_name = value_ssa_machine_strdup(function_name);
        if (!out_view->function_name) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-169: out of memory copying planning-view function name");
            value_ssa_function_machine_planning_view_free(out_view);
            return 0;
        }
    }
    if (!value_ssa_build_machine_register_protection_pressure_view(machine_view, allocation_prep, &out_view->pressure_view, error) ||
        !value_ssa_build_function_machine_register_protection_hotspot_view(
            function_name, &out_view->pressure_view, &out_view->hotspot_view, error) ||
        !value_ssa_build_function_machine_preservation_hint_view(
            function_name, machine_view, &out_view->pressure_view, bank, &out_view->preservation_hint_view, error)) {
        value_ssa_function_machine_planning_view_free(out_view);
        return 0;
    }
    return 1;
}

int value_ssa_compute_function_machine_planning_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePlanningView *out_view,
    ValueSsaError *error) {
    ValueSsaFunctionMachineAllocationView machine_view;
    ValueSsaAllocationPrep owned_prep;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    int ok = 0;

    if (!function || !bank || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-170: invalid function planning view entry contract");
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
        value_ssa_build_function_machine_planning_view(&machine_view, active_prep, bank, out_view, error)) {
        ok = 1;
    }

cleanup:
    value_ssa_allocation_prep_free(&owned_prep);
    value_ssa_function_machine_allocation_view_free(&machine_view);
    return ok;
}

int value_ssa_compute_function_flat_machine_planning_view(
    const ValueSsaFunction *function,
    size_t color_budget,
    size_t machine_register_count,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaFunctionMachinePlanningView *out_view,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!function || !out_view) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-171: invalid flat function planning view entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_function_machine_planning_view(
            function, color_budget, &bank, allocation_prep, out_view, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_compute_program_machine_planning_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachinePlanningReport *out_report,
    ValueSsaError *error) {
    size_t function_index;

    if (!program || !bank || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-172: invalid program planning report entry contract");
        return 0;
    }

    value_ssa_program_machine_planning_report_free(out_report);
    out_report->function_count = program->function_count;
    if (program->function_count == 0) {
        return 1;
    }

    out_report->function_views =
        (ValueSsaFunctionMachinePlanningView *)calloc(program->function_count, sizeof(ValueSsaFunctionMachinePlanningView));
    out_report->function_indices_with_pressure = (size_t *)malloc(program->function_count * sizeof(size_t));
    for (function_index = 0; function_index < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++function_index) {
        out_report->function_indices_with_pressure_reason[function_index] =
            (size_t *)malloc(program->function_count * sizeof(size_t));
    }
    out_report->function_indices_with_hotspots = (size_t *)malloc(program->function_count * sizeof(size_t));
    out_report->function_indices_with_hints = (size_t *)malloc(program->function_count * sizeof(size_t));
    for (function_index = 0; function_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++function_index) {
        out_report->function_indices_with_hint_reason[function_index] =
            (size_t *)malloc(program->function_count * sizeof(size_t));
    }
    if (!out_report->function_views || !out_report->function_indices_with_pressure ||
        !out_report->function_indices_with_hotspots || !out_report->function_indices_with_hints) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-173: out of memory building program planning report");
        value_ssa_program_machine_planning_report_free(out_report);
        return 0;
    }
    for (function_index = 0; function_index < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT; ++function_index) {
        if (!out_report->function_indices_with_pressure_reason[function_index]) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-183: out of memory building planning pressure-reason filters");
            value_ssa_program_machine_planning_report_free(out_report);
            return 0;
        }
    }
    for (function_index = 0; function_index < VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT; ++function_index) {
        if (!out_report->function_indices_with_hint_reason[function_index]) {
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-182: out of memory building planning reason filters");
            value_ssa_program_machine_planning_report_free(out_report);
            return 0;
        }
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        ValueSsaFunctionMachinePlanningView *function_view = &out_report->function_views[function_index];

        value_ssa_function_machine_planning_view_init(function_view);
        if (!value_ssa_compute_function_machine_planning_view(
                &program->functions[function_index], color_budget, bank, NULL, function_view, error)) {
            value_ssa_program_machine_planning_report_free(out_report);
            return 0;
        }

        out_report->total_value_count += function_view->pressure_view.total_value_count;
        out_report->total_pressure_item_count += function_view->pressure_view.total_item_count;
        out_report->total_pressure_priority += function_view->pressure_view.total_protection_priority;
        out_report->total_hotspot_priority += function_view->hotspot_view.has_hotspot
            ? function_view->hotspot_view.hotspot.total_protection_priority
            : 0;
        out_report->total_hint_priority += function_view->preservation_hint_view.has_hint
            ? function_view->preservation_hint_view.protected_total_priority
            : 0;
        if (function_view->pressure_view.pressured_register_count > 0) {
            out_report->function_indices_with_pressure[out_report->functions_with_pressure_count++] = function_index;
            {
                size_t risk_class;

                for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
                     risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
                     ++risk_class) {
                    if (function_view->pressure_view.risk_class_counts[risk_class] == 0) {
                        continue;
                    }
                    ++out_report->pressure_reason_counts[risk_class];
                    out_report->function_indices_with_pressure_reason[risk_class]
                        [out_report->functions_with_pressure_reason_counts[risk_class]++] = function_index;
                }
            }
        }
        if (function_view->hotspot_view.has_hotspot) {
            out_report->function_indices_with_hotspots[out_report->functions_with_hotspots_count++] = function_index;
        }
        if (function_view->preservation_hint_view.has_hint) {
            out_report->function_indices_with_hints[out_report->functions_with_hints_count++] = function_index;
            ++out_report->hint_reason_counts[function_view->preservation_hint_view.primary_reason];
            out_report->function_indices_with_hint_reason[function_view->preservation_hint_view.primary_reason]
                [out_report->functions_with_hint_reason_counts[function_view->preservation_hint_view.primary_reason]++] =
                function_index;
        }
    }

    return 1;
}

int value_ssa_compute_program_flat_machine_planning_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    ValueSsaProgramMachinePlanningReport *out_report,
    ValueSsaError *error) {
    ValueSsaMachineRegisterBank bank;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-174: invalid flat program planning report entry contract");
        return 0;
    }

    value_ssa_machine_register_bank_init(&bank);
    if (value_ssa_build_flat_machine_register_bank(machine_register_count, &bank, error)) {
        ok = value_ssa_compute_program_machine_planning_report(program, color_budget, &bank, out_report, error);
    }
    value_ssa_machine_register_bank_free(&bank);
    return ok;
}

int value_ssa_machine_protection_agenda_get_summary(
    const ValueSsaMachineProtectionAgenda *agenda,
    size_t *out_value_count,
    size_t *out_item_count,
    size_t *out_crossing_count,
    size_t *out_repeated_count,
    size_t *out_hot_count) {
    if (!agenda) {
        return 0;
    }

    if (out_value_count) {
        *out_value_count = agenda->value_count;
    }
    if (out_item_count) {
        *out_item_count = agenda->item_count;
    }
    if (out_crossing_count) {
        *out_crossing_count = agenda->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING];
    }
    if (out_repeated_count) {
        *out_repeated_count = agenda->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED];
    }
    if (out_hot_count) {
        *out_hot_count = agenda->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT];
    }
    return 1;
}

int value_ssa_machine_protection_agenda_find_value(
    const ValueSsaMachineProtectionAgenda *agenda,
    size_t value_id,
    size_t *out_index,
    const ValueSsaMachineProtectionAgendaItem **out_item) {
    size_t item_index;

    if (out_index) {
        *out_index = 0;
    }
    if (out_item) {
        *out_item = NULL;
    }
    if (!agenda) {
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        if (agenda->items[item_index].value_id == value_id) {
            if (out_index) {
                *out_index = item_index;
            }
            if (out_item) {
                *out_item = &agenda->items[item_index];
            }
            return 1;
        }
    }
    return 0;
}

int value_ssa_program_machine_protection_report_get_summary(
    const ValueSsaProgramMachineProtectionReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count) {
    if (!report) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = report->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = report->total_value_count;
    }
    if (out_total_item_count) {
        *out_total_item_count = report->total_item_count;
    }
    return 1;
}

int value_ssa_program_machine_protection_report_get_function_view(
    const ValueSsaProgramMachineProtectionReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineProtectionView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || function_index >= report->function_count || !report->function_views) {
        return 0;
    }

    *out_view = &report->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_protection_report_get_function_view_by_name(
    const ValueSsaProgramMachineProtectionReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineProtectionView **out_view) {
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
        const ValueSsaFunctionMachineProtectionView *function_view = &report->function_views[function_index];

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

int value_ssa_program_machine_protection_report_get_functions_with_items(
    const ValueSsaProgramMachineProtectionReport *report,
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
        *out_count = report->functions_with_items_count;
    }
    if (out_function_indices && report->functions_with_items_count > 0) {
        *out_function_indices = report->function_indices_with_items;
    }
    return 1;
}

int value_ssa_program_machine_protection_report_get_functions_with_risk_class(
    const ValueSsaProgramMachineProtectionReport *report,
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

int value_ssa_machine_register_protection_pressure_view_get_summary(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    size_t *out_machine_register_count,
    size_t *out_pressured_register_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_protection_priority,
    size_t *out_crossing_count,
    size_t *out_repeated_count,
    size_t *out_hot_count) {
    if (!view) {
        return 0;
    }

    if (out_machine_register_count) {
        *out_machine_register_count = view->machine_register_count;
    }
    if (out_pressured_register_count) {
        *out_pressured_register_count = view->pressured_register_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = view->total_value_count;
    }
    if (out_total_item_count) {
        *out_total_item_count = view->total_item_count;
    }
    if (out_total_protection_priority) {
        *out_total_protection_priority = view->total_protection_priority;
    }
    if (out_crossing_count) {
        *out_crossing_count = view->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING];
    }
    if (out_repeated_count) {
        *out_repeated_count = view->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED];
    }
    if (out_hot_count) {
        *out_hot_count = view->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT];
    }
    return 1;
}

int value_ssa_machine_register_protection_pressure_view_get_register_item(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    size_t machine_register_id,
    const ValueSsaMachineRegisterProtectionPressureItem **out_item) {
    size_t item_index;

    if (out_item) {
        *out_item = NULL;
    }
    if (!view || !out_item || !view->items) {
        return 0;
    }

    for (item_index = 0; item_index < view->pressured_register_count; ++item_index) {
        if (view->items[item_index].machine_register_id == machine_register_id) {
            *out_item = &view->items[item_index];
            return 1;
        }
    }
    return 0;
}

int value_ssa_machine_register_protection_pressure_view_get_register_item_at(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    size_t item_index,
    const ValueSsaMachineRegisterProtectionPressureItem **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    if (!view || !out_item || item_index >= view->pressured_register_count || !view->items) {
        return 0;
    }

    *out_item = &view->items[item_index];
    return 1;
}

int value_ssa_program_machine_register_protection_pressure_report_get_summary(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_protection_priority,
    size_t *out_total_pressured_register_count) {
    if (!report) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = report->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = report->total_value_count;
    }
    if (out_total_item_count) {
        *out_total_item_count = report->total_item_count;
    }
    if (out_total_protection_priority) {
        *out_total_protection_priority = report->total_protection_priority;
    }
    if (out_total_pressured_register_count) {
        *out_total_pressured_register_count = report->total_pressured_register_count;
    }
    return 1;
}

int value_ssa_program_machine_register_protection_pressure_report_get_function_view(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineRegisterProtectionPressureView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || function_index >= report->function_count || !report->function_views) {
        return 0;
    }

    *out_view = &report->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_register_protection_pressure_report_get_function_view_by_name(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineRegisterProtectionPressureView **out_view) {
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
        const ValueSsaFunctionMachineRegisterProtectionPressureView *function_view =
            &report->function_views[function_index];

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

int value_ssa_program_machine_register_protection_pressure_report_get_functions_with_pressure(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
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
        *out_count = report->functions_with_pressure_count;
    }
    if (out_function_indices && report->functions_with_pressure_count > 0) {
        *out_function_indices = report->function_indices_with_pressure;
    }
    return 1;
}

int value_ssa_program_machine_register_protection_pressure_report_get_functions_with_risk_class(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
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

int value_ssa_function_machine_register_protection_hotspot_view_get_summary(
    const ValueSsaFunctionMachineRegisterProtectionHotspotView *view,
    const char **out_function_name,
    int *out_has_hotspot,
    size_t *out_total_item_count,
    size_t *out_total_value_count,
    const ValueSsaMachineRegisterProtectionHotspotItem **out_hotspot) {
    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (out_has_hotspot) {
        *out_has_hotspot = 0;
    }
    if (out_total_item_count) {
        *out_total_item_count = 0;
    }
    if (out_total_value_count) {
        *out_total_value_count = 0;
    }
    if (out_hotspot) {
        *out_hotspot = NULL;
    }
    if (!view) {
        return 0;
    }

    if (out_function_name) {
        *out_function_name = view->function_name;
    }
    if (out_has_hotspot) {
        *out_has_hotspot = view->has_hotspot ? 1 : 0;
    }
    if (out_total_item_count) {
        *out_total_item_count = view->hotspot.item_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = view->has_hotspot ? view->hotspot.item_count : 0;
    }
    if (out_hotspot && view->has_hotspot) {
        *out_hotspot = &view->hotspot;
    }
    return 1;
}

int value_ssa_program_machine_register_protection_hotspot_report_get_summary(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_hotspot_priority,
    size_t *out_total_hotspot_item_count) {
    if (!report) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = report->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = report->total_value_count;
    }
    if (out_total_item_count) {
        *out_total_item_count = report->total_item_count;
    }
    if (out_total_hotspot_priority) {
        *out_total_hotspot_priority = report->total_hotspot_priority;
    }
    if (out_total_hotspot_item_count) {
        *out_total_hotspot_item_count = report->total_hotspot_item_count;
    }
    return 1;
}

int value_ssa_program_machine_register_protection_hotspot_report_get_function_view(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineRegisterProtectionHotspotView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || function_index >= report->function_count || !report->function_views) {
        return 0;
    }

    *out_view = &report->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_register_protection_hotspot_report_get_function_view_by_name(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineRegisterProtectionHotspotView **out_view) {
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
        const ValueSsaFunctionMachineRegisterProtectionHotspotView *function_view =
            &report->function_views[function_index];

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

int value_ssa_program_machine_register_protection_hotspot_report_get_functions_with_hotspots(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
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
        *out_count = report->functions_with_hotspots_count;
    }
    if (out_function_indices && report->functions_with_hotspots_count > 0) {
        *out_function_indices = report->function_indices_with_hotspots;
    }
    return 1;
}

int value_ssa_function_machine_preservation_hint_view_get_summary(
    const ValueSsaFunctionMachinePreservationHintView *view,
    const char **out_function_name,
    int *out_has_hint,
    size_t *out_protected_item_count,
    size_t *out_total_value_count,
    size_t *out_total_hint_priority) {
    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (out_has_hint) {
        *out_has_hint = 0;
    }
    if (out_protected_item_count) {
        *out_protected_item_count = 0;
    }
    if (out_total_value_count) {
        *out_total_value_count = 0;
    }
    if (out_total_hint_priority) {
        *out_total_hint_priority = 0;
    }
    if (!view) {
        return 0;
    }

    if (out_function_name) {
        *out_function_name = view->function_name;
    }
    if (out_has_hint) {
        *out_has_hint = view->has_hint ? 1 : 0;
    }
    if (out_protected_item_count) {
        *out_protected_item_count = view->protected_item_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = view->has_hint ? view->protected_item_count : 0;
    }
    if (out_total_hint_priority) {
        *out_total_hint_priority = view->protected_total_priority;
    }
    return 1;
}

int value_ssa_function_machine_preservation_hint_view_get_candidate_at(
    const ValueSsaFunctionMachinePreservationHintView *view,
    size_t candidate_index,
    const ValueSsaMachinePreservationHintCandidate **out_candidate) {
    if (out_candidate) {
        *out_candidate = NULL;
    }
    if (!view || !out_candidate || candidate_index >= view->candidate_count || !view->candidates) {
        return 0;
    }

    *out_candidate = &view->candidates[candidate_index];
    return 1;
}

int value_ssa_program_machine_preservation_hint_report_get_summary(
    const ValueSsaProgramMachinePreservationHintReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_item_count,
    size_t *out_total_hint_priority) {
    if (!report) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = report->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = report->total_value_count;
    }
    if (out_total_item_count) {
        *out_total_item_count = report->total_item_count;
    }
    if (out_total_hint_priority) {
        *out_total_hint_priority = report->total_hint_priority;
    }
    return 1;
}

int value_ssa_program_machine_preservation_hint_report_get_reason_count(
    const ValueSsaProgramMachinePreservationHintReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0;
    }
    if (!report || reason >= VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT) {
        return 0;
    }

    if (out_count) {
        *out_count = report->reason_counts[reason];
    }
    return 1;
}

int value_ssa_program_machine_preservation_hint_report_get_function_view(
    const ValueSsaProgramMachinePreservationHintReport *report,
    size_t function_index,
    const ValueSsaFunctionMachinePreservationHintView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || function_index >= report->function_count || !report->function_views) {
        return 0;
    }

    *out_view = &report->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_preservation_hint_report_get_function_view_by_name(
    const ValueSsaProgramMachinePreservationHintReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachinePreservationHintView **out_view) {
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
        const ValueSsaFunctionMachinePreservationHintView *function_view = &report->function_views[function_index];

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

int value_ssa_program_machine_preservation_hint_report_get_functions_with_hints(
    const ValueSsaProgramMachinePreservationHintReport *report,
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
        *out_count = report->functions_with_hints_count;
    }
    if (out_function_indices && report->functions_with_hints_count > 0) {
        *out_function_indices = report->function_indices_with_hints;
    }
    return 1;
}

int value_ssa_program_machine_preservation_hint_report_get_functions_with_reason(
    const ValueSsaProgramMachinePreservationHintReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count,
    const size_t **out_function_indices) {
    size_t count;

    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report || reason >= VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT) {
        return 0;
    }

    count = report->functions_with_reason_counts[reason];
    if (out_count) {
        *out_count = count;
    }
    if (out_function_indices && count > 0) {
        *out_function_indices = report->function_indices_with_reason[reason];
    }
    return 1;
}

int value_ssa_function_machine_planning_view_get_summary(
    const ValueSsaFunctionMachinePlanningView *view,
    const char **out_function_name,
    size_t *out_total_value_count,
    size_t *out_pressure_item_count,
    size_t *out_pressure_priority,
    int *out_has_hotspot,
    int *out_has_hint) {
    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (out_total_value_count) {
        *out_total_value_count = 0;
    }
    if (out_pressure_item_count) {
        *out_pressure_item_count = 0;
    }
    if (out_pressure_priority) {
        *out_pressure_priority = 0;
    }
    if (out_has_hotspot) {
        *out_has_hotspot = 0;
    }
    if (out_has_hint) {
        *out_has_hint = 0;
    }
    if (!view) {
        return 0;
    }

    if (out_function_name) {
        *out_function_name = view->function_name;
    }
    if (out_total_value_count) {
        *out_total_value_count = view->pressure_view.total_value_count;
    }
    if (out_pressure_item_count) {
        *out_pressure_item_count = view->pressure_view.total_item_count;
    }
    if (out_pressure_priority) {
        *out_pressure_priority = view->pressure_view.total_protection_priority;
    }
    if (out_has_hotspot) {
        *out_has_hotspot = view->hotspot_view.has_hotspot ? 1 : 0;
    }
    if (out_has_hint) {
        *out_has_hint = view->preservation_hint_view.has_hint ? 1 : 0;
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_summary(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_pressure_item_count,
    size_t *out_total_pressure_priority,
    size_t *out_total_hotspot_priority,
    size_t *out_total_hint_priority) {
    if (!report) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = report->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = report->total_value_count;
    }
    if (out_total_pressure_item_count) {
        *out_total_pressure_item_count = report->total_pressure_item_count;
    }
    if (out_total_pressure_priority) {
        *out_total_pressure_priority = report->total_pressure_priority;
    }
    if (out_total_hotspot_priority) {
        *out_total_hotspot_priority = report->total_hotspot_priority;
    }
    if (out_total_hint_priority) {
        *out_total_hint_priority = report->total_hint_priority;
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_hint_reason_count(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0;
    }
    if (!report || reason >= VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT) {
        return 0;
    }

    if (out_count) {
        *out_count = report->hint_reason_counts[reason];
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_function_view(
    const ValueSsaProgramMachinePlanningReport *report,
    size_t function_index,
    const ValueSsaFunctionMachinePlanningView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || function_index >= report->function_count || !report->function_views) {
        return 0;
    }

    *out_view = &report->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_planning_report_get_function_view_by_name(
    const ValueSsaProgramMachinePlanningReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachinePlanningView **out_view) {
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
        const ValueSsaFunctionMachinePlanningView *function_view = &report->function_views[function_index];

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

int value_ssa_program_machine_planning_report_get_functions_with_pressure(
    const ValueSsaProgramMachinePlanningReport *report,
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
        *out_count = report->functions_with_pressure_count;
    }
    if (out_function_indices && report->functions_with_pressure_count > 0) {
        *out_function_indices = report->function_indices_with_pressure;
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_pressure_reason_count(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachineCallClobberRiskClass risk_class,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0;
    }
    if (!report || risk_class >= VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT) {
        return 0;
    }

    if (out_count) {
        *out_count = report->pressure_reason_counts[risk_class];
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_functions_with_pressure_reason(
    const ValueSsaProgramMachinePlanningReport *report,
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
    if (!report || risk_class >= VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT) {
        return 0;
    }

    count = report->functions_with_pressure_reason_counts[risk_class];
    if (out_count) {
        *out_count = count;
    }
    if (out_function_indices && count > 0) {
        *out_function_indices = report->function_indices_with_pressure_reason[risk_class];
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_functions_with_hotspots(
    const ValueSsaProgramMachinePlanningReport *report,
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
        *out_count = report->functions_with_hotspots_count;
    }
    if (out_function_indices && report->functions_with_hotspots_count > 0) {
        *out_function_indices = report->function_indices_with_hotspots;
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_functions_with_hints(
    const ValueSsaProgramMachinePlanningReport *report,
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
        *out_count = report->functions_with_hints_count;
    }
    if (out_function_indices && report->functions_with_hints_count > 0) {
        *out_function_indices = report->function_indices_with_hints;
    }
    return 1;
}

int value_ssa_program_machine_planning_report_get_functions_with_hint_reason(
    const ValueSsaProgramMachinePlanningReport *report,
    ValueSsaMachinePreservationReason reason,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report || reason >= VALUE_SSA_MACHINE_PRESERVATION_REASON_COUNT) {
        return 0;
    }
    if (out_count) {
        *out_count = report->functions_with_hint_reason_counts[reason];
    }
    if (out_function_indices && report->functions_with_hint_reason_counts[reason] > 0) {
        *out_function_indices = report->function_indices_with_hint_reason[reason];
    }
    return 1;
}

int value_ssa_dump_machine_protection_agenda(
    const ValueSsaMachineProtectionAgenda *agenda,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t item_index;

    if (!agenda || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-098: invalid machine protection agenda dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "machine-protection-agenda values=%zu items=%zu crossing=%zu repeated=%zu hot=%zu\n",
            agenda->value_count,
            agenda->item_count,
            agenda->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
            agenda->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
            agenda->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-099: out of memory building machine protection agenda header");
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        const ValueSsaMachineProtectionAgendaItem *item = &agenda->items[item_index];

        if (!value_ssa_machine_append_format(
                &builder,
                "ssa.%zu reg=%s class=%s priority=%zu call_cross=%zu calls=%zu use_calls=%zu loops=%zu use_loops=%zu xblock=%u remat=%u\n",
                item->value_id,
                item->machine_register_name ? item->machine_register_name : "<none>",
                value_ssa_machine_call_clobber_risk_class_name(item->risk_class),
                item->protection_priority,
                item->call_crossing_count,
                item->call_density_sum,
                item->use_call_density_sum,
                item->loop_depth_sum,
                item->use_loop_depth_sum,
                (unsigned)item->single_block_live_range,
                (unsigned)item->rematerializable)) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-100: out of memory appending machine protection agenda item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_protection_report(
    const ValueSsaProgramMachineProtectionReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    size_t risk_class;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-101: invalid program machine protection report dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "program-machine-protection-report functions=%zu values=%zu items=%zu functions_with_items=%zu crossing=%zu repeated=%zu hot=%zu\n",
            report->function_count,
            report->total_value_count,
            report->total_item_count,
            report->functions_with_items_count,
            report->total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
            report->total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
            report->total_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-102: out of memory building program machine protection report header");
        return 0;
    }

    if (report->functions_with_items_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-protection-items:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-103: out of memory appending protection-items header");
            return 0;
        }
        for (function_index = 0; function_index < report->functions_with_items_count; ++function_index) {
            if (!value_ssa_machine_append_format(&builder, " %zu", report->function_indices_with_items[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-104: out of memory appending protection-items filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-105: out of memory finishing protection-items filter");
            return 0;
        }
    }
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        size_t count = report->functions_with_risk_class_counts[risk_class];
        size_t member_index;

        if (count == 0) {
            continue;
        }
        if (!value_ssa_machine_append_format(
                &builder,
                "functions-with-protection-class %s:",
                value_ssa_machine_call_clobber_risk_class_name((ValueSsaMachineCallClobberRiskClass)risk_class))) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-106: out of memory appending protection-class header");
            return 0;
        }
        for (member_index = 0; member_index < count; ++member_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", report->function_indices_with_risk_class[risk_class][member_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-107: out of memory appending protection-class filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-108: out of memory finishing protection-class filter");
            return 0;
        }
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        const ValueSsaFunctionMachineProtectionView *function_view = &report->function_views[function_index];
        char *agenda_text = NULL;

        if (!value_ssa_machine_append_format(
                &builder,
                "function %s items=%zu values=%zu crossing=%zu repeated=%zu hot=%zu\n",
                function_view->function_name ? function_view->function_name : "<anon>",
                function_view->agenda.item_count,
                function_view->agenda.value_count,
                function_view->agenda.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
                function_view->agenda.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
                function_view->agenda.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-109: out of memory appending function protection header");
            return 0;
        }
        if (!value_ssa_dump_machine_protection_agenda(&function_view->agenda, &agenda_text, error) ||
            !value_ssa_machine_append_format(&builder, "%s", agenda_text ? agenda_text : "")) {
            free(agenda_text);
            free(builder.data);
            return 0;
        }
        free(agenda_text);
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_machine_register_protection_pressure_view(
    const ValueSsaMachineRegisterProtectionPressureView *view,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t item_index;

    if (!view || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-121: invalid register protection pressure dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "machine-register-protection-pressure regs=%zu pressured=%zu values=%zu items=%zu total_priority=%zu crossing=%zu repeated=%zu hot=%zu\n",
            view->machine_register_count,
            view->pressured_register_count,
            view->total_value_count,
            view->total_item_count,
            view->total_protection_priority,
            view->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
            view->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
            view->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-122: out of memory building register protection pressure header");
        return 0;
    }

    for (item_index = 0; item_index < view->pressured_register_count; ++item_index) {
        const ValueSsaMachineRegisterProtectionPressureItem *item = &view->items[item_index];
        size_t value_index;

        if (!value_ssa_machine_append_format(
                &builder,
                "reg %s items=%zu total_priority=%zu max_priority=%zu crossing=%zu repeated=%zu hot=%zu values:",
                item->machine_register_name ? item->machine_register_name : "<none>",
                item->item_count,
                item->total_protection_priority,
                item->max_protection_priority,
                item->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
                item->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
                item->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-123: out of memory appending register pressure item header");
            return 0;
        }
        for (value_index = 0; value_index < item->item_count; ++value_index) {
            if (!value_ssa_machine_append_format(&builder, " ssa.%zu", item->value_ids[value_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-124: out of memory appending register pressure values");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-125: out of memory finishing register pressure item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_register_protection_pressure_report(
    const ValueSsaProgramMachineRegisterProtectionPressureReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    size_t risk_class;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-126: invalid program register protection pressure report dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "program-machine-register-protection-pressure functions=%zu values=%zu items=%zu total_priority=%zu pressured_regs=%zu pressure_funcs=%zu crossing=%zu repeated=%zu hot=%zu\n",
            report->function_count,
            report->total_value_count,
            report->total_item_count,
            report->total_protection_priority,
            report->total_pressured_register_count,
            report->functions_with_pressure_count,
            report->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
            report->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
            report->risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-127: out of memory building program register protection pressure header");
        return 0;
    }

    if (report->functions_with_pressure_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-register-pressure:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-128: out of memory appending pressure function header");
            return 0;
        }
        for (function_index = 0; function_index < report->functions_with_pressure_count; ++function_index) {
            if (!value_ssa_machine_append_format(&builder, " %zu", report->function_indices_with_pressure[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-129: out of memory appending pressure function filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-130: out of memory finishing pressure function filter");
            return 0;
        }
    }
    for (risk_class = VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING;
         risk_class < VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_COUNT;
         ++risk_class) {
        size_t count = report->functions_with_risk_class_counts[risk_class];
        size_t member_index;

        if (count == 0) {
            continue;
        }
        if (!value_ssa_machine_append_format(
                &builder,
                "functions-with-register-pressure-class %s:",
                value_ssa_machine_call_clobber_risk_class_name((ValueSsaMachineCallClobberRiskClass)risk_class))) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-131: out of memory appending pressure class header");
            return 0;
        }
        for (member_index = 0; member_index < count; ++member_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", report->function_indices_with_risk_class[risk_class][member_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-132: out of memory appending pressure class filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-133: out of memory finishing pressure class filter");
            return 0;
        }
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        const ValueSsaFunctionMachineRegisterProtectionPressureView *function_view = &report->function_views[function_index];
        char *pressure_text = NULL;

        if (!value_ssa_machine_append_format(
                &builder,
                "function %s pressured=%zu values=%zu items=%zu total_priority=%zu crossing=%zu repeated=%zu hot=%zu\n",
                function_view->function_name ? function_view->function_name : "<anon>",
                function_view->pressure_view.pressured_register_count,
                function_view->pressure_view.total_value_count,
                function_view->pressure_view.total_item_count,
                function_view->pressure_view.total_protection_priority,
                function_view->pressure_view.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
                function_view->pressure_view.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
                function_view->pressure_view.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-134: out of memory appending function register pressure header");
            return 0;
        }
        if (!value_ssa_dump_machine_register_protection_pressure_view(&function_view->pressure_view, &pressure_text, error) ||
            !value_ssa_machine_append_format(&builder, "%s", pressure_text ? pressure_text : "")) {
            free(pressure_text);
            free(builder.data);
            return 0;
        }
        free(pressure_text);
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_function_machine_register_protection_hotspot_view(
    const ValueSsaFunctionMachineRegisterProtectionHotspotView *view,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;

    if (!view || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-142: invalid function register protection hotspot dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!view->has_hotspot) {
        if (!value_ssa_machine_append_format(
                &builder, "function-register-protection-hotspot %s hotspot=none\n",
                view->function_name ? view->function_name : "<anon>")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-143: out of memory building empty hotspot dump");
            return 0;
        }
    } else if (!value_ssa_machine_append_format(
                   &builder,
                   "function-register-protection-hotspot %s reg=%s items=%zu total_priority=%zu max_priority=%zu crossing=%zu repeated=%zu hot=%zu\n",
                   view->function_name ? view->function_name : "<anon>",
                   view->hotspot.machine_register_name ? view->hotspot.machine_register_name : "<none>",
                   view->hotspot.item_count,
                   view->hotspot.total_protection_priority,
                   view->hotspot.max_protection_priority,
                   view->hotspot.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
                   view->hotspot.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
                   view->hotspot.risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-144: out of memory building hotspot dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_register_protection_hotspot_report(
    const ValueSsaProgramMachineRegisterProtectionHotspotReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    char *function_text = NULL;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-145: invalid program register protection hotspot report dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "program-machine-register-protection-hotspot functions=%zu values=%zu items=%zu hotspot_priority=%zu hotspot_items=%zu hotspot_funcs=%zu\n",
            report->function_count,
            report->total_value_count,
            report->total_item_count,
            report->total_hotspot_priority,
            report->total_hotspot_item_count,
            report->functions_with_hotspots_count)) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-146: out of memory building hotspot report header");
        return 0;
    }

    if (report->functions_with_hotspots_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-hotspots:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-147: out of memory appending hotspot filter header");
            return 0;
        }
        for (function_index = 0; function_index < report->functions_with_hotspots_count; ++function_index) {
            if (!value_ssa_machine_append_format(&builder, " %zu", report->function_indices_with_hotspots[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-148: out of memory appending hotspot filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-149: out of memory finishing hotspot filter");
            return 0;
        }
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        if (!value_ssa_dump_function_machine_register_protection_hotspot_view(
                &report->function_views[function_index], &function_text, error) ||
            !value_ssa_machine_append_format(&builder, "%s", function_text ? function_text : "")) {
            free(function_text);
            free(builder.data);
            return 0;
        }
        free(function_text);
        function_text = NULL;
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_function_machine_preservation_hint_view(
    const ValueSsaFunctionMachinePreservationHintView *view,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t candidate_index;

    if (!view || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-157: invalid function preservation hint dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!view->has_hint) {
        if (!value_ssa_machine_append_format(
                &builder, "function-machine-preservation-hint %s hint=none\n",
                view->function_name ? view->function_name : "<anon>")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-158: out of memory building empty preservation hint dump");
            return 0;
        }
    } else if (!value_ssa_machine_append_format(
                   &builder,
                   "function-machine-preservation-hint %s move=%s->%s candidates=%zu items=%zu total_priority=%zu max_priority=%zu crossing=%zu repeated=%zu hot=%zu\n",
                   view->function_name ? view->function_name : "<anon>",
                   view->source_machine_register_name ? view->source_machine_register_name : "<none>",
                   view->suggested_machine_register_name ? view->suggested_machine_register_name : "<none>",
                   view->candidate_count,
                   view->protected_item_count,
                   view->protected_total_priority,
                   view->protected_max_priority,
                   view->source_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_CROSSING],
                   view->source_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_REPEATED],
                   view->source_risk_class_counts[VALUE_SSA_MACHINE_CALL_CLOBBER_RISK_CLASS_HOT])) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-159: out of memory building preservation hint dump");
        return 0;
    }
    if (view->has_hint) {
        for (candidate_index = 0; candidate_index < view->candidate_count; ++candidate_index) {
            const ValueSsaMachinePreservationHintCandidate *candidate = &view->candidates[candidate_index];

            if (!value_ssa_machine_append_format(
                    &builder,
                    "candidate %zu move=%s->%s reason=%s target_items=%zu target_priority=%zu target_max=%zu items=%zu total_priority=%zu max_priority=%zu\n",
                    candidate_index,
                    candidate->machine_register_name ? candidate->machine_register_name : "<none>",
                    candidate->suggested_machine_register_name ? candidate->suggested_machine_register_name : "<none>",
                    candidate->primary_reason ? candidate->primary_reason : "unknown",
                    candidate->suggested_register_item_count,
                    candidate->suggested_register_total_priority,
                    candidate->suggested_register_max_priority,
                    candidate->protected_item_count,
                    candidate->protected_total_priority,
                    candidate->protected_max_priority)) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-166: out of memory appending preservation hint candidate");
                return 0;
            }
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_preservation_hint_report(
    const ValueSsaProgramMachinePreservationHintReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    char *function_text = NULL;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-160: invalid program preservation hint report dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "program-machine-preservation-hint functions=%zu values=%zu items=%zu hint_priority=%zu hint_funcs=%zu\n",
            report->function_count,
            report->total_value_count,
            report->total_item_count,
            report->total_hint_priority,
            report->functions_with_hints_count)) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-161: out of memory building preservation hint report header");
        return 0;
    }

    if (report->functions_with_hints_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-hints:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-162: out of memory appending preservation hint filter header");
            return 0;
        }
        for (function_index = 0; function_index < report->functions_with_hints_count; ++function_index) {
            if (!value_ssa_machine_append_format(&builder, " %zu", report->function_indices_with_hints[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-163: out of memory appending preservation hint filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-164: out of memory finishing preservation hint filter");
            return 0;
        }
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        if (!value_ssa_dump_function_machine_preservation_hint_view(
                &report->function_views[function_index], &function_text, error) ||
            !value_ssa_machine_append_format(&builder, "%s", function_text ? function_text : "")) {
            free(function_text);
            free(builder.data);
            return 0;
        }
        free(function_text);
        function_text = NULL;
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_function_machine_planning_view(
    const ValueSsaFunctionMachinePlanningView *view,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    char *pressure_text = NULL;
    char *hotspot_text = NULL;
    char *hint_text = NULL;

    if (!view || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-175: invalid function planning view dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder, "function-machine-planning %s\n", view->function_name ? view->function_name : "<anon>") ||
        !value_ssa_dump_machine_register_protection_pressure_view(&view->pressure_view, &pressure_text, error) ||
        !value_ssa_dump_function_machine_register_protection_hotspot_view(&view->hotspot_view, &hotspot_text, error) ||
        !value_ssa_dump_function_machine_preservation_hint_view(&view->preservation_hint_view, &hint_text, error) ||
        !value_ssa_machine_append_format(&builder, "%s", pressure_text ? pressure_text : "") ||
        !value_ssa_machine_append_format(&builder, "%s", hotspot_text ? hotspot_text : "") ||
        !value_ssa_machine_append_format(&builder, "%s", hint_text ? hint_text : "")) {
        free(hint_text);
        free(hotspot_text);
        free(pressure_text);
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-176: out of memory building function planning view dump");
        return 0;
    }

    free(hint_text);
    free(hotspot_text);
    free(pressure_text);
    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_planning_report(
    const ValueSsaProgramMachinePlanningReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    char *function_text = NULL;

    if (!report || !out_text) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-177: invalid program planning report dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(
            &builder,
            "program-machine-planning functions=%zu values=%zu pressure_items=%zu pressure_priority=%zu hotspot_priority=%zu hint_priority=%zu pressure_funcs=%zu hotspot_funcs=%zu hint_funcs=%zu\n",
            report->function_count,
            report->total_value_count,
            report->total_pressure_item_count,
            report->total_pressure_priority,
            report->total_hotspot_priority,
            report->total_hint_priority,
            report->functions_with_pressure_count,
            report->functions_with_hotspots_count,
            report->functions_with_hints_count)) {
        free(builder.data);
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-178: out of memory building program planning report header");
        return 0;
    }

    for (function_index = 0; function_index < report->function_count; ++function_index) {
        if (!value_ssa_dump_function_machine_planning_view(&report->function_views[function_index], &function_text, error) ||
            !value_ssa_machine_append_format(&builder, "%s", function_text ? function_text : "")) {
            free(function_text);
            free(builder.data);
            return 0;
        }
        free(function_text);
        function_text = NULL;
    }

    *out_text = builder.data;
    return 1;
}

#endif
