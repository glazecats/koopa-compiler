#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

static ValueSsaAllocatorRetryFamilyEntryKind value_ssa_alloc_retry_family_entry_kind_from_select_option_kind(
    ValueSsaAllocationSelectOptionKind kind) {
    switch (kind) {
        case VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT:
            return VALUE_SSA_ALLOCATOR_RETRY_ENTRY_PREFERRED_EVICT;
        case VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT:
            return VALUE_SSA_ALLOCATOR_RETRY_ENTRY_GENERIC_EVICT;
        case VALUE_SSA_ALLOC_SELECT_OPTION_FREE:
        case VALUE_SSA_ALLOC_SELECT_OPTION_NONE:
        default:
            return VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE;
    }
}

static const char *value_ssa_allocator_retry_family_entry_kind_name(ValueSsaAllocatorRetryFamilyEntryKind kind) {
    switch (kind) {
        case VALUE_SSA_ALLOCATOR_RETRY_ENTRY_PREFERRED_EVICT:
            return "preferred-evict";
        case VALUE_SSA_ALLOCATOR_RETRY_ENTRY_GENERIC_EVICT:
            return "generic-evict";
        case VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE:
        default:
            return "free";
    }
}

void value_ssa_allocator_retry_family_agenda_init(ValueSsaAllocatorRetryFamilyAgenda *agenda) {
    if (!agenda) {
        return;
    }

    agenda->value_count = 0;
    agenda->item_count = 0;
    agenda->items = NULL;
}

void value_ssa_allocator_retry_family_agenda_free(ValueSsaAllocatorRetryFamilyAgenda *agenda) {
    if (!agenda) {
        return;
    }

    free(agenda->items);
    value_ssa_allocator_retry_family_agenda_init(agenda);
}

int value_ssa_compute_allocator_retry_family_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t color_budget,
    ValueSsaAllocatorRetryFamilyAgenda *agenda,
    ValueSsaError *error) {
    ValueSsaAllocationSelectQuery retry_query;
    ValueSsaAllocationRetryFamilyAgendaScratch scratch;
    size_t item_index;

    if (!function || !prep || !interference_graph || !plan || !result || !agenda ||
        function->next_value_id != result->value_count || result->value_count != prep->value_count ||
        interference_graph->value_count != result->value_count || plan->value_count != result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-150: invalid retry family-agenda contract");
        return 0;
    }

    value_ssa_allocator_retry_family_agenda_free(agenda);
    value_ssa_alloc_select_query_init(&retry_query);
    value_ssa_alloc_retry_family_agenda_scratch_init(&scratch);
    agenda->value_count = result->value_count;

    if (!value_ssa_alloc_select_query_allocate(&retry_query, color_budget, error)) {
        return 0;
    }
    if (!value_ssa_alloc_build_retry_family_agenda(prep,
            coalesce_analysis,
            interference_graph,
            plan,
            result,
            color_budget,
            &retry_query,
            &scratch,
            error)) {
        value_ssa_alloc_select_query_free(&retry_query);
        value_ssa_alloc_retry_family_agenda_scratch_free(&scratch);
        value_ssa_allocator_retry_family_agenda_free(agenda);
        return 0;
    }

    agenda->items = (ValueSsaAllocatorRetryFamilyAgendaItem *)calloc(
        scratch.item_count ? scratch.item_count : 1,
        sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    if (!agenda->items) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-151: out of memory preparing retry family agenda");
        value_ssa_alloc_select_query_free(&retry_query);
        value_ssa_alloc_retry_family_agenda_scratch_free(&scratch);
        value_ssa_allocator_retry_family_agenda_free(agenda);
        return 0;
    }
    agenda->item_count = scratch.item_count;

    for (item_index = 0; item_index < scratch.item_count; ++item_index) {
        const ValueSsaAllocationRetryFamilyCandidate *candidate = &scratch.items[item_index];
        ValueSsaAllocatorRetryFamilyAgendaItem *item = &agenda->items[item_index];

        item->root_value_id = candidate->family_root_value_id;
        item->representative_value_id = candidate->value_id;
        item->recoverable_member_count = candidate->recoverable_member_count;
        item->recoverable_intended_count = candidate->recoverable_intended_count;
        item->split_family_root_value_id = candidate->split_family_root_value_id;
        item->split_family_member_count = candidate->split_family_member_count;
        item->split_family_intended_count = candidate->split_family_intended_count;
        item->representative_spill_priority = result->spill_priorities[candidate->value_id];
        item->representative_spill_cost = value_ssa_alloc_value_spill_cost_for_select(prep, candidate->value_id);
        item->representative_split_child =
            prep && prep->split_child_values && candidate->value_id < prep->value_count
            ? prep->split_child_values[candidate->value_id]
            : 0u;
        item->representative_split_child_depth =
            prep && prep->split_child_depths && candidate->value_id < prep->value_count
            ? prep->split_child_depths[candidate->value_id]
            : 0;
        item->entry_kind = value_ssa_alloc_retry_family_entry_kind_from_select_option_kind(candidate->option.kind);
        item->preferred_source = candidate->option.preferred_source;
        item->preferred_class_size = candidate->option.preferred_class_size;
        item->preferred_partner_value_id = candidate->option.preferred_partner_value_id;
        item->color = candidate->option.color;
        item->requires_eviction = value_ssa_alloc_select_option_requires_eviction(&candidate->option) ? 1u : 0u;
        item->blocker_value_id = candidate->option.blocker_value_id;
        item->blocker_recolorable = candidate->option.blocker_recolorable;
        item->blocker_recolor_color = candidate->option.blocker_recolor_color;
    }

    value_ssa_alloc_select_query_free(&retry_query);
    value_ssa_alloc_retry_family_agenda_scratch_free(&scratch);
    return 1;
}

int value_ssa_dump_allocator_retry_family_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t item_index;

    if (!function || !agenda || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-152: invalid retry family-agenda dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder,
            "retry-family-agenda %s count=%zu\n",
            function->name ? function->name : "<anon>",
            agenda->item_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-153: out of memory building retry family agenda");
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        const ValueSsaAllocatorRetryFamilyAgendaItem *item = &agenda->items[item_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: root=ssa.%zu rep=ssa.%zu recoverable=%zu intended=%zu split_root=ssa.%zu split_members=%zu split_intended=%zu priority=%zu spill_cost=%zu split_child=%s depth=%zu kind=%s preferred=%s class=%zu color=%zu",
                item_index,
                item->root_value_id,
                item->representative_value_id,
                item->recoverable_member_count,
                item->recoverable_intended_count,
                item->split_family_root_value_id,
                item->split_family_member_count,
                item->split_family_intended_count,
                item->representative_spill_priority,
                item->representative_spill_cost,
                item->representative_split_child ? "yes" : "no",
                item->representative_split_child_depth,
                value_ssa_allocator_retry_family_entry_kind_name(item->entry_kind),
                value_ssa_allocation_preferred_color_source_name(item->preferred_source),
                item->preferred_class_size,
                item->color)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-154: out of memory appending retry agenda item");
            return 0;
        }
        if (item->preferred_partner_value_id != (size_t)-1) {
            if (!value_ssa_alloc_append_format(&builder, " partner=ssa.%zu", item->preferred_partner_value_id)) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-154: out of memory appending retry agenda partner");
                return 0;
            }
        }
        if (item->requires_eviction && item->blocker_value_id != (size_t)-1) {
            if (!value_ssa_alloc_append_format(&builder, " evict=ssa.%zu", item->blocker_value_id)) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-155: out of memory appending retry agenda blocker");
                return 0;
            }
            if (item->blocker_recolorable) {
                if (!value_ssa_alloc_append_format(&builder, " recolor=color%zu", item->blocker_recolor_color)) {
                    free(builder.data);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-155: out of memory appending retry agenda recolor");
                    return 0;
                }
            }
        }
        if (!value_ssa_alloc_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-156: out of memory finalizing retry agenda item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_allocator_retry_family_agenda_find_root(
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorRetryFamilyAgendaItem **out_item) {
    size_t item_index;

    if (!agenda) {
        return 0;
    }

    for (item_index = 0; item_index < agenda->item_count; ++item_index) {
        if (agenda->items[item_index].root_value_id != root_value_id) {
            continue;
        }
        if (out_index) {
            *out_index = item_index;
        }
        if (out_item) {
            *out_item = &agenda->items[item_index];
        }
        return 1;
    }

    return 0;
}

#endif
