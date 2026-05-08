#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocator_plan_init(ValueSsaAllocatorPlan *plan) {
    if (!plan) {
        return;
    }

    plan->value_count = 0;
    plan->item_count = 0;
    plan->items = NULL;
    plan->final_runtime_coalesce_roots = NULL;
    plan->final_runtime_coalesce_class_sizes = NULL;
    plan->value_item_indices = NULL;
}

void value_ssa_allocator_plan_free(ValueSsaAllocatorPlan *plan) {
    if (!plan) {
        return;
    }

    free(plan->items);
    free(plan->final_runtime_coalesce_roots);
    free(plan->final_runtime_coalesce_class_sizes);
    free(plan->value_item_indices);
    value_ssa_allocator_plan_init(plan);
}

const char *value_ssa_allocator_phase_name(ValueSsaAllocatorPhase phase) {
    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        return "freeze";
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
        return "spill";
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        return "simplify";
    default:
        return "unknown";
    }
}

const char *value_ssa_allocator_removal_kind_name(ValueSsaAllocatorRemovalKind removal_kind) {
    switch (removal_kind) {
    case VALUE_SSA_ALLOCATOR_REMOVE_NONE:
        return "none";
    case VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY:
        return "simplify-remove";
    case VALUE_SSA_ALLOCATOR_REMOVE_SPILL:
        return "spill-remove";
    default:
        return "unknown";
    }
}

static int value_ssa_alloc_phase_selection_rank(ValueSsaAllocatorPhase phase) {
    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        return 0;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        return 1;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
        return 2;
    default:
        return 3;
    }
}

static int value_ssa_alloc_simplify_move_work_rank(ValueSsaAllocatorMoveWorkClass move_work_class) {
    switch (move_work_class) {
    case VALUE_SSA_ALLOC_MOVE_WORK_RELEASED:
        return 0;
    case VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING:
        return 1;
    case VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL:
        return 2;
    case VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY:
    default:
        return 3;
    }
}

static int value_ssa_alloc_spill_move_work_rank(ValueSsaAllocatorMoveWorkClass move_work_class) {
    switch (move_work_class) {
    case VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL:
        return 0;
    case VALUE_SSA_ALLOC_MOVE_WORK_RELEASED:
        return 1;
    case VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING:
        return 2;
    case VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY:
    default:
        return 3;
    }
}

static void value_ssa_alloc_recompute_active_degrees(const ValueSsaInterferenceGraph *interference_graph,
    const unsigned char *scheduled,
    size_t *out_active_degrees) {
    size_t value_id;

    if (!interference_graph || !scheduled || !out_active_degrees) {
        return;
    }

    for (value_id = 0; value_id < interference_graph->value_count; ++value_id) {
        size_t neighbor_value_id;
        size_t degree = 0;

        if (scheduled[value_id]) {
            out_active_degrees[value_id] = 0;
            continue;
        }

        for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
            if (neighbor_value_id == value_id || scheduled[neighbor_value_id]) {
                continue;
            }
            if (interference_graph->interferes[value_id * interference_graph->value_count + neighbor_value_id]) {
                ++degree;
            }
        }
        out_active_degrees[value_id] = degree;
    }
}

static void value_ssa_alloc_recompute_active_move_related(const ValueSsaCopyAffinityGraph *affinity_graph,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    unsigned char *out_active_move_related) {
    size_t value_id;

    if (!affinity_graph || !scheduled || !frozen || !out_active_move_related) {
        return;
    }

    for (value_id = 0; value_id < affinity_graph->value_count; ++value_id) {
        size_t neighbor_value_id;
        int has_active_affinity = 0;

        if (scheduled[value_id] || frozen[value_id]) {
            out_active_move_related[value_id] = 0;
            continue;
        }

        for (neighbor_value_id = 0; neighbor_value_id < affinity_graph->value_count; ++neighbor_value_id) {
            if (neighbor_value_id == value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                continue;
            }
            if (affinity_graph->weights[value_id * affinity_graph->value_count + neighbor_value_id] == 0) {
                continue;
            }
            has_active_affinity = 1;
            break;
        }
        out_active_move_related[value_id] = has_active_affinity ? 1u : 0u;
    }
}

static int value_ssa_alloc_coalesce_analysis_is_usable(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_count) {
    return analysis && analysis->value_count == value_count && analysis->value_roots && analysis->class_sizes;
}

static int value_ssa_alloc_coalesce_analysis_has_nontrivial_classes(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_count) {
    size_t value_id;

    if (!value_ssa_alloc_coalesce_analysis_is_usable(analysis, value_count)) {
        return 0;
    }

    for (value_id = 0; value_id < value_count; ++value_id) {
        if (analysis->value_roots[value_id] != value_id) {
            return 1;
        }
        if (analysis->class_sizes[value_id] > 1) {
            return 1;
        }
    }
    return 0;
}

static size_t value_ssa_alloc_plan_effective_root(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_count,
    size_t value_id) {
    if (!value_ssa_alloc_coalesce_analysis_is_usable(analysis, value_count) || value_id >= value_count) {
        return value_id;
    }
    return analysis->value_roots[value_id];
}

static int value_ssa_alloc_plan_has_final_runtime_coalesce(const ValueSsaAllocatorPlan *plan, size_t value_count) {
    return plan && plan->value_count == value_count && plan->final_runtime_coalesce_roots &&
        plan->final_runtime_coalesce_class_sizes;
}

static size_t value_ssa_alloc_plan_runtime_coalesce_root(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_count,
    size_t value_id) {
    if (value_ssa_alloc_plan_has_final_runtime_coalesce(plan, value_count) && value_id < value_count) {
        return plan->final_runtime_coalesce_roots[value_id];
    }
    if (value_ssa_alloc_coalesce_analysis_is_usable(analysis, value_count) && value_id < value_count) {
        return analysis->value_roots[value_id];
    }
    if (plan && plan->value_count == value_count && value_id < value_count) {
        return value_ssa_alloc_plan_family_root_for_value(plan, value_id);
    }
    return value_ssa_alloc_plan_effective_root(analysis, value_count, value_id);
}

static size_t value_ssa_alloc_plan_runtime_coalesce_class_size(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_count,
    size_t value_id) {
    size_t root_value_id = value_ssa_alloc_plan_runtime_coalesce_root(plan, analysis, value_count, value_id);

    if (root_value_id >= value_count) {
        return 1;
    }
    if (value_ssa_alloc_plan_has_final_runtime_coalesce(plan, value_count)) {
        size_t class_size = plan->final_runtime_coalesce_class_sizes[root_value_id];

        return class_size > 0 ? class_size : 1;
    }
    if (value_ssa_alloc_coalesce_analysis_is_usable(analysis, value_count)) {
        size_t class_size = analysis->class_sizes[root_value_id];

        return class_size > 0 ? class_size : 1;
    }
    if (plan && plan->value_count == value_count) {
        const ValueSsaAllocatorPlanItem *item = NULL;

        if (value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) && item && item->coalesce_class_size > 0) {
            return item->coalesce_class_size;
        }
    }
    return 1;
}

static void value_ssa_alloc_recompute_active_effective_degrees(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const size_t *active_degrees,
    size_t *scratch_roots,
    unsigned char *scratch_root_flags,
    size_t *out_active_effective_degrees) {
    size_t value_id;
    int use_coalesce;

    if (!interference_graph || !scheduled || !active_degrees || !out_active_effective_degrees) {
        return;
    }

    use_coalesce = value_ssa_alloc_coalesce_analysis_is_usable(coalesce_analysis, interference_graph->value_count);
    if (!use_coalesce) {
        for (value_id = 0; value_id < interference_graph->value_count; ++value_id) {
            out_active_effective_degrees[value_id] = active_degrees[value_id];
        }
        return;
    }

    for (value_id = 0; value_id < interference_graph->value_count; ++value_id) {
        size_t neighbor_value_id;
        size_t seen_count = 0;
        size_t effective_degree = 0;
        size_t root_value_id;

        if (scheduled[value_id]) {
            out_active_effective_degrees[value_id] = 0;
            continue;
        }

        root_value_id = value_ssa_alloc_plan_effective_root(
            coalesce_analysis, interference_graph->value_count, value_id);

        for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
            size_t neighbor_root_value_id;

            if (neighbor_value_id == value_id || scheduled[neighbor_value_id]) {
                continue;
            }
            if (!interference_graph
                     ->interferes[value_id * interference_graph->value_count + neighbor_value_id]) {
                continue;
            }

            neighbor_root_value_id = value_ssa_alloc_plan_effective_root(
                coalesce_analysis, interference_graph->value_count, neighbor_value_id);
            if (neighbor_root_value_id == root_value_id) {
                continue;
            }
            if (scratch_root_flags) {
                if (scratch_root_flags[neighbor_root_value_id]) {
                    continue;
                }
                scratch_root_flags[neighbor_root_value_id] = 1u;
            } else {
                size_t seen_index;
                int already_seen = 0;

                for (seen_index = 0; seen_index < seen_count; ++seen_index) {
                    if (scratch_roots[seen_index] == neighbor_root_value_id) {
                        already_seen = 1;
                        break;
                    }
                }
                if (already_seen) {
                    continue;
                }
            }

            scratch_roots[seen_count++] = neighbor_root_value_id;
            ++effective_degree;
        }

        if (scratch_root_flags) {
            size_t seen_index;

            for (seen_index = 0; seen_index < seen_count; ++seen_index) {
                scratch_root_flags[scratch_roots[seen_index]] = 0u;
            }
        }

        out_active_effective_degrees[value_id] = effective_degree;
    }
}

static void value_ssa_alloc_recompute_active_effective_move_related(const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    const unsigned char *active_move_related,
    unsigned char *scratch_class_move_related,
    unsigned char *out_active_effective_move_related) {
    size_t value_id;
    int use_coalesce;

    if (!affinity_graph || !scheduled || !frozen || !active_move_related || !out_active_effective_move_related) {
        return;
    }

    use_coalesce = value_ssa_alloc_coalesce_analysis_is_usable(coalesce_analysis, affinity_graph->value_count);
    if (!use_coalesce) {
        for (value_id = 0; value_id < affinity_graph->value_count; ++value_id) {
            out_active_effective_move_related[value_id] = active_move_related[value_id];
        }
        return;
    }

    memset(scratch_class_move_related, 0, (affinity_graph->value_count ? affinity_graph->value_count : 1));
    for (value_id = 0; value_id < affinity_graph->value_count; ++value_id) {
        size_t neighbor_value_id;
        size_t root_value_id;

        if (scheduled[value_id] || frozen[value_id]) {
            continue;
        }

        root_value_id = value_ssa_alloc_plan_effective_root(
            coalesce_analysis, affinity_graph->value_count, value_id);

        for (neighbor_value_id = 0; neighbor_value_id < affinity_graph->value_count; ++neighbor_value_id) {
            if (neighbor_value_id == value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                continue;
            }
            if (affinity_graph->weights[value_id * affinity_graph->value_count + neighbor_value_id] == 0) {
                continue;
            }
            if (value_ssa_alloc_plan_effective_root(
                    coalesce_analysis, affinity_graph->value_count, neighbor_value_id) == root_value_id) {
                continue;
            }
            scratch_class_move_related[root_value_id] = 1u;
            break;
        }
    }

    for (value_id = 0; value_id < affinity_graph->value_count; ++value_id) {
        size_t root_value_id;

        if (scheduled[value_id] || frozen[value_id]) {
            out_active_effective_move_related[value_id] = 0;
            continue;
        }

        root_value_id = value_ssa_alloc_plan_effective_root(
            coalesce_analysis, affinity_graph->value_count, value_id);
        out_active_effective_move_related[value_id] = scratch_class_move_related[root_value_id];
    }
}

static void value_ssa_alloc_mark_frozen_family(const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t value_id,
    const unsigned char *scheduled,
    unsigned char *frozen,
    unsigned char *was_frozen) {
    size_t family_value_id;
    size_t root_value_id;

    if (!frozen || !was_frozen || !scheduled || value_id >= value_count) {
        return;
    }

    root_value_id = value_ssa_alloc_plan_effective_root(coalesce_analysis, value_count, value_id);
    for (family_value_id = 0; family_value_id < value_count; ++family_value_id) {
        if (scheduled[family_value_id]) {
            continue;
        }
        if (value_ssa_alloc_plan_effective_root(coalesce_analysis, value_count, family_value_id) != root_value_id) {
            continue;
        }
        frozen[family_value_id] = 1;
        was_frozen[family_value_id] = 1;
    }
}

static void value_ssa_alloc_recompute_active_family_move_pressure(const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    unsigned char *neighbor_matrix,
    unsigned char *coalesce_ready_neighbor_matrix,
    size_t *coalesce_ready_affinity_matrix,
    size_t *out_external_neighbor_counts,
    size_t *out_coalesce_ready_neighbor_counts,
    size_t *out_coalesce_ready_affinity_sums,
    size_t *out_best_coalesce_ready_partner_roots,
    size_t *out_best_coalesce_ready_partner_weights,
    unsigned char *out_best_coalesce_ready_partner_mutual_flags,
    size_t *out_external_affinity_sums) {
    size_t value_id;

    if (!affinity_graph || !coalesce_analysis || !scheduled || !frozen || !out_external_neighbor_counts ||
        !out_coalesce_ready_neighbor_counts || !out_coalesce_ready_affinity_sums ||
        !out_best_coalesce_ready_partner_roots || !out_best_coalesce_ready_partner_weights ||
        !out_best_coalesce_ready_partner_mutual_flags ||
        !out_external_affinity_sums) {
        return;
    }

    memset(out_external_neighbor_counts, 0, (affinity_graph->value_count ? affinity_graph->value_count : 1) * sizeof(size_t));
    memset(out_coalesce_ready_neighbor_counts,
        0,
        (affinity_graph->value_count ? affinity_graph->value_count : 1) * sizeof(size_t));
    memset(out_coalesce_ready_affinity_sums,
        0,
        (affinity_graph->value_count ? affinity_graph->value_count : 1) * sizeof(size_t));
    for (value_id = 0; value_id < (affinity_graph->value_count ? affinity_graph->value_count : 1); ++value_id) {
        out_best_coalesce_ready_partner_roots[value_id] = (size_t)-1;
        out_best_coalesce_ready_partner_weights[value_id] = 0;
        out_best_coalesce_ready_partner_mutual_flags[value_id] = 0;
    }
    memset(out_external_affinity_sums, 0, (affinity_graph->value_count ? affinity_graph->value_count : 1) * sizeof(size_t));
    if (neighbor_matrix) {
        memset(neighbor_matrix,
            0,
            (affinity_graph->value_count ? (affinity_graph->value_count * affinity_graph->value_count) : 1) *
                sizeof(unsigned char));
    }
    if (coalesce_ready_neighbor_matrix) {
        memset(coalesce_ready_neighbor_matrix,
            0,
            (affinity_graph->value_count ? (affinity_graph->value_count * affinity_graph->value_count) : 1) *
                sizeof(unsigned char));
    }
    if (coalesce_ready_affinity_matrix) {
        memset(coalesce_ready_affinity_matrix,
            0,
            (affinity_graph->value_count ? (affinity_graph->value_count * affinity_graph->value_count) : 1) *
                sizeof(size_t));
    }

    for (value_id = 0; value_id < affinity_graph->value_count; ++value_id) {
        size_t neighbor_value_id;
        size_t root_value_id;

        if (scheduled[value_id] || frozen[value_id]) {
            continue;
        }

        root_value_id = value_ssa_alloc_plan_effective_root(
            coalesce_analysis, affinity_graph->value_count, value_id);
        for (neighbor_value_id = value_id + 1; neighbor_value_id < affinity_graph->value_count; ++neighbor_value_id) {
            size_t neighbor_root_value_id;
            size_t weight;
            const ValueSsaAllocatorCoalesceItem *coalesce_item = NULL;

            if (scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                continue;
            }
            weight = affinity_graph->weights[value_id * affinity_graph->value_count + neighbor_value_id];
            if (weight == 0) {
                continue;
            }
            neighbor_root_value_id = value_ssa_alloc_plan_effective_root(
                coalesce_analysis, affinity_graph->value_count, neighbor_value_id);
            if (neighbor_root_value_id == root_value_id) {
                continue;
            }
            out_external_affinity_sums[root_value_id] += weight;
            out_external_affinity_sums[neighbor_root_value_id] += weight;
            if (neighbor_matrix &&
                !neighbor_matrix[root_value_id * affinity_graph->value_count + neighbor_root_value_id]) {
                neighbor_matrix[root_value_id * affinity_graph->value_count + neighbor_root_value_id] = 1;
                neighbor_matrix[neighbor_root_value_id * affinity_graph->value_count + root_value_id] = 1;
                ++out_external_neighbor_counts[root_value_id];
                ++out_external_neighbor_counts[neighbor_root_value_id];
            }
            if (value_ssa_allocator_coalesce_analysis_find_pair(
                    coalesce_analysis, value_id, neighbor_value_id, NULL, &coalesce_item) &&
                coalesce_item && coalesce_item->can_coalesce) {
                size_t ready_weight = weight;

                out_coalesce_ready_affinity_sums[root_value_id] += weight;
                out_coalesce_ready_affinity_sums[neighbor_root_value_id] += weight;
                if (coalesce_ready_affinity_matrix) {
                    coalesce_ready_affinity_matrix[root_value_id * affinity_graph->value_count + neighbor_root_value_id] +=
                        weight;
                    coalesce_ready_affinity_matrix[neighbor_root_value_id * affinity_graph->value_count + root_value_id] +=
                        weight;
                    ready_weight =
                        coalesce_ready_affinity_matrix[root_value_id * affinity_graph->value_count + neighbor_root_value_id];
                }
                if (coalesce_ready_neighbor_matrix &&
                    !coalesce_ready_neighbor_matrix[root_value_id * affinity_graph->value_count + neighbor_root_value_id]) {
                    coalesce_ready_neighbor_matrix[root_value_id * affinity_graph->value_count + neighbor_root_value_id] =
                        1;
                    coalesce_ready_neighbor_matrix[neighbor_root_value_id * affinity_graph->value_count + root_value_id] =
                        1;
                    ++out_coalesce_ready_neighbor_counts[root_value_id];
                    ++out_coalesce_ready_neighbor_counts[neighbor_root_value_id];
                }
                if (ready_weight > out_best_coalesce_ready_partner_weights[root_value_id] ||
                    (ready_weight == out_best_coalesce_ready_partner_weights[root_value_id] &&
                        (out_best_coalesce_ready_partner_roots[root_value_id] == (size_t)-1 ||
                            neighbor_root_value_id < out_best_coalesce_ready_partner_roots[root_value_id]))) {
                    out_best_coalesce_ready_partner_roots[root_value_id] = neighbor_root_value_id;
                    out_best_coalesce_ready_partner_weights[root_value_id] = ready_weight;
                }
                if (ready_weight > out_best_coalesce_ready_partner_weights[neighbor_root_value_id] ||
                    (ready_weight == out_best_coalesce_ready_partner_weights[neighbor_root_value_id] &&
                        (out_best_coalesce_ready_partner_roots[neighbor_root_value_id] == (size_t)-1 ||
                            root_value_id < out_best_coalesce_ready_partner_roots[neighbor_root_value_id]))) {
                    out_best_coalesce_ready_partner_roots[neighbor_root_value_id] = root_value_id;
                    out_best_coalesce_ready_partner_weights[neighbor_root_value_id] = ready_weight;
                }
            }
        }
    }

    for (value_id = 0; value_id < affinity_graph->value_count; ++value_id) {
        size_t partner_root_value_id = out_best_coalesce_ready_partner_roots[value_id];

        if (partner_root_value_id >= affinity_graph->value_count) {
            continue;
        }
        if (out_best_coalesce_ready_partner_weights[value_id] == 0) {
            continue;
        }
        if (out_best_coalesce_ready_partner_roots[partner_root_value_id] != value_id) {
            continue;
        }
        if (out_best_coalesce_ready_partner_weights[partner_root_value_id] == 0) {
            continue;
        }
        out_best_coalesce_ready_partner_mutual_flags[value_id] = 1u;
    }
}

static ValueSsaAllocatorMoveWorkClass value_ssa_alloc_classify_plan_move_work(
    int initial_effective_move_related,
    int was_frozen,
    size_t active_family_member_count,
    size_t coalesce_ready_neighbor_count) {
    if (active_family_member_count == 0) {
        return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
    }
    if (was_frozen) {
        return VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING;
    }
    if (initial_effective_move_related) {
        if (coalesce_ready_neighbor_count > 0) {
            return VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
        }
        return VALUE_SSA_ALLOC_MOVE_WORK_RELEASED;
    }
    return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
}

static void value_ssa_alloc_recompute_active_family_move_work_classes(
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    const unsigned char *initial_family_move_related,
    const unsigned char *was_frozen,
    const size_t *active_family_member_counts,
    const size_t *active_family_coalesce_ready_neighbor_counts,
    ValueSsaAllocatorMoveWorkClass *out_active_family_move_work_classes) {
    size_t value_id;

    if (!active_family_member_counts || !active_family_coalesce_ready_neighbor_counts ||
        !out_active_family_move_work_classes) {
        return;
    }

    for (value_id = 0; value_id < value_count; ++value_id) {
        size_t root_value_id = value_ssa_alloc_plan_effective_root(coalesce_analysis, value_count, value_id);

        out_active_family_move_work_classes[value_id] = value_ssa_alloc_classify_plan_move_work(
            initial_family_move_related ? (int)initial_family_move_related[root_value_id] : 0,
            was_frozen ? (int)was_frozen[root_value_id] : 0,
            active_family_member_counts[root_value_id],
            active_family_coalesce_ready_neighbor_counts[root_value_id]);
    }
}

static size_t value_ssa_alloc_compute_plan_move_work_priority(size_t base_priority,
    size_t class_size,
    size_t coalesce_ready_neighbor_count,
    size_t coalesce_ready_affinity_weight_sum,
    size_t best_coalesce_ready_partner_affinity_weight,
    int best_coalesce_ready_partner_is_mutual,
    size_t external_neighbor_count,
    size_t external_affinity_weight_sum) {
    return base_priority + (coalesce_ready_neighbor_count * 16) + (coalesce_ready_affinity_weight_sum * 8) +
        (best_coalesce_ready_partner_affinity_weight * 4) + (best_coalesce_ready_partner_is_mutual ? 8u : 0u) +
        (external_affinity_weight_sum * 8) + (external_neighbor_count * 4) + (class_size * 2);
}

static ValueSsaAllocatorPhase value_ssa_alloc_classify_phase(const ValueSsaAllocationPrep *prep,
    int active_move_related,
    size_t active_degree,
    size_t color_budget) {
    if (!prep) {
        return VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;
    }

    if (active_move_related && active_degree < color_budget) {
        return VALUE_SSA_ALLOCATOR_PHASE_FREEZE;
    }
    if (active_degree >= color_budget) {
        return VALUE_SSA_ALLOCATOR_PHASE_SPILL;
    }
    return VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;
}

static int value_ssa_alloc_plan_item_is_better(const ValueSsaAllocatorPlanItem *lhs,
    const ValueSsaAllocatorPlanItem *rhs) {
    if (!lhs || !rhs) {
        return lhs != NULL;
    }
    if (value_ssa_alloc_phase_selection_rank(lhs->phase) < value_ssa_alloc_phase_selection_rank(rhs->phase)) {
        return 1;
    }
    if (value_ssa_alloc_phase_selection_rank(lhs->phase) > value_ssa_alloc_phase_selection_rank(rhs->phase)) {
        return 0;
    }
    if (lhs->phase == VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY) {
        if (value_ssa_alloc_simplify_move_work_rank(lhs->move_work_class) <
            value_ssa_alloc_simplify_move_work_rank(rhs->move_work_class)) {
            return 1;
        }
        if (value_ssa_alloc_simplify_move_work_rank(lhs->move_work_class) >
            value_ssa_alloc_simplify_move_work_rank(rhs->move_work_class)) {
            return 0;
        }
        if (lhs->move_work_priority > rhs->move_work_priority) {
            return 1;
        }
        if (lhs->move_work_priority < rhs->move_work_priority) {
            return 0;
        }
    }
    if ((int)lhs->work_class < (int)rhs->work_class) {
        return 1;
    }
    if ((int)lhs->work_class > (int)rhs->work_class) {
        return 0;
    }
    if (lhs->phase == VALUE_SSA_ALLOCATOR_PHASE_SPILL) {
        if (lhs->spill_cost < rhs->spill_cost) {
            return 1;
        }
        if (lhs->spill_cost > rhs->spill_cost) {
            return 0;
        }
        if ((int)lhs->rematerializable > (int)rhs->rematerializable) {
            return 1;
        }
        if ((int)lhs->rematerializable < (int)rhs->rematerializable) {
            return 0;
        }
        if ((int)lhs->split_child_value > (int)rhs->split_child_value) {
            return 1;
        }
        if ((int)lhs->split_child_value < (int)rhs->split_child_value) {
            return 0;
        }
        if (lhs->split_child_depth > rhs->split_child_depth) {
            return 1;
        }
        if (lhs->split_child_depth < rhs->split_child_depth) {
            return 0;
        }
        {
            int family_pressure_compare = value_ssa_alloc_compare_family_pressure_items(lhs, rhs);

            if (lhs->family_spill_removed_count < rhs->family_spill_removed_count) {
                return 1;
            }
            if (lhs->family_spill_removed_count > rhs->family_spill_removed_count) {
                return 0;
            }
            if (family_pressure_compare < 0) {
                return 1;
            }
            if (family_pressure_compare > 0) {
                return 0;
            }
        }
        if (lhs->coalesce_class_size < rhs->coalesce_class_size) {
            return 1;
        }
        if (lhs->coalesce_class_size > rhs->coalesce_class_size) {
            return 0;
        }
        if (lhs->active_effective_degree > rhs->active_effective_degree) {
            return 1;
        }
        if (lhs->active_effective_degree < rhs->active_effective_degree) {
            return 0;
        }
    }
    if (lhs->phase == VALUE_SSA_ALLOCATOR_PHASE_FREEZE) {
        if (lhs->active_effective_degree < rhs->active_effective_degree) {
            return 1;
        }
        if (lhs->active_effective_degree > rhs->active_effective_degree) {
            return 0;
        }
        if (lhs->move_work_priority < rhs->move_work_priority) {
            return 1;
        }
        if (lhs->move_work_priority > rhs->move_work_priority) {
            return 0;
        }
    }
    if (lhs->priority > rhs->priority) {
        return 1;
    }
    if (lhs->priority < rhs->priority) {
        return 0;
    }
    return lhs->value_id < rhs->value_id;
}

static void value_ssa_alloc_build_plan_item_preview_base(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorkItem *work_item,
    const size_t *initial_effective_degrees,
    const unsigned char *initial_effective_move_related,
    ValueSsaAllocatorPlanItem *out_item) {
    size_t value_id;

    if (!prep || !work_item || !out_item) {
        return;
    }

    value_id = work_item->value_id;
    memset(out_item, 0, sizeof(*out_item));
    out_item->value_id = value_id;
    out_item->work_class = work_item->work_class;
    out_item->priority = work_item->priority;
    value_ssa_alloc_compute_spill_cost_components(prep,
        value_id,
        &out_item->spill_use_cost,
        &out_item->spill_use_loop_depth_cost,
        &out_item->spill_use_call_density_cost,
        &out_item->spill_live_range_cost,
        &out_item->spill_live_block_cost,
        &out_item->spill_loop_depth_cost,
        &out_item->spill_call_density_cost,
        &out_item->spill_call_crossing_cost,
        &out_item->spill_cross_block_cost,
        &out_item->spill_affinity_cost,
        &out_item->spill_cost);
    out_item->rematerializable = prep->rematerializable ? prep->rematerializable[value_id] : 0u;
    out_item->split_child_value = prep->split_child_values ? prep->split_child_values[value_id] : 0u;
    out_item->split_child_depth = prep->split_child_depths ? prep->split_child_depths[value_id] : 0;
    out_item->initial_degree = prep->interference_degrees[value_id];
    out_item->initial_move_related = prep->move_related[value_id];
    out_item->initial_effective_degree =
        initial_effective_degrees ? initial_effective_degrees[value_id] : out_item->initial_degree;
    out_item->initial_effective_move_related =
        initial_effective_move_related ? initial_effective_move_related[value_id] : out_item->initial_move_related;
}

static void value_ssa_alloc_finish_plan_item_preview_dynamic(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const size_t *runtime_coalesce_roots,
    const size_t *runtime_coalesce_class_sizes,
    const ValueSsaAllocationFamilyRuntimeFacts *active_family_runtime_facts,
    const size_t *active_degrees,
    const size_t *active_effective_degrees,
    const unsigned char *initial_family_move_related,
    const unsigned char *active_move_related,
    const unsigned char *active_effective_move_related,
    const unsigned char *was_frozen,
    size_t color_budget,
    ValueSsaAllocatorPlanItem *out_item) {
    const ValueSsaAllocationFamilyRuntimeFacts *family_facts = NULL;
    size_t value_id;

    if (!prep || !out_item || !active_degrees || !active_move_related) {
        return;
    }

    value_id = out_item->value_id;
    out_item->active_degree = active_degrees[value_id];
    out_item->active_move_related = active_move_related[value_id];
    out_item->coalesce_root_value_id =
        runtime_coalesce_roots ? runtime_coalesce_roots[value_id]
                               : value_ssa_alloc_plan_effective_root(coalesce_analysis, prep->value_count, value_id);
    out_item->coalesce_class_size = 1;
    if (runtime_coalesce_class_sizes) {
        out_item->coalesce_class_size =
            runtime_coalesce_class_sizes[out_item->coalesce_root_value_id]
                ? runtime_coalesce_class_sizes[out_item->coalesce_root_value_id]
                : 1;
    } else if (value_ssa_alloc_coalesce_analysis_is_usable(coalesce_analysis, prep->value_count)) {
        out_item->coalesce_class_size = coalesce_analysis->class_sizes[out_item->coalesce_root_value_id];
    }
    out_item->active_effective_degree =
        active_effective_degrees ? active_effective_degrees[value_id] : out_item->active_degree;
    out_item->active_effective_move_related =
        active_effective_move_related ? active_effective_move_related[value_id] : out_item->active_move_related;
    out_item->was_frozen = was_frozen ? was_frozen[value_id] : 0u;
    if (active_family_runtime_facts) {
        family_facts = &active_family_runtime_facts[out_item->coalesce_root_value_id];
    }
    out_item->family_external_neighbor_count = family_facts ? family_facts->external_neighbor_count : 0;
    out_item->family_coalesce_ready_neighbor_count = family_facts ? family_facts->coalesce_ready_neighbor_count : 0;
    out_item->family_coalesce_ready_affinity_weight_sum =
        family_facts ? family_facts->coalesce_ready_affinity_weight_sum : 0;
    out_item->family_best_coalesce_ready_partner_root_value_id =
        family_facts ? family_facts->best_coalesce_ready_partner_root_value_id : (size_t)-1;
    out_item->family_best_coalesce_ready_partner_affinity_weight =
        family_facts ? family_facts->best_coalesce_ready_partner_affinity_weight : 0;
    out_item->family_best_coalesce_ready_partner_is_mutual =
        family_facts ? family_facts->best_coalesce_ready_partner_is_mutual : 0u;
    out_item->family_external_affinity_weight_sum = family_facts ? family_facts->external_affinity_weight_sum : 0;
    out_item->move_work_class = family_facts
        ? family_facts->move_work_class
        : value_ssa_alloc_classify_plan_move_work(
              initial_family_move_related ? (int)initial_family_move_related[out_item->coalesce_root_value_id]
                                          : (int)out_item->initial_effective_move_related,
              (int)out_item->was_frozen,
              family_facts ? family_facts->active_member_count : 0,
              family_facts ? family_facts->coalesce_ready_neighbor_count : 0);
    out_item->move_work_next_class = out_item->move_work_class;
    out_item->move_work_priority = family_facts
        ? family_facts->queue_priority
        : value_ssa_alloc_compute_plan_move_work_priority(
              out_item->priority,
              out_item->coalesce_class_size,
              out_item->family_coalesce_ready_neighbor_count,
              out_item->family_coalesce_ready_affinity_weight_sum,
              out_item->family_best_coalesce_ready_partner_affinity_weight,
              (int)out_item->family_best_coalesce_ready_partner_is_mutual,
              out_item->family_external_neighbor_count,
              out_item->family_external_affinity_weight_sum);
    out_item->family_spill_removed_count = family_facts ? family_facts->spill_removed_count : 0;
    out_item->family_active_member_count = family_facts ? family_facts->active_member_count : 0;
    out_item->family_active_member_count_after = out_item->family_active_member_count;
    out_item->family_external_neighbor_count_after = out_item->family_external_neighbor_count;
    out_item->family_coalesce_ready_neighbor_count_after = out_item->family_coalesce_ready_neighbor_count;
    out_item->family_coalesce_ready_affinity_weight_sum_after = out_item->family_coalesce_ready_affinity_weight_sum;
    out_item->family_best_coalesce_ready_partner_root_value_id_after =
        out_item->family_best_coalesce_ready_partner_root_value_id;
    out_item->family_best_coalesce_ready_partner_affinity_weight_after =
        out_item->family_best_coalesce_ready_partner_affinity_weight;
    out_item->family_best_coalesce_ready_partner_is_mutual_after =
        out_item->family_best_coalesce_ready_partner_is_mutual;
    out_item->family_external_affinity_weight_sum_after = out_item->family_external_affinity_weight_sum;
    out_item->phase = value_ssa_alloc_classify_phase(
        prep, out_item->active_effective_move_related, out_item->active_effective_degree, color_budget);
    out_item->removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
}

static int value_ssa_alloc_plan_has_no_move_related_values(const ValueSsaAllocationPrep *prep,
    const ValueSsaFunction *function) {
    size_t value_id;

    if (!prep || !function || !prep->move_related || prep->value_count != function->next_value_id) {
        return 0;
    }

    for (value_id = 0; value_id < function->next_value_id; ++value_id) {
        if (prep->move_related[value_id]) {
            return 0;
        }
    }
    return 1;
}

static int value_ssa_alloc_build_no_move_allocator_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    ValueSsaAllocatorPlan *plan,
    ValueSsaError *error) {
    ValueSsaAllocatorPlanItem *base_items = NULL;
    size_t *active_degrees = NULL;
    size_t *active_effective_degrees = NULL;
    size_t *scratch_roots = NULL;
    unsigned char *scratch_root_flags = NULL;
    unsigned char *scheduled = NULL;
    int use_effective_degree_tracking = 0;
    size_t item_index;
    size_t output_index;
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;

    if (!function || !prep || !worklist || !interference_graph || !plan) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-238: invalid no-move allocator-plan contract");
        return 0;
    }
    if (prep->value_count != function->next_value_id || worklist->item_count > function->next_value_id ||
        interference_graph->value_count != function->next_value_id) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-239: inconsistent no-move allocator-plan artifacts");
        return 0;
    }

    value_ssa_allocator_plan_free(plan);
    plan->value_count = function->next_value_id;
    plan->item_count = worklist->item_count;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(plan->item_count ? plan->item_count : 1, sizeof(ValueSsaAllocatorPlanItem));
    plan->final_runtime_coalesce_roots =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    plan->final_runtime_coalesce_class_sizes =
        (size_t *)calloc((function->next_value_id ? function->next_value_id : 1), sizeof(size_t));
    plan->value_item_indices =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    base_items =
        (ValueSsaAllocatorPlanItem *)calloc(worklist->item_count ? worklist->item_count : 1, sizeof(ValueSsaAllocatorPlanItem));
    active_degrees = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    active_effective_degrees =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    scratch_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    scratch_root_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    scheduled = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    if (!plan->items || !plan->final_runtime_coalesce_roots || !plan->final_runtime_coalesce_class_sizes ||
        !plan->value_item_indices || !base_items || !active_degrees || !active_effective_degrees || !scratch_roots ||
        !scratch_root_flags || !scheduled) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-240: out of memory building no-move allocator plan");
        goto fail;
    }

    for (item_index = 0; item_index < function->next_value_id; ++item_index) {
        plan->value_item_indices[item_index] = (size_t)-1;
        if (coalesce_analysis && coalesce_analysis->value_roots && coalesce_analysis->class_sizes &&
            coalesce_analysis->value_count == function->next_value_id) {
            plan->final_runtime_coalesce_roots[item_index] = coalesce_analysis->value_roots[item_index];
            plan->final_runtime_coalesce_class_sizes[item_index] = coalesce_analysis->class_sizes[item_index];
        } else {
            plan->final_runtime_coalesce_roots[item_index] = item_index;
            plan->final_runtime_coalesce_class_sizes[item_index] = 1;
        }
    }
    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        value_ssa_alloc_build_plan_item_preview_base(
            prep, &worklist->items[item_index], prep->interference_degrees, prep->move_related, &base_items[item_index]);
    }
    value_ssa_alloc_recompute_active_degrees(interference_graph, scheduled, active_degrees);
    use_effective_degree_tracking =
        value_ssa_alloc_coalesce_analysis_has_nontrivial_classes(coalesce_analysis, function->next_value_id);
    if (use_effective_degree_tracking) {
        value_ssa_alloc_recompute_active_effective_degrees(interference_graph,
            coalesce_analysis,
            scheduled,
            active_degrees,
            scratch_roots,
            scratch_root_flags,
            active_effective_degrees);
    }

    for (output_index = 0; output_index < worklist->item_count; ++output_index) {
        size_t best_item_index = (size_t)-1;
        ValueSsaAllocatorPlanItem best_item;
        int found = 0;

        memset(&best_item, 0, sizeof(best_item));
        for (item_index = 0; item_index < worklist->item_count; ++item_index) {
            ValueSsaAllocatorPlanItem candidate;
            size_t value_id = worklist->items[item_index].value_id;

            if (value_id >= function->next_value_id || scheduled[value_id]) {
                continue;
            }

            candidate = base_items[item_index];
            candidate.active_degree = active_degrees[value_id];
            candidate.active_effective_degree =
                use_effective_degree_tracking ? active_effective_degrees[value_id] : active_degrees[value_id];
            candidate.active_move_related = 0u;
            candidate.active_effective_move_related = 0u;
            candidate.was_frozen = 0u;
            if (use_effective_degree_tracking) {
                candidate.coalesce_root_value_id =
                    value_ssa_alloc_plan_effective_root(coalesce_analysis, function->next_value_id, value_id);
                candidate.coalesce_class_size = value_ssa_alloc_plan_runtime_coalesce_class_size(
                    NULL, coalesce_analysis, function->next_value_id, value_id);
            } else {
                candidate.coalesce_root_value_id = value_id;
                candidate.coalesce_class_size = 1;
            }
            candidate.move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
            candidate.move_work_next_class = VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
            candidate.move_work_priority = value_ssa_alloc_compute_plan_move_work_priority(
                candidate.priority, candidate.coalesce_class_size, 0, 0, 0, 0, 0, 0);
            candidate.family_spill_removed_count = 0;
            candidate.family_active_member_count = 1;
            candidate.family_active_member_count_after = 0;
            candidate.family_best_coalesce_ready_partner_root_value_id = (size_t)-1;
            candidate.family_best_coalesce_ready_partner_root_value_id_after = (size_t)-1;
            candidate.phase = value_ssa_alloc_classify_phase(prep, 0, candidate.active_effective_degree, color_budget);
            candidate.removal_kind = candidate.phase == VALUE_SSA_ALLOCATOR_PHASE_SPILL
                ? VALUE_SSA_ALLOCATOR_REMOVE_SPILL
                : VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

            if (!found || value_ssa_alloc_plan_item_is_better(&candidate, &best_item)) {
                best_item_index = item_index;
                best_item = candidate;
                found = 1;
            }
        }

        if (!found || best_item_index == (size_t)-1 || best_item.value_id >= function->next_value_id) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-241: no-move allocator plan underflow");
            goto fail;
        }

        plan->items[output_index] = best_item;
        plan->value_item_indices[best_item.value_id] = output_index;
        scheduled[best_item.value_id] = 1u;

        active_degrees[best_item.value_id] = 0;
        active_effective_degrees[best_item.value_id] = 0;
        for (item_index = 0; item_index < function->next_value_id; ++item_index) {
            if (item_index == best_item.value_id || scheduled[item_index]) {
                continue;
            }
            if (!interference_graph->interferes[best_item.value_id * function->next_value_id + item_index]) {
                continue;
            }
            if (active_degrees[item_index] > 0) {
                --active_degrees[item_index];
            }
        }
        if (use_effective_degree_tracking) {
            value_ssa_alloc_recompute_active_effective_degrees(interference_graph,
                coalesce_analysis,
                scheduled,
                active_degrees,
                scratch_roots,
                scratch_root_flags,
                active_effective_degrees);
        }
    }

    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "no-move-plan total", value_ssa_alloc_now_s() - total_start);
    }
    free(base_items);
    free(active_degrees);
    free(active_effective_degrees);
    free(scratch_roots);
    free(scratch_root_flags);
    free(scheduled);
    return 1;

fail:
    free(base_items);
    free(active_degrees);
    free(active_effective_degrees);
    free(scratch_roots);
    free(scratch_root_flags);
    free(scheduled);
    value_ssa_allocator_plan_free(plan);
    return 0;
}

int value_ssa_compute_allocator_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    ValueSsaAllocatorPlan *plan,
    ValueSsaError *error) {
    ValueSsaAllocatorMoveEngineRunResult run_result;

    if (!function || !plan) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-067: invalid allocator-plan contract");
        return 0;
    }

    if (prep && worklist && interference_graph &&
        value_ssa_alloc_plan_has_no_move_related_values(prep, function)) {
        return value_ssa_alloc_build_no_move_allocator_plan(
            function, prep, worklist, interference_graph, coalesce_analysis, color_budget, plan, error);
    }

    value_ssa_alloc_move_engine_run_result_init(&run_result);
    if (!value_ssa_alloc_run_move_engine(function,
            prep,
            worklist,
            interference_graph,
            affinity_graph,
            coalesce_analysis,
            color_budget,
            plan,
            NULL,
            &run_result,
            error)) {
        return 0;
    }
    if (run_result.status != VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_COMPLETED || !run_result.published_plan_artifacts) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-209: allocator-plan run did not publish final plan artifacts");
        return 0;
    }
    return 1;
}

int value_ssa_allocator_plan_find_value(const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocatorPlanItem **out_item) {
    size_t item_index;

    if (!plan || value_id >= plan->value_count) {
        return 0;
    }

    if (plan->value_item_indices) {
        item_index = plan->value_item_indices[value_id];
        if (item_index >= plan->item_count) {
            return 0;
        }
        if (plan->items[item_index].value_id != value_id) {
            return 0;
        }
    } else {
        if (!plan->items) {
            return 0;
        }
        for (item_index = 0; item_index < plan->item_count; ++item_index) {
            if (plan->items[item_index].value_id == value_id) {
                break;
            }
        }
        if (item_index >= plan->item_count) {
            return 0;
        }
    }
    if (out_index) {
        *out_index = item_index;
    }
    if (out_item) {
        *out_item = &plan->items[item_index];
    }

    return 1;
}

int value_ssa_allocator_plan_get_spill_cost(const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t *out_total_cost,
    size_t *out_use_cost,
    size_t *out_live_range_cost,
    size_t *out_affinity_cost) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!out_total_cost || !out_use_cost || !out_live_range_cost || !out_affinity_cost ||
        !value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        return 0;
    }

    *out_total_cost = item->spill_cost;
    *out_use_cost = item->spill_use_cost;
    *out_live_range_cost = item->spill_live_range_cost;
    *out_affinity_cost = item->spill_affinity_cost;
    return 1;
}

int value_ssa_allocator_plan_get_spill_cost_detail(const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t *out_use_loop_depth_cost,
    size_t *out_use_call_density_cost,
    size_t *out_live_block_cost,
    size_t *out_loop_depth_cost,
    size_t *out_call_density_cost,
    size_t *out_call_crossing_cost,
    size_t *out_cross_block_cost) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!out_use_loop_depth_cost || !out_use_call_density_cost || !out_live_block_cost || !out_loop_depth_cost ||
        !out_call_density_cost || !out_call_crossing_cost || !out_cross_block_cost ||
        !value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        return 0;
    }

    *out_use_loop_depth_cost = item->spill_use_loop_depth_cost;
    *out_use_call_density_cost = item->spill_use_call_density_cost;
    *out_live_block_cost = item->spill_live_block_cost;
    *out_loop_depth_cost = item->spill_loop_depth_cost;
    *out_call_density_cost = item->spill_call_density_cost;
    *out_call_crossing_cost = item->spill_call_crossing_cost;
    *out_cross_block_cost = item->spill_cross_block_cost;
    return 1;
}

int value_ssa_dump_allocator_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    ValueSsaAllocationPrep owned_prep;
    ValueSsaAllocatorPlan owned_plan;
    const ValueSsaAllocationPrep *active_prep = prep;
    const ValueSsaAllocatorPlan *active_plan = plan;
    int ok = 0;

    if (!function || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-069: invalid allocator-plan dump contract");
        return 0;
    }

    value_ssa_allocation_prep_init(&owned_prep);
    value_ssa_allocator_plan_init(&owned_plan);
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    }
    if (!active_plan) {
        if (!value_ssa_compute_allocator_plan(
                function, active_prep, NULL, NULL, NULL, NULL, color_budget, &owned_plan, error)) {
            goto cleanup;
        }
        active_plan = &owned_plan;
    }

    if (!value_ssa_alloc_append_format(&builder,
            "allocator-plan %s colors=%zu:\n",
            function->name ? function->name : "<anon>",
            color_budget)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-070: out of memory building allocator plan dump");
        goto cleanup;
    }

    {
        size_t item_index;

        for (item_index = 0; item_index < active_plan->item_count; ++item_index) {
            const ValueSsaAllocatorPlanItem *item = &active_plan->items[item_index];
            size_t value_id = item->value_id;

            if (!value_ssa_alloc_append_format(&builder,
                    "  %zu: ssa.%zu phase=%s remove=%s class=%s coalesce=ssa.%zu/%zu priority=%zu spill_cost=%zu(uses=%zu,blocks=%zu,affinity=%zu) detail=(use_loops=%zu,use_calls=%zu,live=%zu,loops=%zu,calls=%zu,call_cross=%zu,xblock=%zu) split_child=%s depth=%zu degree=%zu->%zu effective_degree=%zu->%zu move_work=%s->%s:%zu family_members=%zu->%zu family_pressure=(neighbors=%zu,ready=%zu,ready_affinity=%zu,best_ready=%s%zu@%zu,mutual=%s,affinity=%zu)->(neighbors=%zu,ready=%zu,ready_affinity=%zu,best_ready=%s%zu@%zu,mutual=%s,affinity=%zu) move_related=%s->%s effective_move=%s->%s frozen=%s\n",
                    item_index,
                    value_id,
                    value_ssa_allocator_phase_name(item->phase),
                    value_ssa_allocator_removal_kind_name(item->removal_kind),
                    value_ssa_allocation_work_class_name(item->work_class),
                    item->coalesce_root_value_id,
                    item->coalesce_class_size,
                    item->priority,
                    item->spill_cost,
                    item->spill_use_cost,
                    item->spill_live_range_cost,
                    item->spill_affinity_cost,
                    item->spill_use_loop_depth_cost,
                    item->spill_use_call_density_cost,
                    item->spill_live_block_cost,
                    item->spill_loop_depth_cost,
                    item->spill_call_density_cost,
                    item->spill_call_crossing_cost,
                    item->spill_cross_block_cost,
                    item->split_child_value ? "yes" : "no",
                    item->split_child_depth,
                    item->initial_degree,
                    item->active_degree,
                    item->initial_effective_degree,
                    item->active_effective_degree,
                    value_ssa_allocator_move_work_class_name(item->move_work_class),
                    value_ssa_allocator_move_work_class_name(item->move_work_next_class),
                    item->move_work_priority,
                    item->family_active_member_count,
                    item->family_active_member_count_after,
                    item->family_external_neighbor_count,
                    item->family_coalesce_ready_neighbor_count,
                    item->family_coalesce_ready_affinity_weight_sum,
                    item->family_best_coalesce_ready_partner_root_value_id == (size_t)-1 ? "-" : "ssa.",
                    item->family_best_coalesce_ready_partner_root_value_id == (size_t)-1
                        ? 0
                        : item->family_best_coalesce_ready_partner_root_value_id,
                    item->family_best_coalesce_ready_partner_affinity_weight,
                    item->family_best_coalesce_ready_partner_is_mutual ? "yes" : "no",
                    item->family_external_affinity_weight_sum,
                    item->family_external_neighbor_count_after,
                    item->family_coalesce_ready_neighbor_count_after,
                    item->family_coalesce_ready_affinity_weight_sum_after,
                    item->family_best_coalesce_ready_partner_root_value_id_after == (size_t)-1 ? "-" : "ssa.",
                    item->family_best_coalesce_ready_partner_root_value_id_after == (size_t)-1
                        ? 0
                        : item->family_best_coalesce_ready_partner_root_value_id_after,
                    item->family_best_coalesce_ready_partner_affinity_weight_after,
                    item->family_best_coalesce_ready_partner_is_mutual_after ? "yes" : "no",
                    item->family_external_affinity_weight_sum_after,
                    item->initial_move_related ? "yes" : "no",
                    item->active_move_related ? "yes" : "no",
                    item->initial_effective_move_related ? "yes" : "no",
                    item->active_effective_move_related ? "yes" : "no",
                    item->was_frozen ? "yes" : "no")) {
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-071: out of memory appending allocator plan item");
                goto cleanup;
            }
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    value_ssa_allocator_plan_free(&owned_plan);
    value_ssa_allocation_prep_free(&owned_prep);
    return ok;
}

#endif
