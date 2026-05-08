#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_coalesce_analysis_init(ValueSsaAllocatorCoalesceAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    analysis->value_count = 0;
    analysis->item_count = 0;
    analysis->items = NULL;
    analysis->value_roots = NULL;
    analysis->class_sizes = NULL;
    analysis->pair_index_capacity = 0;
    analysis->pair_index_lhs = NULL;
    analysis->pair_index_rhs = NULL;
    analysis->pair_index_item_indices = NULL;
}

void value_ssa_allocator_coalesce_analysis_free(ValueSsaAllocatorCoalesceAnalysis *analysis) {
    if (!analysis) {
        return;
    }

    free(analysis->items);
    free(analysis->value_roots);
    free(analysis->class_sizes);
    free(analysis->pair_index_lhs);
    free(analysis->pair_index_rhs);
    free(analysis->pair_index_item_indices);
    value_ssa_allocator_coalesce_analysis_init(analysis);
}

static size_t value_ssa_alloc_coalesce_pair_hash(size_t lhs_value_id,
    size_t rhs_value_id,
    size_t capacity) {
    size_t lhs = lhs_value_id < rhs_value_id ? lhs_value_id : rhs_value_id;
    size_t rhs = lhs_value_id < rhs_value_id ? rhs_value_id : lhs_value_id;
    size_t hash = lhs * 1315423911u + rhs * 2654435761u + 1469598103934665603ull;

    return capacity == 0 ? 0 : hash % capacity;
}

static int value_ssa_alloc_coalesce_build_pair_index(ValueSsaAllocatorCoalesceAnalysis *analysis,
    ValueSsaError *error) {
    size_t capacity = 0;
    size_t item_index;

    if (!analysis) {
        return 0;
    }

    free(analysis->pair_index_lhs);
    free(analysis->pair_index_rhs);
    free(analysis->pair_index_item_indices);
    analysis->pair_index_capacity = 0;
    analysis->pair_index_lhs = NULL;
    analysis->pair_index_rhs = NULL;
    analysis->pair_index_item_indices = NULL;

    if (analysis->item_count == 0) {
        return 1;
    }

    capacity = 1;
    while (capacity < analysis->item_count * 2) {
        if (capacity > ((size_t)-1) / 2) {
            break;
        }
        capacity *= 2;
    }
    if (capacity < analysis->item_count) {
        capacity = analysis->item_count;
    }

    analysis->pair_index_lhs = (size_t *)malloc(capacity * sizeof(size_t));
    analysis->pair_index_rhs = (size_t *)malloc(capacity * sizeof(size_t));
    analysis->pair_index_item_indices = (size_t *)malloc(capacity * sizeof(size_t));
    if (!analysis->pair_index_lhs || !analysis->pair_index_rhs || !analysis->pair_index_item_indices) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-231: out of memory preparing coalesce pair index");
        free(analysis->pair_index_lhs);
        free(analysis->pair_index_rhs);
        free(analysis->pair_index_item_indices);
        analysis->pair_index_lhs = NULL;
        analysis->pair_index_rhs = NULL;
        analysis->pair_index_item_indices = NULL;
        return 0;
    }

    for (item_index = 0; item_index < capacity; ++item_index) {
        analysis->pair_index_lhs[item_index] = (size_t)-1;
        analysis->pair_index_rhs[item_index] = (size_t)-1;
        analysis->pair_index_item_indices[item_index] = (size_t)-1;
    }

    for (item_index = 0; item_index < analysis->item_count; ++item_index) {
        size_t lhs = analysis->items[item_index].lhs_value_id;
        size_t rhs = analysis->items[item_index].rhs_value_id;
        size_t slot = value_ssa_alloc_coalesce_pair_hash(lhs, rhs, capacity);

        if (lhs > rhs) {
            size_t temp = lhs;
            lhs = rhs;
            rhs = temp;
        }

        while (analysis->pair_index_item_indices[slot] != (size_t)-1) {
            slot = (slot + 1) % capacity;
        }
        analysis->pair_index_lhs[slot] = lhs;
        analysis->pair_index_rhs[slot] = rhs;
        analysis->pair_index_item_indices[slot] = item_index;
    }

    analysis->pair_index_capacity = capacity;
    return 1;
}

static size_t value_ssa_alloc_coalesce_find_root(size_t *parents, size_t value_id) {
    while (parents[value_id] != value_id) {
        parents[value_id] = parents[parents[value_id]];
        value_id = parents[value_id];
    }
    return value_id;
}

static size_t value_ssa_alloc_coalesce_peek_root(const size_t *parents, size_t value_id) {
    while (parents[value_id] != value_id) {
        value_id = parents[value_id];
    }
    return value_id;
}

static void value_ssa_alloc_coalesce_union_roots(size_t *parents, size_t lhs_root, size_t rhs_root) {
    if (!parents || lhs_root == rhs_root) {
        return;
    }

    if (lhs_root < rhs_root) {
        parents[rhs_root] = lhs_root;
    } else {
        parents[lhs_root] = rhs_root;
    }
}

static int value_ssa_alloc_coalesce_class_interferes(const ValueSsaInterferenceGraph *interference_graph,
    const size_t *parents,
    size_t lhs_root,
    size_t rhs_root) {
    size_t lhs_value_id;

    if (!interference_graph || !parents || lhs_root == rhs_root) {
        return 0;
    }

    for (lhs_value_id = 0; lhs_value_id < interference_graph->value_count; ++lhs_value_id) {
        size_t rhs_value_id;

        if (value_ssa_alloc_coalesce_peek_root(parents, lhs_value_id) != lhs_root) {
            continue;
        }
        for (rhs_value_id = 0; rhs_value_id < interference_graph->value_count; ++rhs_value_id) {
            if (value_ssa_alloc_coalesce_peek_root(parents, rhs_value_id) != rhs_root) {
                continue;
            }
            if (interference_graph
                    ->interferes[lhs_value_id * interference_graph->value_count + rhs_value_id]) {
                return 1;
            }
        }
    }

    return 0;
}

static size_t value_ssa_alloc_coalesce_value_degree(const ValueSsaInterferenceGraph *interference_graph, size_t value_id) {
    size_t neighbor_value_id;
    size_t degree = 0;

    if (!interference_graph || !interference_graph->interferes || value_id >= interference_graph->value_count) {
        return 0;
    }

    for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
        degree += interference_graph->interferes[value_id * interference_graph->value_count + neighbor_value_id] ? 1u : 0u;
    }
    return degree;
}

static int value_ssa_alloc_coalesce_class_is_low_degree(const ValueSsaInterferenceGraph *interference_graph,
    const size_t *parents,
    size_t class_root,
    size_t color_budget) {
    size_t value_id;

    if (!interference_graph || !parents) {
        return 0;
    }

    for (value_id = 0; value_id < interference_graph->value_count; ++value_id) {
        if (value_ssa_alloc_coalesce_peek_root(parents, value_id) != class_root) {
            continue;
        }
        if (value_ssa_alloc_coalesce_value_degree(interference_graph, value_id) >= color_budget) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_alloc_coalesce_class_satisfies_george(const ValueSsaInterferenceGraph *interference_graph,
    const size_t *parents,
    size_t test_root,
    size_t anchor_root,
    size_t color_budget) {
    size_t *seen_neighbor_roots = NULL;
    size_t seen_count = 0;
    size_t value_id;
    int ok = 0;

    if (!interference_graph || !parents || test_root == anchor_root) {
        return 0;
    }

    seen_neighbor_roots = (size_t *)malloc((interference_graph->value_count ? interference_graph->value_count : 1) *
        sizeof(size_t));
    if (!seen_neighbor_roots) {
        return 0;
    }

    for (value_id = 0; value_id < interference_graph->value_count; ++value_id) {
        size_t neighbor_root;
        size_t seen_index;
        int already_seen = 0;

        if (value_ssa_alloc_coalesce_peek_root(parents, value_id) == test_root ||
            value_ssa_alloc_coalesce_peek_root(parents, value_id) == anchor_root) {
            continue;
        }
        if (!value_ssa_alloc_coalesce_class_interferes(interference_graph, parents, test_root,
                value_ssa_alloc_coalesce_peek_root(parents, value_id))) {
            continue;
        }

        neighbor_root = value_ssa_alloc_coalesce_peek_root(parents, value_id);
        for (seen_index = 0; seen_index < seen_count; ++seen_index) {
            if (seen_neighbor_roots[seen_index] == neighbor_root) {
                already_seen = 1;
                break;
            }
        }
        if (already_seen) {
            continue;
        }
        seen_neighbor_roots[seen_count++] = neighbor_root;

        if (value_ssa_alloc_coalesce_class_interferes(interference_graph, parents, anchor_root, neighbor_root)) {
            continue;
        }
        if (!value_ssa_alloc_coalesce_class_is_low_degree(interference_graph, parents, neighbor_root, color_budget)) {
            goto cleanup;
        }
    }

    ok = 1;

cleanup:
    free(seen_neighbor_roots);
    return ok;
}

static size_t value_ssa_alloc_coalesce_count_merged_neighbor_classes(
    const ValueSsaInterferenceGraph *interference_graph,
    const size_t *parents,
    size_t lhs_root,
    size_t rhs_root) {
    size_t neighbor_count = 0;
    size_t *seen_neighbor_roots = NULL;
    size_t seen_count = 0;
    size_t value_id;

    if (!interference_graph || !parents) {
        return 0;
    }

    seen_neighbor_roots = (size_t *)malloc((interference_graph->value_count ? interference_graph->value_count : 1) *
        sizeof(size_t));
    if (!seen_neighbor_roots) {
        return (size_t)-1;
    }

    for (value_id = 0; value_id < interference_graph->value_count; ++value_id) {
        size_t value_root = value_ssa_alloc_coalesce_peek_root(parents, value_id);
        size_t neighbor_value_id;
        int touches_class = 0;
        size_t seen_index;
        int already_seen = 0;

        if (value_root == lhs_root || value_root == rhs_root) {
            continue;
        }

        for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
            size_t neighbor_root = value_ssa_alloc_coalesce_peek_root(parents, neighbor_value_id);

            if (neighbor_root != lhs_root && neighbor_root != rhs_root) {
                continue;
            }
            if (!interference_graph
                     ->interferes[value_id * interference_graph->value_count + neighbor_value_id]) {
                continue;
            }
            touches_class = 1;
            break;
        }
        if (!touches_class) {
            continue;
        }

        for (seen_index = 0; seen_index < seen_count; ++seen_index) {
            if (seen_neighbor_roots[seen_index] == value_root) {
                already_seen = 1;
                break;
            }
        }
        if (already_seen) {
            continue;
        }
        seen_neighbor_roots[seen_count++] = value_root;
        ++neighbor_count;
    }

    free(seen_neighbor_roots);
    return neighbor_count;
}

int value_ssa_compute_allocator_coalesce_analysis(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    size_t color_budget,
    ValueSsaAllocatorCoalesceAnalysis *analysis,
    ValueSsaError *error) {
    ValueSsaAllocationPrep owned_prep;
    ValueSsaInterferenceGraph owned_interference;
    ValueSsaCopyAffinityGraph owned_affinity;
    const ValueSsaAllocationPrep *active_prep = prep;
    const ValueSsaInterferenceGraph *active_interference = interference_graph;
    const ValueSsaCopyAffinityGraph *active_affinity = affinity_graph;
    size_t *parents = NULL;
    int ok = 0;
    size_t item_index;

    if (!function || !analysis) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-090: invalid allocator-coalesce contract");
        return 0;
    }

    value_ssa_allocation_prep_init(&owned_prep);
    value_ssa_interference_graph_init(&owned_interference);
    value_ssa_copy_affinity_graph_init(&owned_affinity);
    value_ssa_allocator_coalesce_analysis_free(analysis);
    analysis->value_count = function->next_value_id;
    if (function->next_value_id == 0) {
        return 1;
    }

    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    }
    if (!active_interference) {
        if (!value_ssa_compute_interference_graph(function, NULL, NULL, &owned_interference, error)) {
            goto cleanup;
        }
        active_interference = &owned_interference;
    }
    if (!active_affinity) {
        if (!value_ssa_compute_copy_affinity_graph(function, active_interference, &owned_affinity, error)) {
            goto cleanup;
        }
        active_affinity = &owned_affinity;
    }

    analysis->item_count = active_prep->candidate_count;
    analysis->items = (ValueSsaAllocatorCoalesceItem *)calloc(
        analysis->item_count ? analysis->item_count : 1, sizeof(ValueSsaAllocatorCoalesceItem));
    analysis->value_roots = (size_t *)malloc((analysis->value_count ? analysis->value_count : 1) * sizeof(size_t));
    analysis->class_sizes = (size_t *)calloc((analysis->value_count ? analysis->value_count : 1), sizeof(size_t));
    parents = (size_t *)malloc((analysis->value_count ? analysis->value_count : 1) * sizeof(size_t));
    if (!analysis->items || !analysis->value_roots || !analysis->class_sizes || !parents) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-091: out of memory preparing coalesce analysis");
        goto cleanup;
    }

    for (item_index = 0; item_index < analysis->value_count; ++item_index) {
        parents[item_index] = item_index;
    }

    for (item_index = 0; item_index < analysis->item_count; ++item_index) {
        const ValueSsaCopyAffinityCandidate *candidate = &active_prep->candidates[item_index];
        ValueSsaAllocatorCoalesceItem *item = &analysis->items[item_index];
        size_t lhs_root;
        size_t rhs_root;

        item->lhs_value_id = candidate->lhs;
        item->rhs_value_id = candidate->rhs;
        item->weight = candidate->weight;

        lhs_root = value_ssa_alloc_coalesce_find_root(parents, candidate->lhs);
        rhs_root = value_ssa_alloc_coalesce_find_root(parents, candidate->rhs);
        if (lhs_root == rhs_root) {
            item->merged_neighbor_count = 0;
            item->interferes = 0;
            item->can_coalesce = 1;
            continue;
        }

        item->interferes = value_ssa_alloc_coalesce_class_interferes(
            active_interference, parents, lhs_root, rhs_root) ? 1u : 0u;
        if (!item->interferes) {
            size_t merged_neighbor_count = value_ssa_alloc_coalesce_count_merged_neighbor_classes(
                active_interference, parents, lhs_root, rhs_root);

            if (merged_neighbor_count == (size_t)-1) {
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-092: out of memory counting coalesce neighbors");
                goto cleanup;
            }
            item->merged_neighbor_count = merged_neighbor_count;
            if (merged_neighbor_count < color_budget ||
                value_ssa_alloc_coalesce_class_satisfies_george(
                    active_interference, parents, lhs_root, rhs_root, color_budget) ||
                value_ssa_alloc_coalesce_class_satisfies_george(
                    active_interference, parents, rhs_root, lhs_root, color_budget)) {
                item->can_coalesce = 1;
                value_ssa_alloc_coalesce_union_roots(parents, lhs_root, rhs_root);
            }
        }
    }

    for (item_index = 0; item_index < analysis->value_count; ++item_index) {
        size_t root = value_ssa_alloc_coalesce_find_root(parents, item_index);
        analysis->value_roots[item_index] = root;
        ++analysis->class_sizes[root];
    }

    if (!value_ssa_alloc_coalesce_build_pair_index(analysis, error)) {
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(parents);
    value_ssa_copy_affinity_graph_free(&owned_affinity);
    value_ssa_interference_graph_free(&owned_interference);
    value_ssa_allocation_prep_free(&owned_prep);
    if (!ok) {
        value_ssa_allocator_coalesce_analysis_free(analysis);
    }
    return ok;
}

int value_ssa_allocator_coalesce_analysis_find_pair(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t lhs_value_id,
    size_t rhs_value_id,
    size_t *out_index,
    const ValueSsaAllocatorCoalesceItem **out_item) {
    size_t lhs = lhs_value_id;
    size_t rhs = rhs_value_id;
    size_t slot;
    size_t start_slot;
    size_t item_index;

    if (!analysis) {
        return 0;
    }

    if (lhs > rhs) {
        size_t temp = lhs;
        lhs = rhs;
        rhs = temp;
    }
    if (!analysis->pair_index_capacity || !analysis->pair_index_item_indices ||
        !analysis->pair_index_lhs || !analysis->pair_index_rhs) {
        if (!analysis->items) {
            return 0;
        }
        for (item_index = 0; item_index < analysis->item_count; ++item_index) {
            size_t item_lhs = analysis->items[item_index].lhs_value_id;
            size_t item_rhs = analysis->items[item_index].rhs_value_id;

            if (item_lhs > item_rhs) {
                size_t temp = item_lhs;
                item_lhs = item_rhs;
                item_rhs = temp;
            }
            if (item_lhs != lhs || item_rhs != rhs) {
                continue;
            }
            if (out_index) {
                *out_index = item_index;
            }
            if (out_item) {
                *out_item = &analysis->items[item_index];
            }
            return 1;
        }
        return 0;
    }

    slot = value_ssa_alloc_coalesce_pair_hash(lhs, rhs, analysis->pair_index_capacity);
    start_slot = slot;
    while (analysis->pair_index_item_indices[slot] != (size_t)-1) {
        if (analysis->pair_index_lhs[slot] == lhs && analysis->pair_index_rhs[slot] == rhs) {
            size_t item_index = analysis->pair_index_item_indices[slot];
            if (out_index) {
                *out_index = item_index;
            }
            if (out_item) {
                *out_item = &analysis->items[item_index];
            }
            return 1;
        }
        slot = (slot + 1) % analysis->pair_index_capacity;
        if (slot == start_slot) {
            break;
        }
    }

    return 0;
}

int value_ssa_allocator_coalesce_analysis_get_class(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_id,
    size_t *out_root_value_id,
    size_t *out_class_size) {
    if (!analysis || !out_root_value_id || !out_class_size || value_id >= analysis->value_count ||
        !analysis->value_roots || !analysis->class_sizes) {
        return 0;
    }

    *out_root_value_id = analysis->value_roots[value_id];
    *out_class_size = analysis->class_sizes[*out_root_value_id];
    return 1;
}

int value_ssa_dump_allocator_coalesce_analysis(const ValueSsaFunction *function,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t color_budget,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t item_index;

    if (!function || !analysis || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-093: invalid allocator-coalesce dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "coalesce %s colors=%zu candidates=%zu\n",
            function->name ? function->name : "<anon>",
            color_budget,
            analysis->item_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-094: out of memory building coalesce dump");
        return 0;
    }

    for (item_index = 0; item_index < analysis->item_count; ++item_index) {
        const ValueSsaAllocatorCoalesceItem *item = &analysis->items[item_index];

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: ssa.%zu <-> ssa.%zu weight=%zu merged_neighbors=%zu interferes=%s can_coalesce=%s\n",
                item_index,
                item->lhs_value_id,
                item->rhs_value_id,
                item->weight,
                item->merged_neighbor_count,
                item->interferes ? "yes" : "no",
                item->can_coalesce ? "yes" : "no")) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-095: out of memory appending coalesce item");
            return 0;
        }
    }

    if (analysis->value_roots && analysis->class_sizes) {
        for (item_index = 0; item_index < analysis->value_count; ++item_index) {
            if (!value_ssa_alloc_append_format(&builder,
                    "  class ssa.%zu -> root=ssa.%zu size=%zu\n",
                    item_index,
                    analysis->value_roots[item_index],
                    analysis->class_sizes[analysis->value_roots[item_index]])) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-096: out of memory appending coalesce class");
                return 0;
            }
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
