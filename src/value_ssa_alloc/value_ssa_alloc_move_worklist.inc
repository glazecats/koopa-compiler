#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_move_worklist_init(ValueSsaAllocatorMoveWorklist *worklist) {
    if (!worklist) {
        return;
    }

    worklist->value_count = 0;
    worklist->item_count = 0;
    worklist->items = NULL;
    worklist->value_work_indices = NULL;
    worklist->root_work_indices = NULL;
}

void value_ssa_allocator_move_worklist_free(ValueSsaAllocatorMoveWorklist *worklist) {
    if (!worklist) {
        return;
    }

    free(worklist->items);
    free(worklist->value_work_indices);
    free(worklist->root_work_indices);
    value_ssa_allocator_move_worklist_init(worklist);
}

const char *value_ssa_allocator_move_work_class_name(ValueSsaAllocatorMoveWorkClass work_class) {
    switch (work_class) {
    case VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY:
        return "coalesce-ready";
    case VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING:
        return "freeze-pending";
    case VALUE_SSA_ALLOC_MOVE_WORK_RELEASED:
        return "released";
    case VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL:
        return "internal";
    default:
        return "unknown";
    }
}

static ValueSsaAllocatorMoveWorkClass value_ssa_alloc_classify_move_work_item(
    const ValueSsaAllocatorMoveFamilyItem *family_item) {
    if (!family_item) {
        return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
    }
    if (family_item->state == VALUE_SSA_ALLOC_MOVE_FAMILY_FROZEN) {
        return VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING;
    }
    if (family_item->state == VALUE_SSA_ALLOC_MOVE_FAMILY_INTERNAL) {
        return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
    }
    if (family_item->initial_move_related && family_item->coalesce_ready_neighbor_family_count > 0) {
        return VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    }
    if (family_item->initial_move_related) {
        return VALUE_SSA_ALLOC_MOVE_WORK_RELEASED;
    }
    return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
}

static int value_ssa_alloc_move_work_class_rank(ValueSsaAllocatorMoveWorkClass work_class) {
    switch (work_class) {
    case VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY:
        return 0;
    case VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING:
        return 1;
    case VALUE_SSA_ALLOC_MOVE_WORK_RELEASED:
        return 2;
    case VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL:
        return 3;
    default:
        return 4;
    }
}

static int value_ssa_alloc_move_work_item_is_before(const ValueSsaAllocatorMoveWorkItem *lhs,
    const ValueSsaAllocatorMoveWorkItem *rhs) {
    if (!lhs || !rhs) {
        return lhs != NULL;
    }
    if (value_ssa_alloc_move_work_class_rank(lhs->work_class) <
        value_ssa_alloc_move_work_class_rank(rhs->work_class)) {
        return 1;
    }
    if (value_ssa_alloc_move_work_class_rank(lhs->work_class) >
        value_ssa_alloc_move_work_class_rank(rhs->work_class)) {
        return 0;
    }
    if (lhs->priority > rhs->priority) {
        return 1;
    }
    if (lhs->priority < rhs->priority) {
        return 0;
    }
    return lhs->root_value_id < rhs->root_value_id;
}

static int value_ssa_alloc_move_work_item_compare_qsort(const void *lhs_ptr, const void *rhs_ptr) {
    const ValueSsaAllocatorMoveWorkItem *lhs = (const ValueSsaAllocatorMoveWorkItem *)lhs_ptr;
    const ValueSsaAllocatorMoveWorkItem *rhs = (const ValueSsaAllocatorMoveWorkItem *)rhs_ptr;

    if (value_ssa_alloc_move_work_item_is_before(lhs, rhs)) {
        return -1;
    }
    if (value_ssa_alloc_move_work_item_is_before(rhs, lhs)) {
        return 1;
    }
    return 0;
}

int value_ssa_compute_allocator_move_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveFamilyAnalysis *family_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocatorMoveWorklist *worklist,
    ValueSsaError *error) {
    ValueSsaAllocatorMoveFamilyAnalysis owned_family_analysis;
    ValueSsaAllocatorPlan owned_plan;
    const ValueSsaAllocatorMoveFamilyAnalysis *active_family_analysis = family_analysis;
    const ValueSsaAllocatorPlan *active_plan = plan;
    size_t *root_base_priorities = NULL;
    size_t family_index;
    int ok = 0;

    if (!function || !worklist) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-110: invalid move-worklist contract");
        return 0;
    }

    value_ssa_allocator_move_family_analysis_init(&owned_family_analysis);
    value_ssa_allocator_plan_init(&owned_plan);
    value_ssa_allocator_move_worklist_free(worklist);
    worklist->value_count = function->next_value_id;
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
    if (!active_plan) {
        if (!value_ssa_compute_allocator_plan(
                function, NULL, NULL, NULL, NULL, NULL, color_budget, &owned_plan, error)) {
            goto cleanup;
        }
        active_plan = &owned_plan;
    }

    worklist->item_count = active_family_analysis->family_count;
    worklist->items = (ValueSsaAllocatorMoveWorkItem *)calloc(
        worklist->item_count ? worklist->item_count : 1, sizeof(ValueSsaAllocatorMoveWorkItem));
    worklist->value_work_indices =
        (size_t *)malloc((worklist->value_count ? worklist->value_count : 1) * sizeof(size_t));
    worklist->root_work_indices =
        (size_t *)malloc((worklist->value_count ? worklist->value_count : 1) * sizeof(size_t));
    root_base_priorities =
        (size_t *)calloc((worklist->value_count ? worklist->value_count : 1), sizeof(size_t));
    if (!worklist->items || !worklist->value_work_indices || !worklist->root_work_indices || !root_base_priorities) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-111: out of memory preparing move-worklist");
        goto cleanup;
    }

    for (family_index = 0; family_index < worklist->value_count; ++family_index) {
        worklist->value_work_indices[family_index] = (size_t)-1;
        worklist->root_work_indices[family_index] = (size_t)-1;
    }

    if (active_plan && active_plan->items) {
        size_t plan_index;

        for (plan_index = 0; plan_index < active_plan->item_count; ++plan_index) {
            const ValueSsaAllocatorPlanItem *plan_item = &active_plan->items[plan_index];
            size_t root_value_id = plan_item->coalesce_root_value_id;

            if (root_value_id >= worklist->value_count) {
                continue;
            }
            if (plan_item->priority > root_base_priorities[root_value_id]) {
                root_base_priorities[root_value_id] = plan_item->priority;
            }
        }
    }

    for (family_index = 0; family_index < active_family_analysis->family_count; ++family_index) {
        const ValueSsaAllocatorMoveFamilyItem *family_item = &active_family_analysis->items[family_index];
        ValueSsaAllocatorMoveWorkItem *work_item = &worklist->items[family_index];
        size_t priority = 0;

        work_item->root_value_id = family_item->root_value_id;
        work_item->work_class = value_ssa_alloc_classify_move_work_item(family_item);
        work_item->class_size = family_item->class_size;
        work_item->external_neighbor_family_count = family_item->external_neighbor_family_count;
        work_item->coalesce_ready_neighbor_family_count = family_item->coalesce_ready_neighbor_family_count;
        work_item->coalesce_ready_affinity_weight_sum = family_item->coalesce_ready_affinity_weight_sum;
        work_item->best_coalesce_ready_partner_root_value_id = family_item->best_coalesce_ready_partner_root_value_id;
        work_item->best_coalesce_ready_partner_affinity_weight =
            family_item->best_coalesce_ready_partner_affinity_weight;
        work_item->best_coalesce_ready_partner_is_mutual = family_item->best_coalesce_ready_partner_is_mutual;
        work_item->external_affinity_weight_sum = family_item->external_affinity_weight_sum;
        work_item->simplify_removed_count = family_item->simplify_removed_count;
        work_item->spill_removed_count = family_item->spill_removed_count;
        if (family_item->root_value_id < worklist->value_count) {
            priority = root_base_priorities[family_item->root_value_id];
        }
        priority += family_item->coalesce_ready_neighbor_family_count * 16;
        priority += family_item->coalesce_ready_affinity_weight_sum * 8;
        priority += family_item->best_coalesce_ready_partner_affinity_weight * 4;
        priority += family_item->best_coalesce_ready_partner_is_mutual ? 8 : 0;
        priority += family_item->external_affinity_weight_sum * 8;
        priority += family_item->external_neighbor_family_count * 4;
        priority += family_item->class_size * 2;
        work_item->priority = priority;
    }

    if (worklist->item_count > 1) {
        qsort(worklist->items,
            worklist->item_count,
            sizeof(ValueSsaAllocatorMoveWorkItem),
            value_ssa_alloc_move_work_item_compare_qsort);
    }

    for (family_index = 0; family_index < worklist->item_count; ++family_index) {
        size_t root_value_id = worklist->items[family_index].root_value_id;

        if (root_value_id < worklist->value_count) {
            worklist->root_work_indices[root_value_id] = family_index;
        }
    }

    for (family_index = 0; family_index < active_family_analysis->value_count; ++family_index) {
        size_t family_item_index = active_family_analysis->value_family_indices[family_index];
        size_t root_value_id;

        if (family_item_index >= active_family_analysis->family_count) {
            continue;
        }
        root_value_id = active_family_analysis->items[family_item_index].root_value_id;
        if (root_value_id < worklist->value_count) {
            worklist->value_work_indices[family_index] = worklist->root_work_indices[root_value_id];
        }
    }

    ok = 1;

cleanup:
    free(root_base_priorities);
    value_ssa_allocator_plan_free(&owned_plan);
    value_ssa_allocator_move_family_analysis_free(&owned_family_analysis);
    if (!ok) {
        value_ssa_allocator_move_worklist_free(worklist);
    }
    return ok;
}

int value_ssa_allocator_move_worklist_find_root(const ValueSsaAllocatorMoveWorklist *worklist,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveWorkItem **out_item) {
    size_t work_index;
    size_t item_index;

    if (!worklist || root_value_id >= worklist->value_count) {
        return 0;
    }
    if (worklist->root_work_indices) {
        work_index = worklist->root_work_indices[root_value_id];
        if (work_index < worklist->item_count && worklist->items[work_index].root_value_id == root_value_id) {
            if (out_index) {
                *out_index = work_index;
            }
            if (out_item) {
                *out_item = &worklist->items[work_index];
            }
            return 1;
        }
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        if (worklist->items[item_index].root_value_id != root_value_id) {
            continue;
        }
        if (out_index) {
            *out_index = item_index;
        }
        if (out_item) {
            *out_item = &worklist->items[item_index];
        }
        return 1;
    }
    return 0;
}

int value_ssa_allocator_move_worklist_get_value_work(const ValueSsaAllocatorMoveWorklist *worklist,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveWorkItem **out_item) {
    size_t work_index;

    if (!worklist || !worklist->value_work_indices || value_id >= worklist->value_count) {
        return 0;
    }
    work_index = worklist->value_work_indices[value_id];
    if (work_index >= worklist->item_count) {
        return 0;
    }
    if (out_index) {
        *out_index = work_index;
    }
    if (out_item) {
        *out_item = &worklist->items[work_index];
    }
    return 1;
}

int value_ssa_dump_allocator_move_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveWorklist *worklist,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t work_index;

    if (!function || !worklist || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-112: invalid move-worklist dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder,
            "move-worklist %s count=%zu\n",
            function->name ? function->name : "<anon>",
            worklist->item_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-113: out of memory building move-worklist dump");
        return 0;
    }

    for (work_index = 0; work_index < worklist->item_count; ++work_index) {
        const ValueSsaAllocatorMoveWorkItem *item = &worklist->items[work_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: root=ssa.%zu class=%s priority=%zu size=%zu external_neighbors=%zu ready_neighbors=%zu ready_affinity=%zu best_ready=%s%zu@%zu mutual=%s external_affinity=%zu removed=(simplify=%zu,spill=%zu)\n",
                work_index,
                item->root_value_id,
                value_ssa_allocator_move_work_class_name(item->work_class),
                item->priority,
                item->class_size,
                item->external_neighbor_family_count,
                item->coalesce_ready_neighbor_family_count,
                item->coalesce_ready_affinity_weight_sum,
                item->best_coalesce_ready_partner_root_value_id == (size_t)-1 ? "-" : "ssa.",
                item->best_coalesce_ready_partner_root_value_id == (size_t)-1
                    ? 0
                    : item->best_coalesce_ready_partner_root_value_id,
                item->best_coalesce_ready_partner_affinity_weight,
                item->best_coalesce_ready_partner_is_mutual ? "yes" : "no",
                item->external_affinity_weight_sum,
                item->simplify_removed_count,
                item->spill_removed_count)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-114: out of memory appending move-worklist item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
