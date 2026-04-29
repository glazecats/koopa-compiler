#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

static void value_ssa_alloc_compute_spill_cost_components(const ValueSsaAllocationPrep *prep,
    size_t value_id,
    size_t *out_use_cost,
    size_t *out_use_loop_depth_cost,
    size_t *out_use_call_density_cost,
    size_t *out_live_range_cost,
    size_t *out_live_block_cost,
    size_t *out_loop_depth_cost,
    size_t *out_call_density_cost,
    size_t *out_call_crossing_cost,
    size_t *out_cross_block_cost,
    size_t *out_affinity_cost,
    size_t *out_total_cost) {
    size_t use_cost = 0;
    size_t use_loop_depth_cost = 0;
    size_t use_call_density_cost = 0;
    size_t live_range_cost = 0;
    size_t live_block_cost = 0;
    size_t loop_depth_cost = 0;
    size_t call_density_cost = 0;
    size_t call_crossing_cost = 0;
    size_t cross_block_cost = 0;
    size_t affinity_cost = 0;

    if (prep && value_id < prep->value_count) {
        use_cost = prep->use_counts[value_id];
        use_loop_depth_cost = prep->use_loop_depth_sums ? prep->use_loop_depth_sums[value_id] : 0;
        use_call_density_cost = prep->use_call_density_sums ? prep->use_call_density_sums[value_id] : 0;
        live_block_cost = prep->live_block_counts[value_id];
        loop_depth_cost = prep->loop_depth_sums ? prep->loop_depth_sums[value_id] : 0;
        call_density_cost = prep->call_density_sums ? prep->call_density_sums[value_id] : 0;
        call_crossing_cost = prep->call_crossing_counts ? prep->call_crossing_counts[value_id] : 0;
        cross_block_cost = value_ssa_alloc_cross_block_spill_penalty(prep, value_id);
        live_range_cost =
            live_block_cost + loop_depth_cost + call_density_cost + call_crossing_cost + cross_block_cost;
        affinity_cost = prep->affinity_sums[value_id];
    }

    if (out_use_cost) {
        *out_use_cost = use_cost;
    }
    if (out_use_loop_depth_cost) {
        *out_use_loop_depth_cost = use_loop_depth_cost;
    }
    if (out_use_call_density_cost) {
        *out_use_call_density_cost = use_call_density_cost;
    }
    if (out_live_range_cost) {
        *out_live_range_cost = live_range_cost;
    }
    if (out_live_block_cost) {
        *out_live_block_cost = live_block_cost;
    }
    if (out_loop_depth_cost) {
        *out_loop_depth_cost = loop_depth_cost;
    }
    if (out_call_density_cost) {
        *out_call_density_cost = call_density_cost;
    }
    if (out_call_crossing_cost) {
        *out_call_crossing_cost = call_crossing_cost;
    }
    if (out_cross_block_cost) {
        *out_cross_block_cost = cross_block_cost;
    }
    if (out_affinity_cost) {
        *out_affinity_cost = affinity_cost;
    }
    if (out_total_cost) {
        *out_total_cost = use_cost + use_loop_depth_cost + use_call_density_cost + live_range_cost + affinity_cost;
    }
}

#endif
