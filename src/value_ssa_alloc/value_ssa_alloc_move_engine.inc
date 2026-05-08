#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

typedef struct {
    size_t root_value_id;
    ValueSsaAllocatorMoveWorkClass work_class;
    size_t move_work_priority;
    size_t simplify_queue_priority;
    size_t freeze_queue_priority;
    size_t spill_queue_priority;
    size_t family_active_member_count;
    size_t family_external_neighbor_count;
    size_t family_coalesce_ready_neighbor_count;
    size_t family_coalesce_ready_affinity_weight_sum;
    size_t family_best_coalesce_ready_partner_root_value_id;
    size_t family_best_coalesce_ready_partner_affinity_weight;
    unsigned char family_best_coalesce_ready_partner_is_mutual;
    size_t family_external_affinity_weight_sum;
    unsigned char has_simplify;
    unsigned char has_freeze;
    unsigned char has_spill;
    ValueSsaAllocatorPlanItem simplify_candidate;
    ValueSsaAllocatorPlanItem freeze_candidate;
    ValueSsaAllocatorPlanItem spill_candidate;
} ValueSsaAllocatorMoveEngineFamilyPhaseEntry;

typedef struct {
    size_t *active_degrees;
    size_t *active_effective_degrees;
    ValueSsaAllocatorPlanItem *work_item_preview_bases;
    ValueSsaAllocationFamilyRuntimeFacts *family_runtime_facts;
    size_t *active_family_spill_removed_counts;
    size_t *active_family_member_counts;
    size_t *active_family_external_neighbor_counts;
    size_t *active_family_coalesce_ready_neighbor_counts;
    size_t *active_family_coalesce_ready_affinity_sums;
    size_t *active_family_best_coalesce_ready_partner_roots;
    size_t *active_family_best_coalesce_ready_partner_weights;
    unsigned char *active_family_best_coalesce_ready_partner_mutual_flags;
    size_t *active_family_external_affinity_sums;
    size_t *active_family_queue_priorities;
    size_t *scratch_roots;
    unsigned char *scratch_root_flags;
    ValueSsaAllocatorMoveWorkClass *active_family_move_work_classes;
    unsigned char *active_move_related;
    unsigned char *active_effective_move_related;
    size_t *interference_neighbor_offsets;
    size_t *interference_neighbor_value_ids;
    size_t *affinity_neighbor_offsets;
    size_t *affinity_neighbor_value_ids;
    size_t *affinity_neighbor_weights;
    unsigned char *family_neighbor_matrix;
    unsigned char *family_coalesce_ready_neighbor_matrix;
    size_t *family_coalesce_ready_affinity_matrix;
    unsigned char *consumed_family_coalesce_pair_matrix;
    unsigned char *scratch_class_move_related;
    size_t *runtime_coalesce_roots;
    size_t *runtime_coalesce_class_sizes;
} ValueSsaAllocatorMoveEngineRuntimeState;

typedef struct {
    size_t value_count;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *family_phase_entries;
    size_t *active_family_roots;
    size_t active_family_count;
    unsigned char *affected_family_root_flags;
    size_t *affected_family_roots;
    size_t affected_family_count;
    unsigned char *simplify_family_root_flags;
    size_t *simplify_family_roots;
    size_t simplify_family_count;
    unsigned char *freeze_family_root_flags;
    size_t *freeze_family_roots;
    size_t freeze_family_count;
    unsigned char *spill_family_root_flags;
    size_t *spill_family_roots;
    size_t spill_family_count;
} ValueSsaAllocatorMoveEngineCurrentQueues;

typedef struct {
    size_t value_count;
    size_t member_item_count;
    size_t *root_member_offsets;
    size_t *root_member_counts;
    size_t *member_item_indices;
} ValueSsaAllocatorMoveEngineFamilyMembers;

typedef struct {
    int found;
    ValueSsaAllocatorPhase phase;
    size_t lhs_root_value_id;
    size_t rhs_root_value_id;
    size_t lhs_value_id;
    size_t rhs_value_id;
    size_t priority;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry lhs_entry;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry rhs_entry;
} ValueSsaAllocatorMoveEngineCoalesceAction;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_NONE = 0,
    VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE,
    VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE,
    VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE,
} ValueSsaAllocatorMoveEngineDecisionKind;

typedef struct {
    ValueSsaAllocatorMoveEngineDecisionKind kind;
    ValueSsaAllocatorPhase phase;
    ValueSsaAllocatorRemovalKind removal_kind;
    unsigned char has_candidate_entry;
    size_t candidate_root_value_id;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry candidate_entry;
    ValueSsaAllocatorPlanItem candidate;
    ValueSsaAllocatorMoveEngineCoalesceAction coalesce_action;
} ValueSsaAllocatorMoveEngineDecision;

typedef struct {
    const ValueSsaFunction *function;
    const ValueSsaAllocationPrep *prep;
    const ValueSsaAllocationWorklist *worklist;
    const ValueSsaInterferenceGraph *interference_graph;
    const ValueSsaCopyAffinityGraph *affinity_graph;
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis;
    const size_t *initial_effective_degrees;
    const unsigned char *initial_family_move_related;
    const unsigned char *initial_effective_move_related;
    unsigned char *scheduled;
    unsigned char *frozen;
    unsigned char *was_frozen;
    size_t color_budget;
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state;
    ValueSsaAllocatorMoveEngineCurrentQueues *queues;
    ValueSsaAllocatorMoveEngineFamilyMembers *family_members;
    ValueSsaAllocatorPlan *out_plan;
    ValueSsaAllocatorMoveTransitionTrace *out_trace;
    ValueSsaAllocatorMoveEngineRunResult *out_run_result;
    size_t *trace_index;
    size_t output_index;
    ValueSsaError *error;
} ValueSsaAllocatorMoveEngineExecutionContext;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_ENGINE_STEP_CONTINUE = 0,
    VALUE_SSA_ALLOC_MOVE_ENGINE_STEP_COMPLETED_PLAN_ITEM,
} ValueSsaAllocatorMoveEngineStepStatus;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_NONE = 0,
    VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_CONTINUE,
    VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_COMPLETED,
} ValueSsaAllocatorMoveEnginePlanItemStatus;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_NONE = 0,
    VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_GENERAL,
    VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_COALESCE_PAIR,
} ValueSsaAllocatorMoveEngineRefreshKind;

typedef struct {
    ValueSsaAllocatorMoveEngineStepStatus status;
    ValueSsaAllocatorMoveEngineDecisionKind decision_kind;
    ValueSsaAllocatorPhase phase;
    ValueSsaAllocatorRemovalKind removal_kind;
    size_t value_id;
    size_t root_value_id;
    ValueSsaAllocatorMoveEngineRefreshKind refresh_kind;
    size_t transition_value_id;
    unsigned char transition_is_freeze;
    unsigned char transition_is_spill_remove;
    size_t partner_root_value_id;
    size_t partner_value_id;
    size_t coalesce_priority;
    unsigned char append_trace;
    unsigned char record_plan_item;
    unsigned char has_partner_entry;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry before_entry;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry before_partner_entry;
    ValueSsaAllocatorPlanItem plan_item;
} ValueSsaAllocatorMoveEngineStepResult;

typedef struct {
    ValueSsaAllocatorMoveEnginePlanItemStatus status;
    size_t completed_step_count;
} ValueSsaAllocatorMoveEnginePlanItemResult;

typedef struct {
    size_t representative_root_value_id;
    size_t spill_removed_count;
    size_t active_member_count;
    size_t external_neighbor_count;
    size_t coalesce_ready_neighbor_count;
    size_t coalesce_ready_affinity_weight_sum;
    size_t best_coalesce_ready_partner_root_value_id;
    size_t best_coalesce_ready_partner_affinity_weight;
    unsigned char best_coalesce_ready_partner_is_mutual;
    size_t external_affinity_weight_sum;
    size_t queue_priority;
    ValueSsaAllocatorMoveWorkClass move_work_class;
} ValueSsaAllocatorMoveEngineFamilyStateView;

typedef struct {
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry;
    const ValueSsaAllocatorPlanItem *candidate;
    size_t representative_root_value_id;
    int has_pair;
    size_t pair_priority;
} ValueSsaAllocatorMoveEnginePhaseCandidateView;

typedef struct {
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *lhs_entry;
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *rhs_entry;
    const ValueSsaAllocatorPlanItem *lhs_candidate;
    const ValueSsaAllocatorPlanItem *rhs_candidate;
    size_t lhs_representative_root_value_id;
    size_t rhs_representative_root_value_id;
    size_t pair_priority;
} ValueSsaAllocatorMoveEnginePhasePairView;

static size_t value_ssa_alloc_move_engine_compute_family_queue_priority(size_t base_priority,
    size_t active_family_member_count,
    size_t family_coalesce_ready_neighbor_count,
    size_t family_coalesce_ready_affinity_weight_sum,
    size_t family_best_coalesce_ready_partner_affinity_weight,
    int family_best_coalesce_ready_partner_is_mutual,
    size_t family_external_neighbor_count,
    size_t family_external_affinity_weight_sum) {
    return base_priority + (family_coalesce_ready_neighbor_count * 16) +
        (family_coalesce_ready_affinity_weight_sum * 8) + (family_best_coalesce_ready_partner_affinity_weight * 4) +
        (family_best_coalesce_ready_partner_is_mutual ? 8u : 0u) + (family_external_affinity_weight_sum * 8) +
        (family_external_neighbor_count * 4) + (active_family_member_count * 2);
}

static int value_ssa_alloc_move_engine_trace_ensure_capacity(ValueSsaAllocatorMoveTransitionTrace *trace,
    size_t required_step_count) {
    ValueSsaAllocatorMoveTransitionStep *resized_steps;
    size_t new_capacity;

    if (!trace) {
        return 0;
    }
    if (required_step_count <= trace->step_count) {
        return 1;
    }

    new_capacity = trace->step_count ? trace->step_count : 4;
    while (new_capacity < required_step_count) {
        size_t grown_capacity = new_capacity * 2;

        if (grown_capacity <= new_capacity) {
            new_capacity = required_step_count;
            break;
        }
        new_capacity = grown_capacity;
    }

    resized_steps = (ValueSsaAllocatorMoveTransitionStep *)realloc(trace->steps, new_capacity * sizeof(*resized_steps));
    if (!resized_steps) {
        return 0;
    }

    memset(resized_steps + trace->step_count,
        0,
        (new_capacity - trace->step_count) * sizeof(*resized_steps));
    trace->steps = resized_steps;
    trace->step_count = new_capacity;
    return 1;
}

static int value_ssa_alloc_move_engine_append_trace_step(ValueSsaAllocatorMoveTransitionTrace *trace,
    size_t step_index,
    size_t value_id,
    size_t root_value_id,
    size_t partner_value_id,
    size_t partner_root_value_id,
    ValueSsaAllocatorMoveTransitionEventKind event_kind,
    ValueSsaAllocatorPhase phase,
    ValueSsaAllocatorRemovalKind removal_kind,
    ValueSsaAllocatorMoveWorkClass move_work_class_before,
    ValueSsaAllocatorMoveWorkClass move_work_class_after,
    size_t family_active_member_count_before,
    size_t family_active_member_count_after,
    size_t family_external_neighbor_count_before,
    size_t family_coalesce_ready_neighbor_count_before,
    size_t family_coalesce_ready_affinity_weight_sum_before,
    size_t family_best_coalesce_ready_partner_root_value_id_before,
    size_t family_best_coalesce_ready_partner_affinity_weight_before,
    int family_best_coalesce_ready_partner_is_mutual_before,
    size_t family_external_neighbor_count_after,
    size_t family_coalesce_ready_neighbor_count_after,
    size_t family_coalesce_ready_affinity_weight_sum_after,
    size_t family_best_coalesce_ready_partner_root_value_id_after,
    size_t family_best_coalesce_ready_partner_affinity_weight_after,
    int family_best_coalesce_ready_partner_is_mutual_after,
    size_t family_external_affinity_weight_sum_before,
    size_t family_external_affinity_weight_sum_after,
    ValueSsaAllocatorMoveWorkClass partner_move_work_class_before,
    ValueSsaAllocatorMoveWorkClass partner_move_work_class_after,
    size_t partner_family_active_member_count_before,
    size_t partner_family_active_member_count_after,
    size_t partner_family_external_neighbor_count_before,
    size_t partner_family_coalesce_ready_neighbor_count_before,
    size_t partner_family_coalesce_ready_affinity_weight_sum_before,
    size_t partner_family_best_coalesce_ready_partner_root_value_id_before,
    size_t partner_family_best_coalesce_ready_partner_affinity_weight_before,
    int partner_family_best_coalesce_ready_partner_is_mutual_before,
    size_t partner_family_external_neighbor_count_after,
    size_t partner_family_coalesce_ready_neighbor_count_after,
    size_t partner_family_coalesce_ready_affinity_weight_sum_after,
    size_t partner_family_best_coalesce_ready_partner_root_value_id_after,
    size_t partner_family_best_coalesce_ready_partner_affinity_weight_after,
    int partner_family_best_coalesce_ready_partner_is_mutual_after,
    size_t partner_family_external_affinity_weight_sum_before,
    size_t partner_family_external_affinity_weight_sum_after,
    size_t move_work_priority) {
    ValueSsaAllocatorMoveTransitionStep *step;

    if (!trace) {
        return 0;
    }
    if (!value_ssa_alloc_move_engine_trace_ensure_capacity(trace, step_index + 1)) {
        return 0;
    }

    step = &trace->steps[step_index];
    step->step_index = step_index;
    step->value_id = value_id;
    step->root_value_id = root_value_id;
    step->partner_value_id = partner_value_id;
    step->partner_root_value_id = partner_root_value_id;
    step->event_kind = event_kind;
    step->phase = phase;
    step->removal_kind = removal_kind;
    step->move_work_class_before = move_work_class_before;
    step->move_work_class_after = move_work_class_after;
    step->family_active_member_count_before = family_active_member_count_before;
    step->family_active_member_count_after = family_active_member_count_after;
    step->family_external_neighbor_count_before = family_external_neighbor_count_before;
    step->family_coalesce_ready_neighbor_count_before = family_coalesce_ready_neighbor_count_before;
    step->family_coalesce_ready_affinity_weight_sum_before = family_coalesce_ready_affinity_weight_sum_before;
    step->family_best_coalesce_ready_partner_root_value_id_before =
        family_best_coalesce_ready_partner_root_value_id_before;
    step->family_best_coalesce_ready_partner_affinity_weight_before =
        family_best_coalesce_ready_partner_affinity_weight_before;
    step->family_best_coalesce_ready_partner_is_mutual_before =
        family_best_coalesce_ready_partner_is_mutual_before ? 1u : 0u;
    step->family_external_neighbor_count_after = family_external_neighbor_count_after;
    step->family_coalesce_ready_neighbor_count_after = family_coalesce_ready_neighbor_count_after;
    step->family_coalesce_ready_affinity_weight_sum_after = family_coalesce_ready_affinity_weight_sum_after;
    step->family_best_coalesce_ready_partner_root_value_id_after =
        family_best_coalesce_ready_partner_root_value_id_after;
    step->family_best_coalesce_ready_partner_affinity_weight_after =
        family_best_coalesce_ready_partner_affinity_weight_after;
    step->family_best_coalesce_ready_partner_is_mutual_after =
        family_best_coalesce_ready_partner_is_mutual_after ? 1u : 0u;
    step->family_external_affinity_weight_sum_before = family_external_affinity_weight_sum_before;
    step->family_external_affinity_weight_sum_after = family_external_affinity_weight_sum_after;
    step->partner_move_work_class_before = partner_move_work_class_before;
    step->partner_move_work_class_after = partner_move_work_class_after;
    step->partner_family_active_member_count_before = partner_family_active_member_count_before;
    step->partner_family_active_member_count_after = partner_family_active_member_count_after;
    step->partner_family_external_neighbor_count_before = partner_family_external_neighbor_count_before;
    step->partner_family_coalesce_ready_neighbor_count_before = partner_family_coalesce_ready_neighbor_count_before;
    step->partner_family_coalesce_ready_affinity_weight_sum_before =
        partner_family_coalesce_ready_affinity_weight_sum_before;
    step->partner_family_best_coalesce_ready_partner_root_value_id_before =
        partner_family_best_coalesce_ready_partner_root_value_id_before;
    step->partner_family_best_coalesce_ready_partner_affinity_weight_before =
        partner_family_best_coalesce_ready_partner_affinity_weight_before;
    step->partner_family_best_coalesce_ready_partner_is_mutual_before =
        partner_family_best_coalesce_ready_partner_is_mutual_before ? 1u : 0u;
    step->partner_family_external_neighbor_count_after = partner_family_external_neighbor_count_after;
    step->partner_family_coalesce_ready_neighbor_count_after = partner_family_coalesce_ready_neighbor_count_after;
    step->partner_family_coalesce_ready_affinity_weight_sum_after =
        partner_family_coalesce_ready_affinity_weight_sum_after;
    step->partner_family_best_coalesce_ready_partner_root_value_id_after =
        partner_family_best_coalesce_ready_partner_root_value_id_after;
    step->partner_family_best_coalesce_ready_partner_affinity_weight_after =
        partner_family_best_coalesce_ready_partner_affinity_weight_after;
    step->partner_family_best_coalesce_ready_partner_is_mutual_after =
        partner_family_best_coalesce_ready_partner_is_mutual_after ? 1u : 0u;
    step->partner_family_external_affinity_weight_sum_before = partner_family_external_affinity_weight_sum_before;
    step->partner_family_external_affinity_weight_sum_after = partner_family_external_affinity_weight_sum_after;
    step->move_work_priority = move_work_priority;
    return 1;
}

static int value_ssa_alloc_move_engine_family_pair_is_consumed(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id) {
    if (!runtime_state || !runtime_state->consumed_family_coalesce_pair_matrix ||
        lhs_root_value_id >= value_count || rhs_root_value_id >= value_count ||
        lhs_root_value_id == rhs_root_value_id) {
        return 0;
    }

    return runtime_state->consumed_family_coalesce_pair_matrix[lhs_root_value_id * value_count + rhs_root_value_id] != 0;
}

static void value_ssa_alloc_move_engine_consume_family_pair(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id) {
    if (!runtime_state || !runtime_state->consumed_family_coalesce_pair_matrix ||
        lhs_root_value_id >= value_count || rhs_root_value_id >= value_count ||
        lhs_root_value_id == rhs_root_value_id) {
        return;
    }

    runtime_state->consumed_family_coalesce_pair_matrix[lhs_root_value_id * value_count + rhs_root_value_id] = 1u;
    runtime_state->consumed_family_coalesce_pair_matrix[rhs_root_value_id * value_count + lhs_root_value_id] = 1u;
}

static void value_ssa_alloc_move_engine_seed_initial_family_move_related(
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    const unsigned char *initial_effective_move_related,
    unsigned char *initial_family_move_related) {
    size_t value_id;

    if (!initial_effective_move_related || !initial_family_move_related) {
        return;
    }

    memset(initial_family_move_related, 0, (value_count ? value_count : 1) * sizeof(unsigned char));
    for (value_id = 0; value_id < value_count; ++value_id) {
        size_t root_value_id = value_ssa_alloc_plan_effective_root(coalesce_analysis, value_count, value_id);

        if (initial_effective_move_related[value_id]) {
            initial_family_move_related[root_value_id] = 1;
        }
    }
}

static void value_ssa_alloc_move_engine_seed_runtime_coalesce_families(
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t *runtime_coalesce_roots,
    size_t *runtime_coalesce_class_sizes) {
    size_t value_id;

    if (!runtime_coalesce_roots || !runtime_coalesce_class_sizes) {
        return;
    }

    if (value_ssa_alloc_coalesce_analysis_is_usable(coalesce_analysis, value_count)) {
        memcpy(runtime_coalesce_roots, coalesce_analysis->value_roots, value_count * sizeof(size_t));
        memcpy(runtime_coalesce_class_sizes, coalesce_analysis->class_sizes, value_count * sizeof(size_t));
        return;
    }

    for (value_id = 0; value_id < value_count; ++value_id) {
        runtime_coalesce_roots[value_id] = value_id;
        runtime_coalesce_class_sizes[value_id] = 1;
    }
}

static int value_ssa_alloc_move_engine_build_affinity_neighbors(const ValueSsaAllocationPrep *prep,
    size_t value_count,
    size_t **out_offsets,
    size_t **out_neighbor_value_ids,
    size_t **out_neighbor_weights) {
    size_t *offsets = NULL;
    size_t *neighbor_value_ids = NULL;
    size_t *neighbor_weights = NULL;
    size_t *write_offsets = NULL;
    size_t candidate_index;

    if (out_offsets) {
        *out_offsets = NULL;
    }
    if (out_neighbor_value_ids) {
        *out_neighbor_value_ids = NULL;
    }
    if (out_neighbor_weights) {
        *out_neighbor_weights = NULL;
    }
    if (!prep || !out_offsets || !out_neighbor_value_ids || !out_neighbor_weights) {
        return 0;
    }

    offsets = (size_t *)calloc((value_count ? value_count : 1) + 1, sizeof(size_t));
    write_offsets = (size_t *)calloc((value_count ? value_count : 1), sizeof(size_t));
    if (!offsets || !write_offsets) {
        free(offsets);
        free(write_offsets);
        return 0;
    }

    for (candidate_index = 0; candidate_index < prep->candidate_count; ++candidate_index) {
        const ValueSsaCopyAffinityCandidate *candidate = &prep->candidates[candidate_index];

        if (candidate->lhs >= value_count || candidate->rhs >= value_count || candidate->weight == 0) {
            continue;
        }
        ++offsets[candidate->lhs + 1];
        ++offsets[candidate->rhs + 1];
    }
    for (candidate_index = 1; candidate_index <= value_count; ++candidate_index) {
        offsets[candidate_index] += offsets[candidate_index - 1];
    }

    if (offsets[value_count] > 0) {
        neighbor_value_ids = (size_t *)malloc(offsets[value_count] * sizeof(size_t));
        neighbor_weights = (size_t *)malloc(offsets[value_count] * sizeof(size_t));
        if (!neighbor_value_ids || !neighbor_weights) {
            free(offsets);
            free(write_offsets);
            free(neighbor_value_ids);
            free(neighbor_weights);
            return 0;
        }
    }

    memcpy(write_offsets, offsets, (value_count ? value_count : 1) * sizeof(size_t));
    for (candidate_index = 0; candidate_index < prep->candidate_count; ++candidate_index) {
        const ValueSsaCopyAffinityCandidate *candidate = &prep->candidates[candidate_index];
        size_t lhs_index;
        size_t rhs_index;

        if (candidate->lhs >= value_count || candidate->rhs >= value_count || candidate->weight == 0) {
            continue;
        }

        lhs_index = write_offsets[candidate->lhs]++;
        neighbor_value_ids[lhs_index] = candidate->rhs;
        neighbor_weights[lhs_index] = candidate->weight;

        rhs_index = write_offsets[candidate->rhs]++;
        neighbor_value_ids[rhs_index] = candidate->lhs;
        neighbor_weights[rhs_index] = candidate->weight;
    }

    free(write_offsets);
    *out_offsets = offsets;
    *out_neighbor_value_ids = neighbor_value_ids;
    *out_neighbor_weights = neighbor_weights;
    return 1;
}

static int value_ssa_alloc_move_engine_build_interference_neighbors(const ValueSsaInterferenceGraph *interference_graph,
    size_t value_count,
    size_t **out_offsets,
    size_t **out_neighbor_value_ids) {
    size_t *offsets = NULL;
    size_t *neighbor_value_ids = NULL;
    size_t *write_offsets = NULL;
    size_t lhs_value_id;
    size_t rhs_value_id;

    if (out_offsets) {
        *out_offsets = NULL;
    }
    if (out_neighbor_value_ids) {
        *out_neighbor_value_ids = NULL;
    }
    if (!interference_graph || !out_offsets || !out_neighbor_value_ids ||
        !interference_graph->interferes || interference_graph->value_count != value_count) {
        return 0;
    }

    offsets = (size_t *)calloc((value_count ? value_count : 1) + 1, sizeof(size_t));
    write_offsets = (size_t *)calloc((value_count ? value_count : 1), sizeof(size_t));
    if (!offsets || !write_offsets) {
        free(offsets);
        free(write_offsets);
        return 0;
    }

    for (lhs_value_id = 0; lhs_value_id < value_count; ++lhs_value_id) {
        for (rhs_value_id = lhs_value_id + 1; rhs_value_id < value_count; ++rhs_value_id) {
            if (!interference_graph->interferes[lhs_value_id * value_count + rhs_value_id]) {
                continue;
            }
            ++offsets[lhs_value_id + 1];
            ++offsets[rhs_value_id + 1];
        }
    }
    for (lhs_value_id = 1; lhs_value_id <= value_count; ++lhs_value_id) {
        offsets[lhs_value_id] += offsets[lhs_value_id - 1];
    }

    if (offsets[value_count] > 0) {
        neighbor_value_ids = (size_t *)malloc(offsets[value_count] * sizeof(size_t));
        if (!neighbor_value_ids) {
            free(offsets);
            free(write_offsets);
            return 0;
        }
    }

    memcpy(write_offsets, offsets, (value_count ? value_count : 1) * sizeof(size_t));
    for (lhs_value_id = 0; lhs_value_id < value_count; ++lhs_value_id) {
        for (rhs_value_id = lhs_value_id + 1; rhs_value_id < value_count; ++rhs_value_id) {
            size_t lhs_index;
            size_t rhs_index;

            if (!interference_graph->interferes[lhs_value_id * value_count + rhs_value_id]) {
                continue;
            }

            lhs_index = write_offsets[lhs_value_id]++;
            neighbor_value_ids[lhs_index] = rhs_value_id;

            rhs_index = write_offsets[rhs_value_id]++;
            neighbor_value_ids[rhs_index] = lhs_value_id;
        }
    }

    free(write_offsets);
    *out_offsets = offsets;
    *out_neighbor_value_ids = neighbor_value_ids;
    return 1;
}

static size_t value_ssa_alloc_move_engine_effective_root(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t value_id) {
    if (runtime_state && runtime_state->runtime_coalesce_roots && value_id < value_count) {
        return runtime_state->runtime_coalesce_roots[value_id];
    }
    return value_ssa_alloc_plan_effective_root(coalesce_analysis, value_count, value_id);
}

static size_t value_ssa_alloc_move_engine_runtime_root_value_id(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id) {
    if (!runtime_state || !runtime_state->runtime_coalesce_roots || root_value_id >= value_count) {
        return root_value_id;
    }
    return runtime_state->runtime_coalesce_roots[root_value_id];
}

static size_t value_ssa_alloc_move_engine_family_active_member_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_spill_removed_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_coalesce_ready_neighbor_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_best_partner_root(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_best_partner_weight(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static int value_ssa_alloc_move_engine_family_best_partner_is_mutual(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_external_neighbor_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_coalesce_ready_affinity_sum(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_external_affinity_sum(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static size_t value_ssa_alloc_move_engine_family_queue_priority(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);
static ValueSsaAllocatorMoveWorkClass value_ssa_alloc_move_engine_family_move_work_class(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id);

static int value_ssa_alloc_move_engine_has_family_runtime_surface(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    return runtime_state != NULL && runtime_state->family_runtime_facts != NULL;
}

static void value_ssa_alloc_move_engine_clear_family_runtime_facts_entry(
    ValueSsaAllocationFamilyRuntimeFacts *facts, size_t value_count, size_t root_value_id) {
    if (!facts || root_value_id >= value_count) {
        return;
    }

    memset(&facts[root_value_id], 0, sizeof(facts[root_value_id]));
    facts[root_value_id].best_coalesce_ready_partner_root_value_id = (size_t)-1;
    facts[root_value_id].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
}

static void value_ssa_alloc_move_engine_sync_family_runtime_facts_for_root(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    ValueSsaAllocationFamilyRuntimeFacts *facts;
    size_t partner_root_value_id;

    if (!runtime_state || !runtime_state->family_runtime_facts || root_value_id >= value_count) {
        return;
    }

    facts = runtime_state->family_runtime_facts;
    value_ssa_alloc_move_engine_clear_family_runtime_facts_entry(facts, value_count, root_value_id);
    if (runtime_state->active_family_spill_removed_counts) {
        facts[root_value_id].spill_removed_count = runtime_state->active_family_spill_removed_counts[root_value_id];
    }
    if (runtime_state->active_family_member_counts) {
        facts[root_value_id].active_member_count = runtime_state->active_family_member_counts[root_value_id];
    }
    if (runtime_state->active_family_external_neighbor_counts) {
        facts[root_value_id].external_neighbor_count =
            runtime_state->active_family_external_neighbor_counts[root_value_id];
    }
    if (runtime_state->active_family_coalesce_ready_neighbor_counts) {
        facts[root_value_id].coalesce_ready_neighbor_count =
            runtime_state->active_family_coalesce_ready_neighbor_counts[root_value_id];
    }
    if (runtime_state->active_family_coalesce_ready_affinity_sums) {
        facts[root_value_id].coalesce_ready_affinity_weight_sum =
            runtime_state->active_family_coalesce_ready_affinity_sums[root_value_id];
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_roots) {
        facts[root_value_id].best_coalesce_ready_partner_root_value_id =
            runtime_state->active_family_best_coalesce_ready_partner_roots[root_value_id];
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_weights) {
        facts[root_value_id].best_coalesce_ready_partner_affinity_weight =
            runtime_state->active_family_best_coalesce_ready_partner_weights[root_value_id];
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_mutual_flags) {
        facts[root_value_id].best_coalesce_ready_partner_is_mutual =
            runtime_state->active_family_best_coalesce_ready_partner_mutual_flags[root_value_id];
    }
    if (runtime_state->active_family_external_affinity_sums) {
        facts[root_value_id].external_affinity_weight_sum =
            runtime_state->active_family_external_affinity_sums[root_value_id];
    }
    if (runtime_state->active_family_queue_priorities) {
        facts[root_value_id].queue_priority = runtime_state->active_family_queue_priorities[root_value_id];
    }
    if (runtime_state->active_family_move_work_classes) {
        facts[root_value_id].move_work_class = runtime_state->active_family_move_work_classes[root_value_id];
    }

    partner_root_value_id = facts[root_value_id].best_coalesce_ready_partner_root_value_id;
    if (partner_root_value_id != (size_t)-1) {
        partner_root_value_id =
            value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, partner_root_value_id);
        if (partner_root_value_id >= value_count || partner_root_value_id == root_value_id) {
            facts[root_value_id].best_coalesce_ready_partner_root_value_id = (size_t)-1;
            facts[root_value_id].best_coalesce_ready_partner_affinity_weight = 0;
            facts[root_value_id].best_coalesce_ready_partner_is_mutual = 0u;
        } else {
            facts[root_value_id].best_coalesce_ready_partner_root_value_id = partner_root_value_id;
        }
    }
}

static void value_ssa_alloc_move_engine_sync_family_runtime_facts_for_roots(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    const size_t *roots,
    size_t root_count,
    size_t value_count) {
    size_t root_index;

    if (!runtime_state || !roots) {
        return;
    }

    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id =
            value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, roots[root_index]);

        if (root_value_id >= value_count) {
            continue;
        }
        value_ssa_alloc_move_engine_sync_family_runtime_facts_for_root(runtime_state, value_count, root_value_id);
    }
}

static void value_ssa_alloc_move_engine_sync_all_family_runtime_facts(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count) {
    size_t root_value_id;

    if (!runtime_state) {
        return;
    }

    for (root_value_id = 0; root_value_id < value_count; ++root_value_id) {
        value_ssa_alloc_move_engine_sync_family_runtime_facts_for_root(runtime_state, value_count, root_value_id);
    }
}

static void value_ssa_alloc_move_engine_set_family_active_member_count(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t count) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_member_counts) {
        runtime_state->active_family_member_counts[root_value_id] = count;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].active_member_count = count;
    }
}

static void value_ssa_alloc_move_engine_set_family_spill_removed_count(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t count) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_spill_removed_counts) {
        runtime_state->active_family_spill_removed_counts[root_value_id] = count;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].spill_removed_count = count;
    }
}

static void value_ssa_alloc_move_engine_set_family_move_work_class(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    ValueSsaAllocatorMoveWorkClass move_work_class) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_move_work_classes) {
        runtime_state->active_family_move_work_classes[root_value_id] = move_work_class;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].move_work_class = move_work_class;
    }
}

static void value_ssa_alloc_move_engine_set_family_queue_priority(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t queue_priority) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_queue_priorities) {
        runtime_state->active_family_queue_priorities[root_value_id] = queue_priority;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].queue_priority = queue_priority;
    }
}

static void value_ssa_alloc_move_engine_reset_family_move_pressure_fields(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }

    if (runtime_state->active_family_external_neighbor_counts) {
        runtime_state->active_family_external_neighbor_counts[root_value_id] = 0;
    }
    if (runtime_state->active_family_coalesce_ready_neighbor_counts) {
        runtime_state->active_family_coalesce_ready_neighbor_counts[root_value_id] = 0;
    }
    if (runtime_state->active_family_coalesce_ready_affinity_sums) {
        runtime_state->active_family_coalesce_ready_affinity_sums[root_value_id] = 0;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_roots) {
        runtime_state->active_family_best_coalesce_ready_partner_roots[root_value_id] = (size_t)-1;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_weights) {
        runtime_state->active_family_best_coalesce_ready_partner_weights[root_value_id] = 0;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_mutual_flags) {
        runtime_state->active_family_best_coalesce_ready_partner_mutual_flags[root_value_id] = 0u;
    }
    if (runtime_state->active_family_external_affinity_sums) {
        runtime_state->active_family_external_affinity_sums[root_value_id] = 0;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].external_neighbor_count = 0;
        runtime_state->family_runtime_facts[root_value_id].coalesce_ready_neighbor_count = 0;
        runtime_state->family_runtime_facts[root_value_id].coalesce_ready_affinity_weight_sum = 0;
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_root_value_id = (size_t)-1;
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_affinity_weight = 0;
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_is_mutual = 0u;
        runtime_state->family_runtime_facts[root_value_id].external_affinity_weight_sum = 0;
    }
}

static void value_ssa_alloc_move_engine_add_family_external_affinity(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id, size_t weight) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_external_affinity_sums) {
        runtime_state->active_family_external_affinity_sums[root_value_id] += weight;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].external_affinity_weight_sum =
            runtime_state->active_family_external_affinity_sums
                ? runtime_state->active_family_external_affinity_sums[root_value_id]
                : runtime_state->family_runtime_facts[root_value_id].external_affinity_weight_sum + weight;
    }
}

static void value_ssa_alloc_move_engine_increment_family_external_neighbor_count(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_external_neighbor_counts) {
        ++runtime_state->active_family_external_neighbor_counts[root_value_id];
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].external_neighbor_count =
            runtime_state->active_family_external_neighbor_counts
                ? runtime_state->active_family_external_neighbor_counts[root_value_id]
                : runtime_state->family_runtime_facts[root_value_id].external_neighbor_count + 1;
    }
}

static void value_ssa_alloc_move_engine_increment_family_coalesce_ready_neighbor_count(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_coalesce_ready_neighbor_counts) {
        ++runtime_state->active_family_coalesce_ready_neighbor_counts[root_value_id];
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].coalesce_ready_neighbor_count =
            runtime_state->active_family_coalesce_ready_neighbor_counts
                ? runtime_state->active_family_coalesce_ready_neighbor_counts[root_value_id]
                : runtime_state->family_runtime_facts[root_value_id].coalesce_ready_neighbor_count + 1;
    }
}

static void value_ssa_alloc_move_engine_add_family_coalesce_ready_affinity(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id, size_t weight) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_coalesce_ready_affinity_sums) {
        runtime_state->active_family_coalesce_ready_affinity_sums[root_value_id] += weight;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].coalesce_ready_affinity_weight_sum =
            runtime_state->active_family_coalesce_ready_affinity_sums
                ? runtime_state->active_family_coalesce_ready_affinity_sums[root_value_id]
                : runtime_state->family_runtime_facts[root_value_id].coalesce_ready_affinity_weight_sum + weight;
    }
}

static void value_ssa_alloc_move_engine_set_family_best_coalesce_ready_partner(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t partner_root_value_id,
    size_t weight) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_roots) {
        runtime_state->active_family_best_coalesce_ready_partner_roots[root_value_id] = partner_root_value_id;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_weights) {
        runtime_state->active_family_best_coalesce_ready_partner_weights[root_value_id] = weight;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_root_value_id =
            partner_root_value_id;
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_affinity_weight = weight;
    }
}

static void value_ssa_alloc_move_engine_set_family_best_coalesce_ready_partner_mutual(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    int is_mutual) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_mutual_flags) {
        runtime_state->active_family_best_coalesce_ready_partner_mutual_flags[root_value_id] = is_mutual ? 1u : 0u;
    }
    if (runtime_state->family_runtime_facts) {
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_is_mutual =
            is_mutual ? 1u : 0u;
    }
}

static size_t value_ssa_alloc_move_engine_family_active_member_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].active_member_count;
    }
    if (runtime_state->active_family_member_counts) {
        return runtime_state->active_family_member_counts[root_value_id];
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_spill_removed_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].spill_removed_count;
    }
    if (runtime_state->active_family_spill_removed_counts) {
        return runtime_state->active_family_spill_removed_counts[root_value_id];
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_coalesce_ready_neighbor_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].coalesce_ready_neighbor_count;
    }
    if (runtime_state->active_family_coalesce_ready_neighbor_counts) {
        return runtime_state->active_family_coalesce_ready_neighbor_counts[root_value_id];
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_best_partner_root(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return (size_t)-1;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_root_value_id;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_roots) {
        return runtime_state->active_family_best_coalesce_ready_partner_roots[root_value_id];
    }
    return (size_t)-1;
}

static size_t value_ssa_alloc_move_engine_family_best_partner_weight(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_affinity_weight;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_weights) {
        return runtime_state->active_family_best_coalesce_ready_partner_weights[root_value_id];
    }
    return 0;
}

static int value_ssa_alloc_move_engine_family_best_partner_is_mutual(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_is_mutual != 0;
    }
    if (runtime_state->active_family_best_coalesce_ready_partner_mutual_flags) {
        return runtime_state->active_family_best_coalesce_ready_partner_mutual_flags[root_value_id] != 0;
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_external_neighbor_count(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].external_neighbor_count;
    }
    if (runtime_state->active_family_external_neighbor_counts) {
        return runtime_state->active_family_external_neighbor_counts[root_value_id];
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_coalesce_ready_affinity_sum(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].coalesce_ready_affinity_weight_sum;
    }
    if (runtime_state->active_family_coalesce_ready_affinity_sums) {
        return runtime_state->active_family_coalesce_ready_affinity_sums[root_value_id];
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_external_affinity_sum(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].external_affinity_weight_sum;
    }
    if (runtime_state->active_family_external_affinity_sums) {
        return runtime_state->active_family_external_affinity_sums[root_value_id];
    }
    return 0;
}

static size_t value_ssa_alloc_move_engine_family_queue_priority(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return 0;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].queue_priority;
    }
    if (runtime_state->active_family_queue_priorities) {
        return runtime_state->active_family_queue_priorities[root_value_id];
    }
    return 0;
}

static ValueSsaAllocatorMoveWorkClass value_ssa_alloc_move_engine_family_move_work_class(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
    }
    if (runtime_state->family_runtime_facts) {
        return runtime_state->family_runtime_facts[root_value_id].move_work_class;
    }
    if (runtime_state->active_family_move_work_classes) {
        return runtime_state->active_family_move_work_classes[root_value_id];
    }
    return VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL;
}

static size_t value_ssa_alloc_move_engine_collect_unique_runtime_roots(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    const size_t *roots,
    size_t root_count,
    size_t *out_roots) {
    size_t root_index;
    size_t unique_count = 0;

    if (!roots || !out_roots) {
        return 0;
    }

    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id =
            value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, roots[root_index]);

        if (root_value_id >= value_count) {
            continue;
        }
        if (runtime_state && runtime_state->scratch_root_flags) {
            if (runtime_state->scratch_root_flags[root_value_id]) {
                continue;
            }
            runtime_state->scratch_root_flags[root_value_id] = 1u;
            out_roots[unique_count++] = root_value_id;
            continue;
        }

        {
            size_t seen_index;
            int already_seen = 0;

            for (seen_index = 0; seen_index < unique_count; ++seen_index) {
                if (out_roots[seen_index] == root_value_id) {
                    already_seen = 1;
                    break;
                }
            }
            if (already_seen) {
                continue;
            }
            out_roots[unique_count++] = root_value_id;
        }
    }

    if (runtime_state && runtime_state->scratch_root_flags) {
        for (root_index = 0; root_index < unique_count; ++root_index) {
            runtime_state->scratch_root_flags[out_roots[root_index]] = 0u;
        }
    }

    return unique_count;
}

static int value_ssa_alloc_move_engine_read_family_state(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    ValueSsaAllocatorMoveEngineFamilyStateView *out_state) {
    size_t representative_root_value_id;

    if (!runtime_state || !out_state || root_value_id >= value_count) {
        return 0;
    }

    representative_root_value_id =
        value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, root_value_id);
    if (representative_root_value_id >= value_count) {
        return 0;
    }

    memset(out_state, 0, sizeof(*out_state));
    out_state->representative_root_value_id = representative_root_value_id;
    out_state->spill_removed_count =
        value_ssa_alloc_move_engine_family_spill_removed_count(runtime_state, value_count, representative_root_value_id);
    out_state->active_member_count =
        value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, representative_root_value_id);
    out_state->external_neighbor_count =
        value_ssa_alloc_move_engine_family_external_neighbor_count(
            runtime_state, value_count, representative_root_value_id);
    out_state->coalesce_ready_neighbor_count =
        value_ssa_alloc_move_engine_family_coalesce_ready_neighbor_count(
            runtime_state, value_count, representative_root_value_id);
    out_state->coalesce_ready_affinity_weight_sum =
        value_ssa_alloc_move_engine_family_coalesce_ready_affinity_sum(
            runtime_state, value_count, representative_root_value_id);
    out_state->best_coalesce_ready_partner_root_value_id =
        value_ssa_alloc_move_engine_family_best_partner_root(runtime_state, value_count, representative_root_value_id);
    out_state->best_coalesce_ready_partner_affinity_weight =
        value_ssa_alloc_move_engine_family_best_partner_weight(
            runtime_state, value_count, representative_root_value_id);
    out_state->best_coalesce_ready_partner_is_mutual =
        value_ssa_alloc_move_engine_family_best_partner_is_mutual(
            runtime_state, value_count, representative_root_value_id);
    out_state->external_affinity_weight_sum =
        value_ssa_alloc_move_engine_family_external_affinity_sum(
            runtime_state, value_count, representative_root_value_id);
    out_state->queue_priority =
        value_ssa_alloc_move_engine_family_queue_priority(runtime_state, value_count, representative_root_value_id);
    out_state->move_work_class =
        value_ssa_alloc_move_engine_family_move_work_class(runtime_state, value_count, representative_root_value_id);
    return 1;
}

static int value_ssa_alloc_move_engine_read_family_slot_state(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    ValueSsaAllocatorMoveEngineFamilyStateView *out_state) {
    if (!runtime_state || !out_state || root_value_id >= value_count) {
        return 0;
    }
    if (!runtime_state->family_runtime_facts) {
        return 0;
    }

    memset(out_state, 0, sizeof(*out_state));
    out_state->representative_root_value_id = root_value_id;
    out_state->spill_removed_count = runtime_state->family_runtime_facts[root_value_id].spill_removed_count;
    out_state->active_member_count = runtime_state->family_runtime_facts[root_value_id].active_member_count;
    out_state->external_neighbor_count = runtime_state->family_runtime_facts[root_value_id].external_neighbor_count;
    out_state->coalesce_ready_neighbor_count =
        runtime_state->family_runtime_facts[root_value_id].coalesce_ready_neighbor_count;
    out_state->coalesce_ready_affinity_weight_sum =
        runtime_state->family_runtime_facts[root_value_id].coalesce_ready_affinity_weight_sum;
    out_state->best_coalesce_ready_partner_root_value_id =
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_root_value_id;
    out_state->best_coalesce_ready_partner_affinity_weight =
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_affinity_weight;
    out_state->best_coalesce_ready_partner_is_mutual =
        runtime_state->family_runtime_facts[root_value_id].best_coalesce_ready_partner_is_mutual;
    out_state->external_affinity_weight_sum =
        runtime_state->family_runtime_facts[root_value_id].external_affinity_weight_sum;
    out_state->queue_priority = runtime_state->family_runtime_facts[root_value_id].queue_priority;
    out_state->move_work_class = runtime_state->family_runtime_facts[root_value_id].move_work_class;
    return 1;
}

static void value_ssa_alloc_move_engine_apply_family_state_to_phase_entry(
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry,
    const ValueSsaAllocatorMoveEngineFamilyStateView *family_state) {
    if (!entry || !family_state) {
        return;
    }

    memset(entry, 0, sizeof(*entry));
    entry->root_value_id = family_state->representative_root_value_id;
    entry->work_class = family_state->move_work_class;
    entry->family_active_member_count = family_state->active_member_count;
    entry->family_external_neighbor_count = family_state->external_neighbor_count;
    entry->family_coalesce_ready_neighbor_count = family_state->coalesce_ready_neighbor_count;
    entry->family_coalesce_ready_affinity_weight_sum = family_state->coalesce_ready_affinity_weight_sum;
    entry->family_best_coalesce_ready_partner_root_value_id = family_state->best_coalesce_ready_partner_root_value_id;
    entry->family_best_coalesce_ready_partner_affinity_weight =
        family_state->best_coalesce_ready_partner_affinity_weight;
    entry->family_best_coalesce_ready_partner_is_mutual = family_state->best_coalesce_ready_partner_is_mutual;
    entry->family_external_affinity_weight_sum = family_state->external_affinity_weight_sum;
}

static void value_ssa_alloc_move_engine_apply_family_state_to_plan_item_after(
    ValueSsaAllocatorPlanItem *item,
    const ValueSsaAllocatorMoveEngineFamilyStateView *family_state) {
    if (!item || !family_state) {
        return;
    }

    item->move_work_next_class = family_state->move_work_class;
    item->family_active_member_count_after = family_state->active_member_count;
    item->family_external_neighbor_count_after = family_state->external_neighbor_count;
    item->family_coalesce_ready_neighbor_count_after = family_state->coalesce_ready_neighbor_count;
    item->family_coalesce_ready_affinity_weight_sum_after = family_state->coalesce_ready_affinity_weight_sum;
    item->family_best_coalesce_ready_partner_root_value_id_after =
        family_state->best_coalesce_ready_partner_root_value_id;
    item->family_best_coalesce_ready_partner_affinity_weight_after =
        family_state->best_coalesce_ready_partner_affinity_weight;
    item->family_best_coalesce_ready_partner_is_mutual_after =
        family_state->best_coalesce_ready_partner_is_mutual;
    item->family_external_affinity_weight_sum_after = family_state->external_affinity_weight_sum;
}

static void value_ssa_alloc_move_engine_clear_family_runtime_facts_for_root(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id) {
    if (!runtime_state || root_value_id >= value_count) {
        return;
    }

    value_ssa_alloc_move_engine_set_family_spill_removed_count(runtime_state, value_count, root_value_id, 0);
    value_ssa_alloc_move_engine_set_family_active_member_count(runtime_state, value_count, root_value_id, 0);
    value_ssa_alloc_move_engine_reset_family_move_pressure_fields(runtime_state, value_count, root_value_id);
    value_ssa_alloc_move_engine_set_family_queue_priority(runtime_state, value_count, root_value_id, 0);
    value_ssa_alloc_move_engine_set_family_move_work_class(
        runtime_state, value_count, root_value_id, VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL);
}

static void value_ssa_alloc_move_engine_merge_runtime_coalesce_families(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id) {
    size_t winner_root_value_id;
    size_t loser_root_value_id;
    size_t value_id;

    if (!runtime_state || !runtime_state->runtime_coalesce_roots || !runtime_state->runtime_coalesce_class_sizes ||
        lhs_root_value_id >= value_count || rhs_root_value_id >= value_count) {
        return;
    }

    lhs_root_value_id = runtime_state->runtime_coalesce_roots[lhs_root_value_id];
    rhs_root_value_id = runtime_state->runtime_coalesce_roots[rhs_root_value_id];
    if (lhs_root_value_id == rhs_root_value_id) {
        return;
    }

    winner_root_value_id = lhs_root_value_id < rhs_root_value_id ? lhs_root_value_id : rhs_root_value_id;
    loser_root_value_id = lhs_root_value_id < rhs_root_value_id ? rhs_root_value_id : lhs_root_value_id;
    for (value_id = 0; value_id < value_count; ++value_id) {
        if (runtime_state->runtime_coalesce_roots[value_id] == loser_root_value_id) {
            runtime_state->runtime_coalesce_roots[value_id] = winner_root_value_id;
        }
    }
    runtime_state->runtime_coalesce_class_sizes[winner_root_value_id] +=
        runtime_state->runtime_coalesce_class_sizes[loser_root_value_id];
    runtime_state->runtime_coalesce_class_sizes[loser_root_value_id] = 0;
    value_ssa_alloc_move_engine_set_family_active_member_count(runtime_state,
        value_count,
        winner_root_value_id,
        value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, winner_root_value_id) +
            value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, loser_root_value_id));
    value_ssa_alloc_move_engine_set_family_spill_removed_count(runtime_state,
        value_count,
        winner_root_value_id,
        value_ssa_alloc_move_engine_family_spill_removed_count(runtime_state, value_count, winner_root_value_id) +
            value_ssa_alloc_move_engine_family_spill_removed_count(runtime_state, value_count, loser_root_value_id));
    value_ssa_alloc_move_engine_clear_family_runtime_facts_for_root(runtime_state, value_count, loser_root_value_id);
}

static void value_ssa_alloc_move_engine_update_active_degrees(
    const ValueSsaInterferenceGraph *interference_graph,
    const unsigned char *scheduled,
    size_t transition_value_id,
    int transition_is_freeze,
    int full_reseed,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t neighbor_value_id;

    if (!interference_graph || !scheduled || !runtime_state || !runtime_state->active_degrees) {
        return;
    }

    if (full_reseed) {
        value_ssa_alloc_recompute_active_degrees(interference_graph, scheduled, runtime_state->active_degrees);
        return;
    }
    if (transition_is_freeze || transition_value_id >= interference_graph->value_count) {
        return;
    }

    runtime_state->active_degrees[transition_value_id] = 0;
    if (runtime_state->interference_neighbor_offsets && runtime_state->interference_neighbor_value_ids) {
        for (neighbor_value_id = runtime_state->interference_neighbor_offsets[transition_value_id];
             neighbor_value_id < runtime_state->interference_neighbor_offsets[transition_value_id + 1];
             ++neighbor_value_id) {
            size_t neighbor_id = runtime_state->interference_neighbor_value_ids[neighbor_value_id];

            if (neighbor_id == transition_value_id || scheduled[neighbor_id]) {
                continue;
            }
            if (runtime_state->active_degrees[neighbor_id] == 0) {
                continue;
            }
            --runtime_state->active_degrees[neighbor_id];
        }
        return;
    }

    for (neighbor_value_id = 0; neighbor_value_id < interference_graph->value_count; ++neighbor_value_id) {
        if (neighbor_value_id == transition_value_id || scheduled[neighbor_value_id]) {
            continue;
        }
        if (!interference_graph->interferes[transition_value_id * interference_graph->value_count + neighbor_value_id]) {
            continue;
        }
        if (runtime_state->active_degrees[neighbor_value_id] == 0) {
            continue;
        }
        --runtime_state->active_degrees[neighbor_value_id];
    }
}

static void value_ssa_alloc_move_engine_update_active_move_related_for_roots(
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    const size_t *roots,
    size_t root_count,
    size_t value_count,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_index;

    if (!worklist || !family_members || !scheduled || !frozen || !roots || !runtime_state ||
        !runtime_state->active_move_related || !runtime_state->affinity_neighbor_offsets ||
        !runtime_state->affinity_neighbor_value_ids) {
        return;
    }

    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id = roots[root_index];
        size_t root_offset;
        size_t root_member_count;
        size_t member_index;

        if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
            continue;
        }
        root_offset = family_members->root_member_offsets[root_value_id];
        root_member_count = family_members->root_member_counts[root_value_id];
        for (member_index = 0; member_index < root_member_count; ++member_index) {
            size_t item_index = family_members->member_item_indices[root_offset + member_index];
            const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
            size_t value_id = work_item->value_id;
            size_t value_root_value_id =
                value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, value_id);
            size_t neighbor_index;
            int has_active_affinity = 0;

            if (value_id >= value_count) {
                continue;
            }
            if (scheduled[value_id] || frozen[value_id]) {
                runtime_state->active_move_related[value_id] = 0;
                continue;
            }
            for (neighbor_index = runtime_state->affinity_neighbor_offsets[value_id];
                 neighbor_index < runtime_state->affinity_neighbor_offsets[value_id + 1];
                 ++neighbor_index) {
                size_t neighbor_value_id = runtime_state->affinity_neighbor_value_ids[neighbor_index];

                if (neighbor_value_id == value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                    continue;
                }
                if (value_ssa_alloc_move_engine_family_pair_is_consumed(runtime_state,
                        value_count,
                        value_root_value_id,
                        value_ssa_alloc_move_engine_effective_root(
                            runtime_state, coalesce_analysis, value_count, neighbor_value_id))) {
                    continue;
                }
                has_active_affinity = 1;
                break;
            }
            runtime_state->active_move_related[value_id] = has_active_affinity ? 1u : 0u;
        }
    }
}

static void value_ssa_alloc_move_engine_update_active_move_related(
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    const ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    int full_reseed,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    (void)value_count;
    (void)coalesce_analysis;
    (void)interference_graph;
    (void)affinity_graph;

    if (!interference_graph || !scheduled || !frozen ||
        !runtime_state || !runtime_state->active_degrees || !runtime_state->active_effective_degrees ||
        !runtime_state->active_family_member_counts || !runtime_state->active_family_external_neighbor_counts ||
        !runtime_state->active_family_external_affinity_sums || !runtime_state->scratch_roots ||
        !runtime_state->active_move_related || !runtime_state->active_effective_move_related ||
        !runtime_state->family_neighbor_matrix || !runtime_state->scratch_class_move_related ||
        !runtime_state->active_family_move_work_classes) {
        return;
    }

    if (full_reseed) {
        value_ssa_alloc_recompute_active_move_related(affinity_graph, scheduled, frozen, runtime_state->active_move_related);
        return;
    }

    value_ssa_alloc_move_engine_update_active_move_related_for_roots(worklist,
        family_members,
        coalesce_analysis,
        scheduled,
        frozen,
        queues->affected_family_roots,
        queues->affected_family_count,
        value_count,
        runtime_state);
}

static void value_ssa_alloc_move_engine_recompute_active_effective_degrees_for_roots(
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const size_t *roots,
    size_t root_count,
    size_t value_count,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_index;
    int use_coalesce;

    if (!worklist || !family_members || !interference_graph || !scheduled || !roots || !runtime_state ||
        !runtime_state->active_degrees || !runtime_state->active_effective_degrees || !runtime_state->scratch_roots) {
        return;
    }

    use_coalesce = value_ssa_alloc_coalesce_analysis_is_usable(coalesce_analysis, value_count);
    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id = roots[root_index];
        size_t root_offset;
        size_t root_member_count;
        size_t member_index;

        if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
            continue;
        }
        root_offset = family_members->root_member_offsets[root_value_id];
        root_member_count = family_members->root_member_counts[root_value_id];
        for (member_index = 0; member_index < root_member_count; ++member_index) {
            size_t item_index = family_members->member_item_indices[root_offset + member_index];
            size_t value_id = worklist->items[item_index].value_id;

            if (value_id >= value_count) {
                continue;
            }
            if (scheduled[value_id]) {
                runtime_state->active_effective_degrees[value_id] = 0;
                continue;
            }
            if (!use_coalesce) {
                runtime_state->active_effective_degrees[value_id] = runtime_state->active_degrees[value_id];
            } else {
                size_t neighbor_value_id;
                size_t seen_count = 0;
                size_t effective_degree = 0;
                size_t value_root_id =
                    value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, value_id);

                size_t neighbor_begin = 0;
                size_t neighbor_end = value_count;

                if (runtime_state->interference_neighbor_offsets && runtime_state->interference_neighbor_value_ids) {
                    neighbor_begin = runtime_state->interference_neighbor_offsets[value_id];
                    neighbor_end = runtime_state->interference_neighbor_offsets[value_id + 1];
                }

                for (neighbor_value_id = neighbor_begin; neighbor_value_id < neighbor_end; ++neighbor_value_id) {
                    size_t neighbor_root_value_id;
                    size_t seen_index;
                    int already_seen = 0;
                    size_t neighbor_id = runtime_state->interference_neighbor_offsets &&
                            runtime_state->interference_neighbor_value_ids
                        ? runtime_state->interference_neighbor_value_ids[neighbor_value_id]
                        : neighbor_value_id;

                    if (neighbor_id == value_id || scheduled[neighbor_id]) {
                        continue;
                    }
                    if (!(runtime_state->interference_neighbor_offsets && runtime_state->interference_neighbor_value_ids) &&
                        !interference_graph->interferes[value_id * value_count + neighbor_id]) {
                        continue;
                    }
                    neighbor_root_value_id = value_ssa_alloc_move_engine_effective_root(
                        runtime_state, coalesce_analysis, value_count, neighbor_id);
                    if (neighbor_root_value_id == value_root_id) {
                        continue;
                    }
                    if (runtime_state->scratch_root_flags) {
                        if (runtime_state->scratch_root_flags[neighbor_root_value_id]) {
                            continue;
                        }
                        runtime_state->scratch_root_flags[neighbor_root_value_id] = 1u;
                    } else {
                        for (seen_index = 0; seen_index < seen_count; ++seen_index) {
                            if (runtime_state->scratch_roots[seen_index] == neighbor_root_value_id) {
                                already_seen = 1;
                                break;
                            }
                        }
                        if (already_seen) {
                            continue;
                        }
                    }
                    runtime_state->scratch_roots[seen_count++] = neighbor_root_value_id;
                    ++effective_degree;
                }
                if (runtime_state->scratch_root_flags) {
                    size_t seen_index;

                    for (seen_index = 0; seen_index < seen_count; ++seen_index) {
                        runtime_state->scratch_root_flags[runtime_state->scratch_roots[seen_index]] = 0u;
                    }
                }
                runtime_state->active_effective_degrees[value_id] = effective_degree;
            }
        }
    }
}

static void value_ssa_alloc_move_engine_recompute_active_effective_move_related_for_roots(
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    const size_t *roots,
    size_t root_count,
    size_t value_count,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_index;
    int use_coalesce;

    if (!worklist || !family_members || !scheduled || !frozen || !roots || !runtime_state ||
        !runtime_state->active_move_related || !runtime_state->active_effective_move_related ||
        !runtime_state->scratch_class_move_related || !runtime_state->affinity_neighbor_offsets ||
        !runtime_state->affinity_neighbor_value_ids) {
        return;
    }

    use_coalesce = value_ssa_alloc_coalesce_analysis_is_usable(coalesce_analysis, value_count);
    if (!use_coalesce) {
        for (root_index = 0; root_index < root_count; ++root_index) {
            size_t root_value_id = roots[root_index];
            size_t root_offset;
            size_t root_member_count;
            size_t member_index;

            if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
                continue;
            }
            root_offset = family_members->root_member_offsets[root_value_id];
            root_member_count = family_members->root_member_counts[root_value_id];
            for (member_index = 0; member_index < root_member_count; ++member_index) {
                size_t item_index = family_members->member_item_indices[root_offset + member_index];
                size_t value_id = worklist->items[item_index].value_id;

                if (value_id >= value_count) {
                    continue;
                }
                runtime_state->active_effective_move_related[value_id] = runtime_state->active_move_related[value_id];
            }
        }
        return;
    }

    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id = roots[root_index];

        if (root_value_id >= value_count) {
            continue;
        }
        runtime_state->scratch_class_move_related[root_value_id] = 0u;
    }

    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id = roots[root_index];
        size_t root_offset;
        size_t root_member_count;
        size_t member_index;

        if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
            continue;
        }
        root_offset = family_members->root_member_offsets[root_value_id];
        root_member_count = family_members->root_member_counts[root_value_id];
        for (member_index = 0; member_index < root_member_count; ++member_index) {
            size_t item_index = family_members->member_item_indices[root_offset + member_index];
            size_t value_id = worklist->items[item_index].value_id;
            size_t neighbor_index;
            size_t value_root_value_id =
                value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, value_id);

            if (value_id >= value_count || scheduled[value_id] || frozen[value_id]) {
                continue;
            }
            for (neighbor_index = runtime_state->affinity_neighbor_offsets[value_id];
                 neighbor_index < runtime_state->affinity_neighbor_offsets[value_id + 1];
                 ++neighbor_index) {
                size_t neighbor_value_id = runtime_state->affinity_neighbor_value_ids[neighbor_index];

                if (neighbor_value_id == value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                    continue;
                }
                if (value_ssa_alloc_move_engine_effective_root(
                        runtime_state, coalesce_analysis, value_count, neighbor_value_id) == root_value_id) {
                    continue;
                }
                if (value_ssa_alloc_move_engine_family_pair_is_consumed(runtime_state,
                        value_count,
                        value_root_value_id,
                        value_ssa_alloc_move_engine_effective_root(
                            runtime_state, coalesce_analysis, value_count, neighbor_value_id))) {
                    continue;
                }
                runtime_state->scratch_class_move_related[root_value_id] = 1u;
                break;
            }
        }
    }

    for (root_index = 0; root_index < root_count; ++root_index) {
        size_t root_value_id = roots[root_index];
        size_t root_offset;
        size_t root_member_count;
        size_t member_index;

        if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
            continue;
        }
        root_offset = family_members->root_member_offsets[root_value_id];
        root_member_count = family_members->root_member_counts[root_value_id];
        for (member_index = 0; member_index < root_member_count; ++member_index) {
            size_t item_index = family_members->member_item_indices[root_offset + member_index];
            size_t value_id = worklist->items[item_index].value_id;

            if (value_id >= value_count) {
                continue;
            }
            if (scheduled[value_id] || frozen[value_id]) {
                runtime_state->active_effective_move_related[value_id] = 0;
                continue;
            }
            runtime_state->active_effective_move_related[value_id] =
                runtime_state->scratch_class_move_related[root_value_id];
        }
    }
}

static void value_ssa_alloc_move_engine_update_effective_value_runtime_state(
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    const ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    int full_reseed,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    if (!interference_graph || !affinity_graph || !scheduled || !frozen || !runtime_state ||
        !runtime_state->active_degrees || !runtime_state->active_effective_degrees ||
        !runtime_state->scratch_roots || !runtime_state->active_move_related ||
        !runtime_state->active_effective_move_related || !runtime_state->scratch_class_move_related) {
        return;
    }

    if (full_reseed) {
        value_ssa_alloc_recompute_active_effective_degrees(
            interference_graph,
            coalesce_analysis,
            scheduled,
            runtime_state->active_degrees,
            runtime_state->scratch_roots,
            runtime_state->scratch_root_flags,
            runtime_state->active_effective_degrees);
        value_ssa_alloc_recompute_active_effective_move_related(affinity_graph,
            coalesce_analysis,
            scheduled,
            frozen,
            runtime_state->active_move_related,
            runtime_state->scratch_class_move_related,
            runtime_state->active_effective_move_related);
        return;
    }

    value_ssa_alloc_move_engine_recompute_active_effective_degrees_for_roots(worklist,
        family_members,
        interference_graph,
        coalesce_analysis,
        scheduled,
        queues->affected_family_roots,
        queues->affected_family_count,
        value_count,
        runtime_state);
    value_ssa_alloc_move_engine_recompute_active_effective_move_related_for_roots(worklist,
        family_members,
        coalesce_analysis,
        scheduled,
        frozen,
        queues->affected_family_roots,
        queues->affected_family_count,
        value_count,
        runtime_state);
}

static void value_ssa_alloc_move_engine_recompute_family_queue_priorities(
    size_t value_count,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    const ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    int full_reseed) {
    size_t root_index;
    size_t normalized_root_count;
    size_t *roots = NULL;
    size_t root_count = 0;

    if (!runtime_state || !runtime_state->scratch_roots || !queues ||
        (!runtime_state->active_family_queue_priorities && !runtime_state->family_runtime_facts)) {
        return;
    }

    if (full_reseed) {
        roots = queues->active_family_roots;
        root_count = queues->active_family_count;
    } else {
        roots = queues->affected_family_roots;
        root_count = queues->affected_family_count;
    }

    normalized_root_count = value_ssa_alloc_move_engine_collect_unique_runtime_roots(runtime_state,
        value_count,
        roots,
        root_count,
        runtime_state->scratch_roots);
    for (root_index = 0; root_index < normalized_root_count; ++root_index) {
        size_t root_value_id = runtime_state->scratch_roots[root_index];

        if (root_value_id >= value_count) {
            continue;
        }
        value_ssa_alloc_move_engine_set_family_queue_priority(runtime_state, value_count, root_value_id, 0);
    }
}

static void value_ssa_alloc_move_engine_update_active_family_member_counts(
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    const unsigned char *scheduled,
    size_t transition_value_id,
    int transition_is_freeze,
    int full_reseed,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_value_id;

    if (!runtime_state || !scheduled ||
        (!runtime_state->active_family_member_counts && !runtime_state->family_runtime_facts)) {
        return;
    }

    if (full_reseed) {
        size_t value_id;

        memset(runtime_state->active_family_member_counts, 0, (value_count ? value_count : 1) * sizeof(size_t));
        for (value_id = 0; value_id < value_count; ++value_id) {
            value_ssa_alloc_move_engine_set_family_active_member_count(runtime_state, value_count, value_id, 0);
        }
        for (value_id = 0; value_id < value_count; ++value_id) {
            size_t root_value_id;

            if (scheduled[value_id]) {
                continue;
            }
            root_value_id =
                value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, value_id);
            if (root_value_id >= value_count) {
                continue;
            }
            value_ssa_alloc_move_engine_set_family_active_member_count(runtime_state,
                value_count,
                root_value_id,
                value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, root_value_id) + 1);
        }
        return;
    }

    if (transition_is_freeze || transition_value_id >= value_count) {
        return;
    }

    root_value_id =
        value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, transition_value_id);
    if (root_value_id >= value_count) {
        return;
    }
    if (value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, root_value_id) == 0) {
        return;
    }

    value_ssa_alloc_move_engine_set_family_active_member_count(
        runtime_state,
        value_count,
        root_value_id,
        value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, root_value_id) - 1);
}

static void value_ssa_alloc_move_engine_update_active_family_spill_removed_counts(
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t transition_value_id,
    int transition_is_spill_remove,
    int full_reseed,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_value_id;

    if (!runtime_state ||
        (!runtime_state->active_family_spill_removed_counts && !runtime_state->family_runtime_facts)) {
        return;
    }

    if (full_reseed) {
        memset(runtime_state->active_family_spill_removed_counts,
            0,
            (value_count ? value_count : 1) * sizeof(size_t));
        {
            size_t value_id;

            for (value_id = 0; value_id < value_count; ++value_id) {
                value_ssa_alloc_move_engine_set_family_spill_removed_count(runtime_state, value_count, value_id, 0);
            }
        }
        return;
    }
    if (!transition_is_spill_remove || transition_value_id >= value_count) {
        return;
    }

    root_value_id =
        value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, transition_value_id);
    if (root_value_id >= value_count) {
        return;
    }
    value_ssa_alloc_move_engine_set_family_spill_removed_count(runtime_state,
        value_count,
        root_value_id,
        value_ssa_alloc_move_engine_family_spill_removed_count(runtime_state, value_count, root_value_id) + 1);
}

static void value_ssa_alloc_move_engine_recompute_active_family_move_work_classes_for_roots(
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    const unsigned char *initial_family_move_related,
    const unsigned char *was_frozen,
    const size_t *roots,
    size_t root_count,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_index;
    size_t normalized_root_count;

    (void)coalesce_analysis;

    if (!runtime_state || !runtime_state->scratch_roots ||
        (!runtime_state->active_family_member_counts && !runtime_state->family_runtime_facts) ||
        (!runtime_state->active_family_coalesce_ready_neighbor_counts && !runtime_state->family_runtime_facts) ||
        !roots) {
        return;
    }

    normalized_root_count = value_ssa_alloc_move_engine_collect_unique_runtime_roots(
        runtime_state, value_count, roots, root_count, runtime_state->scratch_roots);

    for (root_index = 0; root_index < normalized_root_count; ++root_index) {
        size_t root_value_id = runtime_state->scratch_roots[root_index];

        if (root_value_id >= value_count) {
            continue;
        }
        value_ssa_alloc_move_engine_set_family_move_work_class(runtime_state,
            value_count,
            root_value_id,
            value_ssa_alloc_classify_plan_move_work(
                initial_family_move_related ? (int)initial_family_move_related[root_value_id] : 0,
                was_frozen ? (int)was_frozen[root_value_id] : 0,
                value_ssa_alloc_move_engine_family_active_member_count(runtime_state, value_count, root_value_id),
                value_ssa_alloc_move_engine_family_coalesce_ready_neighbor_count(
                    runtime_state, value_count, root_value_id)));
    }
}

static void value_ssa_alloc_move_engine_clear_family_move_pressure_for_roots(
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    const size_t *roots,
    size_t root_count,
    size_t value_count) {
    size_t root_index;
    size_t normalized_root_count;

    if (!runtime_state || !runtime_state->scratch_roots || !roots ||
        (!runtime_state->family_runtime_facts &&
            (!runtime_state->active_family_external_neighbor_counts ||
                !runtime_state->active_family_coalesce_ready_neighbor_counts ||
                !runtime_state->active_family_coalesce_ready_affinity_sums ||
                !runtime_state->active_family_best_coalesce_ready_partner_roots ||
                !runtime_state->active_family_best_coalesce_ready_partner_weights ||
                !runtime_state->active_family_best_coalesce_ready_partner_mutual_flags ||
                !runtime_state->active_family_external_affinity_sums))) {
        return;
    }

    normalized_root_count = value_ssa_alloc_move_engine_collect_unique_runtime_roots(
        runtime_state, value_count, roots, root_count, runtime_state->scratch_roots);

    for (root_index = 0; root_index < normalized_root_count; ++root_index) {
        size_t root_value_id = runtime_state->scratch_roots[root_index];
        size_t neighbor_root_value_id;

        if (root_value_id >= value_count) {
            continue;
        }
        value_ssa_alloc_move_engine_reset_family_move_pressure_fields(runtime_state, value_count, root_value_id);
        if (!runtime_state->family_neighbor_matrix) {
            if (!runtime_state->family_coalesce_ready_neighbor_matrix) {
                continue;
            }
        }
        for (neighbor_root_value_id = 0; neighbor_root_value_id < value_count; ++neighbor_root_value_id) {
            if (runtime_state->family_neighbor_matrix) {
                runtime_state->family_neighbor_matrix[root_value_id * value_count + neighbor_root_value_id] = 0;
                runtime_state->family_neighbor_matrix[neighbor_root_value_id * value_count + root_value_id] = 0;
            }
            if (runtime_state->family_coalesce_ready_neighbor_matrix) {
                runtime_state
                    ->family_coalesce_ready_neighbor_matrix[root_value_id * value_count + neighbor_root_value_id] = 0;
                runtime_state
                    ->family_coalesce_ready_neighbor_matrix[neighbor_root_value_id * value_count + root_value_id] = 0;
            }
            if (runtime_state->family_coalesce_ready_affinity_matrix) {
                runtime_state->family_coalesce_ready_affinity_matrix[root_value_id * value_count + neighbor_root_value_id] =
                    0;
                runtime_state->family_coalesce_ready_affinity_matrix[neighbor_root_value_id * value_count + root_value_id] =
                    0;
            }
        }
    }
}

static void value_ssa_alloc_move_engine_recompute_active_family_move_pressure_for_roots(
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    const size_t *roots,
    size_t root_count,
    size_t value_count,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t root_index;
    size_t normalized_root_count;

    if (!worklist || !family_members || !affinity_graph || !coalesce_analysis || !scheduled || !frozen || !roots ||
        !runtime_state || !runtime_state->scratch_roots ||
        (!runtime_state->family_runtime_facts &&
            (!runtime_state->active_family_external_neighbor_counts ||
                !runtime_state->active_family_coalesce_ready_neighbor_counts ||
                !runtime_state->active_family_coalesce_ready_affinity_sums ||
                !runtime_state->active_family_best_coalesce_ready_partner_roots ||
                !runtime_state->active_family_best_coalesce_ready_partner_weights ||
                !runtime_state->active_family_best_coalesce_ready_partner_mutual_flags ||
                !runtime_state->active_family_external_affinity_sums))) {
        return;
    }

    normalized_root_count = value_ssa_alloc_move_engine_collect_unique_runtime_roots(
        runtime_state, value_count, roots, root_count, runtime_state->scratch_roots);
    value_ssa_alloc_move_engine_clear_family_move_pressure_for_roots(
        runtime_state, runtime_state->scratch_roots, normalized_root_count, value_count);
    for (root_index = 0; root_index < normalized_root_count; ++root_index) {
        size_t root_value_id = runtime_state->scratch_roots[root_index];
        size_t member_index;
        size_t root_offset;
        size_t root_member_count;

        if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
            continue;
        }

        root_offset = family_members->root_member_offsets[root_value_id];
        root_member_count = family_members->root_member_counts[root_value_id];
        for (member_index = 0; member_index < root_member_count; ++member_index) {
            size_t item_index = family_members->member_item_indices[root_offset + member_index];
            const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
            size_t family_value_id = work_item->value_id;
            size_t neighbor_index;

            if (family_value_id >= value_count || scheduled[family_value_id] || frozen[family_value_id]) {
                continue;
            }
            for (neighbor_index = runtime_state->affinity_neighbor_offsets
                     ? runtime_state->affinity_neighbor_offsets[family_value_id]
                     : 0;
                 runtime_state->affinity_neighbor_offsets &&
                 neighbor_index < runtime_state->affinity_neighbor_offsets[family_value_id + 1];
                 ++neighbor_index) {
                size_t neighbor_value_id = runtime_state->affinity_neighbor_value_ids[neighbor_index];
                size_t neighbor_root_value_id;
                size_t weight =
                    runtime_state->affinity_neighbor_weights ? runtime_state->affinity_neighbor_weights[neighbor_index] : 0;
                const ValueSsaAllocatorCoalesceItem *coalesce_item = NULL;

                if (neighbor_value_id == family_value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                    continue;
                }
                neighbor_root_value_id =
                    value_ssa_alloc_move_engine_effective_root(
                        runtime_state, coalesce_analysis, value_count, neighbor_value_id);
                if (neighbor_root_value_id == root_value_id) {
                    continue;
                }
                if (value_ssa_alloc_move_engine_family_pair_is_consumed(
                        runtime_state, value_count, root_value_id, neighbor_root_value_id)) {
                    continue;
                }
                value_ssa_alloc_move_engine_add_family_external_affinity(
                    runtime_state, value_count, root_value_id, weight);
                if (runtime_state->family_neighbor_matrix &&
                    !runtime_state->family_neighbor_matrix[root_value_id * value_count + neighbor_root_value_id]) {
                    runtime_state->family_neighbor_matrix[root_value_id * value_count + neighbor_root_value_id] = 1u;
                    runtime_state->family_neighbor_matrix[neighbor_root_value_id * value_count + root_value_id] = 1u;
                    value_ssa_alloc_move_engine_increment_family_external_neighbor_count(
                        runtime_state, value_count, root_value_id);
                }
                if (value_ssa_allocator_coalesce_analysis_find_pair(
                        coalesce_analysis, family_value_id, neighbor_value_id, NULL, &coalesce_item) &&
                    coalesce_item && coalesce_item->can_coalesce) {
                    size_t ready_affinity_index = root_value_id * value_count + neighbor_root_value_id;

                    if (runtime_state->family_coalesce_ready_neighbor_matrix &&
                        !runtime_state
                             ->family_coalesce_ready_neighbor_matrix[root_value_id * value_count + neighbor_root_value_id]) {
                        runtime_state->family_coalesce_ready_neighbor_matrix[root_value_id * value_count +
                            neighbor_root_value_id] = 1u;
                        runtime_state->family_coalesce_ready_neighbor_matrix[neighbor_root_value_id * value_count +
                            root_value_id] = 1u;
                        value_ssa_alloc_move_engine_increment_family_coalesce_ready_neighbor_count(
                            runtime_state, value_count, root_value_id);
                    }
                    if (runtime_state->family_coalesce_ready_affinity_matrix) {
                        runtime_state->family_coalesce_ready_affinity_matrix[ready_affinity_index] += weight;
                    }
                    value_ssa_alloc_move_engine_add_family_coalesce_ready_affinity(
                        runtime_state, value_count, root_value_id, weight);
                    if ((runtime_state->family_coalesce_ready_affinity_matrix &&
                            runtime_state->family_coalesce_ready_affinity_matrix[ready_affinity_index] >
                                value_ssa_alloc_move_engine_family_best_partner_weight(
                                    runtime_state, value_count, root_value_id)) ||
                        ((!runtime_state->family_coalesce_ready_affinity_matrix) &&
                            weight >
                                value_ssa_alloc_move_engine_family_best_partner_weight(
                                    runtime_state, value_count, root_value_id)) ||
                        ((runtime_state->family_coalesce_ready_affinity_matrix
                                  ? runtime_state->family_coalesce_ready_affinity_matrix[ready_affinity_index]
                                  : weight) ==
                                value_ssa_alloc_move_engine_family_best_partner_weight(
                                    runtime_state, value_count, root_value_id) &&
                            (value_ssa_alloc_move_engine_family_best_partner_root(
                                 runtime_state, value_count, root_value_id) == (size_t)-1 ||
                                neighbor_root_value_id <
                                    value_ssa_alloc_move_engine_family_best_partner_root(
                                        runtime_state, value_count, root_value_id)))) {
                        value_ssa_alloc_move_engine_set_family_best_coalesce_ready_partner(
                            runtime_state,
                            value_count,
                            root_value_id,
                            neighbor_root_value_id,
                            runtime_state->family_coalesce_ready_affinity_matrix
                                ? runtime_state->family_coalesce_ready_affinity_matrix[ready_affinity_index]
                                : weight);
                    }
                }
            }
        }
    }

    for (root_index = 0; root_index < normalized_root_count; ++root_index) {
        size_t root_value_id = runtime_state->scratch_roots[root_index];
        size_t partner_root_value_id;

        if (root_value_id >= value_count) {
            continue;
        }
        value_ssa_alloc_move_engine_set_family_best_coalesce_ready_partner_mutual(
            runtime_state, value_count, root_value_id, 0);
        partner_root_value_id =
            value_ssa_alloc_move_engine_family_best_partner_root(runtime_state, value_count, root_value_id);
        if (partner_root_value_id >= value_count) {
            continue;
        }
        if (value_ssa_alloc_move_engine_family_best_partner_weight(runtime_state, value_count, root_value_id) == 0 ||
            value_ssa_alloc_move_engine_family_best_partner_weight(runtime_state, value_count, partner_root_value_id) ==
                0) {
            continue;
        }
        if (value_ssa_alloc_move_engine_family_best_partner_root(runtime_state, value_count, partner_root_value_id) !=
            root_value_id) {
            continue;
        }
        value_ssa_alloc_move_engine_set_family_best_coalesce_ready_partner_mutual(
            runtime_state, value_count, root_value_id, 1);
    }
}

static void value_ssa_alloc_move_engine_family_members_init(ValueSsaAllocatorMoveEngineFamilyMembers *family_members) {
    if (!family_members) {
        return;
    }

    family_members->value_count = 0;
    family_members->member_item_count = 0;
    family_members->root_member_offsets = NULL;
    family_members->root_member_counts = NULL;
    family_members->member_item_indices = NULL;
}

static void value_ssa_alloc_move_engine_family_members_free(ValueSsaAllocatorMoveEngineFamilyMembers *family_members) {
    if (!family_members) {
        return;
    }

    free(family_members->root_member_offsets);
    free(family_members->root_member_counts);
    free(family_members->member_item_indices);
    value_ssa_alloc_move_engine_family_members_init(family_members);
}

static int value_ssa_alloc_move_engine_build_family_members(const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const size_t *runtime_coalesce_roots,
    size_t value_count,
    ValueSsaAllocatorMoveEngineFamilyMembers *family_members) {
    size_t *write_offsets = NULL;
    size_t item_index;

    if (!worklist || !family_members) {
        return 0;
    }

    value_ssa_alloc_move_engine_family_members_free(family_members);
    family_members->value_count = value_count;
    family_members->member_item_count = worklist->item_count;
    family_members->root_member_offsets = (size_t *)calloc(value_count ? value_count : 1, sizeof(size_t));
    family_members->root_member_counts = (size_t *)calloc(value_count ? value_count : 1, sizeof(size_t));
    family_members->member_item_indices =
        (size_t *)malloc((worklist->item_count ? worklist->item_count : 1) * sizeof(size_t));
    write_offsets = (size_t *)malloc((value_count ? value_count : 1) * sizeof(size_t));
    if (!family_members->root_member_offsets || !family_members->root_member_counts ||
        !family_members->member_item_indices || !write_offsets) {
        free(write_offsets);
        value_ssa_alloc_move_engine_family_members_free(family_members);
        return 0;
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
        size_t root_value_id;

        if (work_item->value_id >= value_count) {
            continue;
        }
        root_value_id = runtime_coalesce_roots ? runtime_coalesce_roots[work_item->value_id]
                                               : value_ssa_alloc_plan_effective_root(
                                                     coalesce_analysis, value_count, work_item->value_id);
        ++family_members->root_member_counts[root_value_id];
    }

    {
        size_t running_offset = 0;

        for (item_index = 0; item_index < value_count; ++item_index) {
            family_members->root_member_offsets[item_index] = running_offset;
            write_offsets[item_index] = running_offset;
            running_offset += family_members->root_member_counts[item_index];
        }
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
        size_t root_value_id;

        if (work_item->value_id >= value_count) {
            continue;
        }
        root_value_id = runtime_coalesce_roots ? runtime_coalesce_roots[work_item->value_id]
                                               : value_ssa_alloc_plan_effective_root(
                                                     coalesce_analysis, value_count, work_item->value_id);
        family_members->member_item_indices[write_offsets[root_value_id]++] = item_index;
    }

    free(write_offsets);
    return 1;
}

static void value_ssa_alloc_move_engine_reset_affected_family_roots(ValueSsaAllocatorMoveEngineCurrentQueues *queues) {
    if (!queues || !queues->affected_family_root_flags || !queues->affected_family_roots) {
        return;
    }

    memset(queues->affected_family_root_flags, 0, (queues->value_count ? queues->value_count : 1) * sizeof(unsigned char));
    queues->affected_family_count = 0;
}

static void value_ssa_alloc_move_engine_mark_affected_family_root(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t root_value_id) {
    if (!queues || !queues->affected_family_root_flags || !queues->affected_family_roots ||
        root_value_id >= queues->value_count) {
        return;
    }

    root_value_id = value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, queues->value_count, root_value_id);
    if (root_value_id >= queues->value_count) {
        return;
    }
    if (queues->affected_family_root_flags[root_value_id]) {
        return;
    }

    queues->affected_family_root_flags[root_value_id] = 1u;
    queues->affected_family_roots[queues->affected_family_count++] = root_value_id;
}

static void value_ssa_alloc_move_engine_reset_phase_ready_family_roots(ValueSsaAllocatorMoveEngineCurrentQueues *queues) {
    if (!queues) {
        return;
    }

    if (queues->simplify_family_root_flags) {
        memset(queues->simplify_family_root_flags, 0, (queues->value_count ? queues->value_count : 1));
    }
    if (queues->freeze_family_root_flags) {
        memset(queues->freeze_family_root_flags, 0, (queues->value_count ? queues->value_count : 1));
    }
    if (queues->spill_family_root_flags) {
        memset(queues->spill_family_root_flags, 0, (queues->value_count ? queues->value_count : 1));
    }
    queues->simplify_family_count = 0;
    queues->freeze_family_count = 0;
    queues->spill_family_count = 0;
}

static int value_ssa_alloc_move_engine_root_list_contains(const size_t *roots, size_t count, size_t root_value_id);
static const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *value_ssa_alloc_move_engine_phase_entry_for_root(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t *out_representative_root_value_id);

static size_t value_ssa_alloc_move_engine_resolve_representative_root(
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state, size_t value_count, size_t root_value_id) {
    if (root_value_id >= value_count) {
        return (size_t)-1;
    }
    return value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, root_value_id);
}

static void value_ssa_alloc_move_engine_prepare_phase_entry_identity(
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry, size_t representative_root_value_id) {
    if (!entry) {
        return;
    }

    memset(entry, 0, sizeof(*entry));
    entry->root_value_id = representative_root_value_id;
}

static ValueSsaAllocatorMoveEngineFamilyPhaseEntry *value_ssa_alloc_move_engine_phase_entry_slot_for_root(
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t *out_representative_root_value_id) {
    size_t representative_root_value_id;

    if (!entries) {
        return NULL;
    }

    representative_root_value_id = value_ssa_alloc_move_engine_resolve_representative_root(
        runtime_state, value_count, root_value_id);
    if (out_representative_root_value_id) {
        *out_representative_root_value_id = representative_root_value_id;
    }
    if (representative_root_value_id >= value_count) {
        return NULL;
    }

    return &entries[representative_root_value_id];
}

static void value_ssa_alloc_move_engine_clear_obsolete_root_identity(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t root_value_id) {
    if (!queues || root_value_id >= queues->value_count) {
        return;
    }

    if (queues->family_phase_entries) {
        value_ssa_alloc_move_engine_prepare_phase_entry_identity(
            &queues->family_phase_entries[root_value_id], root_value_id);
    }
    if (queues->affected_family_root_flags) {
        queues->affected_family_root_flags[root_value_id] = 0u;
    }
    if (queues->simplify_family_root_flags) {
        queues->simplify_family_root_flags[root_value_id] = 0u;
    }
    if (queues->freeze_family_root_flags) {
        queues->freeze_family_root_flags[root_value_id] = 0u;
    }
    if (queues->spill_family_root_flags) {
        queues->spill_family_root_flags[root_value_id] = 0u;
    }
    if (runtime_state && runtime_state->family_runtime_facts) {
        value_ssa_alloc_move_engine_clear_family_runtime_facts_entry(
            runtime_state->family_runtime_facts, queues->value_count, root_value_id);
    }
}

static void value_ssa_alloc_move_engine_normalize_root_list(
    size_t *roots,
    size_t *count,
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state) {
    size_t read_index;
    size_t write_index = 0;

    if (!roots || !count || !queues || !runtime_state) {
        return;
    }

    for (read_index = 0; read_index < *count; ++read_index) {
        size_t original_root_value_id = roots[read_index];
        size_t representative_root_value_id =
            value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, queues->value_count, original_root_value_id);

        if (representative_root_value_id >= queues->value_count) {
            if (original_root_value_id < queues->value_count) {
                value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                    queues, runtime_state, original_root_value_id);
            }
            continue;
        }
        if (original_root_value_id != representative_root_value_id && original_root_value_id < queues->value_count) {
            value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                queues, runtime_state, original_root_value_id);
        }
        if (value_ssa_alloc_move_engine_root_list_contains(roots, write_index, representative_root_value_id)) {
            continue;
        }
        roots[write_index++] = representative_root_value_id;
    }

    *count = write_index;
}

static void value_ssa_alloc_move_engine_phase_ready_root_list_compact(
    size_t *roots,
    size_t *count,
    const unsigned char *root_flags,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count) {
    size_t read_index;
    size_t write_index = 0;

    if (!roots || !count || !root_flags) {
        return;
    }

    for (read_index = 0; read_index < *count; ++read_index) {
        size_t root_value_id =
            value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, roots[read_index]);

        if (root_value_id >= value_count || !root_flags[root_value_id]) {
            continue;
        }
        if (value_ssa_alloc_move_engine_root_list_contains(roots, write_index, root_value_id)) {
            continue;
        }
        roots[write_index++] = root_value_id;
    }
    *count = write_index;
}

static void value_ssa_alloc_move_engine_update_phase_ready_root(
    size_t root_value_id,
    int should_be_ready,
    unsigned char *root_flags,
    size_t *roots,
    size_t *count,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count) {
    if (!root_flags || !roots || !count || root_value_id >= value_count) {
        return;
    }

    root_value_id = value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, root_value_id);
    if (root_value_id >= value_count) {
        return;
    }

    if (should_be_ready) {
        if (root_flags[root_value_id]) {
            return;
        }
        root_flags[root_value_id] = 1u;
        roots[(*count)++] = root_value_id;
        return;
    }

    root_flags[root_value_id] = 0u;
}
static int value_ssa_alloc_move_engine_root_list_contains(const size_t *roots, size_t count, size_t root_value_id) {
    size_t root_index;

    if (!roots) {
        return 0;
    }

    for (root_index = 0; root_index < count; ++root_index) {
        if (roots[root_index] == root_value_id) {
            return 1;
        }
    }
    return 0;
}

static void value_ssa_alloc_move_engine_reset_active_family_phase_entries(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues) {
    size_t root_value_id;

    if (!queues || !queues->family_phase_entries || !queues->active_family_roots) {
        return;
    }

    for (root_value_id = 0; root_value_id < queues->value_count; ++root_value_id) {
        value_ssa_alloc_move_engine_prepare_phase_entry_identity(
            &queues->family_phase_entries[root_value_id], root_value_id);
    }
}

static void value_ssa_alloc_move_engine_refresh_active_family_roots(ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    int full_reseed) {
    size_t write_index = 0;
    size_t root_value_id;

    if (!queues || !runtime_state || !queues->active_family_roots ||
        (!runtime_state->active_family_member_counts && !value_ssa_alloc_move_engine_has_family_runtime_surface(runtime_state))) {
        return;
    }

    if (full_reseed) {
        queues->active_family_count = 0;
        memset(queues->affected_family_root_flags, 0, (queues->value_count ? queues->value_count : 1));
        for (root_value_id = 0; root_value_id < queues->value_count; ++root_value_id) {
            size_t representative_root_value_id =
                value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, queues->value_count, root_value_id);

            if (value_ssa_alloc_move_engine_family_active_member_count(
                    runtime_state, queues->value_count, root_value_id) == 0 ||
                representative_root_value_id >= queues->value_count) {
                if (representative_root_value_id < queues->value_count && representative_root_value_id != root_value_id) {
                    value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                        queues, runtime_state, root_value_id);
                }
                continue;
            }
            if (queues->affected_family_root_flags[representative_root_value_id]) {
                if (representative_root_value_id != root_value_id) {
                    value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                        queues, runtime_state, root_value_id);
                }
                continue;
            }
            queues->affected_family_root_flags[representative_root_value_id] = 1u;
            queues->active_family_roots[queues->active_family_count++] = representative_root_value_id;
        }
        memset(queues->affected_family_root_flags, 0, (queues->value_count ? queues->value_count : 1));
        return;
    }

    for (root_value_id = 0; root_value_id < queues->active_family_count; ++root_value_id) {
        size_t active_root_value_id =
            value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state,
                queues->value_count,
                queues->active_family_roots[root_value_id]);

        if (active_root_value_id >= queues->value_count ||
            value_ssa_alloc_move_engine_family_active_member_count(
                runtime_state, queues->value_count, active_root_value_id) == 0) {
            if (queues->active_family_roots[root_value_id] < queues->value_count &&
                queues->active_family_roots[root_value_id] != active_root_value_id) {
                value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                    queues, runtime_state, queues->active_family_roots[root_value_id]);
            }
            continue;
        }
        if (runtime_state->scratch_root_flags) {
            if (runtime_state->scratch_root_flags[active_root_value_id]) {
                if (queues->active_family_roots[root_value_id] != active_root_value_id) {
                    value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                        queues, runtime_state, queues->active_family_roots[root_value_id]);
                }
                continue;
            }
        }
        if (value_ssa_alloc_move_engine_root_list_contains(
                queues->active_family_roots, write_index, active_root_value_id)) {
            if (queues->active_family_roots[root_value_id] != active_root_value_id) {
                value_ssa_alloc_move_engine_clear_obsolete_root_identity(
                    queues, runtime_state, queues->active_family_roots[root_value_id]);
            }
            continue;
        }
        queues->active_family_roots[write_index++] = active_root_value_id;
    }
    queues->active_family_count = write_index;
}

static void value_ssa_alloc_move_engine_reset_family_phase_entry_for_root(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t root_value_id) {
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry;
    ValueSsaAllocatorMoveEngineFamilyStateView family_state;

    if (!queues || !runtime_state || !queues->family_phase_entries || root_value_id >= queues->value_count) {
        return;
    }

    if (!value_ssa_alloc_move_engine_read_family_state(runtime_state, queues->value_count, root_value_id, &family_state)) {
        return;
    }

    entry = value_ssa_alloc_move_engine_phase_entry_slot_for_root(
        queues->family_phase_entries,
        runtime_state,
        queues->value_count,
        family_state.representative_root_value_id,
        NULL);
    if (!entry) {
        return;
    }
    value_ssa_alloc_move_engine_apply_family_state_to_phase_entry(entry, &family_state);
}

static void value_ssa_alloc_move_engine_consider_family_candidate(
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    const ValueSsaAllocatorPlanItem *candidate) {
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry;
    size_t representative_root_value_id = (size_t)-1;
    size_t queue_priority;

    if (!entries || !candidate || candidate->coalesce_root_value_id >= value_count) {
        return;
    }

    entry = value_ssa_alloc_move_engine_phase_entry_slot_for_root(
        entries,
        runtime_state,
        value_count,
        candidate->coalesce_root_value_id,
        &representative_root_value_id);
    if (!entry) {
        return;
    }
    if (entry->root_value_id != representative_root_value_id) {
        entry->root_value_id = representative_root_value_id;
    }
    queue_priority = candidate->move_work_priority;
    if (queue_priority > entry->move_work_priority) {
        entry->move_work_priority = queue_priority;
    }
    switch (candidate->phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        if (!entry->has_simplify ||
            value_ssa_alloc_plan_item_is_better(candidate, &entry->simplify_candidate)) {
            entry->has_simplify = 1;
            entry->simplify_candidate = *candidate;
            entry->simplify_queue_priority = queue_priority;
        }
        break;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        if (!entry->has_freeze ||
            value_ssa_alloc_plan_item_is_better(candidate, &entry->freeze_candidate)) {
            entry->has_freeze = 1;
            entry->freeze_candidate = *candidate;
            entry->freeze_queue_priority = queue_priority;
        }
        break;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
    default:
        if (!entry->has_spill ||
            value_ssa_alloc_plan_item_is_better(candidate, &entry->spill_candidate)) {
            entry->has_spill = 1;
            entry->spill_candidate = *candidate;
            entry->spill_queue_priority = queue_priority;
        }
        break;
    }
}

static void value_ssa_alloc_move_engine_rebuild_family_phase_entry_for_root(
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const size_t *initial_effective_degrees,
    const unsigned char *initial_family_move_related,
    const unsigned char *initial_effective_move_related,
    const unsigned char *was_frozen,
    const unsigned char *scheduled,
    size_t color_budget,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    size_t root_value_id) {
    size_t member_index;
    size_t root_offset;
    size_t root_count;
    size_t family_queue_priority = 0;
    ValueSsaAllocatorMoveEngineFamilyStateView family_state;
    ValueSsaAllocatorPlanItem *base_item;

    if (!prep || !worklist || !family_members || !initial_effective_degrees || !initial_family_move_related ||
        !initial_effective_move_related || !was_frozen || !scheduled || !runtime_state || !queues ||
        !queues->family_phase_entries || root_value_id >= queues->value_count) {
        return;
    }

    if (!value_ssa_alloc_move_engine_read_family_state(runtime_state, queues->value_count, root_value_id, &family_state)) {
        return;
    }

    value_ssa_alloc_move_engine_reset_family_phase_entry_for_root(queues, runtime_state, root_value_id);
    if (family_state.representative_root_value_id >= family_members->value_count) {
        return;
    }
    root_offset = family_members->root_member_offsets[family_state.representative_root_value_id];
    root_count = family_members->root_member_counts[family_state.representative_root_value_id];
    for (member_index = 0; member_index < root_count; ++member_index) {
        size_t item_index = family_members->member_item_indices[root_offset + member_index];
        const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
        ValueSsaAllocatorPlanItem candidate;
        size_t candidate_queue_priority;

        if (work_item->value_id >= queues->value_count || scheduled[work_item->value_id]) {
            continue;
        }

        base_item = runtime_state->work_item_preview_bases ? &runtime_state->work_item_preview_bases[item_index] : NULL;
        if (!base_item || base_item->value_id != work_item->value_id) {
            value_ssa_alloc_build_plan_item_preview_base(prep,
                work_item,
                initial_effective_degrees,
                initial_effective_move_related,
                &candidate);
            base_item = &candidate;
        }

        candidate = *base_item;
        value_ssa_alloc_finish_plan_item_preview_dynamic(prep,
            coalesce_analysis,
            runtime_state->runtime_coalesce_roots,
            runtime_state->runtime_coalesce_class_sizes,
            runtime_state->family_runtime_facts,
            runtime_state->active_degrees,
            runtime_state->active_effective_degrees,
            initial_family_move_related,
            runtime_state->active_move_related,
            runtime_state->active_effective_move_related,
            was_frozen,
            color_budget,
            &candidate);
        candidate_queue_priority = value_ssa_alloc_move_engine_compute_family_queue_priority(candidate.priority,
            family_state.active_member_count,
            family_state.coalesce_ready_neighbor_count,
            family_state.coalesce_ready_affinity_weight_sum,
            family_state.best_coalesce_ready_partner_affinity_weight,
            family_state.best_coalesce_ready_partner_is_mutual,
            family_state.external_neighbor_count,
            family_state.external_affinity_weight_sum);
        candidate.move_work_priority = candidate_queue_priority;
        if (candidate_queue_priority > family_queue_priority) {
            family_queue_priority = candidate_queue_priority;
        }
        value_ssa_alloc_move_engine_consider_family_candidate(
            queues->family_phase_entries, runtime_state, queues->value_count, &candidate);
    }
    value_ssa_alloc_move_engine_set_family_queue_priority(runtime_state,
        queues->value_count,
        family_state.representative_root_value_id,
        family_queue_priority);
}

static void value_ssa_alloc_move_engine_collect_phase_ready_family_roots(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    int full_reseed) {
    size_t root_index;

    if (!queues || !queues->family_phase_entries || !queues->active_family_roots || !queues->simplify_family_roots ||
        !queues->freeze_family_roots || !queues->spill_family_roots || !queues->simplify_family_root_flags ||
        !queues->freeze_family_root_flags || !queues->spill_family_root_flags) {
        return;
    }

    if (full_reseed) {
        value_ssa_alloc_move_engine_reset_phase_ready_family_roots(queues);
        for (root_index = 0; root_index < queues->active_family_count; ++root_index) {
            size_t root_value_id = queues->active_family_roots[root_index];
            const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry =
                value_ssa_alloc_move_engine_phase_entry_for_root(
                    queues->family_phase_entries, runtime_state, queues->value_count, root_value_id, NULL);

            if (!entry) {
                continue;
            }

            value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
                entry->has_simplify,
                queues->simplify_family_root_flags,
                queues->simplify_family_roots,
                &queues->simplify_family_count,
                runtime_state,
                queues->value_count);
            value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
                entry->has_freeze,
                queues->freeze_family_root_flags,
                queues->freeze_family_roots,
                &queues->freeze_family_count,
                runtime_state,
                queues->value_count);
            value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
                entry->has_spill,
                queues->spill_family_root_flags,
                queues->spill_family_roots,
                &queues->spill_family_count,
                runtime_state,
                queues->value_count);
        }
        return;
    }

    for (root_index = 0; root_index < queues->affected_family_count; ++root_index) {
        size_t root_value_id = queues->affected_family_roots[root_index];
        const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry =
            value_ssa_alloc_move_engine_phase_entry_for_root(
                queues->family_phase_entries, runtime_state, queues->value_count, root_value_id, NULL);

        if (!entry) {
            value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
                0,
                queues->simplify_family_root_flags,
                queues->simplify_family_roots,
                &queues->simplify_family_count,
                runtime_state,
                queues->value_count);
            value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
                0,
                queues->freeze_family_root_flags,
                queues->freeze_family_roots,
                &queues->freeze_family_count,
                runtime_state,
                queues->value_count);
            value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
                0,
                queues->spill_family_root_flags,
                queues->spill_family_roots,
                &queues->spill_family_count,
                runtime_state,
                queues->value_count);
            continue;
        }

        value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
            entry->has_simplify,
            queues->simplify_family_root_flags,
            queues->simplify_family_roots,
            &queues->simplify_family_count,
            runtime_state,
            queues->value_count);
        value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
            entry->has_freeze,
            queues->freeze_family_root_flags,
            queues->freeze_family_roots,
            &queues->freeze_family_count,
            runtime_state,
            queues->value_count);
        value_ssa_alloc_move_engine_update_phase_ready_root(root_value_id,
            entry->has_spill,
            queues->spill_family_root_flags,
            queues->spill_family_roots,
            &queues->spill_family_count,
            runtime_state,
            queues->value_count);
    }

    value_ssa_alloc_move_engine_phase_ready_root_list_compact(
        queues->simplify_family_roots,
        &queues->simplify_family_count,
        queues->simplify_family_root_flags,
        runtime_state,
        queues->value_count);
    value_ssa_alloc_move_engine_phase_ready_root_list_compact(
        queues->freeze_family_roots,
        &queues->freeze_family_count,
        queues->freeze_family_root_flags,
        runtime_state,
        queues->value_count);
    value_ssa_alloc_move_engine_phase_ready_root_list_compact(
        queues->spill_family_roots,
        &queues->spill_family_count,
        queues->spill_family_root_flags,
        runtime_state,
        queues->value_count);
}

static void value_ssa_alloc_move_engine_collect_affected_family_roots_for_value(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    size_t value_count,
    size_t value_id,
    int include_interference_neighbors,
    int include_family_affinity_neighbors,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members) {
    size_t root_value_id;

    if (!queues || !scheduled || value_id >= value_count) {
        return;
    }

    root_value_id =
        value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, value_id);
    value_ssa_alloc_move_engine_mark_affected_family_root(queues, runtime_state, root_value_id);
    if (include_interference_neighbors && interference_graph && interference_graph->interferes) {
        size_t neighbor_value_id;

        size_t neighbor_begin = 0;
        size_t neighbor_end = value_count;

        if (runtime_state && runtime_state->interference_neighbor_offsets &&
            runtime_state->interference_neighbor_value_ids) {
            neighbor_begin = runtime_state->interference_neighbor_offsets[value_id];
            neighbor_end = runtime_state->interference_neighbor_offsets[value_id + 1];
        }

        for (neighbor_value_id = neighbor_begin; neighbor_value_id < neighbor_end; ++neighbor_value_id) {
            size_t neighbor_root_value_id;
            size_t neighbor_id =
                runtime_state && runtime_state->interference_neighbor_offsets &&
                    runtime_state->interference_neighbor_value_ids
                ? runtime_state->interference_neighbor_value_ids[neighbor_value_id]
                : neighbor_value_id;

            if (neighbor_id == value_id || scheduled[neighbor_id]) {
                continue;
            }
            if (!(runtime_state && runtime_state->interference_neighbor_offsets &&
                    runtime_state->interference_neighbor_value_ids) &&
                !interference_graph->interferes[value_id * value_count + neighbor_id]) {
                continue;
            }
            neighbor_root_value_id = value_ssa_alloc_move_engine_effective_root(
                runtime_state, coalesce_analysis, value_count, neighbor_id);
            value_ssa_alloc_move_engine_mark_affected_family_root(queues, runtime_state, neighbor_root_value_id);
        }
    }

    if (!affinity_graph || !affinity_graph->weights || !frozen) {
        return;
    }
    if (include_family_affinity_neighbors && worklist && family_members && root_value_id < family_members->value_count) {
        size_t member_index;
        size_t root_offset = family_members->root_member_offsets[root_value_id];
        size_t root_count = family_members->root_member_counts[root_value_id];

        for (member_index = 0; member_index < root_count; ++member_index) {
            size_t item_index = family_members->member_item_indices[root_offset + member_index];
            const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
            size_t family_value_id = work_item->value_id;
            size_t neighbor_index;

            if (family_value_id >= value_count || scheduled[family_value_id]) {
                continue;
            }
            for (neighbor_index = runtime_state->affinity_neighbor_offsets
                     ? runtime_state->affinity_neighbor_offsets[family_value_id]
                     : 0;
                 runtime_state->affinity_neighbor_offsets &&
                 neighbor_index < runtime_state->affinity_neighbor_offsets[family_value_id + 1];
                 ++neighbor_index) {
                size_t neighbor_value_id = runtime_state->affinity_neighbor_value_ids[neighbor_index];
                size_t neighbor_root_value_id;

                if (neighbor_value_id == family_value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                    continue;
                }
                neighbor_root_value_id = value_ssa_alloc_move_engine_effective_root(
                    runtime_state, coalesce_analysis, value_count, neighbor_value_id);
                value_ssa_alloc_move_engine_mark_affected_family_root(queues, runtime_state, neighbor_root_value_id);
            }
        }
    } else {
        size_t neighbor_index;

        for (neighbor_index = runtime_state->affinity_neighbor_offsets
                 ? runtime_state->affinity_neighbor_offsets[value_id]
                 : 0;
             runtime_state->affinity_neighbor_offsets &&
             neighbor_index < runtime_state->affinity_neighbor_offsets[value_id + 1];
             ++neighbor_index) {
            size_t neighbor_value_id = runtime_state->affinity_neighbor_value_ids[neighbor_index];
            size_t neighbor_root_value_id;

            if (neighbor_value_id == value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                continue;
            }
            neighbor_root_value_id = value_ssa_alloc_move_engine_effective_root(
                runtime_state, coalesce_analysis, value_count, neighbor_value_id);
            value_ssa_alloc_move_engine_mark_affected_family_root(queues, runtime_state, neighbor_root_value_id);
        }
    }
}

static void value_ssa_alloc_move_engine_collect_affected_family_roots_for_root(
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    size_t value_count,
    size_t root_value_id,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members) {
    size_t member_index;
    size_t root_offset;
    size_t root_count;

    if (!queues || !worklist || !affinity_graph || !scheduled || !frozen || !family_members ||
        root_value_id >= value_count || root_value_id >= family_members->value_count) {
        return;
    }

    root_value_id =
        value_ssa_alloc_move_engine_effective_root(runtime_state, coalesce_analysis, value_count, root_value_id);
    if (root_value_id >= value_count || root_value_id >= family_members->value_count) {
        return;
    }

    value_ssa_alloc_move_engine_mark_affected_family_root(queues, runtime_state, root_value_id);
    root_offset = family_members->root_member_offsets[root_value_id];
    root_count = family_members->root_member_counts[root_value_id];
    for (member_index = 0; member_index < root_count; ++member_index) {
        size_t item_index = family_members->member_item_indices[root_offset + member_index];
        const ValueSsaAllocationWorkItem *work_item = &worklist->items[item_index];
        size_t value_id = work_item->value_id;
        size_t neighbor_index;

        if (value_id >= value_count || scheduled[value_id] || frozen[value_id]) {
            continue;
        }
        for (neighbor_index = runtime_state->affinity_neighbor_offsets
                 ? runtime_state->affinity_neighbor_offsets[value_id]
                 : 0;
             runtime_state->affinity_neighbor_offsets &&
             neighbor_index < runtime_state->affinity_neighbor_offsets[value_id + 1];
             ++neighbor_index) {
            size_t neighbor_value_id = runtime_state->affinity_neighbor_value_ids[neighbor_index];
            size_t neighbor_root_value_id;

            if (neighbor_value_id == value_id || scheduled[neighbor_value_id] || frozen[neighbor_value_id]) {
                continue;
            }
            neighbor_root_value_id = value_ssa_alloc_move_engine_effective_root(
                runtime_state, coalesce_analysis, value_count, neighbor_value_id);
            value_ssa_alloc_move_engine_mark_affected_family_root(queues, runtime_state, neighbor_root_value_id);
        }
    }
}

static void value_ssa_alloc_move_engine_prepare_current_family_phase_queues(
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const size_t *initial_effective_degrees,
    const unsigned char *initial_family_move_related,
    const unsigned char *initial_effective_move_related,
    const unsigned char *was_frozen,
    const unsigned char *scheduled,
    size_t color_budget,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    int full_reseed) {
    size_t root_index;

    if (!prep || !worklist || !initial_effective_degrees || !initial_family_move_related ||
        !initial_effective_move_related || !was_frozen || !scheduled || !runtime_state || !queues || !family_members ||
        !runtime_state->active_degrees || !runtime_state->active_effective_degrees ||
        !runtime_state->active_move_related || !runtime_state->active_effective_move_related ||
        !value_ssa_alloc_move_engine_has_family_runtime_surface(runtime_state)) {
        return;
    }

    value_ssa_alloc_move_engine_refresh_active_family_roots(queues, runtime_state, full_reseed);
    value_ssa_alloc_move_engine_normalize_root_list(
        queues->active_family_roots, &queues->active_family_count, queues, runtime_state);
    value_ssa_alloc_move_engine_normalize_root_list(
        queues->affected_family_roots, &queues->affected_family_count, queues, runtime_state);
    if (full_reseed) {
        value_ssa_alloc_move_engine_reset_active_family_phase_entries(queues);
        for (root_index = 0; root_index < queues->active_family_count; ++root_index) {
            size_t root_value_id = queues->active_family_roots[root_index];

            value_ssa_alloc_move_engine_rebuild_family_phase_entry_for_root(prep,
                coalesce_analysis,
                worklist,
                family_members,
                initial_effective_degrees,
                initial_family_move_related,
                initial_effective_move_related,
                was_frozen,
                scheduled,
                color_budget,
                runtime_state,
                queues,
                root_value_id);
        }
    } else {
        for (root_index = 0; root_index < queues->affected_family_count; ++root_index) {
            size_t root_value_id = queues->affected_family_roots[root_index];

            if (root_value_id >= queues->value_count) {
                continue;
            }
            value_ssa_alloc_move_engine_reset_family_phase_entry_for_root(queues, runtime_state, root_value_id);
            if (value_ssa_alloc_move_engine_family_active_member_count(
                    runtime_state, queues->value_count, root_value_id) == 0) {
                continue;
            }

            value_ssa_alloc_move_engine_rebuild_family_phase_entry_for_root(prep,
                coalesce_analysis,
                worklist,
                family_members,
                initial_effective_degrees,
                initial_family_move_related,
                initial_effective_move_related,
                was_frozen,
                scheduled,
                color_budget,
                runtime_state,
                queues,
                root_value_id);
        }
    }

    value_ssa_alloc_move_engine_collect_phase_ready_family_roots(queues, runtime_state, full_reseed);
}

static void value_ssa_alloc_move_engine_refresh_current_family_queues(
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const size_t *initial_effective_degrees,
    const unsigned char *initial_family_move_related,
    const unsigned char *initial_effective_move_related,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    const unsigned char *was_frozen,
    size_t value_count,
    size_t color_budget,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    size_t transition_value_id,
    int transition_is_freeze,
    int transition_is_spill_remove,
    int full_reseed) {
    int refresh_full_reseed;

    if (!prep || !worklist || !family_members || !interference_graph || !affinity_graph || !coalesce_analysis ||
        !initial_effective_degrees || !initial_family_move_related || !initial_effective_move_related ||
        !scheduled || !frozen || !was_frozen || !runtime_state || !queues) {
        return;
    }

    refresh_full_reseed = full_reseed || transition_is_freeze ||
        (!transition_is_freeze && transition_value_id < value_count && frozen[transition_value_id]);

    value_ssa_alloc_move_engine_reset_affected_family_roots(queues);
    if (!refresh_full_reseed && transition_value_id < value_count) {
        value_ssa_alloc_move_engine_collect_affected_family_roots_for_value(queues,
            worklist,
            interference_graph,
            affinity_graph,
            coalesce_analysis,
            runtime_state,
            scheduled,
            frozen,
            value_count,
            transition_value_id,
            transition_is_freeze ? 0 : 1,
            transition_is_freeze ? 1 : 0,
            family_members);
    }
    value_ssa_alloc_move_engine_update_active_degrees(
        interference_graph, scheduled, transition_value_id, transition_is_freeze, refresh_full_reseed, runtime_state);
    value_ssa_alloc_move_engine_update_active_move_related(worklist,
        family_members,
        interference_graph,
        affinity_graph,
        coalesce_analysis,
        value_count,
        queues,
        scheduled,
        frozen,
        refresh_full_reseed,
        runtime_state);
    value_ssa_alloc_move_engine_update_effective_value_runtime_state(worklist,
        family_members,
        interference_graph,
        affinity_graph,
        coalesce_analysis,
        value_count,
        queues,
        scheduled,
        frozen,
        refresh_full_reseed,
        runtime_state);
    value_ssa_alloc_move_engine_update_active_family_member_counts(coalesce_analysis,
        value_count,
        scheduled,
        transition_value_id,
        transition_is_freeze,
        refresh_full_reseed,
        runtime_state);
    value_ssa_alloc_move_engine_update_active_family_spill_removed_counts(coalesce_analysis,
        value_count,
        transition_value_id,
        transition_is_spill_remove,
        refresh_full_reseed,
        runtime_state);
    if (refresh_full_reseed) {
        value_ssa_alloc_recompute_active_family_move_pressure(affinity_graph,
            coalesce_analysis,
            scheduled,
            frozen,
            runtime_state->family_neighbor_matrix,
            runtime_state->family_coalesce_ready_neighbor_matrix,
            runtime_state->family_coalesce_ready_affinity_matrix,
            runtime_state->active_family_external_neighbor_counts,
            runtime_state->active_family_coalesce_ready_neighbor_counts,
            runtime_state->active_family_coalesce_ready_affinity_sums,
            runtime_state->active_family_best_coalesce_ready_partner_roots,
            runtime_state->active_family_best_coalesce_ready_partner_weights,
            runtime_state->active_family_best_coalesce_ready_partner_mutual_flags,
            runtime_state->active_family_external_affinity_sums);
    } else {
        value_ssa_alloc_move_engine_recompute_active_family_move_pressure_for_roots(worklist,
            family_members,
            affinity_graph,
            coalesce_analysis,
            scheduled,
            frozen,
            queues->affected_family_roots,
            queues->affected_family_count,
            value_count,
            runtime_state);
    }
    if (refresh_full_reseed) {
        value_ssa_alloc_recompute_active_family_move_work_classes(coalesce_analysis,
            value_count,
            initial_family_move_related,
            was_frozen,
            runtime_state->active_family_member_counts,
            runtime_state->active_family_coalesce_ready_neighbor_counts,
            runtime_state->active_family_move_work_classes);
    } else {
        value_ssa_alloc_move_engine_recompute_active_family_move_work_classes_for_roots(coalesce_analysis,
            value_count,
            initial_family_move_related,
            was_frozen,
            queues->affected_family_roots,
            queues->affected_family_count,
            runtime_state);
    }
    value_ssa_alloc_move_engine_recompute_family_queue_priorities(
        value_count, runtime_state, queues, refresh_full_reseed);
    if (refresh_full_reseed) {
        value_ssa_alloc_move_engine_sync_all_family_runtime_facts(runtime_state, value_count);
    } else {
        value_ssa_alloc_move_engine_sync_family_runtime_facts_for_roots(
            runtime_state, queues->affected_family_roots, queues->affected_family_count, value_count);
    }
    value_ssa_alloc_move_engine_prepare_current_family_phase_queues(prep,
        coalesce_analysis,
        worklist,
        family_members,
        initial_effective_degrees,
        initial_family_move_related,
        initial_effective_move_related,
        was_frozen,
        scheduled,
        color_budget,
        runtime_state,
        queues,
        refresh_full_reseed);
}

static void value_ssa_alloc_move_engine_refresh_current_family_queues_for_coalesce_pair(
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaAllocatorMoveEngineFamilyMembers *family_members,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const size_t *initial_effective_degrees,
    const unsigned char *initial_family_move_related,
    const unsigned char *initial_effective_move_related,
    const unsigned char *scheduled,
    const unsigned char *frozen,
    const unsigned char *was_frozen,
    size_t value_count,
    size_t color_budget,
    ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id) {
    if (!prep || !worklist || !family_members || !interference_graph || !affinity_graph || !coalesce_analysis ||
        !initial_effective_degrees || !initial_family_move_related || !initial_effective_move_related ||
        !scheduled || !frozen || !was_frozen || !runtime_state || !queues) {
        return;
    }

    value_ssa_alloc_move_engine_reset_affected_family_roots(queues);
    value_ssa_alloc_move_engine_collect_affected_family_roots_for_root(
        queues,
        worklist,
        affinity_graph,
        coalesce_analysis,
        runtime_state,
        scheduled,
        frozen,
        value_count,
        lhs_root_value_id,
        family_members);
    value_ssa_alloc_move_engine_collect_affected_family_roots_for_root(
        queues,
        worklist,
        affinity_graph,
        coalesce_analysis,
        runtime_state,
        scheduled,
        frozen,
        value_count,
        rhs_root_value_id,
        family_members);
    value_ssa_alloc_move_engine_update_active_move_related(worklist,
        family_members,
        interference_graph,
        affinity_graph,
        coalesce_analysis,
        value_count,
        queues,
        scheduled,
        frozen,
        0,
        runtime_state);
    value_ssa_alloc_move_engine_update_effective_value_runtime_state(worklist,
        family_members,
        interference_graph,
        affinity_graph,
        coalesce_analysis,
        value_count,
        queues,
        scheduled,
        frozen,
        0,
        runtime_state);
    value_ssa_alloc_move_engine_recompute_active_family_move_pressure_for_roots(worklist,
        family_members,
        affinity_graph,
        coalesce_analysis,
        scheduled,
        frozen,
        queues->affected_family_roots,
        queues->affected_family_count,
        value_count,
        runtime_state);
    value_ssa_alloc_move_engine_recompute_active_family_move_work_classes_for_roots(coalesce_analysis,
        value_count,
        initial_family_move_related,
        was_frozen,
        queues->affected_family_roots,
        queues->affected_family_count,
        runtime_state);
    value_ssa_alloc_move_engine_recompute_family_queue_priorities(
        value_count, runtime_state, queues, 0);
    value_ssa_alloc_move_engine_sync_family_runtime_facts_for_roots(
        runtime_state, queues->affected_family_roots, queues->affected_family_count, value_count);
    value_ssa_alloc_move_engine_prepare_current_family_phase_queues(prep,
        coalesce_analysis,
        worklist,
        family_members,
        initial_effective_degrees,
        initial_family_move_related,
        initial_effective_move_related,
        was_frozen,
        scheduled,
        color_budget,
        runtime_state,
        queues,
        0);
}

static int value_ssa_alloc_move_engine_family_queue_entry_is_before(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *lhs,
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *rhs,
    ValueSsaAllocatorPhase phase) {
    size_t lhs_phase_queue_priority = 0;
    size_t rhs_phase_queue_priority = 0;
    int lhs_work_rank = 0;
    int rhs_work_rank = 0;

    if (!lhs || !rhs) {
        return lhs != NULL;
    }
    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        lhs_phase_queue_priority = lhs->simplify_queue_priority;
        rhs_phase_queue_priority = rhs->simplify_queue_priority;
        lhs_work_rank = value_ssa_alloc_simplify_move_work_rank(lhs->work_class);
        rhs_work_rank = value_ssa_alloc_simplify_move_work_rank(rhs->work_class);
        break;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        lhs_phase_queue_priority = lhs->freeze_queue_priority;
        rhs_phase_queue_priority = rhs->freeze_queue_priority;
        lhs_work_rank = value_ssa_alloc_simplify_move_work_rank(lhs->work_class);
        rhs_work_rank = value_ssa_alloc_simplify_move_work_rank(rhs->work_class);
        break;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
    default:
        lhs_phase_queue_priority = lhs->spill_queue_priority;
        rhs_phase_queue_priority = rhs->spill_queue_priority;
        lhs_work_rank = value_ssa_alloc_spill_move_work_rank(lhs->work_class);
        rhs_work_rank = value_ssa_alloc_spill_move_work_rank(rhs->work_class);
        break;
    }
    if (lhs_work_rank < rhs_work_rank) {
        return 1;
    }
    if (lhs_work_rank > rhs_work_rank) {
        return 0;
    }
    if ((phase == VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY || phase == VALUE_SSA_ALLOCATOR_PHASE_FREEZE) &&
        lhs->work_class == VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY &&
        rhs->work_class == VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY) {
        if ((int)lhs->family_best_coalesce_ready_partner_is_mutual !=
            (int)rhs->family_best_coalesce_ready_partner_is_mutual) {
            return (int)lhs->family_best_coalesce_ready_partner_is_mutual >
                (int)rhs->family_best_coalesce_ready_partner_is_mutual;
        }
        if (lhs->family_best_coalesce_ready_partner_affinity_weight !=
            rhs->family_best_coalesce_ready_partner_affinity_weight) {
            return lhs->family_best_coalesce_ready_partner_affinity_weight >
                rhs->family_best_coalesce_ready_partner_affinity_weight;
        }
    }
    if (lhs_phase_queue_priority > rhs_phase_queue_priority) {
        return 1;
    }
    if (lhs_phase_queue_priority < rhs_phase_queue_priority) {
        return 0;
    }
    return lhs->root_value_id < rhs->root_value_id;
}

static size_t value_ssa_alloc_move_engine_phase_queue_priority_for_entry(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry, ValueSsaAllocatorPhase phase) {
    if (!entry) {
        return 0;
    }

    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        return entry->simplify_queue_priority;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        return entry->freeze_queue_priority;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
    default:
        return entry->spill_queue_priority;
    }
}

static const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *value_ssa_alloc_move_engine_phase_entry_for_root(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    size_t *out_representative_root_value_id) {
    size_t representative_root_value_id;

    if (!entries || !runtime_state || root_value_id >= value_count) {
        return NULL;
    }

    representative_root_value_id =
        value_ssa_alloc_move_engine_runtime_root_value_id(runtime_state, value_count, root_value_id);
    if (out_representative_root_value_id) {
        *out_representative_root_value_id = representative_root_value_id;
    }
    if (representative_root_value_id >= value_count) {
        return NULL;
    }

    return &entries[representative_root_value_id];
}

static int value_ssa_alloc_move_engine_family_entry_has_phase_candidate(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry,
    ValueSsaAllocatorPhase phase) {
    if (!entry) {
        return 0;
    }

    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        return entry->has_simplify;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        return entry->has_freeze;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
    default:
        return entry->has_spill;
    }
}

static const ValueSsaAllocatorPlanItem *value_ssa_alloc_move_engine_family_entry_phase_candidate(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry,
    ValueSsaAllocatorPhase phase) {
    if (!value_ssa_alloc_move_engine_family_entry_has_phase_candidate(entry, phase)) {
        return NULL;
    }

    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        return &entry->simplify_candidate;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        return &entry->freeze_candidate;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
    default:
        return &entry->spill_candidate;
    }
}

static int value_ssa_alloc_move_engine_compute_family_phase_pair_priority(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    ValueSsaAllocatorPhase phase,
    int *out_has_pair,
    size_t *out_priority) {
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry;
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *partner_entry;
    size_t representative_root_value_id;
    size_t partner_root_value_id;

    if (out_has_pair) {
        *out_has_pair = 0;
    }
    if (out_priority) {
        *out_priority = 0;
    }
    if (!entries || !runtime_state || !out_has_pair || !out_priority || root_value_id >= value_count) {
        return 0;
    }

    entry = value_ssa_alloc_move_engine_phase_entry_for_root(
        entries, runtime_state, value_count, root_value_id, &representative_root_value_id);
    if (!entry) {
        return 1;
    }
    if ((phase != VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY && phase != VALUE_SSA_ALLOCATOR_PHASE_FREEZE) ||
        entry->work_class != VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY ||
        !entry->family_best_coalesce_ready_partner_is_mutual ||
        entry->family_best_coalesce_ready_partner_root_value_id == (size_t)-1) {
        return 1;
    }

    partner_root_value_id = value_ssa_alloc_move_engine_runtime_root_value_id(
        runtime_state, value_count, entry->family_best_coalesce_ready_partner_root_value_id);
    if (partner_root_value_id >= value_count || partner_root_value_id == representative_root_value_id) {
        return 1;
    }
    partner_entry = value_ssa_alloc_move_engine_phase_entry_for_root(
        entries, runtime_state, value_count, partner_root_value_id, NULL);
    if (!partner_entry) {
        return 1;
    }
    if (partner_entry->work_class != VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY ||
        !value_ssa_alloc_move_engine_family_entry_has_phase_candidate(partner_entry, phase)) {
        return 1;
    }

    *out_has_pair = 1;
    *out_priority = (entry->family_best_coalesce_ready_partner_affinity_weight +
                        partner_entry->family_best_coalesce_ready_partner_affinity_weight) *
        16;
    *out_priority += entry->move_work_priority > partner_entry->move_work_priority
        ? entry->move_work_priority
        : partner_entry->move_work_priority;
    return 1;
}

static int value_ssa_alloc_move_engine_read_phase_candidate_view(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    ValueSsaAllocatorPhase phase,
    int *out_found,
    ValueSsaAllocatorMoveEnginePhaseCandidateView *out_view) {
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entry;
    const ValueSsaAllocatorPlanItem *candidate;
    size_t representative_root_value_id = (size_t)-1;
    int has_pair = 0;
    size_t pair_priority = 0;

    if (out_found) {
        *out_found = 0;
    }
    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->representative_root_value_id = (size_t)-1;
    }
    if (!entries || !runtime_state || !out_found || !out_view) {
        return 0;
    }

    entry = value_ssa_alloc_move_engine_phase_entry_for_root(
        entries, runtime_state, value_count, root_value_id, &representative_root_value_id);
    if (!entry) {
        return 1;
    }

    candidate = value_ssa_alloc_move_engine_family_entry_phase_candidate(entry, phase);
    if (!candidate) {
        return 1;
    }
    if (!value_ssa_alloc_move_engine_compute_family_phase_pair_priority(
            entries,
            runtime_state,
            value_count,
            representative_root_value_id,
            phase,
            &has_pair,
            &pair_priority)) {
        return 0;
    }

    out_view->entry = entry;
    out_view->candidate = candidate;
    out_view->representative_root_value_id = representative_root_value_id;
    out_view->has_pair = has_pair;
    out_view->pair_priority = pair_priority;
    *out_found = 1;
    return 1;
}

static int value_ssa_alloc_move_engine_read_phase_pair_view(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    size_t root_value_id,
    ValueSsaAllocatorPhase phase,
    int *out_found,
    ValueSsaAllocatorMoveEnginePhasePairView *out_view) {
    ValueSsaAllocatorMoveEnginePhaseCandidateView lhs_view;
    ValueSsaAllocatorMoveEnginePhaseCandidateView rhs_view;
    size_t partner_root_value_id;
    int has_lhs = 0;
    int has_rhs = 0;

    if (out_found) {
        *out_found = 0;
    }
    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->lhs_representative_root_value_id = (size_t)-1;
        out_view->rhs_representative_root_value_id = (size_t)-1;
    }
    if (!entries || !runtime_state || !out_found || !out_view) {
        return 0;
    }

    if (!value_ssa_alloc_move_engine_read_phase_candidate_view(
            entries, runtime_state, value_count, root_value_id, phase, &has_lhs, &lhs_view)) {
        return 0;
    }
    if (!has_lhs || !lhs_view.has_pair || !lhs_view.entry ||
        lhs_view.entry->work_class != VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY ||
        lhs_view.entry->family_best_coalesce_ready_partner_root_value_id == (size_t)-1) {
        return 1;
    }

    partner_root_value_id = value_ssa_alloc_move_engine_runtime_root_value_id(
        runtime_state, value_count, lhs_view.entry->family_best_coalesce_ready_partner_root_value_id);
    if (partner_root_value_id >= value_count || partner_root_value_id == lhs_view.representative_root_value_id) {
        return 1;
    }
    if (!value_ssa_alloc_move_engine_read_phase_candidate_view(
            entries, runtime_state, value_count, partner_root_value_id, phase, &has_rhs, &rhs_view)) {
        return 0;
    }
    if (!has_rhs || !rhs_view.entry || !rhs_view.has_pair ||
        rhs_view.entry->work_class != VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY ||
        value_ssa_alloc_move_engine_runtime_root_value_id(
            runtime_state, value_count, rhs_view.entry->family_best_coalesce_ready_partner_root_value_id) !=
            lhs_view.representative_root_value_id ||
        !rhs_view.entry->family_best_coalesce_ready_partner_is_mutual) {
        return 1;
    }

    out_view->lhs_entry = lhs_view.entry;
    out_view->rhs_entry = rhs_view.entry;
    out_view->lhs_candidate = lhs_view.candidate;
    out_view->rhs_candidate = rhs_view.candidate;
    out_view->lhs_representative_root_value_id = lhs_view.representative_root_value_id;
    out_view->rhs_representative_root_value_id = rhs_view.representative_root_value_id;
    out_view->pair_priority = lhs_view.pair_priority;
    *out_found = 1;
    return 1;
}

static void value_ssa_alloc_move_engine_pick_best_family_phase_coalesce_action(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    const size_t *phase_ready_family_roots,
    size_t phase_ready_family_count,
    ValueSsaAllocatorPhase phase,
    ValueSsaAllocatorMoveEngineCoalesceAction *out_action) {
    size_t phase_ready_index;

    if (!out_action) {
        return;
    }

    memset(out_action, 0, sizeof(*out_action));
    out_action->phase = phase;
    out_action->lhs_root_value_id = (size_t)-1;
    out_action->rhs_root_value_id = (size_t)-1;
    out_action->lhs_value_id = (size_t)-1;
    out_action->rhs_value_id = (size_t)-1;
    if (!entries || !runtime_state || !phase_ready_family_roots) {
        return;
    }

    for (phase_ready_index = 0; phase_ready_index < phase_ready_family_count; ++phase_ready_index) {
        ValueSsaAllocatorMoveEnginePhasePairView pair_view;
        size_t root_value_id = phase_ready_family_roots[phase_ready_index];
        int has_pair = 0;

        if (!value_ssa_alloc_move_engine_read_phase_pair_view(
                entries, runtime_state, value_count, root_value_id, phase, &has_pair, &pair_view) ||
            !has_pair) {
            continue;
        }
        if (pair_view.rhs_representative_root_value_id <= pair_view.lhs_representative_root_value_id) {
            continue;
        }

        if (!out_action->found || pair_view.pair_priority > out_action->priority ||
            (pair_view.pair_priority == out_action->priority &&
                (pair_view.lhs_representative_root_value_id < out_action->lhs_root_value_id ||
                    (pair_view.lhs_representative_root_value_id == out_action->lhs_root_value_id &&
                        pair_view.rhs_representative_root_value_id < out_action->rhs_root_value_id)))) {
            out_action->found = 1;
            out_action->phase = phase;
            out_action->lhs_root_value_id = pair_view.lhs_representative_root_value_id;
            out_action->rhs_root_value_id = pair_view.rhs_representative_root_value_id;
            out_action->lhs_value_id = pair_view.lhs_candidate->value_id;
            out_action->rhs_value_id = pair_view.rhs_candidate->value_id;
            out_action->priority = pair_view.pair_priority;
            out_action->lhs_entry = *pair_view.lhs_entry;
            out_action->rhs_entry = *pair_view.rhs_entry;
        }
    }
}

static void value_ssa_alloc_move_engine_note_phase_scan_count(ValueSsaAllocatorMoveEngineRunResult *result,
    ValueSsaAllocatorPhase phase,
    size_t candidate_count,
    int pair_scan) {
    if (!result) {
        return;
    }

    switch (phase) {
    case VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY:
        if (pair_scan) {
            result->simplify_pair_scan_total += candidate_count;
        } else {
            result->simplify_candidate_scan_total += candidate_count;
        }
        break;
    case VALUE_SSA_ALLOCATOR_PHASE_FREEZE:
        if (pair_scan) {
            result->freeze_pair_scan_total += candidate_count;
        } else {
            result->freeze_candidate_scan_total += candidate_count;
        }
        break;
    case VALUE_SSA_ALLOCATOR_PHASE_SPILL:
    default:
        if (!pair_scan) {
            result->spill_candidate_scan_total += candidate_count;
        }
        break;
    }
}

static void value_ssa_alloc_move_engine_choose_best_coalesce_action(
    const ValueSsaAllocatorMoveEngineCoalesceAction *simplify_action,
    const ValueSsaAllocatorMoveEngineCoalesceAction *freeze_action,
    ValueSsaAllocatorMoveEngineCoalesceAction *out_action) {
    if (!out_action) {
        return;
    }

    memset(out_action, 0, sizeof(*out_action));
    out_action->lhs_root_value_id = (size_t)-1;
    out_action->rhs_root_value_id = (size_t)-1;
    out_action->lhs_value_id = (size_t)-1;
    out_action->rhs_value_id = (size_t)-1;

    if (simplify_action && simplify_action->found) {
        *out_action = *simplify_action;
    }
    if (freeze_action && freeze_action->found &&
        (!out_action->found || freeze_action->priority > out_action->priority ||
            (freeze_action->priority == out_action->priority &&
                value_ssa_alloc_phase_selection_rank(freeze_action->phase) <
                    value_ssa_alloc_phase_selection_rank(out_action->phase)) ||
            (freeze_action->priority == out_action->priority &&
                freeze_action->phase == out_action->phase &&
                (freeze_action->lhs_root_value_id < out_action->lhs_root_value_id ||
                    (freeze_action->lhs_root_value_id == out_action->lhs_root_value_id &&
                        freeze_action->rhs_root_value_id < out_action->rhs_root_value_id))))) {
        *out_action = *freeze_action;
    }
}

static void value_ssa_alloc_move_engine_pick_best_family_phase_candidate(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    const size_t *phase_ready_family_roots,
    size_t phase_ready_family_count,
    ValueSsaAllocatorPhase phase,
    int *out_found,
    ValueSsaAllocatorPlanItem *out_candidate,
    ValueSsaAllocatorMoveEnginePhaseCandidateView *out_view) {
    size_t phase_ready_index;

    if (out_found) {
        *out_found = 0;
    }
    if (out_candidate) {
        memset(out_candidate, 0, sizeof(*out_candidate));
    }
    if (out_view) {
        memset(out_view, 0, sizeof(*out_view));
        out_view->representative_root_value_id = (size_t)-1;
    }
    if (!entries || !runtime_state || !phase_ready_family_roots || !out_found || !out_candidate || !out_view) {
        return;
    }

    for (phase_ready_index = 0; phase_ready_index < phase_ready_family_count; ++phase_ready_index) {
        ValueSsaAllocatorMoveEnginePhaseCandidateView candidate_view;
        ValueSsaAllocatorMoveEnginePhaseCandidateView best_view;
        size_t root_value_id = phase_ready_family_roots[phase_ready_index];
        int has_candidate = 0;

        if (!value_ssa_alloc_move_engine_read_phase_candidate_view(
                entries, runtime_state, value_count, root_value_id, phase, &has_candidate, &candidate_view) ||
            !has_candidate) {
            continue;
        }
        if (!*out_found) {
            *out_found = 1;
            *out_candidate = *candidate_view.candidate;
            *out_view = candidate_view;
            continue;
        }
        if (!value_ssa_alloc_move_engine_read_phase_candidate_view(entries,
                runtime_state,
                value_count,
                out_candidate->coalesce_root_value_id,
                phase,
                &has_candidate,
                &best_view) ||
            !has_candidate) {
            continue;
        }
        if (candidate_view.has_pair != best_view.has_pair) {
            if (candidate_view.has_pair > best_view.has_pair) {
                *out_candidate = *candidate_view.candidate;
                *out_view = candidate_view;
            }
            continue;
        }
        if (candidate_view.has_pair && best_view.has_pair && candidate_view.pair_priority != best_view.pair_priority) {
            if (candidate_view.pair_priority > best_view.pair_priority) {
                *out_candidate = *candidate_view.candidate;
                *out_view = candidate_view;
            }
            continue;
        }
        if (!*out_found || value_ssa_alloc_plan_item_is_better(candidate_view.candidate, out_candidate) ||
            (!value_ssa_alloc_plan_item_is_better(out_candidate, candidate_view.candidate) &&
                best_view.entry &&
                value_ssa_alloc_move_engine_family_queue_entry_is_before(
                    candidate_view.entry, best_view.entry, phase))) {
            *out_found = 1;
            *out_candidate = *candidate_view.candidate;
            *out_view = candidate_view;
        }
    }
}

static void value_ssa_alloc_move_engine_decision_init(ValueSsaAllocatorMoveEngineDecision *decision) {
    if (!decision) {
        return;
    }

    memset(decision, 0, sizeof(*decision));
    decision->kind = VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_NONE;
    decision->phase = VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;
    decision->removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_NONE;
    decision->candidate_root_value_id = (size_t)-1;
    decision->candidate_entry.root_value_id = (size_t)-1;
    decision->coalesce_action.lhs_root_value_id = (size_t)-1;
    decision->coalesce_action.rhs_root_value_id = (size_t)-1;
    decision->coalesce_action.lhs_value_id = (size_t)-1;
    decision->coalesce_action.rhs_value_id = (size_t)-1;
    decision->coalesce_action.lhs_entry.root_value_id = (size_t)-1;
    decision->coalesce_action.rhs_entry.root_value_id = (size_t)-1;
}

static int value_ssa_alloc_move_engine_decision_has_action(const ValueSsaAllocatorMoveEngineDecision *decision) {
    return decision && decision->kind != VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_NONE;
}

static void value_ssa_alloc_move_engine_step_result_init(ValueSsaAllocatorMoveEngineStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_STEP_CONTINUE;
    result->decision_kind = VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_NONE;
    result->phase = VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;
    result->removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_NONE;
    result->value_id = (size_t)-1;
    result->root_value_id = (size_t)-1;
    result->refresh_kind = VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_NONE;
    result->transition_value_id = (size_t)-1;
    result->transition_is_freeze = 0;
    result->transition_is_spill_remove = 0;
    result->partner_root_value_id = (size_t)-1;
    result->partner_value_id = (size_t)-1;
    result->coalesce_priority = 0;
    result->append_trace = 0;
    result->record_plan_item = 0;
    result->has_partner_entry = 0;
    memset(&result->before_entry, 0, sizeof(result->before_entry));
    memset(&result->before_partner_entry, 0, sizeof(result->before_partner_entry));
    memset(&result->plan_item, 0, sizeof(result->plan_item));
}

static void value_ssa_alloc_move_engine_plan_item_result_init(ValueSsaAllocatorMoveEnginePlanItemResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_NONE;
}

static void value_ssa_alloc_move_engine_plan_driver_result_init(ValueSsaAllocatorMoveEnginePlanDriverResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_DRIVER_NONE;
}

static void value_ssa_alloc_move_engine_run_result_init(ValueSsaAllocatorMoveEngineRunResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_NONE;
    value_ssa_alloc_move_engine_plan_driver_result_init(&result->plan_driver);
}

static void value_ssa_alloc_move_engine_note_step_for_run_result(
    ValueSsaAllocatorMoveEngineRunResult *result, const ValueSsaAllocatorMoveEngineStepResult *step_result) {
    if (!result || !step_result) {
        return;
    }

    switch (step_result->decision_kind) {
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE:
            ++result->coalesce_step_count;
            return;
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE:
            ++result->freeze_step_count;
            return;
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE:
            if (step_result->removal_kind == VALUE_SSA_ALLOCATOR_REMOVE_SPILL) {
                ++result->spill_remove_step_count;
            } else if (step_result->removal_kind == VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY) {
                ++result->simplify_remove_step_count;
            }
            return;
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_NONE:
        default:
            return;
    }
}

static void value_ssa_alloc_move_engine_note_refresh_for_run_result(ValueSsaAllocatorMoveEngineRunResult *result,
    ValueSsaAllocatorMoveEngineRefreshKind refresh_kind,
    size_t affected_family_root_count) {
    if (!result) {
        return;
    }

    switch (refresh_kind) {
    case VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_GENERAL:
        ++result->general_refresh_count;
        break;
    case VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_COALESCE_PAIR:
        ++result->coalesce_refresh_count;
        break;
    case VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_NONE:
    default:
        break;
    }

    result->refresh_affected_family_root_total += affected_family_root_count;
    if (affected_family_root_count > result->refresh_affected_family_root_max) {
        result->refresh_affected_family_root_max = affected_family_root_count;
    }
    result->phase_entry_rebuild_count += affected_family_root_count;
}

static void value_ssa_alloc_move_engine_prepare_general_refresh_result(
    ValueSsaAllocatorMoveEngineStepResult *result,
    size_t transition_value_id,
    int transition_is_freeze,
    int transition_is_spill_remove);
static void value_ssa_alloc_move_engine_prepare_coalesce_pair_refresh_result(
    ValueSsaAllocatorMoveEngineStepResult *result, size_t lhs_root_value_id, size_t rhs_root_value_id);
static int value_ssa_alloc_move_engine_refresh_after_step_result(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx, const ValueSsaAllocatorMoveEngineStepResult *result);
static void value_ssa_alloc_move_engine_plan_item_result_init(ValueSsaAllocatorMoveEnginePlanItemResult *result);
static void value_ssa_alloc_move_engine_plan_driver_result_init(ValueSsaAllocatorMoveEnginePlanDriverResult *result);
static void value_ssa_alloc_move_engine_run_result_init(ValueSsaAllocatorMoveEngineRunResult *result);

static void value_ssa_alloc_move_engine_choose_current_phase_decision(
    const ValueSsaAllocatorMoveEngineFamilyPhaseEntry *entries,
    const ValueSsaAllocatorMoveEngineRuntimeState *runtime_state,
    size_t value_count,
    const ValueSsaAllocatorMoveEngineCurrentQueues *queues,
    ValueSsaAllocatorMoveEngineRunResult *out_run_result,
    ValueSsaAllocatorMoveEngineDecision *out_decision) {
    int found_simplify = 0;
    int found_freeze = 0;
    int found_spill = 0;
    ValueSsaAllocatorMoveEngineCoalesceAction coalesce_action;
    ValueSsaAllocatorMoveEngineCoalesceAction simplify_coalesce_action;
    ValueSsaAllocatorMoveEngineCoalesceAction freeze_coalesce_action;
    ValueSsaAllocatorPlanItem best_simplify;
    ValueSsaAllocatorPlanItem best_freeze;
    ValueSsaAllocatorPlanItem best_spill;
    ValueSsaAllocatorMoveEnginePhaseCandidateView best_simplify_view;
    ValueSsaAllocatorMoveEnginePhaseCandidateView best_freeze_view;
    ValueSsaAllocatorMoveEnginePhaseCandidateView best_spill_view;

    if (!entries || !runtime_state || !queues || !out_decision) {
        return;
    }

    value_ssa_alloc_move_engine_decision_init(out_decision);
    memset(&coalesce_action, 0, sizeof(coalesce_action));
    memset(&best_simplify, 0, sizeof(best_simplify));
    memset(&best_freeze, 0, sizeof(best_freeze));
    memset(&best_spill, 0, sizeof(best_spill));
    memset(&best_simplify_view, 0, sizeof(best_simplify_view));
    memset(&best_freeze_view, 0, sizeof(best_freeze_view));
    memset(&best_spill_view, 0, sizeof(best_spill_view));
    best_simplify_view.representative_root_value_id = (size_t)-1;
    best_freeze_view.representative_root_value_id = (size_t)-1;
    best_spill_view.representative_root_value_id = (size_t)-1;

    value_ssa_alloc_move_engine_note_phase_scan_count(
        out_run_result, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY, queues->simplify_family_count, 0);
    value_ssa_alloc_move_engine_pick_best_family_phase_candidate(entries,
        runtime_state,
        value_count,
        queues->simplify_family_roots,
        queues->simplify_family_count,
        VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY,
        &found_simplify,
        &best_simplify,
        &best_simplify_view);
    value_ssa_alloc_move_engine_note_phase_scan_count(
        out_run_result, VALUE_SSA_ALLOCATOR_PHASE_FREEZE, queues->freeze_family_count, 0);
    value_ssa_alloc_move_engine_pick_best_family_phase_candidate(entries,
        runtime_state,
        value_count,
        queues->freeze_family_roots,
        queues->freeze_family_count,
        VALUE_SSA_ALLOCATOR_PHASE_FREEZE,
        &found_freeze,
        &best_freeze,
        &best_freeze_view);
    value_ssa_alloc_move_engine_note_phase_scan_count(
        out_run_result, VALUE_SSA_ALLOCATOR_PHASE_SPILL, queues->spill_family_count, 0);
    value_ssa_alloc_move_engine_pick_best_family_phase_candidate(entries,
        runtime_state,
        value_count,
        queues->spill_family_roots,
        queues->spill_family_count,
        VALUE_SSA_ALLOCATOR_PHASE_SPILL,
        &found_spill,
        &best_spill,
        &best_spill_view);

    value_ssa_alloc_move_engine_note_phase_scan_count(
        out_run_result, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY, queues->simplify_family_count, 1);
    value_ssa_alloc_move_engine_pick_best_family_phase_coalesce_action(entries,
        runtime_state,
        value_count,
        queues->simplify_family_roots,
        queues->simplify_family_count,
        VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY,
        &simplify_coalesce_action);
    value_ssa_alloc_move_engine_note_phase_scan_count(
        out_run_result, VALUE_SSA_ALLOCATOR_PHASE_FREEZE, queues->freeze_family_count, 1);
    value_ssa_alloc_move_engine_pick_best_family_phase_coalesce_action(entries,
        runtime_state,
        value_count,
        queues->freeze_family_roots,
        queues->freeze_family_count,
        VALUE_SSA_ALLOCATOR_PHASE_FREEZE,
        &freeze_coalesce_action);
    value_ssa_alloc_move_engine_choose_best_coalesce_action(
        &simplify_coalesce_action, &freeze_coalesce_action, &coalesce_action);

    if (coalesce_action.found) {
        int coalesce_beats_best_competing_phase = 0;
        int has_competing_phase = 0;
        size_t best_competing_signal = 0;
        ValueSsaAllocatorPhase best_competing_phase = VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;

        if (found_simplify && best_simplify_view.entry) {
            has_competing_phase = 1;
            best_competing_signal = value_ssa_alloc_move_engine_phase_queue_priority_for_entry(
                best_simplify_view.entry, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
            best_competing_phase = VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;
        }
        if (found_freeze && best_freeze_view.entry) {
            size_t freeze_signal = value_ssa_alloc_move_engine_phase_queue_priority_for_entry(
                best_freeze_view.entry, VALUE_SSA_ALLOCATOR_PHASE_FREEZE);

            if (!has_competing_phase || freeze_signal > best_competing_signal ||
                (freeze_signal == best_competing_signal &&
                    value_ssa_alloc_phase_selection_rank(VALUE_SSA_ALLOCATOR_PHASE_FREEZE) <
                        value_ssa_alloc_phase_selection_rank(best_competing_phase))) {
                has_competing_phase = 1;
                best_competing_signal = freeze_signal;
                best_competing_phase = VALUE_SSA_ALLOCATOR_PHASE_FREEZE;
            }
        }
        if (found_spill && best_spill_view.entry) {
            size_t spill_signal = value_ssa_alloc_move_engine_phase_queue_priority_for_entry(
                best_spill_view.entry, VALUE_SSA_ALLOCATOR_PHASE_SPILL);

            if (!has_competing_phase || spill_signal > best_competing_signal ||
                (spill_signal == best_competing_signal &&
                    value_ssa_alloc_phase_selection_rank(VALUE_SSA_ALLOCATOR_PHASE_SPILL) <
                        value_ssa_alloc_phase_selection_rank(best_competing_phase))) {
                has_competing_phase = 1;
                best_competing_signal = spill_signal;
                best_competing_phase = VALUE_SSA_ALLOCATOR_PHASE_SPILL;
            }
        }

        if (!has_competing_phase || coalesce_action.priority >= best_competing_signal) {
            coalesce_beats_best_competing_phase = 1;
        }

        if (coalesce_beats_best_competing_phase) {
            out_decision->kind = VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE;
            out_decision->phase = coalesce_action.phase;
            out_decision->coalesce_action = coalesce_action;
            return;
        }
    }
    if (found_simplify) {
        out_decision->kind = VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE;
        out_decision->phase = VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY;
        out_decision->removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
        out_decision->candidate = best_simplify;
        out_decision->candidate.coalesce_root_value_id = best_simplify_view.representative_root_value_id;
        out_decision->candidate_root_value_id = best_simplify_view.representative_root_value_id;
        if (best_simplify_view.entry) {
            out_decision->has_candidate_entry = 1u;
            out_decision->candidate_entry = *best_simplify_view.entry;
        }
        return;
    }
    if (found_freeze) {
        out_decision->kind = VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE;
        out_decision->phase = VALUE_SSA_ALLOCATOR_PHASE_FREEZE;
        out_decision->candidate = best_freeze;
        out_decision->candidate.coalesce_root_value_id = best_freeze_view.representative_root_value_id;
        out_decision->candidate_root_value_id = best_freeze_view.representative_root_value_id;
        if (best_freeze_view.entry) {
            out_decision->has_candidate_entry = 1u;
            out_decision->candidate_entry = *best_freeze_view.entry;
        }
        return;
    }
    if (found_spill) {
        out_decision->kind = VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE;
        out_decision->phase = VALUE_SSA_ALLOCATOR_PHASE_SPILL;
        out_decision->removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
        out_decision->candidate = best_spill;
        out_decision->candidate.coalesce_root_value_id = best_spill_view.representative_root_value_id;
        out_decision->candidate_root_value_id = best_spill_view.representative_root_value_id;
        if (best_spill_view.entry) {
            out_decision->has_candidate_entry = 1u;
            out_decision->candidate_entry = *best_spill_view.entry;
        }
    }
}

static int value_ssa_alloc_move_engine_execute_coalesce_decision(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx,
    const ValueSsaAllocatorMoveEngineDecision *decision,
    ValueSsaAllocatorMoveEngineStepResult *out_result) {
    size_t lhs_root_value_id;
    size_t rhs_root_value_id;

    if (!ctx || !ctx->function || !ctx->runtime_state || !ctx->queues || !ctx->family_members || !decision ||
        decision->kind != VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE) {
        return 0;
    }

    lhs_root_value_id = decision->coalesce_action.lhs_root_value_id;
    rhs_root_value_id = decision->coalesce_action.rhs_root_value_id;
    out_result->append_trace = 1u;
    out_result->has_partner_entry = 1u;
    out_result->before_entry = decision->coalesce_action.lhs_entry;
    out_result->before_partner_entry = decision->coalesce_action.rhs_entry;
    out_result->partner_value_id = decision->coalesce_action.rhs_value_id;
    out_result->coalesce_priority = decision->coalesce_action.priority;

    value_ssa_alloc_move_engine_consume_family_pair(
        ctx->runtime_state, ctx->function->next_value_id, lhs_root_value_id, rhs_root_value_id);
    value_ssa_alloc_move_engine_merge_runtime_coalesce_families(
        ctx->runtime_state, ctx->function->next_value_id, lhs_root_value_id, rhs_root_value_id);
    value_ssa_alloc_move_engine_clear_obsolete_root_identity(ctx->queues,
        ctx->runtime_state,
        lhs_root_value_id < rhs_root_value_id ? rhs_root_value_id : lhs_root_value_id);
    if (!value_ssa_alloc_move_engine_build_family_members(ctx->worklist,
            ctx->coalesce_analysis,
            ctx->runtime_state->runtime_coalesce_roots,
            ctx->function->next_value_id,
            ctx->family_members)) {
        value_ssa_alloc_set_error(ctx->error, 0, 0, "VALUE-SSA-ALLOC-123: out of memory rebuilding runtime family member slices");
        return 0;
    }
    if (ctx->out_run_result) {
        ++ctx->out_run_result->family_member_rebuild_count;
    }
    value_ssa_alloc_move_engine_prepare_coalesce_pair_refresh_result(out_result, lhs_root_value_id, rhs_root_value_id);
    return 1;
}

static int value_ssa_alloc_move_engine_execute_freeze_decision(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx,
    const ValueSsaAllocatorMoveEngineDecision *decision,
    ValueSsaAllocatorMoveEngineStepResult *out_result) {
    size_t root_value_id;
    size_t root_offset;
    size_t root_count;
    size_t member_index;

    if (!ctx || !ctx->function || !ctx->runtime_state || !ctx->queues || !decision ||
        decision->kind != VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE) {
        return 0;
    }

    out_result->append_trace = 1u;
    if (decision->has_candidate_entry) {
        out_result->before_entry = decision->candidate_entry;
    } else {
        out_result->before_entry = ctx->queues->family_phase_entries[decision->candidate.coalesce_root_value_id];
    }
    value_ssa_alloc_mark_frozen_family(ctx->coalesce_analysis,
        ctx->function->next_value_id,
        decision->candidate.value_id,
        ctx->scheduled,
        ctx->frozen,
        ctx->was_frozen);
    root_value_id = value_ssa_alloc_move_engine_effective_root(
        ctx->runtime_state, ctx->coalesce_analysis, ctx->function->next_value_id, decision->candidate.value_id);
    if (ctx->family_members && ctx->runtime_state->active_move_related &&
        ctx->runtime_state->active_effective_move_related &&
        root_value_id < ctx->family_members->value_count) {
        root_offset = ctx->family_members->root_member_offsets[root_value_id];
        root_count = ctx->family_members->root_member_counts[root_value_id];
        for (member_index = 0; member_index < root_count; ++member_index) {
            size_t item_index = ctx->family_members->member_item_indices[root_offset + member_index];
            size_t value_id = ctx->worklist->items[item_index].value_id;

            if (value_id >= ctx->function->next_value_id || !ctx->frozen[value_id]) {
                continue;
            }
            ctx->runtime_state->active_move_related[value_id] = 0u;
            ctx->runtime_state->active_effective_move_related[value_id] = 0u;
        }
    }
    value_ssa_alloc_move_engine_prepare_general_refresh_result(out_result, decision->candidate.value_id, 1, 0);
    return 1;
}

static int value_ssa_alloc_move_engine_execute_remove_decision(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx,
    const ValueSsaAllocatorMoveEngineDecision *decision,
    ValueSsaAllocatorMoveEngineStepResult *out_result) {
    if (!ctx || !ctx->function || !ctx->runtime_state || !ctx->queues || !decision ||
        decision->kind != VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE) {
        return 0;
    }

    out_result->append_trace = 1u;
    out_result->record_plan_item = 1u;
    if (decision->has_candidate_entry) {
        out_result->before_entry = decision->candidate_entry;
    } else {
        out_result->before_entry = ctx->queues->family_phase_entries[decision->candidate.coalesce_root_value_id];
    }
    out_result->plan_item = decision->candidate;
    if (decision->candidate_root_value_id != (size_t)-1) {
        out_result->plan_item.coalesce_root_value_id = decision->candidate_root_value_id;
    }
    out_result->plan_item.removal_kind = decision->removal_kind;
    ctx->scheduled[decision->candidate.value_id] = 1;
    value_ssa_alloc_move_engine_prepare_general_refresh_result(
        out_result, decision->candidate.value_id, 0, decision->removal_kind == VALUE_SSA_ALLOCATOR_REMOVE_SPILL);
    return 1;
}

static int value_ssa_alloc_move_engine_execute_decision(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx,
    const ValueSsaAllocatorMoveEngineDecision *decision,
    ValueSsaAllocatorMoveEngineStepResult *out_result) {
    if (out_result) {
        value_ssa_alloc_move_engine_step_result_init(out_result);
    }
    if (!ctx || !decision || !out_result) {
        return 0;
    }

    out_result->decision_kind = decision->kind;
    out_result->phase = decision->phase;
    out_result->removal_kind = decision->removal_kind;
    if (decision->kind == VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE) {
        out_result->value_id = decision->coalesce_action.lhs_value_id;
        out_result->root_value_id = decision->coalesce_action.lhs_root_value_id;
    } else {
        out_result->value_id = decision->candidate.value_id;
        out_result->root_value_id = decision->candidate_root_value_id != (size_t)-1
            ? decision->candidate_root_value_id
            : decision->candidate.coalesce_root_value_id;
    }

    switch (decision->kind) {
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE:
            return value_ssa_alloc_move_engine_execute_coalesce_decision(ctx, decision, out_result);
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE:
            return value_ssa_alloc_move_engine_execute_freeze_decision(ctx, decision, out_result);
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE:
            out_result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_STEP_COMPLETED_PLAN_ITEM;
            return value_ssa_alloc_move_engine_execute_remove_decision(ctx, decision, out_result);
        case VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_NONE:
        default:
            return 1;
    }
}

static void value_ssa_alloc_move_engine_prepare_general_refresh_result(
    ValueSsaAllocatorMoveEngineStepResult *result,
    size_t transition_value_id,
    int transition_is_freeze,
    int transition_is_spill_remove) {
    if (!result) {
        return;
    }

    result->refresh_kind = VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_GENERAL;
    result->transition_value_id = transition_value_id;
    result->transition_is_freeze = transition_is_freeze ? 1u : 0u;
    result->transition_is_spill_remove = transition_is_spill_remove ? 1u : 0u;
}

static void value_ssa_alloc_move_engine_prepare_coalesce_pair_refresh_result(
    ValueSsaAllocatorMoveEngineStepResult *result, size_t lhs_root_value_id, size_t rhs_root_value_id) {
    if (!result) {
        return;
    }

    result->refresh_kind = VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_COALESCE_PAIR;
    result->root_value_id = lhs_root_value_id;
    result->partner_root_value_id = rhs_root_value_id;
}

static int value_ssa_alloc_move_engine_refresh_after_step_result(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx, const ValueSsaAllocatorMoveEngineStepResult *result) {
    if (!ctx || !result) {
        return 0;
    }

    switch (result->refresh_kind) {
        case VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_GENERAL:
            value_ssa_alloc_move_engine_refresh_current_family_queues(ctx->prep,
                ctx->worklist,
                ctx->family_members,
                ctx->interference_graph,
                ctx->affinity_graph,
                ctx->coalesce_analysis,
                ctx->initial_effective_degrees,
                ctx->initial_family_move_related,
                ctx->initial_effective_move_related,
                ctx->scheduled,
                ctx->frozen,
                ctx->was_frozen,
                ctx->function->next_value_id,
                ctx->color_budget,
                ctx->runtime_state,
                ctx->queues,
                result->transition_value_id,
                result->transition_is_freeze,
                result->transition_is_spill_remove,
                0);
            if (ctx->out_run_result) {
                value_ssa_alloc_move_engine_note_refresh_for_run_result(
                    ctx->out_run_result, result->refresh_kind, ctx->queues->affected_family_count);
            }
            return 1;
        case VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_COALESCE_PAIR:
            value_ssa_alloc_move_engine_refresh_current_family_queues_for_coalesce_pair(ctx->prep,
                ctx->worklist,
                ctx->family_members,
                ctx->interference_graph,
                ctx->affinity_graph,
                ctx->coalesce_analysis,
                ctx->initial_effective_degrees,
                ctx->initial_family_move_related,
                ctx->initial_effective_move_related,
                ctx->scheduled,
                ctx->frozen,
                ctx->was_frozen,
                ctx->function->next_value_id,
                ctx->color_budget,
                ctx->runtime_state,
                ctx->queues,
                result->root_value_id,
                result->partner_root_value_id);
            if (ctx->out_run_result) {
                value_ssa_alloc_move_engine_note_refresh_for_run_result(
                    ctx->out_run_result, result->refresh_kind, ctx->queues->affected_family_count);
            }
            return 1;
        case VALUE_SSA_ALLOC_MOVE_ENGINE_REFRESH_NONE:
        default:
            return 1;
    }
}

static int value_ssa_alloc_move_engine_finalize_step_result_observation(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx, ValueSsaAllocatorMoveEngineStepResult *result) {
    ValueSsaAllocatorMoveEngineFamilyStateView family_state_after;
    int has_family_state_after = 0;

    if (!ctx || !result) {
        return 0;
    }

    if (result->decision_kind == VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_COALESCE) {
        ValueSsaAllocatorMoveEngineFamilyStateView partner_slot_state_after;
        int has_partner_slot_state_after = 0;

        has_family_state_after = value_ssa_alloc_move_engine_read_family_state(
            ctx->runtime_state, ctx->function->next_value_id, result->root_value_id, &family_state_after);
        has_partner_slot_state_after = value_ssa_alloc_move_engine_read_family_slot_state(
            ctx->runtime_state, ctx->function->next_value_id, result->partner_root_value_id, &partner_slot_state_after);

        if (ctx->out_trace && result->append_trace) {
            if (!value_ssa_alloc_move_engine_append_trace_step(ctx->out_trace,
                    (*ctx->trace_index)++,
                    result->value_id,
                    result->root_value_id,
                    result->partner_value_id,
                    result->partner_root_value_id,
                    VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE,
                    result->phase,
                    VALUE_SSA_ALLOCATOR_REMOVE_NONE,
                    result->before_entry.work_class,
                    has_family_state_after ? family_state_after.move_work_class : result->before_entry.work_class,
                    result->before_entry.family_active_member_count,
                    has_family_state_after ? family_state_after.active_member_count
                                           : result->before_entry.family_active_member_count,
                    result->before_entry.family_external_neighbor_count,
                    result->before_entry.family_coalesce_ready_neighbor_count,
                    result->before_entry.family_coalesce_ready_affinity_weight_sum,
                    result->before_entry.family_best_coalesce_ready_partner_root_value_id,
                    result->before_entry.family_best_coalesce_ready_partner_affinity_weight,
                    result->before_entry.family_best_coalesce_ready_partner_is_mutual,
                    has_family_state_after ? family_state_after.external_neighbor_count : 0,
                    has_family_state_after ? family_state_after.coalesce_ready_neighbor_count : 0,
                    has_family_state_after ? family_state_after.coalesce_ready_affinity_weight_sum : 0,
                    has_family_state_after ? family_state_after.best_coalesce_ready_partner_root_value_id : (size_t)-1,
                    has_family_state_after ? family_state_after.best_coalesce_ready_partner_affinity_weight : 0,
                    has_family_state_after ? family_state_after.best_coalesce_ready_partner_is_mutual : 0,
                    result->before_entry.family_external_affinity_weight_sum,
                    has_family_state_after ? family_state_after.external_affinity_weight_sum : 0,
                    result->before_partner_entry.work_class,
                    has_partner_slot_state_after ? partner_slot_state_after.move_work_class
                                                 : result->before_partner_entry.work_class,
                    result->before_partner_entry.family_active_member_count,
                    has_partner_slot_state_after ? partner_slot_state_after.active_member_count
                                                 : result->before_partner_entry.family_active_member_count,
                    result->before_partner_entry.family_external_neighbor_count,
                    result->before_partner_entry.family_coalesce_ready_neighbor_count,
                    result->before_partner_entry.family_coalesce_ready_affinity_weight_sum,
                    result->before_partner_entry.family_best_coalesce_ready_partner_root_value_id,
                    result->before_partner_entry.family_best_coalesce_ready_partner_affinity_weight,
                    result->before_partner_entry.family_best_coalesce_ready_partner_is_mutual,
                    has_partner_slot_state_after ? partner_slot_state_after.external_neighbor_count : 0,
                    has_partner_slot_state_after ? partner_slot_state_after.coalesce_ready_neighbor_count : 0,
                    has_partner_slot_state_after ? partner_slot_state_after.coalesce_ready_affinity_weight_sum : 0,
                    has_partner_slot_state_after ? partner_slot_state_after.best_coalesce_ready_partner_root_value_id
                                                 : (size_t)-1,
                    has_partner_slot_state_after ? partner_slot_state_after.best_coalesce_ready_partner_affinity_weight
                                                 : 0,
                    has_partner_slot_state_after ? partner_slot_state_after.best_coalesce_ready_partner_is_mutual : 0,
                    result->before_partner_entry.family_external_affinity_weight_sum,
                    has_partner_slot_state_after ? partner_slot_state_after.external_affinity_weight_sum : 0,
                    result->coalesce_priority)) {
                value_ssa_alloc_set_error(ctx->error, 0, 0, "VALUE-SSA-ALLOC-120: move-transition trace append overflow");
                return 0;
            }
        }
        return 1;
    }

    has_family_state_after = value_ssa_alloc_move_engine_read_family_state(
        ctx->runtime_state, ctx->function->next_value_id, result->root_value_id, &family_state_after);

    if (result->decision_kind == VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE) {
        if (has_family_state_after) {
            value_ssa_alloc_move_engine_apply_family_state_to_plan_item_after(&result->plan_item, &family_state_after);
        }
        if (ctx->out_plan && result->record_plan_item) {
            ctx->out_plan->items[ctx->output_index] = result->plan_item;
        }
    }

    if (ctx->out_trace && result->append_trace) {
        if (!value_ssa_alloc_move_engine_append_trace_step(ctx->out_trace,
                (*ctx->trace_index)++,
                result->value_id,
                result->root_value_id,
                (size_t)-1,
                (size_t)-1,
                result->decision_kind == VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE
                    ? VALUE_SSA_ALLOC_MOVE_EVENT_FREEZE
                    : VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE,
                result->phase,
                result->decision_kind == VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_FREEZE
                    ? VALUE_SSA_ALLOCATOR_REMOVE_NONE
                    : result->removal_kind,
                result->before_entry.work_class,
                has_family_state_after ? family_state_after.move_work_class : result->before_entry.work_class,
                result->before_entry.family_active_member_count,
                has_family_state_after ? family_state_after.active_member_count
                                       : result->before_entry.family_active_member_count,
                result->before_entry.family_external_neighbor_count,
                result->before_entry.family_coalesce_ready_neighbor_count,
                result->before_entry.family_coalesce_ready_affinity_weight_sum,
                result->before_entry.family_best_coalesce_ready_partner_root_value_id,
                result->before_entry.family_best_coalesce_ready_partner_affinity_weight,
                result->before_entry.family_best_coalesce_ready_partner_is_mutual,
                has_family_state_after ? family_state_after.external_neighbor_count : 0,
                has_family_state_after ? family_state_after.coalesce_ready_neighbor_count : 0,
                has_family_state_after ? family_state_after.coalesce_ready_affinity_weight_sum : 0,
                has_family_state_after ? family_state_after.best_coalesce_ready_partner_root_value_id : (size_t)-1,
                has_family_state_after ? family_state_after.best_coalesce_ready_partner_affinity_weight : 0,
                has_family_state_after ? family_state_after.best_coalesce_ready_partner_is_mutual : 0,
                result->before_entry.family_external_affinity_weight_sum,
                has_family_state_after ? family_state_after.external_affinity_weight_sum : 0,
                VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
                VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
                0,
                0,
                0,
                0,
                0,
                (size_t)-1,
                0,
                0,
                0,
                0,
                0,
                (size_t)-1,
                0,
                0,
                0,
                0,
                result->decision_kind == VALUE_SSA_ALLOC_MOVE_ENGINE_DECISION_REMOVE ? result->plan_item.move_work_priority
                                                                                     : result->before_entry.move_work_priority)) {
            value_ssa_alloc_set_error(ctx->error, 0, 0, "VALUE-SSA-ALLOC-120: move-transition trace append overflow");
            return 0;
        }
    }

    return 1;
}

static int value_ssa_alloc_move_engine_run_current_phase_step(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx, ValueSsaAllocatorMoveEngineStepResult *out_result) {
    ValueSsaAllocatorMoveEngineDecision decision;

    if (out_result) {
        value_ssa_alloc_move_engine_step_result_init(out_result);
    }
    if (!ctx || !ctx->function || !ctx->queues || !ctx->runtime_state || !out_result) {
        return 0;
    }

    value_ssa_alloc_move_engine_decision_init(&decision);
    value_ssa_alloc_move_engine_choose_current_phase_decision(ctx->queues->family_phase_entries,
        ctx->runtime_state,
        ctx->function->next_value_id,
        ctx->queues,
        ctx->out_run_result,
        &decision);
    if (!value_ssa_alloc_move_engine_decision_has_action(&decision)) {
        value_ssa_alloc_set_error(ctx->error, 0, 0, "VALUE-SSA-ALLOC-073: allocator move-engine scheduling underflow");
        return 0;
    }
    if (!value_ssa_alloc_move_engine_execute_decision(ctx, &decision, out_result)) {
        return 0;
    }
    if (!value_ssa_alloc_move_engine_refresh_after_step_result(ctx, out_result)) {
        return 0;
    }
    return value_ssa_alloc_move_engine_finalize_step_result_observation(ctx, out_result);
}

static int value_ssa_alloc_move_engine_complete_current_plan_item(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx,
    ValueSsaAllocatorMoveEnginePlanItemResult *out_result) {
    ValueSsaAllocatorMoveEnginePlanItemResult local_result;

    if (out_result) {
        value_ssa_alloc_move_engine_plan_item_result_init(out_result);
    } else {
        value_ssa_alloc_move_engine_plan_item_result_init(&local_result);
        out_result = &local_result;
    }
    if (!ctx) {
        return 0;
    }

    for (;;) {
        ValueSsaAllocatorMoveEngineStepResult step_result;

        value_ssa_alloc_move_engine_step_result_init(&step_result);
        if (!value_ssa_alloc_move_engine_run_current_phase_step(ctx, &step_result)) {
            return 0;
        }
        if (ctx->out_run_result) {
            value_ssa_alloc_move_engine_note_step_for_run_result(ctx->out_run_result, &step_result);
        }
        ++out_result->completed_step_count;
        if (step_result.status == VALUE_SSA_ALLOC_MOVE_ENGINE_STEP_COMPLETED_PLAN_ITEM) {
            out_result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_COMPLETED;
            return 1;
        }
        out_result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_CONTINUE;
    }
}

static int value_ssa_alloc_move_engine_complete_plan_items(
    ValueSsaAllocatorMoveEngineExecutionContext *ctx,
    size_t item_count,
    ValueSsaAllocatorMoveEnginePlanDriverResult *out_result) {
    size_t output_index;

    if (out_result) {
        value_ssa_alloc_move_engine_plan_driver_result_init(out_result);
    }
    if (!ctx) {
        return 0;
    }

    for (output_index = 0; output_index < item_count; ++output_index) {
        ValueSsaAllocatorMoveEnginePlanItemResult plan_item_result;

        ctx->output_index = output_index;
        value_ssa_alloc_move_engine_plan_item_result_init(&plan_item_result);
        if (!value_ssa_alloc_move_engine_complete_current_plan_item(ctx, &plan_item_result)) {
            return 0;
        }
        if (plan_item_result.status != VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_ITEM_COMPLETED) {
            value_ssa_alloc_set_error(
                ctx->error, 0, 0, "VALUE-SSA-ALLOC-207: plan-item driver ended without completion");
            return 0;
        }
        if (out_result) {
            ++out_result->completed_plan_item_count;
            out_result->completed_step_count += plan_item_result.completed_step_count;
        }
    }

    if (out_result) {
        out_result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_DRIVER_COMPLETED;
    }
    return 1;
}

static int value_ssa_alloc_run_move_engine(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    ValueSsaAllocatorPlan *out_plan,
    ValueSsaAllocatorMoveTransitionTrace *out_trace,
    ValueSsaAllocatorMoveEngineRunResult *out_run_result,
    ValueSsaError *error) {
    ValueSsaAllocationPrep owned_prep;
    ValueSsaAllocationWorklist owned_worklist;
    ValueSsaInterferenceGraph owned_interference;
    ValueSsaCopyAffinityGraph owned_affinity;
    ValueSsaAllocatorCoalesceAnalysis owned_coalesce;
    const ValueSsaAllocationPrep *active_prep = prep;
    const ValueSsaAllocationWorklist *active_worklist = worklist;
    const ValueSsaInterferenceGraph *active_interference = interference_graph;
    const ValueSsaCopyAffinityGraph *active_affinity = affinity_graph;
    const ValueSsaAllocatorCoalesceAnalysis *active_coalesce = coalesce_analysis;
    size_t *active_degrees = NULL;
    size_t *initial_effective_degrees = NULL;
    size_t *active_effective_degrees = NULL;
    ValueSsaAllocationFamilyRuntimeFacts *family_runtime_facts = NULL;
    size_t *active_family_spill_removed_counts = NULL;
    size_t *active_family_member_counts = NULL;
    size_t *active_family_external_neighbor_counts = NULL;
    size_t *active_family_coalesce_ready_neighbor_counts = NULL;
    size_t *active_family_coalesce_ready_affinity_sums = NULL;
    size_t *active_family_best_coalesce_ready_partner_roots = NULL;
    size_t *active_family_best_coalesce_ready_partner_weights = NULL;
    unsigned char *active_family_best_coalesce_ready_partner_mutual_flags = NULL;
    size_t *active_family_external_affinity_sums = NULL;
    size_t *active_family_queue_priorities = NULL;
    size_t *active_family_roots = NULL;
    unsigned char *affected_family_root_flags = NULL;
    size_t *affected_family_roots = NULL;
    unsigned char *simplify_family_root_flags = NULL;
    size_t *simplify_family_roots = NULL;
    unsigned char *freeze_family_root_flags = NULL;
    size_t *freeze_family_roots = NULL;
    unsigned char *spill_family_root_flags = NULL;
    size_t *spill_family_roots = NULL;
    size_t *scratch_roots = NULL;
    unsigned char *scratch_root_flags = NULL;
    ValueSsaAllocatorMoveEngineFamilyPhaseEntry *family_phase_entries = NULL;
    ValueSsaAllocatorMoveWorkClass *active_family_move_work_classes = NULL;
    unsigned char *active_move_related = NULL;
    unsigned char *initial_family_move_related = NULL;
    unsigned char *initial_effective_move_related = NULL;
    unsigned char *active_effective_move_related = NULL;
    size_t *affinity_neighbor_offsets = NULL;
    size_t *affinity_neighbor_value_ids = NULL;
    size_t *affinity_neighbor_weights = NULL;
    size_t *interference_neighbor_offsets = NULL;
    size_t *interference_neighbor_value_ids = NULL;
    ValueSsaAllocatorPlanItem *work_item_preview_bases = NULL;
    unsigned char *family_neighbor_matrix = NULL;
    unsigned char *family_coalesce_ready_neighbor_matrix = NULL;
    size_t *family_coalesce_ready_affinity_matrix = NULL;
    unsigned char *consumed_family_coalesce_pair_matrix = NULL;
    unsigned char *scratch_class_move_related = NULL;
    size_t *runtime_coalesce_roots = NULL;
    size_t *runtime_coalesce_class_sizes = NULL;
    unsigned char *frozen = NULL;
    unsigned char *was_frozen = NULL;
    unsigned char *scheduled = NULL;
    ValueSsaAllocatorMoveEngineRuntimeState runtime_state;
    ValueSsaAllocatorMoveEngineCurrentQueues current_queues;
    ValueSsaAllocatorMoveEngineFamilyMembers family_members;
    size_t trace_index = 0;
    size_t move_related_value_count = 0;
    int ok = 0;
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (!function || (!out_plan && !out_trace)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-122: invalid move-engine contract");
        return 0;
    }
    if (out_run_result) {
        value_ssa_alloc_move_engine_run_result_init(out_run_result);
    }

    value_ssa_allocation_prep_init(&owned_prep);
    value_ssa_allocation_worklist_init(&owned_worklist);
    value_ssa_interference_graph_init(&owned_interference);
    value_ssa_copy_affinity_graph_init(&owned_affinity);
    value_ssa_allocator_coalesce_analysis_init(&owned_coalesce);
    if (out_plan) {
        value_ssa_allocator_plan_free(out_plan);
        out_plan->value_count = function->next_value_id;
    }
    if (out_trace) {
        value_ssa_allocator_move_transition_trace_free(out_trace);
        out_trace->value_count = function->next_value_id;
    }
    if (function->next_value_id == 0) {
        if (out_run_result) {
            out_run_result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_COMPLETED;
            out_run_result->plan_driver.status = VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_DRIVER_COMPLETED;
            out_run_result->published_plan_artifacts = out_plan ? 1u : 0u;
            out_run_result->published_trace_artifacts = out_trace ? 1u : 0u;
            out_run_result->published_trace_step_count = 0;
        }
        return 1;
    }

    memset(&runtime_state, 0, sizeof(runtime_state));
    memset(&current_queues, 0, sizeof(current_queues));
    value_ssa_alloc_move_engine_family_members_init(&family_members);

    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    }
    if (!active_worklist) {
        if (!value_ssa_compute_allocation_worklist(function, active_prep, &owned_worklist, error)) {
            goto cleanup;
        }
        active_worklist = &owned_worklist;
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
    if (!active_coalesce) {
        if (!value_ssa_compute_allocator_coalesce_analysis(
                function, active_prep, active_interference, active_affinity, color_budget, &owned_coalesce, error)) {
            goto cleanup;
        }
        active_coalesce = &owned_coalesce;
    }
    if (active_prep && active_prep->move_related) {
        size_t value_id;

        for (value_id = 0; value_id < function->next_value_id; ++value_id) {
            if (active_prep->move_related[value_id]) {
                ++move_related_value_count;
            }
        }
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "move-engine prerequisites", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (out_plan) {
        out_plan->item_count = active_worklist->item_count;
        out_plan->items = (ValueSsaAllocatorPlanItem *)calloc(
            out_plan->item_count ? out_plan->item_count : 1, sizeof(ValueSsaAllocatorPlanItem));
        out_plan->final_runtime_coalesce_roots =
            (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
        out_plan->final_runtime_coalesce_class_sizes =
            (size_t *)calloc((function->next_value_id ? function->next_value_id : 1), sizeof(size_t));
        out_plan->value_item_indices =
            (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
        if (!out_plan->items || !out_plan->final_runtime_coalesce_roots || !out_plan->final_runtime_coalesce_class_sizes ||
            !out_plan->value_item_indices) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-068: out of memory preparing allocator plan");
            goto cleanup;
        }
        {
            size_t value_index;
            for (value_index = 0; value_index < function->next_value_id; ++value_index) {
                out_plan->value_item_indices[value_index] = (size_t)-1;
            }
        }
    }
    if (out_trace) {
        out_trace->step_count = active_worklist->item_count * 2;
        out_trace->steps = (ValueSsaAllocatorMoveTransitionStep *)calloc(
            out_trace->step_count ? out_trace->step_count : 1, sizeof(ValueSsaAllocatorMoveTransitionStep));
        if (!out_trace->steps) {
            value_ssa_alloc_set_error(
                error, 0, 0, "VALUE-SSA-ALLOC-116: out of memory preparing move-transition trace");
            goto cleanup;
        }
    }

    active_degrees = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    initial_effective_degrees =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    active_effective_degrees =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    family_runtime_facts = (ValueSsaAllocationFamilyRuntimeFacts *)calloc(
        function->next_value_id ? function->next_value_id : 1, sizeof(ValueSsaAllocationFamilyRuntimeFacts));
    active_family_spill_removed_counts =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_member_counts =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_external_neighbor_counts =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_coalesce_ready_neighbor_counts =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_coalesce_ready_affinity_sums =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_best_coalesce_ready_partner_roots =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    active_family_best_coalesce_ready_partner_weights =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_best_coalesce_ready_partner_mutual_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    active_family_external_affinity_sums =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_queue_priorities =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    active_family_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    affected_family_root_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    affected_family_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    simplify_family_root_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    simplify_family_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    freeze_family_root_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    freeze_family_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    spill_family_root_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    spill_family_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    scratch_roots = (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    scratch_root_flags =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    family_phase_entries = (ValueSsaAllocatorMoveEngineFamilyPhaseEntry *)calloc(
        function->next_value_id ? function->next_value_id : 1, sizeof(ValueSsaAllocatorMoveEngineFamilyPhaseEntry));
    active_family_move_work_classes = (ValueSsaAllocatorMoveWorkClass *)malloc(
        (function->next_value_id ? function->next_value_id : 1) * sizeof(ValueSsaAllocatorMoveWorkClass));
    active_move_related =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    initial_family_move_related =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    initial_effective_move_related =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    active_effective_move_related =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    if (!value_ssa_alloc_move_engine_build_affinity_neighbors(active_prep,
            function->next_value_id,
            &affinity_neighbor_offsets,
            &affinity_neighbor_value_ids,
            &affinity_neighbor_weights)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-072: out of memory preparing move-engine state");
        goto cleanup;
    }
    if (!value_ssa_alloc_move_engine_build_interference_neighbors(
            active_interference,
            function->next_value_id,
            &interference_neighbor_offsets,
            &interference_neighbor_value_ids)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-072: out of memory preparing move-engine state");
        goto cleanup;
    }
    family_neighbor_matrix = (unsigned char *)calloc(
        (function->next_value_id ? (function->next_value_id * function->next_value_id) : 1), sizeof(unsigned char));
    family_coalesce_ready_neighbor_matrix = (unsigned char *)calloc(
        (function->next_value_id ? (function->next_value_id * function->next_value_id) : 1), sizeof(unsigned char));
    family_coalesce_ready_affinity_matrix = (size_t *)calloc(
        (function->next_value_id ? (function->next_value_id * function->next_value_id) : 1), sizeof(size_t));
    consumed_family_coalesce_pair_matrix = (unsigned char *)calloc(
        (function->next_value_id ? (function->next_value_id * function->next_value_id) : 1), sizeof(unsigned char));
    scratch_class_move_related =
        (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    work_item_preview_bases = (ValueSsaAllocatorPlanItem *)calloc(
        active_worklist->item_count ? active_worklist->item_count : 1, sizeof(ValueSsaAllocatorPlanItem));
    runtime_coalesce_roots =
        (size_t *)malloc((function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    runtime_coalesce_class_sizes =
        (size_t *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(size_t));
    frozen = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    was_frozen = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    scheduled = (unsigned char *)calloc(function->next_value_id ? function->next_value_id : 1, sizeof(unsigned char));
    if (!active_degrees || !initial_effective_degrees || !active_effective_degrees || !family_runtime_facts ||
        !active_family_spill_removed_counts || !active_family_member_counts ||
        !active_family_external_neighbor_counts || !active_family_coalesce_ready_neighbor_counts ||
        !active_family_coalesce_ready_affinity_sums || !active_family_best_coalesce_ready_partner_roots ||
        !active_family_best_coalesce_ready_partner_weights ||
        !active_family_best_coalesce_ready_partner_mutual_flags || !active_family_external_affinity_sums ||
        !active_family_queue_priorities || !active_family_roots || !affected_family_root_flags ||
        !affected_family_roots || !simplify_family_root_flags || !simplify_family_roots || !freeze_family_root_flags ||
        !freeze_family_roots || !spill_family_root_flags || !spill_family_roots || !scratch_roots ||
        !scratch_root_flags || !family_phase_entries ||
        !active_family_move_work_classes ||
        !active_move_related || !initial_family_move_related || !initial_effective_move_related ||
        !active_effective_move_related || !work_item_preview_bases || !family_neighbor_matrix ||
        !family_coalesce_ready_neighbor_matrix ||
        !family_coalesce_ready_affinity_matrix || !consumed_family_coalesce_pair_matrix ||
        !scratch_class_move_related || !runtime_coalesce_roots || !runtime_coalesce_class_sizes || !frozen ||
        !was_frozen || !scheduled || !interference_neighbor_offsets) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-072: out of memory preparing move-engine state");
        goto cleanup;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "move-engine allocate-state", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    runtime_state.active_degrees = active_degrees;
    runtime_state.active_effective_degrees = active_effective_degrees;
    runtime_state.family_runtime_facts = family_runtime_facts;
    runtime_state.active_family_spill_removed_counts = active_family_spill_removed_counts;
    runtime_state.active_family_member_counts = active_family_member_counts;
    runtime_state.active_family_external_neighbor_counts = active_family_external_neighbor_counts;
    runtime_state.active_family_coalesce_ready_neighbor_counts = active_family_coalesce_ready_neighbor_counts;
    runtime_state.active_family_coalesce_ready_affinity_sums = active_family_coalesce_ready_affinity_sums;
    runtime_state.active_family_best_coalesce_ready_partner_roots = active_family_best_coalesce_ready_partner_roots;
    runtime_state.active_family_best_coalesce_ready_partner_weights =
        active_family_best_coalesce_ready_partner_weights;
    runtime_state.active_family_best_coalesce_ready_partner_mutual_flags =
        active_family_best_coalesce_ready_partner_mutual_flags;
    runtime_state.active_family_external_affinity_sums = active_family_external_affinity_sums;
    runtime_state.active_family_queue_priorities = active_family_queue_priorities;
    runtime_state.scratch_roots = scratch_roots;
    runtime_state.scratch_root_flags = scratch_root_flags;
    runtime_state.active_family_move_work_classes = active_family_move_work_classes;
    runtime_state.active_move_related = active_move_related;
    runtime_state.active_effective_move_related = active_effective_move_related;
    runtime_state.interference_neighbor_offsets = interference_neighbor_offsets;
    runtime_state.interference_neighbor_value_ids = interference_neighbor_value_ids;
    runtime_state.affinity_neighbor_offsets = affinity_neighbor_offsets;
    runtime_state.affinity_neighbor_value_ids = affinity_neighbor_value_ids;
    runtime_state.affinity_neighbor_weights = affinity_neighbor_weights;
    runtime_state.work_item_preview_bases = work_item_preview_bases;
    runtime_state.family_neighbor_matrix = family_neighbor_matrix;
    runtime_state.family_coalesce_ready_neighbor_matrix = family_coalesce_ready_neighbor_matrix;
    runtime_state.family_coalesce_ready_affinity_matrix = family_coalesce_ready_affinity_matrix;
    runtime_state.consumed_family_coalesce_pair_matrix = consumed_family_coalesce_pair_matrix;
    runtime_state.scratch_class_move_related = scratch_class_move_related;
    runtime_state.runtime_coalesce_roots = runtime_coalesce_roots;
    runtime_state.runtime_coalesce_class_sizes = runtime_coalesce_class_sizes;

    current_queues.value_count = function->next_value_id;
    current_queues.family_phase_entries = family_phase_entries;
    current_queues.active_family_roots = active_family_roots;
    current_queues.affected_family_root_flags = affected_family_root_flags;
    current_queues.affected_family_roots = affected_family_roots;
    current_queues.simplify_family_root_flags = simplify_family_root_flags;
    current_queues.simplify_family_roots = simplify_family_roots;
    current_queues.freeze_family_root_flags = freeze_family_root_flags;
    current_queues.freeze_family_roots = freeze_family_roots;
    current_queues.spill_family_root_flags = spill_family_root_flags;
    current_queues.spill_family_roots = spill_family_roots;
    value_ssa_alloc_move_engine_seed_runtime_coalesce_families(
        active_coalesce, function->next_value_id, runtime_coalesce_roots, runtime_coalesce_class_sizes);
    if (!value_ssa_alloc_move_engine_build_family_members(
            active_worklist, active_coalesce, runtime_coalesce_roots, function->next_value_id, &family_members)) {
        value_ssa_alloc_set_error(
            error, 0, 0, "VALUE-SSA-ALLOC-123: out of memory preparing family member slices");
        goto cleanup;
    }

    value_ssa_alloc_move_engine_update_active_degrees(
        active_interference, scheduled, function->next_value_id, 0, 1, &runtime_state);
    value_ssa_alloc_move_engine_update_active_move_related(active_worklist,
        &family_members,
        active_interference,
        active_affinity,
        active_coalesce,
        function->next_value_id,
        &current_queues,
        scheduled,
        frozen,
        1,
        &runtime_state);
    value_ssa_alloc_move_engine_update_effective_value_runtime_state(active_worklist,
        &family_members,
        active_interference,
        active_affinity,
        active_coalesce,
        function->next_value_id,
        &current_queues,
        scheduled,
        frozen,
        1,
        &runtime_state);
    value_ssa_alloc_move_engine_update_active_family_member_counts(
        active_coalesce, function->next_value_id, scheduled, function->next_value_id, 0, 1, &runtime_state);
    value_ssa_alloc_move_engine_update_active_family_spill_removed_counts(
        active_coalesce, function->next_value_id, function->next_value_id, 0, 1, &runtime_state);
    value_ssa_alloc_recompute_active_family_move_pressure(active_affinity,
        active_coalesce,
        scheduled,
        frozen,
        runtime_state.family_neighbor_matrix,
        runtime_state.family_coalesce_ready_neighbor_matrix,
        runtime_state.family_coalesce_ready_affinity_matrix,
        runtime_state.active_family_external_neighbor_counts,
        runtime_state.active_family_coalesce_ready_neighbor_counts,
        runtime_state.active_family_coalesce_ready_affinity_sums,
        runtime_state.active_family_best_coalesce_ready_partner_roots,
        runtime_state.active_family_best_coalesce_ready_partner_weights,
        runtime_state.active_family_best_coalesce_ready_partner_mutual_flags,
        runtime_state.active_family_external_affinity_sums);
    memcpy(initial_effective_degrees,
        runtime_state.active_effective_degrees,
        (function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
    memcpy(initial_effective_move_related,
        runtime_state.active_effective_move_related,
        (function->next_value_id ? function->next_value_id : 1) * sizeof(unsigned char));
    for (size_t preview_index = 0; preview_index < active_worklist->item_count; ++preview_index) {
        value_ssa_alloc_build_plan_item_preview_base(active_prep,
            &active_worklist->items[preview_index],
            initial_effective_degrees,
            initial_effective_move_related,
            &work_item_preview_bases[preview_index]);
    }
    value_ssa_alloc_move_engine_seed_initial_family_move_related(
        active_coalesce, function->next_value_id, initial_effective_move_related, initial_family_move_related);
    value_ssa_alloc_move_engine_refresh_current_family_queues(active_prep,
        active_worklist,
        &family_members,
        active_interference,
        active_affinity,
        active_coalesce,
        initial_effective_degrees,
        initial_family_move_related,
        initial_effective_move_related,
        scheduled,
        frozen,
        was_frozen,
        function->next_value_id,
        color_budget,
        &runtime_state,
        &current_queues,
        function->next_value_id,
        0,
        0,
        1);
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "move-engine initialize-queues", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    {
        ValueSsaAllocatorMoveEngineExecutionContext execution_context;
        ValueSsaAllocatorMoveEnginePlanDriverResult plan_driver_result;

        memset(&execution_context, 0, sizeof(execution_context));
        value_ssa_alloc_move_engine_plan_driver_result_init(&plan_driver_result);
        execution_context.function = function;
        execution_context.prep = active_prep;
        execution_context.worklist = active_worklist;
        execution_context.interference_graph = active_interference;
        execution_context.affinity_graph = active_affinity;
        execution_context.coalesce_analysis = active_coalesce;
        execution_context.initial_effective_degrees = initial_effective_degrees;
        execution_context.initial_family_move_related = initial_family_move_related;
        execution_context.initial_effective_move_related = initial_effective_move_related;
        execution_context.scheduled = scheduled;
        execution_context.frozen = frozen;
        execution_context.was_frozen = was_frozen;
        execution_context.color_budget = color_budget;
        execution_context.runtime_state = &runtime_state;
        execution_context.queues = &current_queues;
        execution_context.family_members = &family_members;
        execution_context.out_plan = out_plan;
        execution_context.out_trace = out_trace;
        execution_context.out_run_result = out_run_result;
        execution_context.trace_index = &trace_index;
        execution_context.error = error;

        if (!value_ssa_alloc_move_engine_complete_plan_items(
                &execution_context, active_worklist->item_count, &plan_driver_result)) {
            goto cleanup;
        }
        if (plan_driver_result.status != VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_DRIVER_COMPLETED ||
            plan_driver_result.completed_plan_item_count != active_worklist->item_count) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-208: plan-driver ended without completing all items");
            goto cleanup;
        }
        if (out_run_result) {
            out_run_result->plan_driver = plan_driver_result;
        }
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "move-engine plan-driver", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (out_trace) {
        out_trace->step_count = trace_index;
        if (out_run_result) {
            out_run_result->published_trace_artifacts = 1u;
            out_run_result->published_trace_step_count = trace_index;
        }
    }
    if (out_plan && out_plan->final_runtime_coalesce_roots && out_plan->final_runtime_coalesce_class_sizes) {
        size_t item_index;

        memcpy(out_plan->final_runtime_coalesce_roots,
            runtime_state.runtime_coalesce_roots,
            (function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
        memcpy(out_plan->final_runtime_coalesce_class_sizes,
            runtime_state.runtime_coalesce_class_sizes,
            (function->next_value_id ? function->next_value_id : 1) * sizeof(size_t));
        if (out_plan->value_item_indices) {
            for (item_index = 0; item_index < out_plan->item_count; ++item_index) {
                size_t value_id = out_plan->items[item_index].value_id;

                if (value_id < function->next_value_id) {
                    out_plan->value_item_indices[value_id] = item_index;
                }
            }
        }
        if (out_run_result) {
            out_run_result->published_plan_artifacts = 1u;
        }
    }
    if (out_run_result) {
        out_run_result->status = VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_COMPLETED;
    }
    if (trace_timing) {
        fprintf(stderr,
            "[value-ssa-alloc-timing] fn=%s move-engine summary items=%zu values=%zu move-related-values=%zu steps=%zu coalesce=%zu freeze=%zu simplify=%zu spill=%zu family-member-rebuild=%zu refresh=(general=%zu,coalesce=%zu,affected_total=%zu,affected_max=%zu,phase-entry-rebuild=%zu) scans=(simplify=%zu,freeze=%zu,spill=%zu,simplify-pair=%zu,freeze-pair=%zu)\n",
            function->name ? function->name : "<anon>",
            active_worklist ? active_worklist->item_count : 0u,
            function->next_value_id,
            move_related_value_count,
            out_run_result ? out_run_result->plan_driver.completed_step_count : 0u,
            out_run_result ? out_run_result->coalesce_step_count : 0u,
            out_run_result ? out_run_result->freeze_step_count : 0u,
            out_run_result ? out_run_result->simplify_remove_step_count : 0u,
            out_run_result ? out_run_result->spill_remove_step_count : 0u,
            out_run_result ? out_run_result->family_member_rebuild_count : 0u,
            out_run_result ? out_run_result->general_refresh_count : 0u,
            out_run_result ? out_run_result->coalesce_refresh_count : 0u,
            out_run_result ? out_run_result->refresh_affected_family_root_total : 0u,
            out_run_result ? out_run_result->refresh_affected_family_root_max : 0u,
            out_run_result ? out_run_result->phase_entry_rebuild_count : 0u,
            out_run_result ? out_run_result->simplify_candidate_scan_total : 0u,
            out_run_result ? out_run_result->freeze_candidate_scan_total : 0u,
            out_run_result ? out_run_result->spill_candidate_scan_total : 0u,
            out_run_result ? out_run_result->simplify_pair_scan_total : 0u,
            out_run_result ? out_run_result->freeze_pair_scan_total : 0u);
        value_ssa_alloc_trace_function_timing(function->name, "move-engine finalize", value_ssa_alloc_now_s() - stage_start);
        value_ssa_alloc_trace_function_timing(function->name, "move-engine total", value_ssa_alloc_now_s() - total_start);
    }
    ok = 1;

cleanup:
    free(active_degrees);
    free(initial_effective_degrees);
    free(active_effective_degrees);
    free(family_runtime_facts);
    free(active_family_spill_removed_counts);
    free(active_family_member_counts);
    free(active_family_external_neighbor_counts);
    free(active_family_coalesce_ready_neighbor_counts);
    free(active_family_coalesce_ready_affinity_sums);
    free(active_family_best_coalesce_ready_partner_roots);
    free(active_family_best_coalesce_ready_partner_weights);
    free(active_family_best_coalesce_ready_partner_mutual_flags);
    free(active_family_external_affinity_sums);
    free(active_family_queue_priorities);
    free(active_family_roots);
    free(affected_family_root_flags);
    free(affected_family_roots);
    free(simplify_family_root_flags);
    free(simplify_family_roots);
    free(freeze_family_root_flags);
    free(freeze_family_roots);
    free(spill_family_root_flags);
    free(spill_family_roots);
    free(scratch_roots);
    free(scratch_root_flags);
    free(family_phase_entries);
    free(active_family_move_work_classes);
    free(active_move_related);
    free(initial_family_move_related);
    free(initial_effective_move_related);
    free(active_effective_move_related);
    free(interference_neighbor_offsets);
    free(interference_neighbor_value_ids);
    free(affinity_neighbor_offsets);
    free(affinity_neighbor_value_ids);
    free(affinity_neighbor_weights);
    free(work_item_preview_bases);
    free(family_neighbor_matrix);
    free(family_coalesce_ready_neighbor_matrix);
    free(family_coalesce_ready_affinity_matrix);
    free(consumed_family_coalesce_pair_matrix);
    free(scratch_class_move_related);
    free(runtime_coalesce_roots);
    free(runtime_coalesce_class_sizes);
    free(frozen);
    free(was_frozen);
    free(scheduled);
    value_ssa_alloc_move_engine_family_members_free(&family_members);
    value_ssa_allocator_coalesce_analysis_free(&owned_coalesce);
    value_ssa_copy_affinity_graph_free(&owned_affinity);
    value_ssa_interference_graph_free(&owned_interference);
    value_ssa_allocation_worklist_free(&owned_worklist);
    value_ssa_allocation_prep_free(&owned_prep);
    if (!ok) {
        if (out_plan) {
            value_ssa_allocator_plan_free(out_plan);
        }
        if (out_trace) {
            value_ssa_allocator_move_transition_trace_free(out_trace);
        }
    }
    return ok;
}

#endif
