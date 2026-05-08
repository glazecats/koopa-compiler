#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_move_family_analysis_init(ValueSsaAllocatorMoveFamilyAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->value_count = 0;
    analysis->family_count = 0;
    analysis->items = NULL;
    analysis->value_family_indices = NULL;
    analysis->root_family_indices = NULL;
}

void value_ssa_allocator_move_family_analysis_free(ValueSsaAllocatorMoveFamilyAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->items);
    free(analysis->value_family_indices);
    free(analysis->root_family_indices);
    value_ssa_allocator_move_family_analysis_init(analysis);
}

const char *value_ssa_allocator_move_family_state_name(ValueSsaAllocatorMoveFamilyState state) {
    switch (state) {
    case VALUE_SSA_ALLOC_MOVE_FAMILY_INTERNAL:
        return "internal";
    case VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED:
        return "released";
    case VALUE_SSA_ALLOC_MOVE_FAMILY_FROZEN:
        return "frozen";
    default:
        return "unknown";
    }
}

int value_ssa_compute_allocator_move_family_analysis(const ValueSsaFunction *function,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    ValueSsaError *error) {
    ValueSsaInterferenceGraph owned_interference;
    ValueSsaCopyAffinityGraph owned_affinity;
    ValueSsaAllocatorCoalesceAnalysis owned_coalesce;
    ValueSsaAllocatorPlan owned_plan;
    const ValueSsaCopyAffinityGraph *active_affinity = affinity_graph;
    const ValueSsaAllocatorCoalesceAnalysis *active_coalesce = coalesce_analysis;
    const ValueSsaAllocatorPlan *active_plan = plan;
    size_t *root_to_family_index = NULL;
    unsigned char *neighbor_seen = NULL;
    unsigned char *coalesce_ready_seen = NULL;
    size_t *coalesce_ready_weight_matrix = NULL;
    int ok = 0;
    size_t value_id;

    if (!function || !analysis) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-102: invalid move-family analysis contract");
        return 0;
    }

    value_ssa_interference_graph_init(&owned_interference);
    value_ssa_copy_affinity_graph_init(&owned_affinity);
    value_ssa_allocator_coalesce_analysis_init(&owned_coalesce);
    value_ssa_allocator_plan_init(&owned_plan);
    value_ssa_allocator_move_family_analysis_free(analysis);
    analysis->value_count = function->next_value_id;
    if (function->next_value_id == 0) {
        return 1;
    }

    if (!active_affinity) {
        if (!value_ssa_compute_interference_graph(function, NULL, NULL, &owned_interference, error) ||
            !value_ssa_compute_copy_affinity_graph(function, &owned_interference, &owned_affinity, error)) {
            goto cleanup;
        }
        active_affinity = &owned_affinity;
    }
    if (!active_coalesce) {
        if (!value_ssa_compute_allocator_coalesce_analysis(
                function, NULL, NULL, active_affinity, color_budget, &owned_coalesce, error)) {
            goto cleanup;
        }
        active_coalesce = &owned_coalesce;
    }
    if (!active_plan) {
        if (!value_ssa_compute_allocator_plan(
                function, NULL, NULL, NULL, active_affinity, active_coalesce, color_budget, &owned_plan, error)) {
            goto cleanup;
        }
        active_plan = &owned_plan;
    }

    analysis->value_family_indices =
        (size_t *)malloc((analysis->value_count ? analysis->value_count : 1) * sizeof(size_t));
    analysis->root_family_indices =
        (size_t *)malloc((analysis->value_count ? analysis->value_count : 1) * sizeof(size_t));
    root_to_family_index = (size_t *)malloc((analysis->value_count ? analysis->value_count : 1) * sizeof(size_t));
    if (!analysis->value_family_indices || !analysis->root_family_indices || !root_to_family_index) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-103: out of memory preparing move-family analysis");
        goto cleanup;
    }

    analysis->family_count = 0;
    for (value_id = 0; value_id < analysis->value_count; ++value_id) {
        analysis->value_family_indices[value_id] = (size_t)-1;
        analysis->root_family_indices[value_id] = (size_t)-1;
        root_to_family_index[value_id] = (size_t)-1;
        if (!active_coalesce->class_sizes || active_coalesce->class_sizes[value_id] == 0) {
            continue;
        }
        ++analysis->family_count;
    }

    analysis->items = (ValueSsaAllocatorMoveFamilyItem *)calloc(
        analysis->family_count ? analysis->family_count : 1, sizeof(ValueSsaAllocatorMoveFamilyItem));
    neighbor_seen = (unsigned char *)calloc(
        analysis->family_count ? (analysis->family_count * analysis->family_count) : 1, sizeof(unsigned char));
    coalesce_ready_seen = (unsigned char *)calloc(
        analysis->family_count ? (analysis->family_count * analysis->family_count) : 1, sizeof(unsigned char));
    coalesce_ready_weight_matrix = (size_t *)calloc(
        analysis->family_count ? (analysis->family_count * analysis->family_count) : 1, sizeof(size_t));
    if (!analysis->items || !neighbor_seen || !coalesce_ready_seen || !coalesce_ready_weight_matrix) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-104: out of memory allocating move-family tables");
        goto cleanup;
    }

    {
        size_t family_index = 0;

        for (value_id = 0; value_id < analysis->value_count; ++value_id) {
            if (!active_coalesce->class_sizes || active_coalesce->class_sizes[value_id] == 0) {
                continue;
            }
            analysis->items[family_index].root_value_id = value_id;
            analysis->items[family_index].class_size = active_coalesce->class_sizes[value_id];
            analysis->items[family_index].best_coalesce_ready_partner_root_value_id = (size_t)-1;
            root_to_family_index[value_id] = family_index;
            ++family_index;
        }
    }

    for (value_id = 0; value_id < analysis->value_count; ++value_id) {
        size_t root_value_id;
        size_t family_index;

        root_value_id = active_coalesce->value_roots ? active_coalesce->value_roots[value_id] : value_id;
        if (root_value_id >= analysis->value_count || root_to_family_index[root_value_id] == (size_t)-1) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-105: invalid move-family root mapping");
            goto cleanup;
        }
        family_index = root_to_family_index[root_value_id];
        analysis->value_family_indices[value_id] = family_index;
    }

    for (value_id = 0; value_id < analysis->value_count; ++value_id) {
        size_t root_value_id = active_coalesce->value_roots ? active_coalesce->value_roots[value_id] : value_id;

        if (root_value_id < analysis->value_count) {
            analysis->root_family_indices[root_value_id] = analysis->value_family_indices[value_id];
        }
    }

    if (active_affinity && active_affinity->weights) {
        size_t lhs_value_id;

        for (lhs_value_id = 0; lhs_value_id < analysis->value_count; ++lhs_value_id) {
            size_t rhs_value_id;
            size_t lhs_family_index = analysis->value_family_indices[lhs_value_id];

            for (rhs_value_id = lhs_value_id + 1; rhs_value_id < analysis->value_count; ++rhs_value_id) {
                size_t rhs_family_index = analysis->value_family_indices[rhs_value_id];
                size_t weight = active_affinity->weights[lhs_value_id * active_affinity->value_count + rhs_value_id];
                const ValueSsaAllocatorCoalesceItem *coalesce_item = NULL;

                if (weight == 0 || lhs_family_index == rhs_family_index) {
                    continue;
                }

                analysis->items[lhs_family_index].external_affinity_weight_sum += weight;
                analysis->items[rhs_family_index].external_affinity_weight_sum += weight;
                if (!neighbor_seen[lhs_family_index * analysis->family_count + rhs_family_index]) {
                    neighbor_seen[lhs_family_index * analysis->family_count + rhs_family_index] = 1;
                    ++analysis->items[lhs_family_index].external_neighbor_family_count;
                }
                if (!neighbor_seen[rhs_family_index * analysis->family_count + lhs_family_index]) {
                    neighbor_seen[rhs_family_index * analysis->family_count + lhs_family_index] = 1;
                    ++analysis->items[rhs_family_index].external_neighbor_family_count;
                }
                if (value_ssa_allocator_coalesce_analysis_find_pair(
                        active_coalesce, lhs_value_id, rhs_value_id, NULL, &coalesce_item) &&
                    coalesce_item && coalesce_item->can_coalesce) {
                    size_t lhs_pair_weight_index = lhs_family_index * analysis->family_count + rhs_family_index;
                    size_t rhs_pair_weight_index = rhs_family_index * analysis->family_count + lhs_family_index;
                    ValueSsaAllocatorMoveFamilyItem *lhs_family_item = &analysis->items[lhs_family_index];
                    ValueSsaAllocatorMoveFamilyItem *rhs_family_item = &analysis->items[rhs_family_index];

                    analysis->items[lhs_family_index].coalesce_ready_affinity_weight_sum += weight;
                    analysis->items[rhs_family_index].coalesce_ready_affinity_weight_sum += weight;
                    coalesce_ready_weight_matrix[lhs_pair_weight_index] += weight;
                    coalesce_ready_weight_matrix[rhs_pair_weight_index] += weight;
                    if (coalesce_ready_weight_matrix[lhs_pair_weight_index] >
                            lhs_family_item->best_coalesce_ready_partner_affinity_weight ||
                        (coalesce_ready_weight_matrix[lhs_pair_weight_index] ==
                                lhs_family_item->best_coalesce_ready_partner_affinity_weight &&
                            (lhs_family_item->best_coalesce_ready_partner_root_value_id == (size_t)-1 ||
                                analysis->items[rhs_family_index].root_value_id <
                                    lhs_family_item->best_coalesce_ready_partner_root_value_id))) {
                        lhs_family_item->best_coalesce_ready_partner_root_value_id =
                            analysis->items[rhs_family_index].root_value_id;
                        lhs_family_item->best_coalesce_ready_partner_affinity_weight =
                            coalesce_ready_weight_matrix[lhs_pair_weight_index];
                    }
                    if (coalesce_ready_weight_matrix[rhs_pair_weight_index] >
                            rhs_family_item->best_coalesce_ready_partner_affinity_weight ||
                        (coalesce_ready_weight_matrix[rhs_pair_weight_index] ==
                                rhs_family_item->best_coalesce_ready_partner_affinity_weight &&
                            (rhs_family_item->best_coalesce_ready_partner_root_value_id == (size_t)-1 ||
                                analysis->items[lhs_family_index].root_value_id <
                                    rhs_family_item->best_coalesce_ready_partner_root_value_id))) {
                        rhs_family_item->best_coalesce_ready_partner_root_value_id =
                            analysis->items[lhs_family_index].root_value_id;
                        rhs_family_item->best_coalesce_ready_partner_affinity_weight =
                            coalesce_ready_weight_matrix[rhs_pair_weight_index];
                    }
                    if (!coalesce_ready_seen[lhs_family_index * analysis->family_count + rhs_family_index]) {
                        coalesce_ready_seen[lhs_family_index * analysis->family_count + rhs_family_index] = 1;
                        ++analysis->items[lhs_family_index].coalesce_ready_neighbor_family_count;
                    }
                    if (!coalesce_ready_seen[rhs_family_index * analysis->family_count + lhs_family_index]) {
                        coalesce_ready_seen[rhs_family_index * analysis->family_count + lhs_family_index] = 1;
                        ++analysis->items[rhs_family_index].coalesce_ready_neighbor_family_count;
                    }
                }
            }
        }
    }

    if (active_plan) {
        size_t item_index;

        for (item_index = 0; item_index < active_plan->item_count; ++item_index) {
            const ValueSsaAllocatorPlanItem *item = &active_plan->items[item_index];
            size_t family_index;

            if (item->value_id >= analysis->value_count) {
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-106: move-family plan value out of range");
                goto cleanup;
            }
            family_index = analysis->value_family_indices[item->value_id];
            if (item->initial_effective_move_related) {
                analysis->items[family_index].initial_move_related = 1;
            }
            if (item->was_frozen) {
                analysis->items[family_index].was_frozen = 1;
            }
            if (item->removal_kind == VALUE_SSA_ALLOCATOR_REMOVE_SPILL) {
                ++analysis->items[family_index].spill_removed_count;
            } else {
                ++analysis->items[family_index].simplify_removed_count;
            }
        }
    }

    for (value_id = 0; value_id < analysis->family_count; ++value_id) {
        ValueSsaAllocatorMoveFamilyItem *item = &analysis->items[value_id];
        const ValueSsaAllocatorMoveFamilyItem *partner_item = NULL;

        if (item->best_coalesce_ready_partner_root_value_id != (size_t)-1 &&
            value_ssa_allocator_move_family_analysis_find_root(analysis,
                item->best_coalesce_ready_partner_root_value_id,
                NULL,
                &partner_item) &&
            partner_item &&
            partner_item->best_coalesce_ready_partner_root_value_id == item->root_value_id &&
            item->best_coalesce_ready_partner_affinity_weight > 0 &&
            partner_item->best_coalesce_ready_partner_affinity_weight > 0) {
            item->best_coalesce_ready_partner_is_mutual = 1u;
        }

        if (!item->initial_move_related) {
            item->state = VALUE_SSA_ALLOC_MOVE_FAMILY_INTERNAL;
        } else if (item->was_frozen) {
            item->state = VALUE_SSA_ALLOC_MOVE_FAMILY_FROZEN;
        } else {
            item->state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;
        }
    }

    ok = 1;

cleanup:
    free(coalesce_ready_weight_matrix);
    free(coalesce_ready_seen);
    free(neighbor_seen);
    free(root_to_family_index);
    value_ssa_allocator_plan_free(&owned_plan);
    value_ssa_allocator_coalesce_analysis_free(&owned_coalesce);
    value_ssa_copy_affinity_graph_free(&owned_affinity);
    value_ssa_interference_graph_free(&owned_interference);
    if (!ok) {
        value_ssa_allocator_move_family_analysis_free(analysis);
    }
    return ok;
}

int value_ssa_allocator_move_family_analysis_find_root(const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveFamilyItem **out_item) {
    size_t family_index;

    if (!analysis || !analysis->root_family_indices || root_value_id >= analysis->value_count) {
        return 0;
    }

    family_index = analysis->root_family_indices[root_value_id];
    if (family_index >= analysis->family_count || analysis->items[family_index].root_value_id != root_value_id) {
        return 0;
    }
    if (out_index) {
        *out_index = family_index;
    }
    if (out_item) {
        *out_item = &analysis->items[family_index];
    }
    return 1;

}

int value_ssa_allocator_move_family_analysis_get_value_family(const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveFamilyItem **out_item) {
    size_t family_index;

    if (!analysis || !analysis->value_family_indices || value_id >= analysis->value_count) {
        return 0;
    }

    family_index = analysis->value_family_indices[value_id];
    if (family_index >= analysis->family_count) {
        return 0;
    }
    if (out_index) {
        *out_index = family_index;
    }
    if (out_item) {
        *out_item = &analysis->items[family_index];
    }
    return 1;
}

int value_ssa_dump_allocator_move_family_analysis(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t family_index;

    if (!function || !analysis || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-107: invalid move-family dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder,
            "move-families %s count=%zu\n",
            function->name ? function->name : "<anon>",
            analysis->family_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-108: out of memory building move-family dump");
        return 0;
    }

    for (family_index = 0; family_index < analysis->family_count; ++family_index) {
        const ValueSsaAllocatorMoveFamilyItem *item = &analysis->items[family_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: root=ssa.%zu size=%zu state=%s initial_move=%s frozen=%s external_neighbors=%zu ready_neighbors=%zu ready_affinity=%zu best_ready=%s%zu@%zu mutual=%s external_affinity=%zu removed=(simplify=%zu,spill=%zu)\n",
                family_index,
                item->root_value_id,
                item->class_size,
                value_ssa_allocator_move_family_state_name(item->state),
                item->initial_move_related ? "yes" : "no",
                item->was_frozen ? "yes" : "no",
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
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-109: out of memory appending move-family item");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
