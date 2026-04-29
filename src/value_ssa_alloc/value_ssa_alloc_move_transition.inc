#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_move_transition_trace_init(ValueSsaAllocatorMoveTransitionTrace *trace) {
    if (!trace) {
        return;
    }

    trace->value_count = 0;
    trace->step_count = 0;
    trace->steps = NULL;
}

void value_ssa_allocator_move_transition_trace_free(ValueSsaAllocatorMoveTransitionTrace *trace) {
    if (!trace) {
        return;
    }

    free(trace->steps);
    value_ssa_allocator_move_transition_trace_init(trace);
}

const char *value_ssa_allocator_move_transition_event_kind_name(
    ValueSsaAllocatorMoveTransitionEventKind event_kind) {
    switch (event_kind) {
    case VALUE_SSA_ALLOC_MOVE_EVENT_FREEZE:
        return "freeze";
    case VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE:
        return "coalesce";
    case VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE:
        return "remove";
    default:
        return "unknown";
    }
}

int value_ssa_compute_allocator_move_transition_trace(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocatorMoveTransitionTrace *trace,
    ValueSsaError *error) {
    ValueSsaAllocatorMoveEngineRunResult run_result;

    if (!function || !trace) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-115: invalid move-transition trace contract");
        return 0;
    }
    (void)plan;

    value_ssa_alloc_move_engine_run_result_init(&run_result);
    if (!value_ssa_alloc_run_move_engine(function,
            prep,
            worklist,
            interference_graph,
            affinity_graph,
            coalesce_analysis,
            color_budget,
            NULL,
            trace,
            &run_result,
            error)) {
        return 0;
    }
    if (run_result.status != VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_COMPLETED || !run_result.published_trace_artifacts) {
        value_ssa_alloc_set_error(
            error, 0, 0, "VALUE-SSA-ALLOC-210: move-transition run did not publish final trace artifacts");
        return 0;
    }
    return 1;
}

int value_ssa_allocator_move_transition_trace_find_value_step(const ValueSsaAllocatorMoveTransitionTrace *trace,
    size_t value_id,
    ValueSsaAllocatorMoveTransitionEventKind event_kind,
    size_t *out_index,
    const ValueSsaAllocatorMoveTransitionStep **out_step) {
    size_t step_index;

    if (!trace) {
        return 0;
    }

    for (step_index = 0; step_index < trace->step_count; ++step_index) {
        if (trace->steps[step_index].value_id != value_id || trace->steps[step_index].event_kind != event_kind) {
            continue;
        }
        if (out_index) {
            *out_index = step_index;
        }
        if (out_step) {
            *out_step = &trace->steps[step_index];
        }
        return 1;
    }
    return 0;
}

int value_ssa_dump_allocator_move_transition_trace(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveTransitionTrace *trace,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t step_index;

    if (!function || !trace || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-117: invalid move-transition dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder,
            "move-transitions %s count=%zu\n",
            function->name ? function->name : "<anon>",
            trace->step_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-118: out of memory building move-transition dump");
        return 0;
    }

    for (step_index = 0; step_index < trace->step_count; ++step_index) {
        const ValueSsaAllocatorMoveTransitionStep *step = &trace->steps[step_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: event=%s phase=%s ssa.%zu root=ssa.%zu partner=%s%zu/%s%zu remove=%s move_work=%s->%s partner_move_work=%s->%s priority=%zu family_members=%zu->%zu partner_family_members=%zu->%zu family_pressure=(neighbors=%zu,ready=%zu,ready_affinity=%zu,best_ready=%s%zu@%zu,mutual=%s,affinity=%zu)->(neighbors=%zu,ready=%zu,ready_affinity=%zu,best_ready=%s%zu@%zu,mutual=%s,affinity=%zu) partner_family_pressure=(neighbors=%zu,ready=%zu,ready_affinity=%zu,best_ready=%s%zu@%zu,mutual=%s,affinity=%zu)->(neighbors=%zu,ready=%zu,ready_affinity=%zu,best_ready=%s%zu@%zu,mutual=%s,affinity=%zu)\n",
                step->step_index,
                value_ssa_allocator_move_transition_event_kind_name(step->event_kind),
                value_ssa_allocator_phase_name(step->phase),
                step->value_id,
                step->root_value_id,
                step->partner_value_id == (size_t)-1 ? "-" : "ssa.",
                step->partner_value_id == (size_t)-1 ? 0 : step->partner_value_id,
                step->partner_root_value_id == (size_t)-1 ? "-" : "ssa.",
                step->partner_root_value_id == (size_t)-1 ? 0 : step->partner_root_value_id,
                value_ssa_allocator_removal_kind_name(step->removal_kind),
                value_ssa_allocator_move_work_class_name(step->move_work_class_before),
                value_ssa_allocator_move_work_class_name(step->move_work_class_after),
                value_ssa_allocator_move_work_class_name(step->partner_move_work_class_before),
                value_ssa_allocator_move_work_class_name(step->partner_move_work_class_after),
                step->move_work_priority,
                step->family_active_member_count_before,
                step->family_active_member_count_after,
                step->partner_family_active_member_count_before,
                step->partner_family_active_member_count_after,
                step->family_external_neighbor_count_before,
                step->family_coalesce_ready_neighbor_count_before,
                step->family_coalesce_ready_affinity_weight_sum_before,
                step->family_best_coalesce_ready_partner_root_value_id_before == (size_t)-1 ? "-" : "ssa.",
                step->family_best_coalesce_ready_partner_root_value_id_before == (size_t)-1
                    ? 0
                    : step->family_best_coalesce_ready_partner_root_value_id_before,
                step->family_best_coalesce_ready_partner_affinity_weight_before,
                step->family_best_coalesce_ready_partner_is_mutual_before ? "yes" : "no",
                step->family_external_affinity_weight_sum_before,
                step->family_external_neighbor_count_after,
                step->family_coalesce_ready_neighbor_count_after,
                step->family_coalesce_ready_affinity_weight_sum_after,
                step->family_best_coalesce_ready_partner_root_value_id_after == (size_t)-1 ? "-" : "ssa.",
                step->family_best_coalesce_ready_partner_root_value_id_after == (size_t)-1
                    ? 0
                    : step->family_best_coalesce_ready_partner_root_value_id_after,
                step->family_best_coalesce_ready_partner_affinity_weight_after,
                step->family_best_coalesce_ready_partner_is_mutual_after ? "yes" : "no",
                step->family_external_affinity_weight_sum_after,
                step->partner_family_external_neighbor_count_before,
                step->partner_family_coalesce_ready_neighbor_count_before,
                step->partner_family_coalesce_ready_affinity_weight_sum_before,
                step->partner_family_best_coalesce_ready_partner_root_value_id_before == (size_t)-1 ? "-" : "ssa.",
                step->partner_family_best_coalesce_ready_partner_root_value_id_before == (size_t)-1
                    ? 0
                    : step->partner_family_best_coalesce_ready_partner_root_value_id_before,
                step->partner_family_best_coalesce_ready_partner_affinity_weight_before,
                step->partner_family_best_coalesce_ready_partner_is_mutual_before ? "yes" : "no",
                step->partner_family_external_affinity_weight_sum_before,
                step->partner_family_external_neighbor_count_after,
                step->partner_family_coalesce_ready_neighbor_count_after,
                step->partner_family_coalesce_ready_affinity_weight_sum_after,
                step->partner_family_best_coalesce_ready_partner_root_value_id_after == (size_t)-1 ? "-" : "ssa.",
                step->partner_family_best_coalesce_ready_partner_root_value_id_after == (size_t)-1
                    ? 0
                    : step->partner_family_best_coalesce_ready_partner_root_value_id_after,
                step->partner_family_best_coalesce_ready_partner_affinity_weight_after,
                step->partner_family_best_coalesce_ready_partner_is_mutual_after ? "yes" : "no",
                step->partner_family_external_affinity_weight_sum_after)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-119: out of memory appending move-transition item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
