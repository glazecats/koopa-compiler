#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

void value_ssa_allocation_worklist_init(ValueSsaAllocationWorklist *worklist) {
    if (!worklist) {
        return;
    }

    worklist->value_count = 0;
    worklist->item_count = 0;
    worklist->items = NULL;
}

void value_ssa_allocation_worklist_free(ValueSsaAllocationWorklist *worklist) {
    if (!worklist) {
        return;
    }

    free(worklist->items);
    value_ssa_allocation_worklist_init(worklist);
}

static ValueSsaAllocationWorkClass value_ssa_classify_allocation_work_item(const ValueSsaAllocationPrep *prep,
    size_t value_id) {
    if (prep->affinity_sums[value_id] > 0) {
        return VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    }
    if (prep->interference_degrees[value_id] == 0) {
        return VALUE_SSA_ALLOC_WORK_ISOLATED;
    }
    if (prep->single_block_live_ranges[value_id]) {
        return VALUE_SSA_ALLOC_WORK_SINGLE_BLOCK;
    }
    if (prep->interference_degrees[value_id] >= 2) {
        return VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    }
    return VALUE_SSA_ALLOC_WORK_GLOBAL;
}

static size_t value_ssa_compute_allocation_work_priority(const ValueSsaAllocationPrep *prep, size_t value_id) {
    size_t cross_block_penalty = 0;

    if (prep->single_block_live_ranges && !prep->single_block_live_ranges[value_id]) {
        cross_block_penalty = 1;
    }

    return (prep->affinity_sums[value_id] * 8u) +
        (prep->interference_degrees[value_id] * 4u) +
        (prep->live_block_counts[value_id] * 2u) +
        (prep->use_loop_depth_sums ? prep->use_loop_depth_sums[value_id] : 0u) +
        (prep->use_call_density_sums ? prep->use_call_density_sums[value_id] : 0u) +
        (prep->loop_depth_sums ? prep->loop_depth_sums[value_id] : 0u) +
        (prep->call_density_sums ? prep->call_density_sums[value_id] : 0u) +
        (prep->call_crossing_counts ? prep->call_crossing_counts[value_id] : 0u) +
        cross_block_penalty +
        prep->use_counts[value_id];
}

static int value_ssa_compare_allocation_work_items(const void *lhs_ptr, const void *rhs_ptr) {
    const ValueSsaAllocationWorkItem *lhs = (const ValueSsaAllocationWorkItem *)lhs_ptr;
    const ValueSsaAllocationWorkItem *rhs = (const ValueSsaAllocationWorkItem *)rhs_ptr;

    if ((int)lhs->work_class < (int)rhs->work_class) {
        return -1;
    }
    if ((int)lhs->work_class > (int)rhs->work_class) {
        return 1;
    }
    if (lhs->priority > rhs->priority) {
        return -1;
    }
    if (lhs->priority < rhs->priority) {
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

int value_ssa_compute_allocation_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    ValueSsaAllocationWorklist *worklist,
    ValueSsaError *error) {
    ValueSsaAllocationPrep owned_prep;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    int ok = 0;

    if (!function || !worklist) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-223: invalid allocation-worklist contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-224: cannot compute allocation worklist for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_allocation_worklist_free(worklist);
    worklist->value_count = function->next_value_id;
    worklist->item_count = function->next_value_id;
    if (function->next_value_id == 0) {
        return 1;
    }

    value_ssa_allocation_prep_init(&owned_prep);
    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    } else if (active_prep->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-225: allocation-worklist prep value_count=%zu does not match function value_count=%zu",
            active_prep->value_count,
            function->next_value_id);
        goto cleanup;
    }

    worklist->items = (ValueSsaAllocationWorkItem *)calloc(
        worklist->item_count, sizeof(ValueSsaAllocationWorkItem));
    if (!worklist->items) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-226: out of memory while building allocation worklist");
        goto cleanup;
    }

    {
        size_t value_id;

        for (value_id = 0; value_id < worklist->item_count; ++value_id) {
            worklist->items[value_id].value_id = value_id;
            worklist->items[value_id].work_class =
                value_ssa_classify_allocation_work_item(active_prep, value_id);
            worklist->items[value_id].priority =
                value_ssa_compute_allocation_work_priority(active_prep, value_id);
        }
    }

    qsort(worklist->items,
        worklist->item_count,
        sizeof(ValueSsaAllocationWorkItem),
        value_ssa_compare_allocation_work_items);
    ok = 1;

cleanup:
    value_ssa_allocation_prep_free(&owned_prep);
    if (!ok) {
        value_ssa_allocation_worklist_free(worklist);
    }
    return ok;
}

#endif
