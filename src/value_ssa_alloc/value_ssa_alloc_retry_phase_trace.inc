#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_retry_phase_trace_init(ValueSsaAllocatorRetryPhaseTrace *trace) {
    if (!trace) {
        return;
    }

    trace->value_count = 0;
    trace->phase_count = 0;
    trace->items = NULL;
}

void value_ssa_allocator_retry_phase_trace_free(ValueSsaAllocatorRetryPhaseTrace *trace) {
    if (!trace) {
        return;
    }

    free(trace->items);
    value_ssa_allocator_retry_phase_trace_init(trace);
}

int value_ssa_compute_allocator_retry_phase_trace(const ValueSsaAllocationResult *result,
    ValueSsaAllocatorRetryPhaseTrace *trace,
    ValueSsaError *error) {
    size_t value_id;
    size_t phase_capacity = 0;
    size_t *phase_seen = NULL;

    if (!result || !trace) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-157: invalid retry phase-trace contract");
        return 0;
    }

    value_ssa_allocator_retry_phase_trace_free(trace);
    trace->value_count = result->value_count;
    phase_capacity = result->next_spill_recovery_phase_id;
    if (phase_capacity == 0) {
        return 1;
    }

    trace->items = (ValueSsaAllocatorRetryPhaseTraceItem *)calloc(phase_capacity, sizeof(ValueSsaAllocatorRetryPhaseTraceItem));
    phase_seen = (size_t *)malloc(phase_capacity * sizeof(size_t));
    if (!trace->items || !phase_seen) {
        free(phase_seen);
        value_ssa_allocator_retry_phase_trace_free(trace);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-158: out of memory preparing retry phase trace");
        return 0;
    }

    for (value_id = 0; value_id < phase_capacity; ++value_id) {
        phase_seen[value_id] = 0;
    }

    for (value_id = 0; value_id < result->value_count; ++value_id) {
        size_t phase_id;
        ValueSsaAllocatorRetryPhaseTraceItem *item;

        if (!result->spill_recovered_flags || !result->spill_recovered_flags[value_id] ||
            !result->spill_recovery_phase_ids || result->spill_recovery_phase_ids[value_id] == (size_t)-1) {
            continue;
        }
        phase_id = result->spill_recovery_phase_ids[value_id];
        if (phase_id >= phase_capacity) {
            free(phase_seen);
            value_ssa_allocator_retry_phase_trace_free(trace);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-159: retry phase id out of range");
            return 0;
        }

        item = &trace->items[phase_id];
        if (!phase_seen[phase_id]) {
            phase_seen[phase_id] = 1;
            value_ssa_alloc_retry_phase_trace_item_clear(item);
            item->phase_id = phase_id;
            if (result->spill_recovery_phase_entries) {
                item->entry = result->spill_recovery_phase_entries[value_id];
            }
            item->family_root_value_id = item->entry.root_value_id;
            item->recovered_member_count = result->spill_recovery_phase_member_counts
                ? result->spill_recovery_phase_member_counts[value_id]
                : 0;
            item->first_recovery_order = result->spill_recovery_orders ? result->spill_recovery_orders[value_id] : 0;
            item->last_recovery_order = result->spill_recovery_orders ? result->spill_recovery_orders[value_id] : 0;
            ++trace->phase_count;
            continue;
        }

        if (result->spill_recovery_orders) {
            if (result->spill_recovery_orders[value_id] < item->first_recovery_order) {
                item->first_recovery_order = result->spill_recovery_orders[value_id];
            }
            if (result->spill_recovery_orders[value_id] > item->last_recovery_order) {
                item->last_recovery_order = result->spill_recovery_orders[value_id];
            }
        }
    }

    if (trace->phase_count < phase_capacity) {
        size_t compact_index = 0;
        size_t phase_id;

        for (phase_id = 0; phase_id < phase_capacity; ++phase_id) {
            if (!phase_seen[phase_id]) {
                continue;
            }
            if (compact_index != phase_id) {
                trace->items[compact_index] = trace->items[phase_id];
            }
            ++compact_index;
        }
    }

    free(phase_seen);
    return 1;
}

int value_ssa_dump_allocator_retry_phase_trace(const ValueSsaAllocatorRetryPhaseTrace *trace,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t item_index;

    if (!trace || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-160: invalid retry phase-trace dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder, "retry-phase-trace count=%zu\n", trace->phase_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-161: out of memory building retry phase trace");
        return 0;
    }

    for (item_index = 0; item_index < trace->phase_count; ++item_index) {
        const ValueSsaAllocatorRetryPhaseTraceItem *item = &trace->items[item_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: phase=%zu root=ssa.%zu entry=ssa.%zu summary=%zu/%zu recovered=%zu orders=%zu..%zu\n",
                item_index,
                item->phase_id,
                item->family_root_value_id,
                item->entry.representative_value_id,
                item->entry.recoverable_member_count,
                item->entry.recoverable_intended_count,
                item->recovered_member_count,
                item->first_recovery_order,
                item->last_recovery_order)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-162: out of memory appending retry phase trace");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_allocator_retry_phase_trace_find_phase_id(
    const ValueSsaAllocatorRetryPhaseTrace *trace,
    size_t phase_id,
    size_t *out_index,
    const ValueSsaAllocatorRetryPhaseTraceItem **out_item) {
    size_t item_index;

    if (!trace) {
        return 0;
    }

    for (item_index = 0; item_index < trace->phase_count; ++item_index) {
        if (trace->items[item_index].phase_id != phase_id) {
            continue;
        }
        if (out_index) {
            *out_index = item_index;
        }
        if (out_item) {
            *out_item = &trace->items[item_index];
        }
        return 1;
    }

    return 0;
}

#endif
