#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

static int value_ssa_alloc_eviction_candidate_is_better(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t best_value_id);

typedef struct {
    unsigned char has_candidate;
    size_t family_root_value_id;
    size_t blocker_value_id;
    size_t blocked_color;
    unsigned char blocker_recolorable;
    size_t blocker_recolor_color;
} ValueSsaAllocationFamilyBlockerCandidate;

static void value_ssa_alloc_family_blocker_candidate_init(ValueSsaAllocationFamilyBlockerCandidate *candidate) {
    if (!candidate) {
        return;
    }

    memset(candidate, 0, sizeof(*candidate));
    candidate->family_root_value_id = (size_t)-1;
    candidate->blocker_value_id = (size_t)-1;
    candidate->blocked_color = (size_t)-1;
    candidate->blocker_recolor_color = (size_t)-1;
}

static int value_ssa_alloc_eviction_candidate_has_lighter_family_pressure(const ValueSsaAllocatorPlan *plan,
    size_t candidate_value_id,
    size_t best_value_id) {
    int compare = 0;

    if (!value_ssa_alloc_compare_family_pressure_values(plan, candidate_value_id, best_value_id, &compare)) {
        return 0;
    }
    return compare < 0;
}

static int value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_activity(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t best_value_id) {
    int compare = 0;

    if (!value_ssa_alloc_compare_runtime_family_activity_values(
            plan, result, candidate_value_id, best_value_id, &compare)) {
        return 0;
    }
    return compare < 0;
}

static int value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_color_support(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t candidate_color,
    size_t best_value_id,
    size_t best_color) {
    int compare = 0;

    if (!result ||
        !value_ssa_alloc_value_has_explicit_family_color_authority(
            plan, coalesce_analysis, result->value_count, candidate_value_id) ||
        !value_ssa_alloc_value_has_explicit_family_color_authority(
            plan, coalesce_analysis, result->value_count, best_value_id)) {
        return 0;
    }

    if (!value_ssa_alloc_compare_runtime_family_color_support(plan,
            coalesce_analysis,
            result,
            candidate_value_id,
            candidate_color,
            best_value_id,
            best_color,
            &compare)) {
        return 0;
    }
    return compare < 0;
}

static size_t value_ssa_alloc_coalesce_class_size(const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t value_id) {
    size_t root_value_id;

    if (!coalesce_analysis || value_id >= value_count || coalesce_analysis->value_count != value_count ||
        !coalesce_analysis->value_roots || !coalesce_analysis->class_sizes) {
        return 1;
    }

    root_value_id = coalesce_analysis->value_roots[value_id];
    if (root_value_id >= value_count || coalesce_analysis->class_sizes[root_value_id] == 0) {
        return 1;
    }
    return coalesce_analysis->class_sizes[root_value_id];
}

static size_t value_ssa_alloc_value_spill_cost_for_select(const ValueSsaAllocationPrep *prep, size_t value_id) {
    if (!prep || value_id >= prep->value_count) {
        return 0;
    }

    return prep->use_counts[value_id] + prep->live_block_counts[value_id] +
        (prep->loop_depth_sums ? prep->loop_depth_sums[value_id] : 0) +
        (prep->call_density_sums ? prep->call_density_sums[value_id] : 0) +
        (prep->call_crossing_counts ? prep->call_crossing_counts[value_id] : 0) +
        value_ssa_alloc_cross_block_spill_penalty(prep, value_id) + prep->affinity_sums[value_id];
}

static int value_ssa_alloc_value_is_split_child_for_select(const ValueSsaAllocationPrep *prep, size_t value_id) {
    return prep && prep->split_child_values && value_id < prep->value_count && prep->split_child_values[value_id];
}

static size_t value_ssa_alloc_split_child_depth_for_select(const ValueSsaAllocationPrep *prep, size_t value_id) {
    return prep && prep->split_child_depths && value_id < prep->value_count ? prep->split_child_depths[value_id] : 0;
}

static void value_ssa_alloc_mark_evicted_value_spilled(ValueSsaAllocationResult *result, size_t value_id) {
    if (!result || value_id >= result->value_count) {
        return;
    }

    result->has_color[value_id] = 0;
    result->colors[value_id] = (size_t)-1;
    result->spill_flags[value_id] = 1;
    if (result->spill_confirmed_flags) {
        result->spill_confirmed_flags[value_id] = 1;
    }
    if (result->spill_recovered_flags) {
        result->spill_recovered_flags[value_id] = 0;
    }
    if (result->used_preferred_color_flags) {
        result->used_preferred_color_flags[value_id] = 0;
    }
    if (result->preferred_color_sources) {
        result->preferred_color_sources[value_id] = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    }
    if (result->preferred_color_partner_value_ids) {
        result->preferred_color_partner_value_ids[value_id] = (size_t)-1;
    }
    if (result->used_generic_eviction_flags) {
        result->used_generic_eviction_flags[value_id] = 0;
    }
    if (result->generic_eviction_blocker_value_ids) {
        result->generic_eviction_blocker_value_ids[value_id] = (size_t)-1;
    }
    if (result->generic_eviction_blocker_recolored_flags) {
        result->generic_eviction_blocker_recolored_flags[value_id] = 0;
    }
    if (result->generic_eviction_blocker_new_colors) {
        result->generic_eviction_blocker_new_colors[value_id] = (size_t)-1;
    }
    if (result->used_targeted_eviction_flags) {
        result->used_targeted_eviction_flags[value_id] = 0;
    }
    if (result->targeted_eviction_blocker_value_ids) {
        result->targeted_eviction_blocker_value_ids[value_id] = (size_t)-1;
    }
    if (result->targeted_eviction_blocker_recolored_flags) {
        result->targeted_eviction_blocker_recolored_flags[value_id] = 0;
    }
    if (result->targeted_eviction_blocker_new_colors) {
        result->targeted_eviction_blocker_new_colors[value_id] = (size_t)-1;
    }
    if (result->used_retry_eviction_flags) {
        result->used_retry_eviction_flags[value_id] = 0;
    }
    if (result->retry_eviction_blocker_value_ids) {
        result->retry_eviction_blocker_value_ids[value_id] = (size_t)-1;
    }
    if (result->retry_eviction_blocker_recolored_flags) {
        result->retry_eviction_blocker_recolored_flags[value_id] = 0;
    }
    if (result->retry_eviction_blocker_new_colors) {
        result->retry_eviction_blocker_new_colors[value_id] = (size_t)-1;
    }
}

static void value_ssa_alloc_clear_value_choice_metadata(ValueSsaAllocationResult *result, size_t value_id) {
    if (!result || value_id >= result->value_count) {
        return;
    }

    if (result->used_preferred_color_flags) {
        result->used_preferred_color_flags[value_id] = 0;
    }
    if (result->preferred_color_sources) {
        result->preferred_color_sources[value_id] = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    }
    if (result->preferred_color_partner_value_ids) {
        result->preferred_color_partner_value_ids[value_id] = (size_t)-1;
    }
    if (result->used_generic_eviction_flags) {
        result->used_generic_eviction_flags[value_id] = 0;
    }
    if (result->generic_eviction_blocker_value_ids) {
        result->generic_eviction_blocker_value_ids[value_id] = (size_t)-1;
    }
    if (result->generic_eviction_blocker_recolored_flags) {
        result->generic_eviction_blocker_recolored_flags[value_id] = 0;
    }
    if (result->generic_eviction_blocker_new_colors) {
        result->generic_eviction_blocker_new_colors[value_id] = (size_t)-1;
    }
    if (result->used_targeted_eviction_flags) {
        result->used_targeted_eviction_flags[value_id] = 0;
    }
    if (result->targeted_eviction_blocker_value_ids) {
        result->targeted_eviction_blocker_value_ids[value_id] = (size_t)-1;
    }
    if (result->targeted_eviction_blocker_recolored_flags) {
        result->targeted_eviction_blocker_recolored_flags[value_id] = 0;
    }
    if (result->targeted_eviction_blocker_new_colors) {
        result->targeted_eviction_blocker_new_colors[value_id] = (size_t)-1;
    }
    if (result->used_retry_eviction_flags) {
        result->used_retry_eviction_flags[value_id] = 0;
    }
    if (result->retry_eviction_blocker_value_ids) {
        result->retry_eviction_blocker_value_ids[value_id] = (size_t)-1;
    }
    if (result->retry_eviction_blocker_recolored_flags) {
        result->retry_eviction_blocker_recolored_flags[value_id] = 0;
    }
    if (result->retry_eviction_blocker_new_colors) {
        result->retry_eviction_blocker_new_colors[value_id] = (size_t)-1;
    }
}

static int value_ssa_alloc_find_recolor_color_for_blocker(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t blocker_value_id,
    size_t forbidden_color,
    size_t color_budget,
    int *out_has_recolor,
    size_t *out_recolor_color,
    ValueSsaError *error) {
    unsigned char *blocked_colors = NULL;
    size_t color;
    size_t neighbor_value_id;
    size_t best_color = 0;
    size_t best_support_count = 0;
    int has_preferred_color = 0;
    size_t preferred_color = 0;
    int found = 0;

    if (!interference_graph || !result || !out_has_recolor || !out_recolor_color ||
        blocker_value_id >= interference_graph->value_count || blocker_value_id >= result->value_count ||
        forbidden_color >= color_budget) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-108: invalid blocker recolor query");
        return 0;
    }

    *out_has_recolor = 0;
    *out_recolor_color = 0;
    blocked_colors = (unsigned char *)calloc(color_budget ? color_budget : 1, sizeof(unsigned char));
    if (!blocked_colors) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-125: out of memory preparing blocker recolor mask");
        return 0;
    }

    if (result->has_color && result->colors && result->has_color[blocker_value_id] &&
        result->colors[blocker_value_id] < color_budget) {
        blocked_colors[result->colors[blocker_value_id]] = 1u;
    }
    blocked_colors[forbidden_color] = 1u;
    for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
        if (neighbor_value_id == blocker_value_id || neighbor_value_id >= result->value_count ||
            !interference_graph
                 ->interferes[blocker_value_id * interference_graph->value_count + neighbor_value_id] ||
            !result->has_color[neighbor_value_id] || result->spill_flags[neighbor_value_id] ||
            result->colors[neighbor_value_id] >= color_budget) {
            continue;
        }
        blocked_colors[result->colors[neighbor_value_id]] = 1u;
    }

    if (prep && plan &&
        !value_ssa_alloc_result_find_preferred_color(prep,
            coalesce_analysis,
            plan,
            result,
            blocked_colors,
            1,
            0,
            blocker_value_id,
            color_budget,
            &has_preferred_color,
            &preferred_color,
            NULL,
            NULL,
            error)) {
        free(blocked_colors);
        return 0;
    }
    if (has_preferred_color && preferred_color < color_budget && !blocked_colors[preferred_color]) {
        *out_has_recolor = 1;
        *out_recolor_color = preferred_color;
        free(blocked_colors);
        return 1;
    }

    for (color = 0; color < color_budget; ++color) {
        if (blocked_colors[color]) {
            continue;
        }

        if (!found) {
            size_t support_count = 0;

            if (!value_ssa_alloc_count_runtime_family_members_on_color(
                    plan, coalesce_analysis, result, blocker_value_id, color, &support_count)) {
                support_count = 0;
            }
            found = 1;
            best_color = color;
            best_support_count = support_count;
            continue;
        } else {
            size_t support_count = 0;

            if (!value_ssa_alloc_count_runtime_family_members_on_color(
                    plan, coalesce_analysis, result, blocker_value_id, color, &support_count)) {
                support_count = 0;
            }
            if (support_count > best_support_count ||
                (support_count == best_support_count && color < best_color)) {
                best_color = color;
                best_support_count = support_count;
            }
        }
    }

    if (found) {
        *out_has_recolor = 1;
        *out_recolor_color = best_color;
    }
    free(blocked_colors);
    return 1;
}

static int value_ssa_alloc_try_recolor_blocker(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t blocker_value_id,
    size_t forbidden_color,
    size_t color_budget,
    int *out_recolored,
    ValueSsaError *error) {
    size_t color;

    if (!interference_graph || !result || !out_recolored || blocker_value_id >= interference_graph->value_count ||
        blocker_value_id >= result->value_count || forbidden_color >= color_budget) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-108: invalid blocker recolor query");
        return 0;
    }

    if (!value_ssa_alloc_find_recolor_color_for_blocker(
            interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            blocker_value_id,
            forbidden_color,
            color_budget,
            out_recolored,
            &color,
            error)) {
        return 0;
    }
    if (*out_recolored) {
        result->has_color[blocker_value_id] = 1;
        result->colors[blocker_value_id] = color;
        result->spill_flags[blocker_value_id] = 0;
        if (result->spill_confirmed_flags) {
            result->spill_confirmed_flags[blocker_value_id] = 0;
        }
        value_ssa_alloc_clear_value_choice_metadata(result, blocker_value_id);
    }

    return 1;
}

static int value_ssa_alloc_eviction_candidate_is_better_for_color(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t candidate_blocked_color,
    size_t best_value_id,
    size_t best_blocked_color,
    size_t color_budget,
    ValueSsaError *error) {
    int candidate_has_recolor = 0;
    int best_has_recolor = 0;
    size_t candidate_recolor_color = 0;
    size_t best_recolor_color = 0;
    size_t candidate_class_size;
    size_t best_class_size;
    size_t candidate_priority;
    size_t best_priority;
    int recolor_support_compare = 0;

    if (!interference_graph || !result || candidate_value_id >= result->value_count ||
        best_value_id >= result->value_count || candidate_value_id >= interference_graph->value_count ||
        best_value_id >= interference_graph->value_count || candidate_blocked_color >= color_budget ||
        best_blocked_color >= color_budget) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-109: invalid eviction candidate comparison");
        return 0;
    }

    candidate_priority = result->spill_priorities[candidate_value_id];
    best_priority = result->spill_priorities[best_value_id];
    if (candidate_priority != best_priority) {
        return candidate_priority < best_priority;
    }

    if (!value_ssa_alloc_find_recolor_color_for_blocker(
            interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            candidate_value_id,
            candidate_blocked_color,
            color_budget,
            &candidate_has_recolor,
            &candidate_recolor_color,
            error) ||
        !value_ssa_alloc_find_recolor_color_for_blocker(
            interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            best_value_id,
            best_blocked_color,
            color_budget,
            &best_has_recolor,
            &best_recolor_color,
            error)) {
        return 0;
    }
    if (candidate_has_recolor != best_has_recolor) {
        return candidate_has_recolor > best_has_recolor;
    }
    if (candidate_has_recolor && best_has_recolor) {
        if (value_ssa_alloc_compare_weighted_runtime_family_color_support(
                plan,
                coalesce_analysis,
                result,
                candidate_value_id,
                candidate_recolor_color,
                best_value_id,
                best_recolor_color,
                &recolor_support_compare)) {
            if (recolor_support_compare != 0) {
                return recolor_support_compare > 0;
            }
        }
    }

    if (value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_color_support(plan,
            coalesce_analysis,
            result,
            candidate_value_id,
            candidate_blocked_color,
            best_value_id,
            best_blocked_color)) {
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_color_support(plan,
            coalesce_analysis,
            result,
            best_value_id,
            best_blocked_color,
            candidate_value_id,
            candidate_blocked_color)) {
        return 0;
    }

    if (value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_activity(
            plan, result, candidate_value_id, best_value_id)) {
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_activity(
            plan, result, best_value_id, candidate_value_id)) {
        return 0;
    }

    if (value_ssa_alloc_eviction_candidate_has_lighter_family_pressure(plan, candidate_value_id, best_value_id)) {
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_has_lighter_family_pressure(plan, best_value_id, candidate_value_id)) {
        return 0;
    }

    candidate_class_size =
        value_ssa_alloc_coalesce_class_size(coalesce_analysis, result->value_count, candidate_value_id);
    best_class_size = value_ssa_alloc_coalesce_class_size(coalesce_analysis, result->value_count, best_value_id);
    if (candidate_class_size != best_class_size) {
        return candidate_class_size < best_class_size;
    }
    if (value_ssa_alloc_value_is_split_child_for_select(prep, candidate_value_id) !=
        value_ssa_alloc_value_is_split_child_for_select(prep, best_value_id)) {
        return value_ssa_alloc_value_is_split_child_for_select(prep, candidate_value_id);
    }
    if (value_ssa_alloc_split_child_depth_for_select(prep, candidate_value_id) !=
        value_ssa_alloc_split_child_depth_for_select(prep, best_value_id)) {
        return value_ssa_alloc_split_child_depth_for_select(prep, candidate_value_id) >
            value_ssa_alloc_split_child_depth_for_select(prep, best_value_id);
    }

    return value_ssa_alloc_eviction_candidate_is_better(prep, plan, result, candidate_value_id, best_value_id);
}

static int value_ssa_alloc_compare_family_blocker_candidates(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationFamilyBlockerCandidate *candidate,
    const ValueSsaAllocationFamilyBlockerCandidate *best,
    size_t color_budget,
    int *out_compare,
    ValueSsaError *error) {
    if (out_compare) {
        *out_compare = 0;
    }
    if (!interference_graph || !prep || !plan || !result || !candidate || !best || !out_compare ||
        candidate->blocker_value_id == (size_t)-1 || best->blocker_value_id == (size_t)-1 ||
        candidate->blocked_color >= color_budget || best->blocked_color >= color_budget) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-130: invalid family blocker comparison");
        return 0;
    }

    if (value_ssa_alloc_eviction_candidate_is_better_for_color(interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            candidate->blocker_value_id,
            candidate->blocked_color,
            best->blocker_value_id,
            best->blocked_color,
            color_budget,
            error)) {
        *out_compare = 1;
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_is_better_for_color(interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            best->blocker_value_id,
            best->blocked_color,
            candidate->blocker_value_id,
            candidate->blocked_color,
            color_budget,
            error)) {
        *out_compare = -1;
        return 1;
    }

    if (candidate->blocker_recolorable != best->blocker_recolorable) {
        *out_compare = candidate->blocker_recolorable > best->blocker_recolorable ? 1 : -1;
        return 1;
    }
    if (candidate->blocker_recolorable && best->blocker_recolorable &&
        candidate->blocker_recolor_color != best->blocker_recolor_color) {
        *out_compare = candidate->blocker_recolor_color < best->blocker_recolor_color ? 1 : -1;
        return 1;
    }
    if (candidate->blocked_color != best->blocked_color) {
        *out_compare = candidate->blocked_color < best->blocked_color ? 1 : -1;
        return 1;
    }
    if (candidate->blocker_value_id != best->blocker_value_id) {
        *out_compare = candidate->blocker_value_id < best->blocker_value_id ? 1 : -1;
    }
    return 1;
}

static int value_ssa_alloc_update_family_blocker_candidate(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t family_root_value_id,
    size_t blocker_value_id,
    size_t blocked_color,
    int blocker_recolorable,
    size_t blocker_recolor_color,
    size_t color_budget,
    ValueSsaAllocationFamilyBlockerCandidate *family_candidate,
    ValueSsaError *error) {
    ValueSsaAllocationFamilyBlockerCandidate candidate;
    int compare = 0;

    if (!interference_graph || !prep || !plan || !result || !family_candidate) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-131: invalid family blocker update");
        return 0;
    }

    value_ssa_alloc_family_blocker_candidate_init(&candidate);
    candidate.has_candidate = 1u;
    candidate.family_root_value_id = family_root_value_id;
    candidate.blocker_value_id = blocker_value_id;
    candidate.blocked_color = blocked_color;
    candidate.blocker_recolorable = blocker_recolorable ? 1u : 0u;
    candidate.blocker_recolor_color = blocker_recolorable ? blocker_recolor_color : (size_t)-1;

    if (!family_candidate->has_candidate) {
        *family_candidate = candidate;
        return 1;
    }

    if (!value_ssa_alloc_compare_family_blocker_candidates(interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            &candidate,
            family_candidate,
            color_budget,
            &compare,
            error)) {
        return 0;
    }
    if (compare > 0) {
        *family_candidate = candidate;
    }
    return 1;
}

static int value_ssa_alloc_collect_family_blocker_candidates_from_color_facts(
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t value_priority,
    const size_t *color_blocker_counts,
    const size_t *color_blocker_ids,
    size_t color_budget,
    ValueSsaAllocationFamilyBlockerCandidate *out_family_candidates,
    size_t family_candidate_count,
    ValueSsaError *error) {
    size_t color;

    if (!interference_graph || !prep || !plan || !result || !color_blocker_counts || !color_blocker_ids ||
        !out_family_candidates || family_candidate_count < result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-133: invalid family blocker collection");
        return 0;
    }

    for (color = 0; color < color_budget; ++color) {
        size_t blocker_value_id;
        size_t family_root_value_id;
        int blocker_recolorable = 0;
        size_t blocker_recolor_color = 0;

        if (color_blocker_counts[color] != 1) {
            continue;
        }
        blocker_value_id = color_blocker_ids[color];
        if (blocker_value_id == (size_t)-1 || blocker_value_id >= result->value_count ||
            result->spill_priorities[blocker_value_id] >= value_priority) {
            continue;
        }

        if (!value_ssa_alloc_find_recolor_color_for_blocker(
                interference_graph,
                prep,
                coalesce_analysis,
                plan,
                result,
                blocker_value_id,
                color,
                color_budget,
                &blocker_recolorable,
                &blocker_recolor_color,
                error)) {
            return 0;
        }
        family_root_value_id = value_ssa_alloc_plan_family_root_for_value(plan, blocker_value_id);
        if (family_root_value_id >= result->value_count) {
            family_root_value_id = blocker_value_id;
        }

        if (!value_ssa_alloc_update_family_blocker_candidate(interference_graph,
                prep,
                coalesce_analysis,
                plan,
                result,
                family_root_value_id,
                blocker_value_id,
                color,
                blocker_recolorable,
                blocker_recolor_color,
                color_budget,
                &out_family_candidates[family_root_value_id],
                error)) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_alloc_pick_best_family_blocker_candidate(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationFamilyBlockerCandidate *family_candidates,
    size_t family_candidate_count,
    size_t color_budget,
    int *out_found,
    ValueSsaAllocationFamilyBlockerCandidate *out_best_candidate,
    ValueSsaError *error) {
    size_t family_index;
    int found = 0;
    int compare = 0;

    if (out_found) {
        *out_found = 0;
    }
    if (out_best_candidate) {
        value_ssa_alloc_family_blocker_candidate_init(out_best_candidate);
    }
    if (!interference_graph || !prep || !plan || !result || !family_candidates || !out_found || !out_best_candidate) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-132: invalid family blocker pick");
        return 0;
    }

    for (family_index = 0; family_index < family_candidate_count; ++family_index) {
        const ValueSsaAllocationFamilyBlockerCandidate *candidate = &family_candidates[family_index];

        if (!candidate->has_candidate || candidate->blocker_value_id == (size_t)-1) {
            continue;
        }
        if (!found) {
            found = 1;
            *out_best_candidate = *candidate;
            continue;
        }
        if (!value_ssa_alloc_compare_family_blocker_candidates(interference_graph,
                prep,
                coalesce_analysis,
                plan,
                result,
                candidate,
                out_best_candidate,
                color_budget,
                &compare,
                error)) {
            return 0;
        }
        if (compare > 0) {
            *out_best_candidate = *candidate;
        }
    }

    *out_found = found;
    return 1;
}

static int value_ssa_alloc_eviction_candidate_is_better(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t best_value_id) {
    size_t candidate_priority;
    size_t best_priority;
    int candidate_intended;
    int best_intended;
    int candidate_rematerializable;
    int best_rematerializable;
    size_t candidate_cost;
    size_t best_cost;

    if (!result || candidate_value_id >= result->value_count || best_value_id >= result->value_count) {
        return 0;
    }

    candidate_priority = result->spill_priorities[candidate_value_id];
    best_priority = result->spill_priorities[best_value_id];
    if (candidate_priority != best_priority) {
        return candidate_priority < best_priority;
    }

    candidate_intended = result->spill_intended_flags && result->spill_intended_flags[candidate_value_id];
    best_intended = result->spill_intended_flags && result->spill_intended_flags[best_value_id];
    if (candidate_intended != best_intended) {
        return candidate_intended > best_intended;
    }

    candidate_rematerializable = prep && prep->rematerializable && candidate_value_id < prep->value_count &&
        prep->rematerializable[candidate_value_id];
    best_rematerializable =
        prep && prep->rematerializable && best_value_id < prep->value_count && prep->rematerializable[best_value_id];
    if (candidate_rematerializable != best_rematerializable) {
        return candidate_rematerializable > best_rematerializable;
    }

    if (result->has_color && result->colors &&
        value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_color_support(plan,
            NULL,
            result,
            candidate_value_id,
            result->colors[candidate_value_id],
            best_value_id,
            result->colors[best_value_id])) {
        return 1;
    }
    if (result->has_color && result->colors &&
        value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_color_support(plan,
            NULL,
            result,
            best_value_id,
            result->colors[best_value_id],
            candidate_value_id,
            result->colors[candidate_value_id])) {
        return 0;
    }

    if (value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_activity(
            plan, result, candidate_value_id, best_value_id)) {
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_has_lighter_runtime_family_activity(
            plan, result, best_value_id, candidate_value_id)) {
        return 0;
    }

    if (value_ssa_alloc_eviction_candidate_has_lighter_family_pressure(plan, candidate_value_id, best_value_id)) {
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_has_lighter_family_pressure(plan, best_value_id, candidate_value_id)) {
        return 0;
    }

    candidate_cost = value_ssa_alloc_value_spill_cost_for_select(prep, candidate_value_id);
    best_cost = value_ssa_alloc_value_spill_cost_for_select(prep, best_value_id);
    if (candidate_cost != best_cost) {
        return candidate_cost < best_cost;
    }

    return candidate_value_id < best_value_id;
}

static int value_ssa_alloc_assign_spill_slots(const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    ValueSsaAllocationResult *result,
    ValueSsaError *error) {
    size_t value_id;

    if (!interference_graph || !result || interference_graph->value_count != result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-062: invalid spill-slot assignment contract");
        return 0;
    }

    result->spill_slot_count = 0;
    for (value_id = 0; value_id < result->value_count; ++value_id) {
        if (!result->spill_flags[value_id]) {
            result->spill_slots[value_id] = (size_t)-1;
            continue;
        }

        {
            size_t candidate_slot = 0;
            size_t preferred_parent_slot = (size_t)-1;

            if (prep && prep->split_child_values &&
                ((prep->split_family_root_value_ids && value_id < prep->value_count &&
                     prep->split_family_root_value_ids[value_id] != value_id) ||
                    (prep->split_parent_value_ids && value_id < prep->value_count &&
                        prep->split_parent_value_ids[value_id] != (size_t)-1))) {
                size_t parent_value_id =
                    (prep->split_family_root_value_ids && value_id < prep->value_count)
                    ? prep->split_family_root_value_ids[value_id]
                    : prep->split_parent_value_ids[value_id];
                if (parent_value_id == value_id) {
                    parent_value_id = (size_t)-1;
                }
                if (parent_value_id != (size_t)-1 &&
                    parent_value_id < result->value_count && result->spill_flags[parent_value_id] &&
                    result->spill_slots[parent_value_id] != (size_t)-1) {
                    preferred_parent_slot = result->spill_slots[parent_value_id];
                }
            } else if (prep && prep->split_child_values && prep->split_parent_value_ids &&
                value_id < prep->value_count && prep->split_child_values[value_id]) {
                size_t parent_value_id = prep->split_parent_value_ids[value_id];
                if (parent_value_id < result->value_count && result->spill_flags[parent_value_id] &&
                    result->spill_slots[parent_value_id] != (size_t)-1) {
                    preferred_parent_slot = result->spill_slots[parent_value_id];
                }
            }

            if (preferred_parent_slot != (size_t)-1) {
                int blocked = 0;
                size_t neighbor_value_id;

                for (neighbor_value_id = 0; neighbor_value_id < result->value_count; ++neighbor_value_id) {
                    if (neighbor_value_id == value_id || !result->spill_flags[neighbor_value_id] ||
                        result->spill_slots[neighbor_value_id] != preferred_parent_slot) {
                        continue;
                    }
                    if (interference_graph->interferes[value_id * interference_graph->value_count + neighbor_value_id]) {
                        blocked = 1;
                        break;
                    }
                }
                if (!blocked) {
                    result->spill_slots[value_id] = preferred_parent_slot;
                    if (preferred_parent_slot + 1 > result->spill_slot_count) {
                        result->spill_slot_count = preferred_parent_slot + 1;
                    }
                    continue;
                }
            }

            for (;;) {
                int blocked = 0;
                size_t neighbor_value_id;

                for (neighbor_value_id = 0; neighbor_value_id < result->value_count; ++neighbor_value_id) {
                    if (neighbor_value_id == value_id || !result->spill_flags[neighbor_value_id] ||
                        result->spill_slots[neighbor_value_id] != candidate_slot) {
                        continue;
                    }
                    if (interference_graph->interferes[value_id * interference_graph->value_count + neighbor_value_id]) {
                        blocked = 1;
                        break;
                    }
                }

                if (!blocked) {
                    result->spill_slots[value_id] = candidate_slot;
                    if (candidate_slot + 1 > result->spill_slot_count) {
                        result->spill_slot_count = candidate_slot + 1;
                    }
                    break;
                }
                ++candidate_slot;
            }
        }
    }

    return 1;
}

static int value_ssa_alloc_try_evict_lower_priority_neighbor(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t value_priority,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color,
    ValueSsaError *error) {
    size_t neighbor_value_id;
    int found = 0;
    size_t *color_blocker_counts = NULL;
    size_t *color_blocker_ids = NULL;
    ValueSsaAllocationFamilyBlockerCandidate *family_candidates = NULL;
    ValueSsaAllocationFamilyBlockerCandidate best_candidate;
    size_t alloc_count;

    if (!interference_graph || !result || !out_has_color || !out_color || value_id >= interference_graph->value_count ||
        value_id >= result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-015: invalid spill-eviction query");
        return 0;
    }

    *out_has_color = 0;
    *out_color = 0;
    if (out_blocker_value_id) {
        *out_blocker_value_id = (size_t)-1;
    }
    if (out_blocker_recolored) {
        *out_blocker_recolored = 0;
    }
    if (out_blocker_new_color) {
        *out_blocker_new_color = (size_t)-1;
    }
    alloc_count = color_budget ? color_budget : 1;
    color_blocker_counts = (size_t *)calloc(alloc_count, sizeof(size_t));
    color_blocker_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    family_candidates = (ValueSsaAllocationFamilyBlockerCandidate *)malloc(
        (result->value_count ? result->value_count : 1) * sizeof(ValueSsaAllocationFamilyBlockerCandidate));
    if (!color_blocker_counts || !color_blocker_ids || !family_candidates) {
        free(color_blocker_counts);
        free(color_blocker_ids);
        free(family_candidates);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-018: out of memory preparing eviction scan");
        return 0;
    }
    value_ssa_alloc_family_blocker_candidate_init(&best_candidate);

    for (neighbor_value_id = 0; neighbor_value_id < alloc_count; ++neighbor_value_id) {
        color_blocker_ids[neighbor_value_id] = (size_t)-1;
    }
    for (neighbor_value_id = 0; neighbor_value_id < result->value_count; ++neighbor_value_id) {
        value_ssa_alloc_family_blocker_candidate_init(&family_candidates[neighbor_value_id]);
    }
    for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
        if (!interference_graph->interferes[value_id * interference_graph->value_count + neighbor_value_id] ||
            neighbor_value_id >= result->value_count || !result->has_color[neighbor_value_id] ||
            result->spill_flags[neighbor_value_id] || result->colors[neighbor_value_id] >= color_budget) {
            continue;
        }
        ++color_blocker_counts[result->colors[neighbor_value_id]];
        color_blocker_ids[result->colors[neighbor_value_id]] = neighbor_value_id;
    }

    if (!value_ssa_alloc_collect_family_blocker_candidates_from_color_facts(interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            value_priority,
            color_blocker_counts,
            color_blocker_ids,
            color_budget,
            family_candidates,
            result->value_count,
            error)) {
        free(color_blocker_counts);
        free(color_blocker_ids);
        free(family_candidates);
        return 0;
    }

    if (!value_ssa_alloc_pick_best_family_blocker_candidate(interference_graph,
            prep,
            coalesce_analysis,
            plan,
            result,
            family_candidates,
            result->value_count,
            color_budget,
            &found,
            &best_candidate,
            error)) {
        free(color_blocker_counts);
        free(color_blocker_ids);
        free(family_candidates);
        return 0;
    }

    if (!found) {
        free(color_blocker_counts);
        free(color_blocker_ids);
        free(family_candidates);
        return 1;
    }

    {
        int recolored = 0;
        size_t best_priority = result->spill_priorities[best_candidate.blocker_value_id];

        if (!value_ssa_alloc_try_recolor_blocker(interference_graph,
                prep,
                coalesce_analysis,
                plan,
                result,
                best_candidate.blocker_value_id,
                best_candidate.blocked_color,
                color_budget,
                &recolored,
                error)) {
            free(color_blocker_counts);
            free(color_blocker_ids);
            free(family_candidates);
            return 0;
        }
        if (recolored) {
            *out_has_color = 1;
            *out_color = best_candidate.blocked_color;
            if (out_blocker_value_id) {
                *out_blocker_value_id = best_candidate.blocker_value_id;
            }
            if (out_blocker_recolored) {
                *out_blocker_recolored = 1;
            }
            if (out_blocker_new_color) {
                *out_blocker_new_color = result->colors[best_candidate.blocker_value_id];
            }
            free(color_blocker_counts);
            free(color_blocker_ids);
            free(family_candidates);
            return 1;
        }

        value_ssa_alloc_mark_evicted_value_spilled(result, best_candidate.blocker_value_id);
        result->spill_priorities[best_candidate.blocker_value_id] = best_priority;
        *out_has_color = 1;
        *out_color = best_candidate.blocked_color;
        if (out_blocker_value_id) {
            *out_blocker_value_id = best_candidate.blocker_value_id;
        }
    }
    free(color_blocker_counts);
    free(color_blocker_ids);
    free(family_candidates);
    return 1;
}

static int value_ssa_alloc_try_evict_lower_priority_neighbor_for_color(const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t value_priority,
    size_t requested_color,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color,
    ValueSsaError *error) {
    size_t neighbor_value_id;
    size_t blocker_count = 0;
    size_t blocker_value_id = (size_t)-1;
    size_t blocker_priority = 0;

    if (!interference_graph || !result || !out_has_color || !out_color || value_id >= interference_graph->value_count ||
        value_id >= result->value_count || requested_color >= color_budget) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-097: invalid targeted spill-eviction query");
        return 0;
    }

    *out_has_color = 0;
    *out_color = 0;
    if (out_blocker_value_id) {
        *out_blocker_value_id = (size_t)-1;
    }
    if (out_blocker_recolored) {
        *out_blocker_recolored = 0;
    }
    if (out_blocker_new_color) {
        *out_blocker_new_color = (size_t)-1;
    }
    for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
        if (!interference_graph->interferes[value_id * interference_graph->value_count + neighbor_value_id] ||
            neighbor_value_id >= result->value_count || !result->has_color[neighbor_value_id] ||
            result->spill_flags[neighbor_value_id] || result->colors[neighbor_value_id] != requested_color) {
            continue;
        }
        ++blocker_count;
        blocker_value_id = neighbor_value_id;
        blocker_priority = result->spill_priorities[neighbor_value_id];
        if (blocker_count > 1) {
            return 1;
        }
    }

    if (blocker_count != 1 || blocker_priority >= value_priority) {
        return 1;
    }

    {
        int recolored = 0;

        if (!value_ssa_alloc_try_recolor_blocker(interference_graph,
                prep,
                coalesce_analysis,
                plan,
                result,
                blocker_value_id,
                requested_color,
                color_budget,
                &recolored,
                error)) {
            return 0;
        }
        if (recolored) {
            *out_has_color = 1;
            *out_color = requested_color;
            if (out_blocker_value_id) {
                *out_blocker_value_id = blocker_value_id;
            }
            if (out_blocker_recolored) {
                *out_blocker_recolored = 1;
            }
            if (out_blocker_new_color) {
                *out_blocker_new_color = result->colors[blocker_value_id];
            }
            return 1;
        }
    }

    value_ssa_alloc_mark_evicted_value_spilled(result, blocker_value_id);
    result->spill_priorities[blocker_value_id] = blocker_priority;
    *out_has_color = 1;
    *out_color = requested_color;
    if (out_blocker_value_id) {
        *out_blocker_value_id = blocker_value_id;
    }
    return 1;
}

#endif
