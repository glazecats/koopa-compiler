#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_coalesce_opportunity_agenda_init(ValueSsaAllocatorCoalesceOpportunityAgenda *agenda) {
    if (!agenda) {
        return;
    }

    agenda->value_count = 0;
    agenda->item_count = 0;
    agenda->items = NULL;
}

void value_ssa_allocator_coalesce_opportunity_agenda_free(ValueSsaAllocatorCoalesceOpportunityAgenda *agenda) {
    if (!agenda) {
        return;
    }

    free(agenda->items);
    value_ssa_allocator_coalesce_opportunity_agenda_init(agenda);
}

static int value_ssa_alloc_coalesce_opportunity_find_item_index(
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id,
    size_t *out_index) {
    size_t item_index;

    if (!agenda) {
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        const ValueSsaAllocatorCoalesceOpportunityItem *item = &agenda->items[item_index];

        if (item->lhs_root_value_id != lhs_root_value_id || item->rhs_root_value_id != rhs_root_value_id) {
            continue;
        }
        if (out_index) {
            *out_index = item_index;
        }
        return 1;
    }

    return 0;
}

static int value_ssa_alloc_coalesce_opportunity_is_before(const ValueSsaAllocatorCoalesceOpportunityItem *lhs,
    const ValueSsaAllocatorCoalesceOpportunityItem *rhs) {
    size_t lhs_priority_hint;
    size_t rhs_priority_hint;

    if (!lhs || !rhs) {
        return lhs != NULL;
    }

    if ((int)lhs->mutual_best != (int)rhs->mutual_best) {
        return (int)lhs->mutual_best > (int)rhs->mutual_best;
    }
    if (lhs->total_preference_weight != rhs->total_preference_weight) {
        return lhs->total_preference_weight > rhs->total_preference_weight;
    }
    lhs_priority_hint = lhs->lhs_move_work_priority > lhs->rhs_move_work_priority
        ? lhs->lhs_move_work_priority
        : lhs->rhs_move_work_priority;
    rhs_priority_hint = rhs->lhs_move_work_priority > rhs->rhs_move_work_priority
        ? rhs->lhs_move_work_priority
        : rhs->rhs_move_work_priority;
    if (lhs_priority_hint != rhs_priority_hint) {
        return lhs_priority_hint > rhs_priority_hint;
    }
    if ((int)lhs->lhs_work_class != (int)rhs->lhs_work_class) {
        return (int)lhs->lhs_work_class < (int)rhs->lhs_work_class;
    }
    if ((int)lhs->rhs_work_class != (int)rhs->rhs_work_class) {
        return (int)lhs->rhs_work_class < (int)rhs->rhs_work_class;
    }
    if (lhs->lhs_root_value_id != rhs->lhs_root_value_id) {
        return lhs->lhs_root_value_id < rhs->lhs_root_value_id;
    }
    return lhs->rhs_root_value_id < rhs->rhs_root_value_id;
}

int value_ssa_compute_allocator_coalesce_opportunity_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveFamilyAnalysis *family_analysis,
    const ValueSsaAllocatorMoveWorklist *worklist,
    size_t color_budget,
    ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    ValueSsaError *error) {
    ValueSsaAllocatorMoveFamilyAnalysis owned_family_analysis;
    ValueSsaAllocatorMoveWorklist owned_worklist;
    const ValueSsaAllocatorMoveFamilyAnalysis *active_family_analysis = family_analysis;
    const ValueSsaAllocatorMoveWorklist *active_worklist = worklist;
    size_t family_index;
    int ok = 0;

    if (!function || !agenda) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-124: invalid coalesce-opportunity contract");
        return 0;
    }

    value_ssa_allocator_move_family_analysis_init(&owned_family_analysis);
    value_ssa_allocator_move_worklist_init(&owned_worklist);
    value_ssa_allocator_coalesce_opportunity_agenda_free(agenda);
    agenda->value_count = function->next_value_id;
    if (function->next_value_id == 0) {
        return 1;
    }

    if (!active_family_analysis) {
        if (!value_ssa_compute_allocator_move_family_analysis(
                function, NULL, NULL, NULL, color_budget, &owned_family_analysis, error)) {
            goto cleanup;
        }
        active_family_analysis = &owned_family_analysis;
    }
    if (!active_worklist) {
        if (!value_ssa_compute_allocator_move_worklist(
                function, active_family_analysis, NULL, color_budget, &owned_worklist, error)) {
            goto cleanup;
        }
        active_worklist = &owned_worklist;
    }

    agenda->items = (ValueSsaAllocatorCoalesceOpportunityItem *)calloc(
        active_family_analysis->family_count ? active_family_analysis->family_count : 1,
        sizeof(ValueSsaAllocatorCoalesceOpportunityItem));
    if (!agenda->items) {
        value_ssa_alloc_set_error(
            error, 0, 0, "VALUE-SSA-ALLOC-125: out of memory preparing coalesce-opportunity agenda");
        goto cleanup;
    }

    for (family_index = 0; family_index < active_family_analysis->family_count; ++family_index) {
        const ValueSsaAllocatorMoveFamilyItem *family_item = &active_family_analysis->items[family_index];
        const ValueSsaAllocatorMoveWorkItem *work_item = NULL;
        size_t partner_root_value_id;
        size_t lhs_root_value_id;
        size_t rhs_root_value_id;
        size_t item_index;
        ValueSsaAllocatorCoalesceOpportunityItem *item;

        if (family_item->best_coalesce_ready_partner_root_value_id == (size_t)-1 ||
            family_item->best_coalesce_ready_partner_affinity_weight == 0) {
            continue;
        }
        if (!value_ssa_allocator_move_worklist_find_root(
                active_worklist, family_item->root_value_id, NULL, &work_item) ||
            !work_item) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-126: coalesce-opportunity missing work item");
            goto cleanup;
        }

        partner_root_value_id = family_item->best_coalesce_ready_partner_root_value_id;
        lhs_root_value_id = family_item->root_value_id < partner_root_value_id ? family_item->root_value_id
                                                                                : partner_root_value_id;
        rhs_root_value_id = family_item->root_value_id < partner_root_value_id ? partner_root_value_id
                                                                                : family_item->root_value_id;

        if (!value_ssa_alloc_coalesce_opportunity_find_item_index(
                agenda, lhs_root_value_id, rhs_root_value_id, &item_index)) {
            item_index = agenda->item_count++;
            agenda->items[item_index].lhs_root_value_id = lhs_root_value_id;
            agenda->items[item_index].rhs_root_value_id = rhs_root_value_id;
            agenda->items[item_index].lhs_work_class = VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
            agenda->items[item_index].rhs_work_class = VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
        }
        item = &agenda->items[item_index];

        if (family_item->root_value_id == lhs_root_value_id) {
            item->lhs_prefers_rhs = 1u;
            item->lhs_preference_weight = family_item->best_coalesce_ready_partner_affinity_weight;
            item->lhs_work_class = work_item->work_class;
            item->lhs_move_work_priority = work_item->priority;
        } else {
            item->rhs_prefers_lhs = 1u;
            item->rhs_preference_weight = family_item->best_coalesce_ready_partner_affinity_weight;
            item->rhs_work_class = work_item->work_class;
            item->rhs_move_work_priority = work_item->priority;
        }
    }

    for (family_index = 0; family_index < agenda->item_count; ++family_index) {
        ValueSsaAllocatorCoalesceOpportunityItem *item = &agenda->items[family_index];
        size_t stronger_priority = item->lhs_move_work_priority > item->rhs_move_work_priority
            ? item->lhs_move_work_priority
            : item->rhs_move_work_priority;

        item->mutual_best = (item->lhs_prefers_rhs && item->rhs_prefers_lhs) ? 1u : 0u;
        item->total_preference_weight = item->lhs_preference_weight + item->rhs_preference_weight;
        item->priority = item->total_preference_weight * 16 + stronger_priority;
        if (item->mutual_best) {
            item->priority += 8;
        }
    }

    for (family_index = 0; family_index < agenda->item_count; ++family_index) {
        size_t scan_index;

        for (scan_index = family_index + 1; scan_index < agenda->item_count; ++scan_index) {
            ValueSsaAllocatorCoalesceOpportunityItem temp;

            if (!value_ssa_alloc_coalesce_opportunity_is_before(&agenda->items[scan_index], &agenda->items[family_index])) {
                continue;
            }
            temp = agenda->items[family_index];
            agenda->items[family_index] = agenda->items[scan_index];
            agenda->items[scan_index] = temp;
        }
    }

    ok = 1;

cleanup:
    value_ssa_allocator_move_worklist_free(&owned_worklist);
    value_ssa_allocator_move_family_analysis_free(&owned_family_analysis);
    if (!ok) {
        value_ssa_allocator_coalesce_opportunity_agenda_free(agenda);
    }
    return ok;
}

int value_ssa_dump_allocator_coalesce_opportunity_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t item_index;

    if (!function || !agenda || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-127: invalid coalesce-opportunity dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder,
            "coalesce-opportunities %s count=%zu\n",
            function->name ? function->name : "<anon>",
            agenda->item_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(
            error, 0, 0, "VALUE-SSA-ALLOC-128: out of memory building coalesce-opportunity dump");
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        const ValueSsaAllocatorCoalesceOpportunityItem *item = &agenda->items[item_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: pair=ssa.%zu<->ssa.%zu priority=%zu mutual=%s prefer=(%s@%zu,%s@%zu) class=(%s,%s) work_priority=(%zu,%zu)\n",
                item_index,
                item->lhs_root_value_id,
                item->rhs_root_value_id,
                item->priority,
                item->mutual_best ? "yes" : "no",
                item->lhs_prefers_rhs ? "yes" : "no",
                item->lhs_preference_weight,
                item->rhs_prefers_lhs ? "yes" : "no",
                item->rhs_preference_weight,
                value_ssa_allocator_move_work_class_name(item->lhs_work_class),
                value_ssa_allocator_move_work_class_name(item->rhs_work_class),
                item->lhs_move_work_priority,
                item->rhs_move_work_priority)) {
            free(builder.data);
            value_ssa_alloc_set_error(
                error, 0, 0, "VALUE-SSA-ALLOC-129: out of memory appending coalesce-opportunity item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_allocator_coalesce_opportunity_agenda_find_pair(
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorCoalesceOpportunityItem **out_item) {
    size_t ordered_lhs_root_value_id;
    size_t ordered_rhs_root_value_id;
    size_t item_index;

    if (!agenda) {
        return 0;
    }

    ordered_lhs_root_value_id =
        lhs_root_value_id < rhs_root_value_id ? lhs_root_value_id : rhs_root_value_id;
    ordered_rhs_root_value_id =
        lhs_root_value_id < rhs_root_value_id ? rhs_root_value_id : lhs_root_value_id;
    if (!value_ssa_alloc_coalesce_opportunity_find_item_index(
            agenda, ordered_lhs_root_value_id, ordered_rhs_root_value_id, &item_index)) {
        return 0;
    }
    if (out_index) {
        *out_index = item_index;
    }
    if (out_item) {
        *out_item = &agenda->items[item_index];
    }
    return 1;
}

int value_ssa_allocator_coalesce_opportunity_agenda_find_root_best(
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorCoalesceOpportunityItem **out_item) {
    size_t item_index;

    if (!agenda) {
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        const ValueSsaAllocatorCoalesceOpportunityItem *item = &agenda->items[item_index];

        if (item->lhs_root_value_id != root_value_id && item->rhs_root_value_id != root_value_id) {
            continue;
        }
        if (out_index) {
            *out_index = item_index;
        }
        if (out_item) {
            *out_item = item;
        }
        return 1;
    }

    return 0;
}

#endif
