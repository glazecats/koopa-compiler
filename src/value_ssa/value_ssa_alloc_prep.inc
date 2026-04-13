#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

void value_ssa_allocation_prep_init(ValueSsaAllocationPrep *prep) {
    if (!prep) {
        return;
    }

    prep->value_count = 0;
    prep->def_block_ids = NULL;
    prep->use_counts = NULL;
    prep->live_block_counts = NULL;
    prep->single_block_live_ranges = NULL;
    prep->live_block_offsets = NULL;
    prep->live_blocks = NULL;
    prep->live_blocks_count = 0;
    prep->interference_degrees = NULL;
    prep->affinity_sums = NULL;
    prep->move_related = NULL;
    prep->candidates = NULL;
    prep->candidate_count = 0;
}

void value_ssa_allocation_prep_free(ValueSsaAllocationPrep *prep) {
    if (!prep) {
        return;
    }

    free(prep->def_block_ids);
    free(prep->use_counts);
    free(prep->live_block_counts);
    free(prep->single_block_live_ranges);
    free(prep->live_block_offsets);
    free(prep->live_blocks);
    free(prep->interference_degrees);
    free(prep->affinity_sums);
    free(prep->move_related);
    free(prep->candidates);
    value_ssa_allocation_prep_init(prep);
}

static int value_ssa_compare_copy_affinity_candidates(const void *lhs_ptr, const void *rhs_ptr) {
    const ValueSsaCopyAffinityCandidate *lhs = (const ValueSsaCopyAffinityCandidate *)lhs_ptr;
    const ValueSsaCopyAffinityCandidate *rhs = (const ValueSsaCopyAffinityCandidate *)rhs_ptr;

    if (lhs->weight > rhs->weight) {
        return -1;
    }
    if (lhs->weight < rhs->weight) {
        return 1;
    }
    if (lhs->lhs < rhs->lhs) {
        return -1;
    }
    if (lhs->lhs > rhs->lhs) {
        return 1;
    }
    if (lhs->rhs < rhs->rhs) {
        return -1;
    }
    if (lhs->rhs > rhs->rhs) {
        return 1;
    }
    return 0;
}

int value_ssa_compute_allocation_prep(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use_analysis,
    const ValueSsaLivenessAnalysis *liveness_analysis,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *copy_affinity_graph,
    ValueSsaAllocationPrep *prep,
    ValueSsaError *error) {
    ValueSsaDefUseAnalysis owned_def_use;
    ValueSsaLivenessAnalysis owned_liveness;
    ValueSsaInterferenceGraph owned_interference;
    ValueSsaCopyAffinityGraph owned_affinity;
    const ValueSsaDefUseAnalysis *active_def_use = def_use_analysis;
    const ValueSsaLivenessAnalysis *active_liveness = liveness_analysis;
    const ValueSsaInterferenceGraph *active_interference = interference_graph;
    const ValueSsaCopyAffinityGraph *active_affinity = copy_affinity_graph;
    unsigned char *covered_blocks = NULL;
    int ok = 0;

    if (!function || !prep) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-214: invalid allocation-prep contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-215: cannot compute allocation prep for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    value_ssa_allocation_prep_free(prep);
    prep->value_count = function->next_value_id;
    if (prep->value_count == 0) {
        return 1;
    }

    value_ssa_def_use_analysis_init(&owned_def_use);
    value_ssa_liveness_analysis_init(&owned_liveness);
    value_ssa_interference_graph_init(&owned_interference);
    value_ssa_copy_affinity_graph_init(&owned_affinity);

    if (!active_def_use) {
        if (!value_ssa_compute_def_use_analysis(function, &owned_def_use, error)) {
            goto cleanup;
        }
        active_def_use = &owned_def_use;
    } else if (active_def_use->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-216: allocation-prep def-use value_count=%zu does not match function value_count=%zu",
            active_def_use->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_liveness) {
        if (!value_ssa_compute_liveness_analysis(function, NULL, &owned_liveness, error)) {
            goto cleanup;
        }
        active_liveness = &owned_liveness;
    } else if (active_liveness->block_count != function->block_count ||
        active_liveness->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-217: allocation-prep liveness shape (%zu blocks, %zu values) does not match function (%zu blocks, %zu values)",
            active_liveness->block_count,
            active_liveness->value_count,
            function->block_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_interference) {
        if (!value_ssa_compute_interference_graph(
                function, NULL, active_liveness, &owned_interference, error)) {
            goto cleanup;
        }
        active_interference = &owned_interference;
    } else if (active_interference->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-218: allocation-prep interference value_count=%zu does not match function value_count=%zu",
            active_interference->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_affinity) {
        if (!value_ssa_compute_copy_affinity_graph(
                function, active_interference, &owned_affinity, error)) {
            goto cleanup;
        }
        active_affinity = &owned_affinity;
    } else if (active_affinity->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-219: allocation-prep affinity value_count=%zu does not match function value_count=%zu",
            active_affinity->value_count,
            function->next_value_id);
        goto cleanup;
    }

    prep->def_block_ids = (size_t *)malloc(prep->value_count * sizeof(size_t));
    prep->use_counts = (size_t *)malloc(prep->value_count * sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->single_block_live_ranges = (unsigned char *)calloc(prep->value_count, sizeof(unsigned char));
    prep->live_block_offsets = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(prep->value_count, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(prep->value_count, sizeof(unsigned char));
    covered_blocks = (unsigned char *)calloc(prep->value_count * function->block_count, sizeof(unsigned char));
    if (!prep->def_block_ids || !prep->use_counts || !prep->live_block_counts || !prep->single_block_live_ranges ||
        !prep->live_block_offsets || !prep->interference_degrees || !prep->affinity_sums ||
        !prep->move_related || !covered_blocks) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-220: out of memory while preparing allocation summary");
        goto cleanup;
    }

    {
        size_t value_index;
        size_t total_live_blocks = 0;

        for (value_index = 0; value_index < prep->value_count; ++value_index) {
            size_t block_index;
            size_t degree = 0;
            size_t affinity_sum = 0;
            size_t live_block_count = 0;

            prep->def_block_ids[value_index] = active_def_use->has_def[value_index]
                ? active_def_use->def_block_ids[value_index]
                : (size_t)-1;
            prep->use_counts[value_index] = active_def_use->use_counts[value_index];

            if (prep->def_block_ids[value_index] != (size_t)-1 &&
                prep->def_block_ids[value_index] < function->block_count) {
                covered_blocks[value_index * function->block_count + prep->def_block_ids[value_index]] = 1;
            }

            for (block_index = 0; block_index < function->block_count; ++block_index) {
                size_t slot = block_index * prep->value_count + value_index;
                size_t cover_slot = value_index * function->block_count + block_index;

                if (active_liveness->live_in[slot] || active_liveness->live_out[slot]) {
                    covered_blocks[cover_slot] = 1;
                }

                if (covered_blocks[cover_slot]) {
                    ++live_block_count;
                }
            }

            for (block_index = 0; block_index < prep->value_count; ++block_index) {
                degree += active_interference->interferes[value_index * prep->value_count + block_index];
                affinity_sum += active_affinity->weights[value_index * prep->value_count + block_index];
            }

            prep->live_block_counts[value_index] = live_block_count;
            prep->single_block_live_ranges[value_index] = live_block_count <= 1 ? 1u : 0u;
            prep->interference_degrees[value_index] = degree;
            prep->affinity_sums[value_index] = affinity_sum;
            prep->move_related[value_index] = affinity_sum > 0 ? 1u : 0u;
            prep->live_block_offsets[value_index] = total_live_blocks;
            total_live_blocks += live_block_count;
        }

        prep->live_blocks_count = total_live_blocks;
        if (total_live_blocks > 0) {
            prep->live_blocks = (size_t *)malloc(total_live_blocks * sizeof(size_t));
            if (!prep->live_blocks) {
                value_ssa_set_error(error, 0, 0, "VALUE-SSA-221: out of memory while materializing live-block lists");
                goto cleanup;
            }

            for (value_index = 0; value_index < prep->value_count; ++value_index) {
                size_t block_index;
                size_t write_index = prep->live_block_offsets[value_index];

                for (block_index = 0; block_index < function->block_count; ++block_index) {
                    if (covered_blocks[value_index * function->block_count + block_index]) {
                        prep->live_blocks[write_index++] = block_index;
                    }
                }
            }
        }
    }

    {
        size_t lhs;

        for (lhs = 0; lhs < prep->value_count; ++lhs) {
            size_t rhs;

            for (rhs = lhs + 1; rhs < prep->value_count; ++rhs) {
                if (active_affinity->weights[lhs * prep->value_count + rhs] > 0) {
                    ++prep->candidate_count;
                }
            }
        }
    }

    if (prep->candidate_count > 0) {
        size_t lhs;
        size_t write_index = 0;

        prep->candidates = (ValueSsaCopyAffinityCandidate *)calloc(
            prep->candidate_count, sizeof(ValueSsaCopyAffinityCandidate));
        if (!prep->candidates) {
            value_ssa_set_error(error, 0, 0, "VALUE-SSA-222: out of memory while materializing affinity candidates");
            goto cleanup;
        }

        for (lhs = 0; lhs < prep->value_count; ++lhs) {
            size_t rhs;

            for (rhs = lhs + 1; rhs < prep->value_count; ++rhs) {
                size_t weight = active_affinity->weights[lhs * prep->value_count + rhs];

                if (weight == 0) {
                    continue;
                }

                prep->candidates[write_index].lhs = lhs;
                prep->candidates[write_index].rhs = rhs;
                prep->candidates[write_index].weight = weight;
                ++write_index;
            }
        }

        qsort(prep->candidates,
            prep->candidate_count,
            sizeof(ValueSsaCopyAffinityCandidate),
            value_ssa_compare_copy_affinity_candidates);
    }

    ok = 1;

cleanup:
    free(covered_blocks);
    value_ssa_def_use_analysis_free(&owned_def_use);
    value_ssa_liveness_analysis_free(&owned_liveness);
    value_ssa_interference_graph_free(&owned_interference);
    value_ssa_copy_affinity_graph_free(&owned_affinity);
    if (!ok) {
        value_ssa_allocation_prep_free(prep);
    }
    return ok;
}

#endif
