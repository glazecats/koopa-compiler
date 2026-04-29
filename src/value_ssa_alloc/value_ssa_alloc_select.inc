#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

static void value_ssa_alloc_reset_value_choice_metadata(ValueSsaAllocationResult *result, size_t value_id) {
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

static void value_ssa_alloc_mark_value_recovered(ValueSsaAllocationResult *result,
    size_t value_id,
    size_t recovery_phase_id,
    size_t recovery_phase_member_order) {
    if (!result || value_id >= result->value_count || !result->spill_recovered_flags) {
        return;
    }

    if (result->spill_recovery_orders && result->spill_recovery_orders[value_id] == (size_t)-1) {
        result->spill_recovery_orders[value_id] = result->next_spill_recovery_order++;
    }
    if (result->spill_recovery_phase_ids && result->spill_recovery_phase_ids[value_id] == (size_t)-1) {
        result->spill_recovery_phase_ids[value_id] = recovery_phase_id;
    }
    if (result->spill_recovery_phase_member_orders &&
        result->spill_recovery_phase_member_orders[value_id] == (size_t)-1) {
        result->spill_recovery_phase_member_orders[value_id] = recovery_phase_member_order;
    }
    result->spill_recovered_flags[value_id] = 1;
}

typedef enum {
    VALUE_SSA_ALLOC_SELECT_OPTION_NONE = 0,
    VALUE_SSA_ALLOC_SELECT_OPTION_FREE,
    VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT,
    VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT,
} ValueSsaAllocationSelectOptionKind;

typedef struct {
    ValueSsaAllocationSelectOptionKind kind;
    size_t color;
    size_t blocker_value_id;
    unsigned char blocker_recolorable;
    size_t blocker_recolor_color;
    size_t preferred_class_size;
    ValueSsaAllocationPreferredColorSource preferred_source;
    size_t preferred_partner_value_id;
} ValueSsaAllocationSelectOption;

typedef struct {
    unsigned char has_candidate;
    size_t family_root_value_id;
    size_t value_id;
    ValueSsaAllocationSelectOption option;
    size_t recoverable_member_count;
    size_t recoverable_intended_count;
    size_t split_family_root_value_id;
    size_t split_family_member_count;
    size_t split_family_intended_count;
} ValueSsaAllocationRetryFamilyCandidate;

typedef struct {
    size_t item_count;
    ValueSsaAllocationRetryFamilyCandidate *items;
} ValueSsaAllocationRetryFamilyAgendaScratch;

typedef struct {
    unsigned char active;
    unsigned char keep_active_family_phase;
    size_t recovery_phase_id;
    size_t recovered_member_count;
    size_t eviction_recovery_count;
    size_t spill_count_before;
    ValueSsaAllocatorRetryFamilyAgendaItem entry;
} ValueSsaAllocationRetryPhase;

typedef enum {
    VALUE_SSA_ALLOC_EVICTION_METADATA_GENERIC = 0,
    VALUE_SSA_ALLOC_EVICTION_METADATA_TARGETED,
    VALUE_SSA_ALLOC_EVICTION_METADATA_RETRY,
} ValueSsaAllocationEvictionMetadataKind;

typedef struct {
    unsigned char has_color;
    size_t color;
    ValueSsaAllocationPreferredColorSource preferred_source;
    size_t preferred_partner_value_id;
    int has_eviction;
    int also_record_retry_eviction;
    ValueSsaAllocationEvictionMetadataKind eviction_kind;
    size_t blocker_value_id;
    int blocker_recolored;
    size_t blocker_new_color;
} ValueSsaAllocationSelectClaim;

typedef struct {
    unsigned char *blocked_colors;
    size_t *color_blocker_counts;
    size_t *color_blocker_ids;
    int has_preferred_option;
    ValueSsaAllocationSelectOption preferred_option;
} ValueSsaAllocationSelectQuery;

typedef struct {
    size_t signature;
    size_t attempted_root_count;
    size_t attempted_root_capacity;
    size_t *attempted_root_value_ids;
} ValueSsaAllocationRetryFrontierRecord;

typedef struct {
    ValueSsaAllocatorRetryFamilyAgenda agenda;
    ValueSsaAllocationSelectQuery query;
    size_t color_budget;
    size_t rebuild_count;
    size_t phase_count;
    size_t max_phase_count;
    ValueSsaAllocationRetryFrontierRecord *frontiers;
    size_t frontier_count;
    size_t frontier_capacity;
    unsigned char field_ready;
    unsigned char has_active_phase_entry;
    unsigned char stopped_on_exhausted_frontier;
    ValueSsaAllocatorRetryFamilyAgendaItem active_phase_entry;
} ValueSsaAllocationRetryField;

typedef enum {
    VALUE_SSA_ALLOC_RETRY_PHASE_STEP_NONE = 0,
    VALUE_SSA_ALLOC_RETRY_PHASE_STEP_COMPLETED,
    VALUE_SSA_ALLOC_RETRY_PHASE_STEP_EXHAUSTED,
    VALUE_SSA_ALLOC_RETRY_PHASE_STEP_NO_PROGRESS,
} ValueSsaAllocationRetryPhaseStepStatus;

typedef struct {
    ValueSsaAllocationRetryPhaseStepStatus status;
    ValueSsaAllocationRetryPhase phase;
} ValueSsaAllocationRetryPhaseStepResult;

typedef enum {
    VALUE_SSA_ALLOC_MAIN_SELECT_STEP_NONE = 0,
    VALUE_SSA_ALLOC_MAIN_SELECT_STEP_COMPLETED,
} ValueSsaAllocationMainSelectStepStatus;

typedef struct {
    ValueSsaAllocationMainSelectStepStatus status;
    size_t value_id;
    size_t spill_priority;
    unsigned char spill_intended;
    unsigned char colored;
    unsigned char spilled;
    ValueSsaAllocationSelectClaim claim;
} ValueSsaAllocationMainSelectStepResult;

typedef enum {
    VALUE_SSA_ALLOC_SELECT_PHASE_STEP_NONE = 0,
    VALUE_SSA_ALLOC_SELECT_PHASE_STEP_COMPLETED,
} ValueSsaAllocationSelectPhaseStepStatus;

typedef struct {
    ValueSsaAllocationSelectPhaseStepStatus status;
    size_t completed_main_select_steps;
    size_t completed_retry_phase_steps;
} ValueSsaAllocationSelectPhaseStepResult;

typedef enum {
    VALUE_SSA_ALLOC_EXECUTE_STEP_NONE = 0,
    VALUE_SSA_ALLOC_EXECUTE_STEP_COMPLETED,
} ValueSsaAllocationExecuteStepStatus;

typedef struct {
    ValueSsaAllocationExecuteStepStatus status;
    ValueSsaAllocationSelectPhaseStepResult select_phase;
    unsigned char spill_slots_assigned;
} ValueSsaAllocationExecuteStepResult;

static void value_ssa_alloc_retry_family_candidate_init(ValueSsaAllocationRetryFamilyCandidate *candidate);
static void value_ssa_alloc_retry_field_clear_entries(ValueSsaAllocationRetryField *field);
static void value_ssa_alloc_retry_field_clear_active_phase_entry(ValueSsaAllocationRetryField *field);
static void value_ssa_alloc_retry_phase_step_result_init(ValueSsaAllocationRetryPhaseStepResult *result);
static void value_ssa_alloc_main_select_step_result_init(ValueSsaAllocationMainSelectStepResult *result);
static void value_ssa_alloc_select_phase_step_result_init(ValueSsaAllocationSelectPhaseStepResult *result);
static void value_ssa_alloc_execute_step_result_init(ValueSsaAllocationExecuteStepResult *result);
static void value_ssa_alloc_retry_phase_init(ValueSsaAllocationRetryPhase *phase);
static void value_ssa_alloc_select_claim_init(ValueSsaAllocationSelectClaim *claim);
static int value_ssa_alloc_run_main_select_step(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    const ValueSsaAllocatorPlanItem *plan_item,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *select_query,
    ValueSsaAllocationMainSelectStepResult *out_result,
    ValueSsaError *error);
static int value_ssa_alloc_retry_field_read_active_candidate(const ValueSsaAllocationRetryField *field,
    ValueSsaAllocationRetryFamilyCandidate *out_candidate,
    ValueSsaError *error);
static int value_ssa_alloc_retry_family_agenda_entry_from_candidate(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationRetryFamilyCandidate *candidate,
    ValueSsaAllocatorRetryFamilyAgendaItem *out_entry,
    ValueSsaError *error);
static int value_ssa_alloc_retry_frontier_has_attempted_root(
    const ValueSsaAllocationRetryFrontierRecord *frontier, size_t root_value_id);
static int value_ssa_alloc_retry_frontier_note_attempted_root(
    ValueSsaAllocationRetryFrontierRecord *frontier, size_t root_value_id, ValueSsaError *error);
static int value_ssa_alloc_retry_field_note_frontier(ValueSsaAllocationRetryField *field,
    const ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryFrontierRecord **out_frontier,
    int *out_seen_before,
    ValueSsaError *error);
static void value_ssa_alloc_retry_phase_begin(ValueSsaAllocationRetryPhase *phase,
    const ValueSsaAllocationRetryField *field,
    ValueSsaAllocationResult *result);

static void value_ssa_alloc_select_option_init(ValueSsaAllocationSelectOption *option) {
    if (!option) {
        return;
    }

    option->kind = VALUE_SSA_ALLOC_SELECT_OPTION_NONE;
    option->color = 0;
    option->blocker_value_id = (size_t)-1;
    option->blocker_recolorable = 0;
    option->blocker_recolor_color = 0;
    option->preferred_class_size = 0;
    option->preferred_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    option->preferred_partner_value_id = (size_t)-1;
}

static ValueSsaAllocationSelectOptionKind value_ssa_alloc_select_option_kind_from_retry_entry_kind(
    ValueSsaAllocatorRetryFamilyEntryKind kind) {
    switch (kind) {
        case VALUE_SSA_ALLOCATOR_RETRY_ENTRY_PREFERRED_EVICT:
            return VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT;
        case VALUE_SSA_ALLOCATOR_RETRY_ENTRY_GENERIC_EVICT:
            return VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT;
        case VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE:
        default:
            return VALUE_SSA_ALLOC_SELECT_OPTION_FREE;
    }
}

static int value_ssa_alloc_retry_family_candidate_from_agenda_item(
    const ValueSsaAllocatorRetryFamilyAgendaItem *item,
    ValueSsaAllocationRetryFamilyCandidate *out_candidate,
    ValueSsaError *error) {
    if (out_candidate) {
        value_ssa_alloc_retry_family_candidate_init(out_candidate);
    }
    if (!item || !out_candidate) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-158: invalid retry family agenda entry");
        return 0;
    }

    out_candidate->has_candidate = 1u;
    out_candidate->family_root_value_id = item->root_value_id;
    out_candidate->value_id = item->representative_value_id;
    out_candidate->recoverable_member_count = item->recoverable_member_count;
    out_candidate->recoverable_intended_count = item->recoverable_intended_count;
    out_candidate->split_family_root_value_id = item->split_family_root_value_id;
    out_candidate->split_family_member_count = item->split_family_member_count;
    out_candidate->split_family_intended_count = item->split_family_intended_count;
    out_candidate->option.kind = value_ssa_alloc_select_option_kind_from_retry_entry_kind(item->entry_kind);
    out_candidate->option.color = item->color;
    out_candidate->option.blocker_value_id = item->blocker_value_id;
    out_candidate->option.blocker_recolorable = item->blocker_recolorable;
    out_candidate->option.blocker_recolor_color = item->blocker_recolor_color;
    out_candidate->option.preferred_class_size = item->preferred_class_size;
    out_candidate->option.preferred_source = item->preferred_source;
    out_candidate->option.preferred_partner_value_id = item->preferred_partner_value_id;

    if (item->requires_eviction &&
        out_candidate->option.kind != VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT &&
        out_candidate->option.kind != VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-159: retry family agenda entry lost eviction kind");
        return 0;
    }
    if (!item->requires_eviction && out_candidate->option.kind != VALUE_SSA_ALLOC_SELECT_OPTION_FREE) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-160: retry family agenda entry lost free kind");
        return 0;
    }
    if (item->requires_eviction && item->blocker_value_id == (size_t)-1) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-161: retry family agenda eviction entry missing blocker");
        return 0;
    }

    return 1;
}

static void value_ssa_alloc_retry_field_clear_active_phase_entry(ValueSsaAllocationRetryField *field) {
    if (!field) {
        return;
    }

    field->has_active_phase_entry = 0;
    value_ssa_alloc_retry_family_agenda_entry_clear(&field->active_phase_entry);
}

static void value_ssa_alloc_retry_phase_step_result_init(ValueSsaAllocationRetryPhaseStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_RETRY_PHASE_STEP_NONE;
    value_ssa_alloc_retry_phase_init(&result->phase);
}

static void value_ssa_alloc_main_select_step_result_init(ValueSsaAllocationMainSelectStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_MAIN_SELECT_STEP_NONE;
    result->value_id = (size_t)-1;
    value_ssa_alloc_select_claim_init(&result->claim);
}

static void value_ssa_alloc_select_phase_step_result_init(ValueSsaAllocationSelectPhaseStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_SELECT_PHASE_STEP_NONE;
}

static void value_ssa_alloc_execute_step_result_init(ValueSsaAllocationExecuteStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_EXECUTE_STEP_NONE;
    value_ssa_alloc_select_phase_step_result_init(&result->select_phase);
}

static int value_ssa_alloc_retry_field_read_active_candidate(const ValueSsaAllocationRetryField *field,
    ValueSsaAllocationRetryFamilyCandidate *out_candidate,
    ValueSsaError *error) {
    if (out_candidate) {
        value_ssa_alloc_retry_family_candidate_init(out_candidate);
    }
    if (!field || !out_candidate) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-182: invalid retry field active-candidate query");
        return 0;
    }
    if (!field->has_active_phase_entry) {
        return 1;
    }

    return value_ssa_alloc_retry_family_candidate_from_agenda_item(&field->active_phase_entry, out_candidate, error);
}

static int value_ssa_alloc_retry_family_agenda_entry_from_candidate(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationRetryFamilyCandidate *candidate,
    ValueSsaAllocatorRetryFamilyAgendaItem *out_entry,
    ValueSsaError *error) {
    if (out_entry) {
        value_ssa_alloc_retry_family_agenda_entry_clear(out_entry);
    }
    if (!prep || !result || !candidate || !out_entry || !candidate->has_candidate ||
        candidate->value_id >= result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-181: invalid retry family active-entry materialization");
        return 0;
    }

    out_entry->root_value_id = candidate->family_root_value_id;
    out_entry->representative_value_id = candidate->value_id;
    out_entry->recoverable_member_count = candidate->recoverable_member_count;
    out_entry->recoverable_intended_count = candidate->recoverable_intended_count;
    out_entry->split_family_root_value_id = candidate->split_family_root_value_id;
    out_entry->split_family_member_count = candidate->split_family_member_count;
    out_entry->split_family_intended_count = candidate->split_family_intended_count;
    out_entry->representative_spill_priority = result->spill_priorities[candidate->value_id];
    out_entry->representative_spill_cost = value_ssa_alloc_value_spill_cost_for_select(prep, candidate->value_id);
    switch (candidate->option.kind) {
    case VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT:
        out_entry->entry_kind = VALUE_SSA_ALLOCATOR_RETRY_ENTRY_PREFERRED_EVICT;
        break;
    case VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT:
        out_entry->entry_kind = VALUE_SSA_ALLOCATOR_RETRY_ENTRY_GENERIC_EVICT;
        break;
    case VALUE_SSA_ALLOC_SELECT_OPTION_FREE:
    case VALUE_SSA_ALLOC_SELECT_OPTION_NONE:
    default:
        out_entry->entry_kind = VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE;
        break;
    }
    out_entry->preferred_source = candidate->option.preferred_source;
    out_entry->preferred_class_size = candidate->option.preferred_class_size;
    out_entry->preferred_partner_value_id = candidate->option.preferred_partner_value_id;
    out_entry->color = candidate->option.color;
    out_entry->requires_eviction =
        (candidate->option.kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT ||
            candidate->option.kind == VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT)
        ? 1u
        : 0u;
    out_entry->blocker_value_id = candidate->option.blocker_value_id;
    out_entry->blocker_recolorable = candidate->option.blocker_recolorable ? 1u : 0u;
    out_entry->blocker_recolor_color =
        candidate->option.blocker_recolorable ? candidate->option.blocker_recolor_color : (size_t)-1;
    return 1;
}

static int value_ssa_alloc_retry_field_rebuild(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryField *field,
    ValueSsaError *error) {
    if (!function || !prep || !interference || !plan || !result || !field) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-163: invalid retry field rebuild");
        return 0;
    }
    if (field->max_phase_count == 0) {
        size_t base = result->value_count ? result->value_count : 1;

        field->max_phase_count = base * (field->color_budget + 2);
    }

    value_ssa_alloc_retry_field_clear_entries(field);
    if (!value_ssa_compute_allocator_retry_family_agenda(function,
            prep,
            interference,
            coalesce_analysis,
            plan,
            result,
            field->color_budget,
            &field->agenda,
            error)) {
        value_ssa_alloc_retry_field_clear_entries(field);
        return 0;
    }
    field->field_ready = 1u;
    value_ssa_alloc_retry_field_clear_active_phase_entry(field);
    ++field->rebuild_count;
    return 1;
}

static int value_ssa_alloc_retry_field_has_phase_entry(const ValueSsaAllocationRetryField *field) {
    return field && field->field_ready && field->agenda.item_count > 0;
}

static int value_ssa_alloc_retry_field_begin_phase(ValueSsaAllocationRetryField *field,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryPhase *phase,
    ValueSsaError *error) {
    ValueSsaAllocationRetryFrontierRecord *frontier = NULL;
    int seen_before = 0;
    size_t item_index;

    if (!field || !result || !phase) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-165: invalid retry field phase begin");
        return 0;
    }
    if (field->phase_count >= field->max_phase_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-180: retry phase count exceeded retry field budget");
        return 0;
    }
    if (!value_ssa_alloc_retry_field_note_frontier(field, result, &frontier, &seen_before, error) || !frontier) {
        return 0;
    }
    for (item_index = 0; item_index < field->agenda.item_count; ++item_index) {
        const ValueSsaAllocatorRetryFamilyAgendaItem *entry = &field->agenda.items[item_index];

        if (seen_before && value_ssa_alloc_retry_frontier_has_attempted_root(frontier, entry->root_value_id)) {
            continue;
        }
        if (!value_ssa_alloc_retry_frontier_note_attempted_root(frontier, entry->root_value_id, error)) {
            return 0;
        }
        field->active_phase_entry = *entry;
        field->has_active_phase_entry = 1u;
        ++field->phase_count;
        value_ssa_alloc_retry_phase_begin(phase, field, result);
        return 1;
    }

    field->stopped_on_exhausted_frontier = seen_before ? 1u : 0u;
    field->field_ready = 0;
    value_ssa_alloc_retry_field_clear_active_phase_entry(field);
    return 1;
}

static void value_ssa_alloc_select_query_init(ValueSsaAllocationSelectQuery *query) {
    if (!query) {
        return;
    }

    query->blocked_colors = NULL;
    query->color_blocker_counts = NULL;
    query->color_blocker_ids = NULL;
    query->has_preferred_option = 0;
    value_ssa_alloc_select_option_init(&query->preferred_option);
}

static int value_ssa_alloc_select_query_allocate(
    ValueSsaAllocationSelectQuery *query, size_t color_budget, ValueSsaError *error) {
    size_t alloc_count;

    if (!query) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-143: invalid select-query allocation");
        return 0;
    }

    alloc_count = color_budget ? color_budget : 1;
    query->blocked_colors = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    query->color_blocker_counts = (size_t *)calloc(alloc_count, sizeof(size_t));
    query->color_blocker_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    if (!query->blocked_colors || !query->color_blocker_counts || !query->color_blocker_ids) {
        free(query->blocked_colors);
        free(query->color_blocker_counts);
        free(query->color_blocker_ids);
        query->blocked_colors = NULL;
        query->color_blocker_counts = NULL;
        query->color_blocker_ids = NULL;
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-144: out of memory allocating select-query");
        return 0;
    }

    return 1;
}

static void value_ssa_alloc_select_query_free(ValueSsaAllocationSelectQuery *query) {
    if (!query) {
        return;
    }

    free(query->blocked_colors);
    free(query->color_blocker_counts);
    free(query->color_blocker_ids);
    value_ssa_alloc_select_query_init(query);
}

static void value_ssa_alloc_retry_field_init(ValueSsaAllocationRetryField *field) {
    if (!field) {
        return;
    }

    memset(field, 0, sizeof(*field));
    value_ssa_allocator_retry_family_agenda_init(&field->agenda);
    value_ssa_alloc_select_query_init(&field->query);
    field->frontiers = NULL;
    value_ssa_alloc_retry_family_agenda_entry_clear(&field->active_phase_entry);
}

static void value_ssa_alloc_retry_field_clear_entries(ValueSsaAllocationRetryField *field) {
    if (!field) {
        return;
    }

    value_ssa_allocator_retry_family_agenda_free(&field->agenda);
    field->field_ready = 0;
    value_ssa_alloc_retry_field_clear_active_phase_entry(field);
}

static void value_ssa_alloc_retry_field_free(ValueSsaAllocationRetryField *field) {
    size_t frontier_index;

    if (!field) {
        return;
    }

    value_ssa_alloc_retry_field_clear_entries(field);
    value_ssa_alloc_select_query_free(&field->query);
    for (frontier_index = 0; frontier_index < field->frontier_count; ++frontier_index) {
        free(field->frontiers[frontier_index].attempted_root_value_ids);
    }
    free(field->frontiers);
    field->frontiers = NULL;
    field->frontier_count = 0;
    field->frontier_capacity = 0;
    field->color_budget = 0;
    field->rebuild_count = 0;
    field->phase_count = 0;
    field->max_phase_count = 0;
    field->stopped_on_exhausted_frontier = 0;
}

static int value_ssa_alloc_retry_field_prepare(
    ValueSsaAllocationRetryField *field, size_t color_budget, ValueSsaError *error) {
    if (!field) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-162: invalid retry field prepare");
        return 0;
    }

    value_ssa_alloc_retry_field_init(field);
    if (!value_ssa_alloc_select_query_allocate(&field->query, color_budget, error)) {
        value_ssa_alloc_retry_field_free(field);
        return 0;
    }
    field->color_budget = color_budget;
    return 1;
}

static size_t value_ssa_alloc_retry_frontier_signature(const ValueSsaAllocationResult *result) {
    size_t value_id;
    size_t hash = (size_t)1469598103934665603ull;
    const size_t prime = (size_t)1099511628211ull;

    if (!result) {
        return 0;
    }

    hash ^= result->value_count + 1;
    hash *= prime;
    for (value_id = 0; value_id < result->value_count; ++value_id) {
        size_t state = 0;

        if (result->spill_flags && result->spill_flags[value_id]) {
            state |= 1u;
        }
        if (result->has_color && result->has_color[value_id]) {
            size_t color = result->colors ? result->colors[value_id] : 0;

            state |= 2u;
            state ^= (color + 1) << 2;
        }
        hash ^= state + (value_id + 1) * 131u;
        hash *= prime;
    }

    return hash;
}

static int value_ssa_alloc_retry_frontier_has_attempted_root(const ValueSsaAllocationRetryFrontierRecord *frontier,
    size_t root_value_id) {
    size_t index;

    if (!frontier) {
        return 0;
    }
    for (index = 0; index < frontier->attempted_root_count; ++index) {
        if (frontier->attempted_root_value_ids[index] == root_value_id) {
            return 1;
        }
    }
    return 0;
}

static int value_ssa_alloc_retry_frontier_note_attempted_root(ValueSsaAllocationRetryFrontierRecord *frontier,
    size_t root_value_id,
    ValueSsaError *error) {
    size_t next_capacity;
    size_t *grown = NULL;

    if (!frontier) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-175: invalid retry frontier root note");
        return 0;
    }
    if (value_ssa_alloc_retry_frontier_has_attempted_root(frontier, root_value_id)) {
        return 1;
    }
    if (frontier->attempted_root_count == frontier->attempted_root_capacity) {
        if (!value_ssa_alloc_next_growth_capacity(
                frontier->attempted_root_capacity, 4, sizeof(size_t), &next_capacity)) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-176: retry frontier root history capacity overflow");
            return 0;
        }
        grown = (size_t *)realloc(frontier->attempted_root_value_ids, next_capacity * sizeof(size_t));
        if (!grown) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-177: out of memory growing retry frontier root history");
            return 0;
        }
        frontier->attempted_root_value_ids = grown;
        frontier->attempted_root_capacity = next_capacity;
    }
    frontier->attempted_root_value_ids[frontier->attempted_root_count++] = root_value_id;
    return 1;
}

static int value_ssa_alloc_retry_field_note_frontier(ValueSsaAllocationRetryField *field,
    const ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryFrontierRecord **out_frontier,
    int *out_seen_before,
    ValueSsaError *error) {
    size_t signature;
    size_t index;
    size_t next_capacity;
    ValueSsaAllocationRetryFrontierRecord *grown = NULL;

    if (out_frontier) {
        *out_frontier = NULL;
    }
    if (out_seen_before) {
        *out_seen_before = 0;
    }
    if (!field || !result || !out_frontier || !out_seen_before) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-174: invalid retry frontier note");
        return 0;
    }

    signature = value_ssa_alloc_retry_frontier_signature(result);
    for (index = 0; index < field->frontier_count; ++index) {
        if (field->frontiers[index].signature == signature) {
            *out_seen_before = 1;
            *out_frontier = &field->frontiers[index];
            return 1;
        }
    }

    if (field->frontier_count == field->frontier_capacity) {
        if (!value_ssa_alloc_next_growth_capacity(
                field->frontier_capacity, 8, sizeof(ValueSsaAllocationRetryFrontierRecord), &next_capacity)) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-178: retry frontier history capacity overflow");
            return 0;
        }
        grown = (ValueSsaAllocationRetryFrontierRecord *)realloc(
            field->frontiers, next_capacity * sizeof(ValueSsaAllocationRetryFrontierRecord));
        if (!grown) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-179: out of memory growing retry frontier history");
            return 0;
        }
        field->frontiers = grown;
        field->frontier_capacity = next_capacity;
    }

    field->frontiers[field->frontier_count].signature = signature;
    field->frontiers[field->frontier_count].attempted_root_count = 0;
    field->frontiers[field->frontier_count].attempted_root_capacity = 0;
    field->frontiers[field->frontier_count].attempted_root_value_ids = NULL;
    *out_frontier = &field->frontiers[field->frontier_count];
    ++field->frontier_count;
    return 1;
}

static void value_ssa_alloc_retry_family_candidate_init(ValueSsaAllocationRetryFamilyCandidate *candidate) {
    if (!candidate) {
        return;
    }

    memset(candidate, 0, sizeof(*candidate));
    candidate->family_root_value_id = (size_t)-1;
    candidate->value_id = (size_t)-1;
    candidate->split_family_root_value_id = (size_t)-1;
    value_ssa_alloc_select_option_init(&candidate->option);
}

static size_t value_ssa_alloc_split_family_root_value_id(const ValueSsaAllocationPrep *prep, size_t value_id) {
    size_t split_root_value_id;

    if (!prep || value_id >= prep->value_count) {
        return value_id;
    }
    if (prep->split_family_root_value_ids) {
        split_root_value_id = prep->split_family_root_value_ids[value_id];
        if (split_root_value_id < prep->value_count) {
            return split_root_value_id;
        }
    }
    return value_id;
}

static int value_ssa_alloc_retry_candidate_is_spill_intended(const ValueSsaAllocationResult *result, size_t value_id) {
    return result && value_id < result->value_count && result->spill_intended_flags &&
        result->spill_intended_flags[value_id];
}

static int value_ssa_alloc_compare_retry_recovery_candidates(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    const ValueSsaAllocationSelectOption *candidate_option,
    size_t best_value_id,
    const ValueSsaAllocationSelectOption *best_option,
    size_t color_budget,
    int *out_compare,
    ValueSsaError *error);
static int value_ssa_alloc_collect_retry_family_candidate_for_root(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t family_root_value_id,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *retry_query,
    int *out_found,
    ValueSsaAllocationRetryFamilyCandidate *out_candidate,
    ValueSsaError *error);
static void value_ssa_alloc_retry_phase_begin(ValueSsaAllocationRetryPhase *phase,
    const ValueSsaAllocationRetryField *field,
    ValueSsaAllocationResult *result);
static int value_ssa_alloc_resolve_retry_recovery_claim(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *retry_option,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error);

static int value_ssa_alloc_compare_retry_family_candidates(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationRetryFamilyCandidate *candidate,
    const ValueSsaAllocationRetryFamilyCandidate *best_candidate,
    size_t color_budget,
    int *out_compare,
    ValueSsaError *error) {
    if (out_compare) {
        *out_compare = 0;
    }
    if (!prep || !interference || !plan || !result || !candidate || !best_candidate || !out_compare) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-145: invalid retry family-candidate comparison");
        return 0;
    }

    if (candidate->recoverable_intended_count != best_candidate->recoverable_intended_count) {
        *out_compare = candidate->recoverable_intended_count > best_candidate->recoverable_intended_count ? 1 : -1;
        return 1;
    }
    if (candidate->recoverable_member_count != best_candidate->recoverable_member_count) {
        *out_compare = candidate->recoverable_member_count > best_candidate->recoverable_member_count ? 1 : -1;
        return 1;
    }
    if (candidate->split_family_member_count != best_candidate->split_family_member_count) {
        *out_compare = candidate->split_family_member_count > best_candidate->split_family_member_count ? 1 : -1;
        return 1;
    }
    if (candidate->split_family_intended_count != best_candidate->split_family_intended_count) {
        *out_compare = candidate->split_family_intended_count > best_candidate->split_family_intended_count ? 1 : -1;
        return 1;
    }

    return value_ssa_alloc_compare_retry_recovery_candidates(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        candidate->value_id,
        &candidate->option,
        best_candidate->value_id,
        &best_candidate->option,
        color_budget,
        out_compare,
        error);
}

static void value_ssa_alloc_retry_family_agenda_scratch_init(ValueSsaAllocationRetryFamilyAgendaScratch *agenda) {
    if (!agenda) {
        return;
    }

    agenda->item_count = 0;
    agenda->items = NULL;
}

static void value_ssa_alloc_retry_family_agenda_scratch_free(ValueSsaAllocationRetryFamilyAgendaScratch *agenda) {
    if (!agenda) {
        return;
    }

    free(agenda->items);
    value_ssa_alloc_retry_family_agenda_scratch_init(agenda);
}

static int value_ssa_alloc_build_retry_family_agenda(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *retry_query,
    ValueSsaAllocationRetryFamilyAgendaScratch *agenda,
    ValueSsaError *error) {
    size_t value_id;

    if (!prep || !interference || !plan || !result || !retry_query || !agenda) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-148: invalid retry family-agenda build");
        return 0;
    }

    value_ssa_alloc_retry_family_agenda_scratch_free(agenda);
    agenda->items = (ValueSsaAllocationRetryFamilyCandidate *)calloc(
        result->value_count ? result->value_count : 1,
        sizeof(ValueSsaAllocationRetryFamilyCandidate));
    if (!agenda->items) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-149: out of memory preparing retry family agenda");
        return 0;
    }

    for (value_id = 0; value_id < result->value_count; ++value_id) {
        ValueSsaAllocationRetryFamilyCandidate candidate;
        int found = 0;
        size_t insert_index;

        if (!value_ssa_alloc_collect_retry_family_candidate_for_root(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                value_id,
                color_budget,
                retry_query,
                &found,
                &candidate,
                error)) {
            value_ssa_alloc_retry_family_agenda_scratch_free(agenda);
            return 0;
        }
        if (!found) {
            continue;
        }

        insert_index = agenda->item_count;
        while (insert_index > 0) {
            int compare = 0;

            if (!value_ssa_alloc_compare_retry_family_candidates(prep,
                    coalesce_analysis,
                    interference,
                    plan,
                    result,
                    &candidate,
                    &agenda->items[insert_index - 1],
                    color_budget,
                    &compare,
                    error)) {
                value_ssa_alloc_retry_family_agenda_scratch_free(agenda);
                return 0;
            }
            if (compare <= 0) {
                break;
            }
            agenda->items[insert_index] = agenda->items[insert_index - 1];
            --insert_index;
        }
        agenda->items[insert_index] = candidate;
        ++agenda->item_count;
    }

    return 1;
}

static void value_ssa_alloc_retry_phase_init(ValueSsaAllocationRetryPhase *phase) {
    if (!phase) {
        return;
    }

    memset(phase, 0, sizeof(*phase));
    phase->recovery_phase_id = (size_t)-1;
    value_ssa_alloc_retry_family_agenda_entry_clear(&phase->entry);
}

static void value_ssa_alloc_retry_phase_begin(ValueSsaAllocationRetryPhase *phase,
    const ValueSsaAllocationRetryField *field,
    ValueSsaAllocationResult *result) {
    size_t spill_count_before = 0;
    const ValueSsaAllocatorRetryFamilyAgendaItem *entry = NULL;

    if (!phase || !field || !result || !field->has_active_phase_entry) {
        return;
    }
    entry = &field->active_phase_entry;

    value_ssa_alloc_retry_phase_init(phase);
    phase->active = 1u;
    phase->entry = *entry;
    phase->keep_active_family_phase =
        (entry->recoverable_intended_count > 0 || entry->recoverable_member_count > 1) ? 1u : 0u;
    phase->recovery_phase_id = result->next_spill_recovery_phase_id++;
    phase->recovered_member_count = 0;
    phase->eviction_recovery_count = 0;
    if (!value_ssa_allocation_result_count_spilled_values(result, &spill_count_before)) {
        spill_count_before = 0;
    }
    phase->spill_count_before = spill_count_before;
}

static int value_ssa_alloc_retry_field_refresh_active_family_candidate(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryField *field,
    const ValueSsaAllocationRetryPhase *phase,
    int *out_found,
    ValueSsaError *error) {
    if (out_found) {
        *out_found = 0;
    }
    if (!prep || !interference || !plan || !result || !field || !phase || !out_found) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-167: invalid retry field family refresh");
        return 0;
    }
    if (!phase->active || phase->entry.root_value_id >= result->value_count) {
        return 1;
    }

    {
        ValueSsaAllocationRetryFamilyCandidate candidate;

        value_ssa_alloc_retry_family_candidate_init(&candidate);
        if (!value_ssa_alloc_collect_retry_family_candidate_for_root(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                phase->entry.root_value_id,
                field->color_budget,
                &field->query,
                out_found,
                &candidate,
                error)) {
            return 0;
        }
        if (*out_found &&
            !value_ssa_alloc_retry_family_agenda_entry_from_candidate(
                prep, result, &candidate, &field->active_phase_entry, error)) {
            return 0;
        }
    }
    return 1;
}

static int value_ssa_alloc_retry_phase_refresh_active_entry(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryField *field,
    ValueSsaAllocationRetryPhase *phase,
    int *out_found,
    ValueSsaError *error) {
    if (out_found) {
        *out_found = 0;
    }
    if (!phase || !field || !out_found) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-183: invalid retry phase active-entry refresh");
        return 0;
    }
    if (!value_ssa_alloc_retry_field_refresh_active_family_candidate(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        field,
        phase,
        out_found,
        error)) {
        return 0;
    }
    return 1;
}

static void value_ssa_alloc_select_claim_init(ValueSsaAllocationSelectClaim *claim) {
    if (!claim) {
        return;
    }

    memset(claim, 0, sizeof(*claim));
    claim->preferred_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    claim->preferred_partner_value_id = (size_t)-1;
    claim->eviction_kind = VALUE_SSA_ALLOC_EVICTION_METADATA_GENERIC;
    claim->blocker_value_id = (size_t)-1;
    claim->blocker_new_color = (size_t)-1;
}

static void value_ssa_alloc_select_claim_set_no_color(ValueSsaAllocationSelectClaim *claim) {
    if (!claim) {
        return;
    }

    claim->has_color = 0u;
    claim->color = 0;
    claim->preferred_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    claim->preferred_partner_value_id = (size_t)-1;
    claim->has_eviction = 0;
    claim->also_record_retry_eviction = 0;
    claim->eviction_kind = VALUE_SSA_ALLOC_EVICTION_METADATA_GENERIC;
    claim->blocker_value_id = (size_t)-1;
    claim->blocker_recolored = 0;
    claim->blocker_new_color = (size_t)-1;
}

static void value_ssa_alloc_select_claim_set_direct_color(ValueSsaAllocationSelectClaim *claim,
    size_t color,
    ValueSsaAllocationPreferredColorSource preferred_source,
    size_t preferred_partner_value_id) {
    if (!claim) {
        return;
    }

    value_ssa_alloc_select_claim_init(claim);
    claim->has_color = 1u;
    claim->color = color;
    claim->preferred_source = preferred_source;
    claim->preferred_partner_value_id = preferred_partner_value_id;
}

static void value_ssa_alloc_select_claim_set_eviction_color(ValueSsaAllocationSelectClaim *claim,
    size_t color,
    ValueSsaAllocationPreferredColorSource preferred_source,
    size_t preferred_partner_value_id,
    ValueSsaAllocationEvictionMetadataKind eviction_kind,
    int also_record_retry_eviction,
    size_t blocker_value_id,
    int blocker_recolored,
    size_t blocker_new_color) {
    if (!claim) {
        return;
    }

    value_ssa_alloc_select_claim_init(claim);
    claim->has_color = 1u;
    claim->color = color;
    claim->preferred_source = preferred_source;
    claim->preferred_partner_value_id = preferred_partner_value_id;
    claim->has_eviction = blocker_value_id != (size_t)-1;
    claim->also_record_retry_eviction = also_record_retry_eviction ? 1 : 0;
    claim->eviction_kind = eviction_kind;
    claim->blocker_value_id = blocker_value_id;
    claim->blocker_recolored = blocker_recolored ? 1 : 0;
    claim->blocker_new_color = blocker_recolored ? blocker_new_color : (size_t)-1;
}

static void value_ssa_alloc_apply_preferred_color_metadata(ValueSsaAllocationResult *result,
    size_t value_id,
    ValueSsaAllocationPreferredColorSource source,
    size_t partner_value_id) {
    if (!result || value_id >= result->value_count || source == VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE) {
        return;
    }

    if (result->used_preferred_color_flags) {
        result->used_preferred_color_flags[value_id] = 1;
    }
    if (result->preferred_color_sources) {
        result->preferred_color_sources[value_id] = source;
    }
    if (result->preferred_color_partner_value_ids) {
        result->preferred_color_partner_value_ids[value_id] = partner_value_id;
    }
}

static void value_ssa_alloc_apply_eviction_metadata(ValueSsaAllocationResult *result,
    size_t value_id,
    ValueSsaAllocationEvictionMetadataKind kind,
    size_t blocker_value_id,
    int blocker_recolored,
    size_t blocker_new_color) {
    if (!result || value_id >= result->value_count || blocker_value_id == (size_t)-1) {
        return;
    }

    switch (kind) {
    case VALUE_SSA_ALLOC_EVICTION_METADATA_GENERIC:
        if (result->used_generic_eviction_flags) {
            result->used_generic_eviction_flags[value_id] = 1;
        }
        if (result->generic_eviction_blocker_value_ids) {
            result->generic_eviction_blocker_value_ids[value_id] = blocker_value_id;
        }
        if (result->generic_eviction_blocker_recolored_flags) {
            result->generic_eviction_blocker_recolored_flags[value_id] = blocker_recolored ? 1u : 0u;
        }
        if (result->generic_eviction_blocker_new_colors) {
            result->generic_eviction_blocker_new_colors[value_id] =
                blocker_recolored ? blocker_new_color : (size_t)-1;
        }
        break;
    case VALUE_SSA_ALLOC_EVICTION_METADATA_TARGETED:
        if (result->used_targeted_eviction_flags) {
            result->used_targeted_eviction_flags[value_id] = 1;
        }
        if (result->targeted_eviction_blocker_value_ids) {
            result->targeted_eviction_blocker_value_ids[value_id] = blocker_value_id;
        }
        if (result->targeted_eviction_blocker_recolored_flags) {
            result->targeted_eviction_blocker_recolored_flags[value_id] = blocker_recolored ? 1u : 0u;
        }
        if (result->targeted_eviction_blocker_new_colors) {
            result->targeted_eviction_blocker_new_colors[value_id] =
                blocker_recolored ? blocker_new_color : (size_t)-1;
        }
        break;
    case VALUE_SSA_ALLOC_EVICTION_METADATA_RETRY:
        if (result->used_retry_eviction_flags) {
            result->used_retry_eviction_flags[value_id] = 1;
        }
        if (result->retry_eviction_blocker_value_ids) {
            result->retry_eviction_blocker_value_ids[value_id] = blocker_value_id;
        }
        if (result->retry_eviction_blocker_recolored_flags) {
            result->retry_eviction_blocker_recolored_flags[value_id] = blocker_recolored ? 1u : 0u;
        }
        if (result->retry_eviction_blocker_new_colors) {
            result->retry_eviction_blocker_new_colors[value_id] =
                blocker_recolored ? blocker_new_color : (size_t)-1;
        }
        break;
    default:
        break;
    }
}

static void value_ssa_alloc_assign_colored_result(ValueSsaAllocationResult *result,
    size_t value_id,
    size_t color,
    int clear_confirmed_spill) {
    if (!result || value_id >= result->value_count) {
        return;
    }

    result->has_color[value_id] = 1;
    result->colors[value_id] = color;
    if (clear_confirmed_spill && result->spill_confirmed_flags) {
        result->spill_confirmed_flags[value_id] = 0;
    }
}

static void value_ssa_alloc_apply_select_claim(ValueSsaAllocationResult *result,
    size_t value_id,
    const ValueSsaAllocationSelectClaim *claim) {
    if (!result || !claim || value_id >= result->value_count || !claim->has_color) {
        return;
    }

    value_ssa_alloc_assign_colored_result(result, value_id, claim->color, 1);
    value_ssa_alloc_apply_preferred_color_metadata(
        result, value_id, claim->preferred_source, claim->preferred_partner_value_id);
    if (claim->has_eviction && claim->blocker_value_id != (size_t)-1) {
        value_ssa_alloc_apply_eviction_metadata(result,
            value_id,
            claim->eviction_kind,
            claim->blocker_value_id,
            claim->blocker_recolored,
            claim->blocker_new_color);
        if (claim->also_record_retry_eviction && claim->eviction_kind != VALUE_SSA_ALLOC_EVICTION_METADATA_RETRY) {
            value_ssa_alloc_apply_eviction_metadata(result,
                value_id,
                VALUE_SSA_ALLOC_EVICTION_METADATA_RETRY,
                claim->blocker_value_id,
                claim->blocker_recolored,
                claim->blocker_new_color);
        }
    }
}

static void value_ssa_alloc_apply_recovered_select_claim(ValueSsaAllocationResult *result,
    size_t value_id,
    size_t recovery_phase_id,
    size_t recovery_phase_member_order,
    const ValueSsaAllocatorRetryFamilyAgendaItem *recovery_phase_entry,
    const ValueSsaAllocationSelectClaim *claim) {
    if (!result || !claim || value_id >= result->value_count) {
        return;
    }

    result->spill_flags[value_id] = 0;
    result->spill_slots[value_id] = (size_t)-1;
    if (result->spill_confirmed_flags) {
        result->spill_confirmed_flags[value_id] = 0;
    }
    value_ssa_alloc_mark_value_recovered(result, value_id, recovery_phase_id, recovery_phase_member_order);
    if (result->spill_recovery_phase_entries && recovery_phase_entry &&
        result->spill_recovery_phase_entries[value_id].representative_value_id == (size_t)-1) {
        result->spill_recovery_phase_entries[value_id] = *recovery_phase_entry;
    }
    value_ssa_alloc_reset_value_choice_metadata(result, value_id);
    value_ssa_alloc_apply_select_claim(result, value_id, claim);
}

static int value_ssa_alloc_drain_retry_family_phase(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryField *field,
    ValueSsaAllocationRetryPhase *phase,
    ValueSsaError *error) {
    ValueSsaAllocationRetryFamilyCandidate active_candidate;
    int found_retry = 0;
    ValueSsaAllocationSelectClaim retry_claim;

    if (!prep || !interference || !plan || !result || !field || !phase) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-147: invalid retry family-phase drain");
        return 0;
    }

    value_ssa_alloc_retry_family_candidate_init(&active_candidate);
    value_ssa_alloc_select_claim_init(&retry_claim);
    for (;;) {
        if (!value_ssa_alloc_retry_field_read_active_candidate(field, &active_candidate, error)) {
            return 0;
        }
        if (!active_candidate.has_candidate) {
            break;
        }
        value_ssa_alloc_select_claim_init(&retry_claim);
        if (!value_ssa_alloc_resolve_retry_recovery_claim(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                active_candidate.value_id,
                result->spill_priorities[active_candidate.value_id],
                field->color_budget,
                &active_candidate.option,
                &retry_claim,
                error)) {
            return 0;
        }
        if (retry_claim.has_color) {
            value_ssa_alloc_apply_recovered_select_claim(result,
                active_candidate.value_id,
                phase->recovery_phase_id,
                phase->recovered_member_count,
                &phase->entry,
                &retry_claim);
            ++phase->recovered_member_count;
            if (retry_claim.has_eviction && !retry_claim.blocker_recolored) {
                ++phase->eviction_recovery_count;
            }
        }
        if (!phase->keep_active_family_phase || phase->entry.root_value_id >= result->value_count) {
            break;
        }
        if (!value_ssa_alloc_retry_phase_refresh_active_entry(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                field,
                phase,
                &found_retry,
                error)) {
            return 0;
        }
        if (!found_retry) {
            break;
        }
    }

    return 1;
}

static void value_ssa_alloc_finalize_retry_family_phase(ValueSsaAllocationResult *result,
    const ValueSsaAllocationRetryPhase *phase) {
    size_t value_id;

    if (!result || !phase || !phase->active || !result->spill_recovery_phase_member_counts) {
        return;
    }

    for (value_id = 0; value_id < result->value_count; ++value_id) {
        if (!result->spill_recovered_flags || !result->spill_recovered_flags[value_id] ||
            !result->spill_recovery_phase_ids || result->spill_recovery_phase_ids[value_id] != phase->recovery_phase_id) {
            continue;
        }
        result->spill_recovery_phase_member_counts[value_id] = phase->recovered_member_count;
    }
}

static int value_ssa_alloc_retry_field_finish_phase(
    ValueSsaAllocationRetryField *field,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryPhase *phase,
    ValueSsaError *error) {
    size_t spill_count_after = 0;
    size_t expected_spill_count_after = 0;

    if (!field) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-168: invalid retry field phase finish");
        return 0;
    }
    if (!result || !phase || !phase->active) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-169: invalid retry phase finish contract");
        return 0;
    }
    if (phase->recovered_member_count == 0) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-170: retry phase made no recovery progress");
        return 0;
    }
    if (!value_ssa_allocation_result_count_spilled_values(result, &spill_count_after)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-171: failed counting spills after retry phase");
        return 0;
    }
    if (phase->recovered_member_count < phase->eviction_recovery_count ||
        phase->spill_count_before < (phase->recovered_member_count - phase->eviction_recovery_count)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-172: invalid retry phase spill accounting");
        return 0;
    }

    expected_spill_count_after = phase->spill_count_before -
        (phase->recovered_member_count - phase->eviction_recovery_count);
    if (spill_count_after != expected_spill_count_after) {
        value_ssa_alloc_set_error(
            error, 0, 0, "VALUE-SSA-ALLOC-173: retry phase spill accounting drifted");
        return 0;
    }

    value_ssa_alloc_finalize_retry_family_phase(result, phase);
    value_ssa_alloc_retry_field_clear_active_phase_entry(field);
    value_ssa_alloc_retry_phase_init(phase);
    return 1;
}

static int value_ssa_alloc_retry_run_phase_step(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationRetryField *field,
    ValueSsaAllocationRetryPhaseStepResult *out_result,
    ValueSsaError *error) {
    if (out_result) {
        value_ssa_alloc_retry_phase_step_result_init(out_result);
    }
    if (!function || !prep || !interference || !result || !field || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-184: invalid retry phase-step contract");
        return 0;
    }

    if (!value_ssa_alloc_retry_field_rebuild(function,
            prep,
            coalesce_analysis,
            interference,
            plan,
            result,
            field,
            error)) {
        return 0;
    }
    if (!value_ssa_alloc_retry_field_has_phase_entry(field)) {
        out_result->status = VALUE_SSA_ALLOC_RETRY_PHASE_STEP_EXHAUSTED;
        return 1;
    }
    if (!value_ssa_alloc_retry_field_begin_phase(field, result, &out_result->phase, error)) {
        return 0;
    }
    if (!out_result->phase.active) {
        out_result->status = VALUE_SSA_ALLOC_RETRY_PHASE_STEP_NO_PROGRESS;
        return 1;
    }

    if (!value_ssa_alloc_drain_retry_family_phase(prep,
            coalesce_analysis,
            interference,
            plan,
            result,
            field,
            &out_result->phase,
            error)) {
        return 0;
    }

    if (!value_ssa_alloc_retry_field_finish_phase(field, result, &out_result->phase, error)) {
        return 0;
    }
    out_result->status = VALUE_SSA_ALLOC_RETRY_PHASE_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_collect_color_blocker_facts(const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    size_t color_budget,
    unsigned char *out_blocked_colors,
    size_t *out_color_blocker_counts,
    size_t *out_color_blocker_ids,
    ValueSsaError *error) {
    size_t neighbor_value_id;
    size_t color_index;

    if (!interference || !result || value_id >= interference->value_count || value_id >= result->value_count ||
        !out_blocked_colors || !out_color_blocker_counts || !out_color_blocker_ids) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-138: invalid color-blocker facts query");
        return 0;
    }

    memset(out_blocked_colors, 0, (color_budget ? color_budget : 1) * sizeof(unsigned char));
    memset(out_color_blocker_counts, 0, (color_budget ? color_budget : 1) * sizeof(size_t));
    for (color_index = 0; color_index < color_budget; ++color_index) {
        out_color_blocker_ids[color_index] = (size_t)-1;
    }

    for (neighbor_value_id = 0; neighbor_value_id < interference->value_count; ++neighbor_value_id) {
        size_t neighbor_color;

        if (!interference->interferes[value_id * interference->value_count + neighbor_value_id] ||
            neighbor_value_id >= result->value_count || !result->has_color[neighbor_value_id] ||
            result->spill_flags[neighbor_value_id] || result->colors[neighbor_value_id] >= color_budget) {
            continue;
        }
        neighbor_color = result->colors[neighbor_value_id];
        out_blocked_colors[neighbor_color] = 1u;
        ++out_color_blocker_counts[neighbor_color];
        out_color_blocker_ids[neighbor_color] = neighbor_value_id;
    }

    return 1;
}

static int value_ssa_alloc_materialize_eviction_backed_claim(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *option,
    ValueSsaAllocationEvictionMetadataKind eviction_kind,
    int also_record_retry_eviction,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error) {
    int has_evicted_color = 0;
    size_t evicted_color = 0;
    size_t blocker_value_id = (size_t)-1;
    int blocker_recolored = 0;
    size_t blocker_new_color = (size_t)-1;

    if (!prep || !interference || !plan || !result || !option || !out_claim) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-137: invalid eviction-backed claim query");
        return 0;
    }

    if (option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT) {
        if (!value_ssa_alloc_try_evict_lower_priority_neighbor_for_color(interference,
                prep,
                coalesce_analysis,
                plan,
                result,
                value_id,
                spill_priority,
                option->color,
                color_budget,
                &has_evicted_color,
                &evicted_color,
                &blocker_value_id,
                &blocker_recolored,
                &blocker_new_color,
                error)) {
            return 0;
        }
    } else {
        if (!value_ssa_alloc_try_evict_lower_priority_neighbor(interference,
                prep,
                coalesce_analysis,
                plan,
                result,
                value_id,
                spill_priority,
                color_budget,
                &has_evicted_color,
                &evicted_color,
                &blocker_value_id,
                &blocker_recolored,
                &blocker_new_color,
                error)) {
            return 0;
        }
    }

    if (!has_evicted_color) {
        value_ssa_alloc_select_claim_set_no_color(out_claim);
        return 1;
    }

    value_ssa_alloc_select_claim_set_eviction_color(out_claim,
        evicted_color,
        option->preferred_source,
        option->preferred_partner_value_id,
        eviction_kind,
        also_record_retry_eviction,
        blocker_value_id,
        blocker_recolored,
        blocker_new_color);
    return 1;
}

static int value_ssa_alloc_select_option_requires_eviction(const ValueSsaAllocationSelectOption *option);

static int value_ssa_alloc_choose_best_free_color(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    size_t value_id,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color);

static int value_ssa_alloc_materialize_select_claim_from_option(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *option,
    ValueSsaAllocationEvictionMetadataKind generic_eviction_kind,
    int also_record_retry_eviction,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error) {
    if (!prep || !interference || !plan || !result || !option || !out_claim) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-139: invalid select-claim materialization");
        return 0;
    }

    value_ssa_alloc_select_claim_init(out_claim);
    if (option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_NONE) {
        return 1;
    }
    if (!value_ssa_alloc_select_option_requires_eviction(option)) {
        value_ssa_alloc_select_claim_set_direct_color(
            out_claim, option->color, option->preferred_source, option->preferred_partner_value_id);
        return 1;
    }

    return value_ssa_alloc_materialize_eviction_backed_claim(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        value_id,
        spill_priority,
        color_budget,
        option,
        option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT
            ? VALUE_SSA_ALLOC_EVICTION_METADATA_TARGETED
            : generic_eviction_kind,
        also_record_retry_eviction && option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT,
        out_claim,
        error);
}

static int value_ssa_alloc_find_preferred_claim_option(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    int allow_analysis_only_class_color,
    size_t value_id,
    size_t color_budget,
    int *out_has_option,
    ValueSsaAllocationSelectOption *out_option,
    ValueSsaError *error);

static int value_ssa_alloc_find_generic_eviction_option_from_color_facts(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t value_priority,
    const size_t *color_blocker_counts,
    const size_t *color_blocker_ids,
    size_t color_budget,
    int *out_has_option,
    ValueSsaAllocationSelectOption *out_option,
    ValueSsaError *error);

static int value_ssa_alloc_prepare_select_query(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    int allow_analysis_only_class_color,
    size_t value_id,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *query,
    ValueSsaError *error);

static int value_ssa_alloc_find_select_option_from_query(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationSelectQuery *query,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    int *out_has_option,
    ValueSsaAllocationSelectOption *out_option,
    ValueSsaError *error);

static int value_ssa_alloc_choose_best_free_color(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    size_t value_id,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color);

static int value_ssa_alloc_materialize_eviction_backed_claim(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *option,
    ValueSsaAllocationEvictionMetadataKind eviction_kind,
    int also_record_retry_eviction,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error);

static int value_ssa_alloc_materialize_select_claim_from_option(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *option,
    ValueSsaAllocationEvictionMetadataKind generic_eviction_kind,
    int also_record_retry_eviction,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error);

static int value_ssa_alloc_resolve_retry_recovery_claim(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *retry_option,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error);

static int value_ssa_alloc_select_option_requires_eviction(const ValueSsaAllocationSelectOption *option) {
    return option && (option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT ||
        option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT);
}

static int value_ssa_alloc_retry_preferred_source_rank(ValueSsaAllocationPreferredColorSource source) {
    switch (source) {
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT:
        return 3;
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS:
        return 2;
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_AFFINITY:
        return 1;
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE:
    default:
        return 0;
    }
}

static int value_ssa_alloc_retry_candidate_has_heavier_family_pressure(const ValueSsaAllocatorPlan *plan,
    size_t candidate_value_id,
    size_t best_value_id) {
    int compare = 0;

    if (!value_ssa_alloc_compare_family_pressure_values(plan, candidate_value_id, best_value_id, &compare)) {
        return 0;
    }
    return compare > 0;
}

static int value_ssa_alloc_retry_candidate_has_heavier_runtime_family_activity(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t best_value_id) {
    int compare = 0;

    if (!value_ssa_alloc_compare_runtime_family_activity_values(
            plan, result, candidate_value_id, best_value_id, &compare)) {
        return 0;
    }
    return compare > 0;
}

static int value_ssa_alloc_retry_candidate_has_heavier_runtime_family_color_support(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    size_t candidate_color,
    size_t best_value_id,
    size_t best_color) {
    int compare = 0;

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
    return compare > 0;
}

static int value_ssa_alloc_retry_candidate_has_heavier_blocker_recolor_target_support(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationSelectOption *candidate_option,
    const ValueSsaAllocationSelectOption *best_option) {
    int compare = 0;

    if (!plan || !result || !candidate_option || !best_option ||
        candidate_option->blocker_value_id == (size_t)-1 || best_option->blocker_value_id == (size_t)-1) {
        return 0;
    }
    if (!candidate_option->blocker_recolorable || !best_option->blocker_recolorable) {
        return 0;
    }
    if (!value_ssa_alloc_compare_weighted_runtime_family_color_support(
            plan,
            coalesce_analysis,
            result,
            candidate_option->blocker_value_id,
            candidate_option->blocker_recolor_color,
            best_option->blocker_value_id,
            best_option->blocker_recolor_color,
            &compare)) {
        return 0;
    }
    return compare > 0;
}

static int value_ssa_alloc_value_is_split_child(const ValueSsaAllocationPrep *prep, size_t value_id) {
    return prep && prep->split_child_values && value_id < prep->value_count && prep->split_child_values[value_id];
}

static size_t value_ssa_alloc_split_child_depth(const ValueSsaAllocationPrep *prep, size_t value_id) {
    return prep && prep->split_child_depths && value_id < prep->value_count ? prep->split_child_depths[value_id] : 0;
}

static int value_ssa_alloc_compare_retry_generic_eviction_options(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationSelectOption *candidate_option,
    const ValueSsaAllocationSelectOption *best_option,
    size_t color_budget,
    int *out_compare,
    ValueSsaError *error) {
    if (out_compare) {
        *out_compare = 0;
    }
    if (!prep || !interference || !plan || !result || !candidate_option || !best_option || !out_compare ||
        candidate_option->blocker_value_id == (size_t)-1 || best_option->blocker_value_id == (size_t)-1 ||
        candidate_option->color >= color_budget || best_option->color >= color_budget) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-126: invalid retry generic-eviction comparison");
        return 0;
    }

    if (value_ssa_alloc_eviction_candidate_is_better_for_color(interference,
            prep,
            coalesce_analysis,
            plan,
            result,
            candidate_option->blocker_value_id,
            candidate_option->color,
            best_option->blocker_value_id,
            best_option->color,
            color_budget,
            error)) {
        *out_compare = 1;
        return 1;
    }
    if (value_ssa_alloc_eviction_candidate_is_better_for_color(interference,
            prep,
            coalesce_analysis,
            plan,
            result,
            best_option->blocker_value_id,
            best_option->color,
            candidate_option->blocker_value_id,
            candidate_option->color,
            color_budget,
            error)) {
        *out_compare = -1;
        return 1;
    }

    if (candidate_option->blocker_recolorable != best_option->blocker_recolorable) {
        *out_compare = candidate_option->blocker_recolorable > best_option->blocker_recolorable ? 1 : -1;
        return 1;
    }
    if (candidate_option->blocker_recolorable && best_option->blocker_recolorable) {
        if (value_ssa_alloc_retry_candidate_has_heavier_blocker_recolor_target_support(
                plan, coalesce_analysis, result, candidate_option, best_option)) {
            *out_compare = 1;
            return 1;
        }
        if (value_ssa_alloc_retry_candidate_has_heavier_blocker_recolor_target_support(
                plan, coalesce_analysis, result, best_option, candidate_option)) {
            *out_compare = -1;
            return 1;
        }
    }

    if (candidate_option->color != best_option->color) {
        *out_compare = candidate_option->color < best_option->color ? 1 : -1;
        return 1;
    }
    if (candidate_option->blocker_value_id != best_option->blocker_value_id) {
        *out_compare = candidate_option->blocker_value_id < best_option->blocker_value_id ? 1 : -1;
        return 1;
    }

    return 1;
}

static int value_ssa_alloc_find_preferred_claim_option(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    int allow_analysis_only_class_color,
    size_t value_id,
    size_t color_budget,
    int *out_has_option,
    ValueSsaAllocationSelectOption *out_option,
    ValueSsaError *error) {
    int has_preferred_color = 0;
    size_t preferred_color = 0;
    size_t preferred_class_size = 0;
    int blocker_recolorable = 0;
    size_t blocker_recolor_color = 0;
    ValueSsaAllocationPreferredColorSource preferred_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    size_t preferred_partner_value_id = (size_t)-1;

    if (out_has_option) {
        *out_has_option = 0;
    }
    if (out_option) {
        value_ssa_alloc_select_option_init(out_option);
    }
    if (!prep || !interference || !plan || !result || !blocked_colors || !out_has_option || !out_option ||
        value_id >= result->value_count || value_id >= interference->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-134: invalid preferred claim query");
        return 0;
    }

    if (!value_ssa_alloc_result_find_preferred_color(prep,
            coalesce_analysis,
            plan,
            result,
            blocked_colors,
            allow_analysis_only_class_color,
            0,
            value_id,
            color_budget,
            &has_preferred_color,
            &preferred_color,
            &preferred_source,
            &preferred_partner_value_id,
            error)) {
        return 0;
    }
    if (has_preferred_color) {
        if (coalesce_analysis && coalesce_analysis->value_count == result->value_count &&
            preferred_source != VALUE_SSA_ALLOC_PREFERRED_COLOR_AFFINITY && value_id < coalesce_analysis->value_count) {
            preferred_class_size = value_ssa_alloc_plan_runtime_coalesce_class_size(
                plan, coalesce_analysis, result->value_count, value_id);
        }
        *out_has_option = 1;
        out_option->kind = VALUE_SSA_ALLOC_SELECT_OPTION_FREE;
        out_option->color = preferred_color;
        out_option->preferred_class_size = preferred_class_size;
        out_option->preferred_source = preferred_source;
        out_option->preferred_partner_value_id = preferred_partner_value_id;
        return 1;
    }

    if (!value_ssa_alloc_result_find_preferred_color(prep,
            coalesce_analysis,
            plan,
            result,
            blocked_colors,
            allow_analysis_only_class_color,
            1,
            value_id,
            color_budget,
            &has_preferred_color,
            &preferred_color,
            &preferred_source,
            &preferred_partner_value_id,
            error)) {
        return 0;
    }
    if (has_preferred_color && preferred_color < color_budget) {
        size_t blocker_value_id = (size_t)-1;
        size_t blocker_count = 0;
        size_t neighbor_value_id;

        for (neighbor_value_id = 0; neighbor_value_id < interference->value_count; ++neighbor_value_id) {
            if (!interference->interferes[value_id * interference->value_count + neighbor_value_id] ||
                neighbor_value_id >= result->value_count || !result->has_color[neighbor_value_id] ||
                result->spill_flags[neighbor_value_id] || result->colors[neighbor_value_id] != preferred_color) {
                continue;
            }
            ++blocker_count;
            blocker_value_id = neighbor_value_id;
            if (blocker_count > 1) {
                break;
            }
        }

        if (blocker_count == 1 && blocker_value_id != (size_t)-1 &&
            result->spill_priorities[blocker_value_id] < result->spill_priorities[value_id]) {
            if (coalesce_analysis && coalesce_analysis->value_count == result->value_count &&
                preferred_source != VALUE_SSA_ALLOC_PREFERRED_COLOR_AFFINITY && value_id < coalesce_analysis->value_count) {
                preferred_class_size = value_ssa_alloc_plan_runtime_coalesce_class_size(
                    plan, coalesce_analysis, result->value_count, value_id);
            }
            if (!value_ssa_alloc_find_recolor_color_for_blocker(
                    interference,
                    prep,
                    coalesce_analysis,
                    plan,
                    result,
                    blocker_value_id,
                    preferred_color,
                    color_budget,
                    &blocker_recolorable,
                    &blocker_recolor_color,
                    error)) {
                return 0;
            }
            *out_has_option = 1;
            out_option->kind = VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT;
            out_option->color = preferred_color;
            out_option->blocker_value_id = blocker_value_id;
            out_option->blocker_recolorable = blocker_recolorable ? 1u : 0u;
            out_option->blocker_recolor_color = blocker_recolor_color;
            out_option->preferred_class_size = preferred_class_size;
            out_option->preferred_source = preferred_source;
            out_option->preferred_partner_value_id = preferred_partner_value_id;
        }
    }

    return 1;
}

static int value_ssa_alloc_find_generic_eviction_option_from_color_facts(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t value_priority,
    const size_t *color_blocker_counts,
    const size_t *color_blocker_ids,
    size_t color_budget,
    int *out_has_option,
    ValueSsaAllocationSelectOption *out_option,
    ValueSsaError *error) {
    ValueSsaAllocationFamilyBlockerCandidate *family_blocker_candidates = NULL;
    size_t color;

    if (out_has_option) {
        *out_has_option = 0;
    }
    if (out_option) {
        value_ssa_alloc_select_option_init(out_option);
    }
    if (!prep || !interference || !plan || !result || !color_blocker_counts || !color_blocker_ids ||
        !out_has_option || !out_option) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-140: invalid generic eviction option query");
        return 0;
    }

    family_blocker_candidates = (ValueSsaAllocationFamilyBlockerCandidate *)malloc(
        (result->value_count ? result->value_count : 1) * sizeof(ValueSsaAllocationFamilyBlockerCandidate));
    if (!family_blocker_candidates) {
        free(family_blocker_candidates);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-141: out of memory preparing generic eviction option");
        return 0;
    }
    for (color = 0; color < result->value_count; ++color) {
        value_ssa_alloc_family_blocker_candidate_init(&family_blocker_candidates[color]);
    }

    if (!value_ssa_alloc_collect_family_blocker_candidates_from_color_facts(interference,
            prep,
            coalesce_analysis,
            plan,
            result,
            value_priority,
            color_blocker_counts,
            color_blocker_ids,
            color_budget,
            family_blocker_candidates,
            result->value_count,
            error)) {
        free(family_blocker_candidates);
        return 0;
    }

    for (color = 0; color < result->value_count; ++color) {
        const ValueSsaAllocationFamilyBlockerCandidate *family_blocker_candidate = &family_blocker_candidates[color];
        ValueSsaAllocationSelectOption family_generic_option;
        int compare = 0;

        if (!family_blocker_candidate->has_candidate || family_blocker_candidate->blocker_value_id == (size_t)-1) {
            continue;
        }
        value_ssa_alloc_select_option_init(&family_generic_option);
        family_generic_option.kind = VALUE_SSA_ALLOC_SELECT_OPTION_GENERIC_EVICT;
        family_generic_option.color = family_blocker_candidate->blocked_color;
        family_generic_option.blocker_value_id = family_blocker_candidate->blocker_value_id;
        family_generic_option.blocker_recolorable = family_blocker_candidate->blocker_recolorable;
        family_generic_option.blocker_recolor_color = family_blocker_candidate->blocker_recolor_color;
        if (!*out_has_option) {
            *out_has_option = 1;
            *out_option = family_generic_option;
            continue;
        }
        if (!value_ssa_alloc_compare_retry_generic_eviction_options(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                &family_generic_option,
                out_option,
                color_budget,
                &compare,
                error)) {
            free(family_blocker_candidates);
            return 0;
        }
        if (compare > 0) {
            *out_has_option = 1;
            *out_option = family_generic_option;
        }
    }

    free(family_blocker_candidates);
    return 1;
}

static int value_ssa_alloc_prepare_select_query(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    int allow_analysis_only_class_color,
    size_t value_id,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *query,
    ValueSsaError *error) {
    if (!prep || !interference || !plan || !result || !query || !query->blocked_colors ||
        !query->color_blocker_counts || !query->color_blocker_ids) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-145: invalid select-query preparation");
        return 0;
    }

    query->has_preferred_option = 0;
    value_ssa_alloc_select_option_init(&query->preferred_option);
    if (!value_ssa_alloc_collect_color_blocker_facts(interference,
            result,
            value_id,
            color_budget,
            query->blocked_colors,
            query->color_blocker_counts,
            query->color_blocker_ids,
            error)) {
        return 0;
    }

    return value_ssa_alloc_find_preferred_claim_option(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        query->blocked_colors,
        allow_analysis_only_class_color,
        value_id,
        color_budget,
        &query->has_preferred_option,
        &query->preferred_option,
        error);
}

static int value_ssa_alloc_find_select_option_from_query(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const ValueSsaAllocationSelectQuery *query,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    int *out_has_option,
    ValueSsaAllocationSelectOption *out_option,
    ValueSsaError *error) {
    int has_free_color = 0;
    size_t free_color = 0;

    if (out_has_option) {
        *out_has_option = 0;
    }
    if (out_option) {
        value_ssa_alloc_select_option_init(out_option);
    }
    if (!prep || !interference || !plan || !result || !query || !query->blocked_colors ||
        !query->color_blocker_counts || !query->color_blocker_ids || !out_has_option || !out_option) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-146: invalid select-option query");
        return 0;
    }

    if (query->has_preferred_option && query->preferred_option.kind == VALUE_SSA_ALLOC_SELECT_OPTION_FREE) {
        *out_has_option = 1;
        *out_option = query->preferred_option;
        return 1;
    }

    if (!value_ssa_alloc_choose_best_free_color(plan,
            coalesce_analysis,
            result,
            query->blocked_colors,
            value_id,
            color_budget,
            &has_free_color,
            &free_color)) {
        return 0;
    }
    if (has_free_color) {
        *out_has_option = 1;
        out_option->kind = VALUE_SSA_ALLOC_SELECT_OPTION_FREE;
        out_option->color = free_color;
        return 1;
    }

    if (query->has_preferred_option && query->preferred_option.kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT &&
        query->preferred_option.color < color_budget && query->blocked_colors[query->preferred_option.color]) {
        *out_has_option = 1;
        *out_option = query->preferred_option;
        return 1;
    }

    return value_ssa_alloc_find_generic_eviction_option_from_color_facts(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        spill_priority,
        query->color_blocker_counts,
        query->color_blocker_ids,
        color_budget,
        out_has_option,
        out_option,
        error);
}

static int value_ssa_alloc_compare_retry_recovery_candidates(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t candidate_value_id,
    const ValueSsaAllocationSelectOption *candidate_option,
    size_t best_value_id,
    const ValueSsaAllocationSelectOption *best_option,
    size_t color_budget,
    int *out_compare,
    ValueSsaError *error) {
    size_t candidate_priority;
    size_t best_priority;
    size_t candidate_cost;
    size_t best_cost;
    size_t candidate_preferred_class_size;
    size_t best_preferred_class_size;
    int candidate_preferred_rank;
    int best_preferred_rank;
    int candidate_rematerializable;
    int best_rematerializable;
    int candidate_intended;
    int best_intended;
    int eviction_compare = 0;

    if (out_compare) {
        *out_compare = 0;
    }
    if (!prep || !interference || !plan || !result || !candidate_option || !best_option || !out_compare ||
        candidate_value_id >= result->value_count || best_value_id >= result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-127: invalid retry recovery comparison");
        return 0;
    }

    candidate_priority = result->spill_priorities[candidate_value_id];
    best_priority = result->spill_priorities[best_value_id];
    if (candidate_priority != best_priority) {
        *out_compare = candidate_priority > best_priority ? 1 : -1;
        return 1;
    }

    candidate_preferred_rank = value_ssa_alloc_retry_preferred_source_rank(candidate_option->preferred_source);
    best_preferred_rank = value_ssa_alloc_retry_preferred_source_rank(best_option->preferred_source);
    if (candidate_preferred_rank != best_preferred_rank) {
        *out_compare = candidate_preferred_rank > best_preferred_rank ? 1 : -1;
        return 1;
    }
    candidate_preferred_class_size = candidate_option->preferred_class_size;
    best_preferred_class_size = best_option->preferred_class_size;
    if (candidate_preferred_class_size != best_preferred_class_size) {
        *out_compare = candidate_preferred_class_size > best_preferred_class_size ? 1 : -1;
        return 1;
    }

    candidate_intended = value_ssa_alloc_retry_candidate_is_spill_intended(result, candidate_value_id);
    best_intended = value_ssa_alloc_retry_candidate_is_spill_intended(result, best_value_id);
    if (candidate_intended != best_intended) {
        *out_compare = candidate_intended > best_intended ? 1 : -1;
        return 1;
    }

    if (value_ssa_alloc_select_option_requires_eviction(candidate_option) !=
        value_ssa_alloc_select_option_requires_eviction(best_option)) {
        *out_compare = !value_ssa_alloc_select_option_requires_eviction(candidate_option) ? 1 : -1;
        return 1;
    }
    if (candidate_option->kind != best_option->kind &&
        (candidate_option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT ||
            best_option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT)) {
        *out_compare = candidate_option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_PREFERRED_EVICT ? 1 : -1;
        return 1;
    }

    if (value_ssa_alloc_select_option_requires_eviction(candidate_option) &&
        value_ssa_alloc_select_option_requires_eviction(best_option)) {
        if (!value_ssa_alloc_compare_retry_generic_eviction_options(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                candidate_option,
                best_option,
                color_budget,
                &eviction_compare,
                error)) {
            return 0;
        }
        if (eviction_compare != 0) {
            *out_compare = eviction_compare;
            return 1;
        }
    }

    if (value_ssa_alloc_retry_candidate_has_heavier_runtime_family_color_support(plan,
            coalesce_analysis,
            result,
            candidate_value_id,
            candidate_option->color,
            best_value_id,
            best_option->color)) {
        *out_compare = 1;
        return 1;
    }
    if (value_ssa_alloc_retry_candidate_has_heavier_runtime_family_color_support(plan,
            coalesce_analysis,
            result,
            best_value_id,
            best_option->color,
            candidate_value_id,
            candidate_option->color)) {
        *out_compare = -1;
        return 1;
    }

    if (value_ssa_alloc_retry_candidate_has_heavier_runtime_family_activity(
            plan, result, candidate_value_id, best_value_id)) {
        *out_compare = 1;
        return 1;
    }
    if (value_ssa_alloc_retry_candidate_has_heavier_runtime_family_activity(
            plan, result, best_value_id, candidate_value_id)) {
        *out_compare = -1;
        return 1;
    }

    if (value_ssa_alloc_retry_candidate_has_heavier_family_pressure(plan, candidate_value_id, best_value_id)) {
        *out_compare = 1;
        return 1;
    }
    if (value_ssa_alloc_retry_candidate_has_heavier_family_pressure(plan, best_value_id, candidate_value_id)) {
        *out_compare = -1;
        return 1;
    }

    if (value_ssa_alloc_select_option_requires_eviction(candidate_option) &&
        value_ssa_alloc_select_option_requires_eviction(best_option) &&
        candidate_option->blocker_value_id != (size_t)-1 && best_option->blocker_value_id != (size_t)-1) {
        if (value_ssa_alloc_eviction_candidate_is_better(
                prep, plan, result, candidate_option->blocker_value_id, best_option->blocker_value_id)) {
            *out_compare = 1;
            return 1;
        }
        if (value_ssa_alloc_eviction_candidate_is_better(
                prep, plan, result, best_option->blocker_value_id, candidate_option->blocker_value_id)) {
            *out_compare = -1;
            return 1;
        }
    }

    candidate_cost = value_ssa_alloc_value_spill_cost_for_select(prep, candidate_value_id);
    best_cost = value_ssa_alloc_value_spill_cost_for_select(prep, best_value_id);
    if (candidate_cost != best_cost) {
        *out_compare = candidate_cost > best_cost ? 1 : -1;
        return 1;
    }

    if (value_ssa_alloc_value_is_split_child(prep, candidate_value_id) !=
        value_ssa_alloc_value_is_split_child(prep, best_value_id)) {
        *out_compare = value_ssa_alloc_value_is_split_child(prep, candidate_value_id) ? 1 : -1;
        return 1;
    }
    if (value_ssa_alloc_split_child_depth(prep, candidate_value_id) !=
        value_ssa_alloc_split_child_depth(prep, best_value_id)) {
        *out_compare = value_ssa_alloc_split_child_depth(prep, candidate_value_id) >
                value_ssa_alloc_split_child_depth(prep, best_value_id)
            ? 1
            : -1;
        return 1;
    }

    candidate_rematerializable = prep->rematerializable && candidate_value_id < prep->value_count &&
        prep->rematerializable[candidate_value_id];
    best_rematerializable =
        prep->rematerializable && best_value_id < prep->value_count && prep->rematerializable[best_value_id];
    if (candidate_rematerializable != best_rematerializable) {
        *out_compare = candidate_rematerializable < best_rematerializable ? 1 : -1;
        return 1;
    }

    if (value_ssa_alloc_select_option_requires_eviction(candidate_option) &&
        value_ssa_alloc_select_option_requires_eviction(best_option) &&
        candidate_option->color != best_option->color) {
        *out_compare = candidate_option->color < best_option->color ? 1 : -1;
        return 1;
    }

    if (candidate_value_id != best_value_id) {
        *out_compare = candidate_value_id < best_value_id ? 1 : -1;
    }
    return 1;
}

static int value_ssa_alloc_update_retry_family_candidate(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t family_root_value_id,
    size_t value_id,
    const ValueSsaAllocationSelectOption *option,
    size_t color_budget,
    ValueSsaAllocationRetryFamilyCandidate *family_candidate,
    ValueSsaError *error) {
    int compare = 0;
    size_t split_family_root_value_id;

    if (!prep || !interference || !plan || !result || !option || !family_candidate) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-128: invalid retry family-candidate update");
        return 0;
    }

    split_family_root_value_id = value_ssa_alloc_split_family_root_value_id(prep, value_id);
    family_candidate->recoverable_member_count += 1;
    if (value_ssa_alloc_retry_candidate_is_spill_intended(result, value_id)) {
        family_candidate->recoverable_intended_count += 1;
    }
    if (family_candidate->family_root_value_id == (size_t)-1) {
        family_candidate->family_root_value_id = family_root_value_id;
    }
    if (family_candidate->split_family_root_value_id == (size_t)-1) {
        family_candidate->split_family_root_value_id = split_family_root_value_id;
    }
    if (split_family_root_value_id == family_candidate->split_family_root_value_id) {
        family_candidate->split_family_member_count += 1;
        if (value_ssa_alloc_retry_candidate_is_spill_intended(result, value_id)) {
            family_candidate->split_family_intended_count += 1;
        }
    }

    if (!family_candidate->has_candidate) {
        family_candidate->has_candidate = 1u;
        family_candidate->family_root_value_id = family_root_value_id;
        family_candidate->value_id = value_id;
        family_candidate->option = *option;
        return 1;
    }

    if (!value_ssa_alloc_compare_retry_recovery_candidates(prep,
            coalesce_analysis,
            interference,
            plan,
            result,
            value_id,
            option,
            family_candidate->value_id,
            &family_candidate->option,
            color_budget,
            &compare,
            error)) {
        return 0;
    }
    if (compare > 0) {
        family_candidate->has_candidate = 1u;
        family_candidate->family_root_value_id = family_root_value_id;
        family_candidate->value_id = value_id;
        family_candidate->option = *option;
    }
    return 1;
}

static int value_ssa_alloc_collect_retry_family_candidate_for_root(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t family_root_value_id,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *retry_query,
    int *out_found,
    ValueSsaAllocationRetryFamilyCandidate *out_candidate,
    ValueSsaError *error) {
    ValueSsaAllocationRetryFamilyCandidate *split_candidates = NULL;
    unsigned char *split_candidate_used = NULL;
    size_t value_id;
    size_t family_recoverable_member_count = 0;
    size_t family_recoverable_intended_count = 0;
    int ok = 0;

    if (out_found) {
        *out_found = 0;
    }
    if (out_candidate) {
        value_ssa_alloc_retry_family_candidate_init(out_candidate);
    }
    if (!prep || !interference || !plan || !result || !retry_query || !out_found || !out_candidate ||
        family_root_value_id >= result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-146: invalid retry family-root collection");
        return 0;
    }

    split_candidates = (ValueSsaAllocationRetryFamilyCandidate *)calloc(
        result->value_count ? result->value_count : 1,
        sizeof(ValueSsaAllocationRetryFamilyCandidate));
    split_candidate_used =
        (unsigned char *)calloc(result->value_count ? result->value_count : 1, sizeof(unsigned char));
    if (!split_candidates || !split_candidate_used) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-184: out of memory preparing retry split-family collection");
        goto cleanup;
    }
    for (value_id = 0; value_id < result->value_count; ++value_id) {
        value_ssa_alloc_retry_family_candidate_init(&split_candidates[value_id]);
    }

    for (value_id = 0; value_id < result->value_count; ++value_id) {
        int has_retry_option = 0;
        ValueSsaAllocationSelectOption retry_option;
        size_t split_family_root_value_id;
        size_t value_family_root_value_id;

        if (!result->spill_flags[value_id]) {
            continue;
        }
        value_family_root_value_id = value_ssa_alloc_plan_family_root_for_value(plan, value_id);
        if (value_family_root_value_id >= result->value_count) {
            value_family_root_value_id = value_id;
        }
        if (value_family_root_value_id != family_root_value_id) {
            continue;
        }

        value_ssa_alloc_select_option_init(&retry_option);
        if (!value_ssa_alloc_prepare_select_query(
                prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                1,
                value_id,
                color_budget,
                retry_query,
                error) ||
            !value_ssa_alloc_find_select_option_from_query(
                prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                retry_query,
                value_id,
                result->spill_priorities[value_id],
                color_budget,
                &has_retry_option,
                &retry_option,
                error)) {
            return 0;
        }
        if (!has_retry_option) {
            continue;
        }

        split_family_root_value_id = value_ssa_alloc_split_family_root_value_id(prep, value_id);
        if (split_family_root_value_id >= result->value_count) {
            split_family_root_value_id = value_id;
        }
        ++family_recoverable_member_count;
        if (value_ssa_alloc_retry_candidate_is_spill_intended(result, value_id)) {
            ++family_recoverable_intended_count;
        }

        if (!value_ssa_alloc_update_retry_family_candidate(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                family_root_value_id,
                value_id,
                &retry_option,
                color_budget,
                &split_candidates[split_family_root_value_id],
                error)) {
            goto cleanup;
        }
        split_candidate_used[split_family_root_value_id] = 1u;
    }

    for (value_id = 0; value_id < result->value_count; ++value_id) {
        int compare = 0;
        ValueSsaAllocationRetryFamilyCandidate *split_candidate = &split_candidates[value_id];

        if (!split_candidate_used[value_id] || !split_candidate->has_candidate ||
            split_candidate->value_id == (size_t)-1) {
            continue;
        }
        if (!out_candidate->has_candidate) {
            *out_candidate = *split_candidate;
            continue;
        }
        if (!value_ssa_alloc_compare_retry_family_candidates(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                split_candidate,
                out_candidate,
                color_budget,
                &compare,
                error)) {
            goto cleanup;
        }
        if (compare > 0) {
            *out_candidate = *split_candidate;
        }
    }

    if (out_candidate->has_candidate && out_candidate->value_id != (size_t)-1) {
        out_candidate->family_root_value_id = family_root_value_id;
        out_candidate->recoverable_member_count = family_recoverable_member_count;
        out_candidate->recoverable_intended_count = family_recoverable_intended_count;
        *out_found = 1;
    }

    ok = 1;

cleanup:
    free(split_candidate_used);
    free(split_candidates);
    return ok;
}

static int value_ssa_alloc_result_find_runtime_family_color(const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    int allow_analysis_only_class_color,
    int allow_blocked_colors,
    size_t value_id,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color,
    ValueSsaAllocationPreferredColorSource *out_source,
    size_t *out_partner_value_id,
    ValueSsaError *error) {
    size_t root_value_id;
    size_t color;
    ValueSsaAllocatorRuntimeFamilyColorSupport best_support;
    int found = 0;

    if (out_has_color) {
        *out_has_color = 0;
    }
    if (out_color) {
        *out_color = 0;
    }
    if (out_source) {
        *out_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    }
    if (out_partner_value_id) {
        *out_partner_value_id = (size_t)-1;
    }
    if (!plan || !result || !out_has_color || !out_color || value_id >= result->value_count ||
        plan->value_count != result->value_count || color_budget == 0) {
        return 1;
    }

    root_value_id =
        value_ssa_alloc_plan_runtime_coalesce_root(plan, coalesce_analysis, result->value_count, value_id);
    if (root_value_id >= result->value_count) {
        return 1;
    }
    if (!allow_analysis_only_class_color && coalesce_analysis &&
        coalesce_analysis->value_count == result->value_count && coalesce_analysis->item_count == 0 &&
        !coalesce_analysis->items && (!plan->final_runtime_coalesce_roots || !plan->final_runtime_coalesce_class_sizes)) {
        const ValueSsaAllocatorPlanItem *plan_item = NULL;
        int has_explicit_plan_family = 0;

        if (value_ssa_allocator_plan_find_value(plan, value_id, NULL, &plan_item) && plan_item &&
            plan_item->coalesce_class_size > 1 && plan_item->coalesce_root_value_id < plan->value_count) {
            has_explicit_plan_family = 1;
        }
        if (!has_explicit_plan_family) {
            return 1;
        }
    }

    memset(&best_support, 0, sizeof(best_support));
    best_support.best_direct_partner_value_id = (size_t)-1;
    for (color = 0; color < color_budget; ++color) {
        ValueSsaAllocatorRuntimeFamilyColorSupport candidate_support;

        if (!allow_blocked_colors && blocked_colors && blocked_colors[color]) {
            continue;
        }
        if (!value_ssa_alloc_collect_runtime_family_color_support(
                plan, coalesce_analysis, result, value_id, color, 1, 0, &candidate_support)) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-124: failed querying runtime family color state");
            return 0;
        }
        if (candidate_support.root_value_id != root_value_id || candidate_support.colored_member_count == 0) {
            continue;
        }

        if (!found || candidate_support.colored_member_count > best_support.colored_member_count ||
            (candidate_support.colored_member_count == best_support.colored_member_count &&
                candidate_support.direct_coalesce_weight_sum > best_support.direct_coalesce_weight_sum) ||
            (candidate_support.colored_member_count == best_support.colored_member_count &&
                candidate_support.direct_coalesce_weight_sum == best_support.direct_coalesce_weight_sum &&
                candidate_support.best_direct_partner_weight > best_support.best_direct_partner_weight) ||
            (candidate_support.colored_member_count == best_support.colored_member_count &&
                candidate_support.direct_coalesce_weight_sum == best_support.direct_coalesce_weight_sum &&
                candidate_support.best_direct_partner_weight == best_support.best_direct_partner_weight &&
                color < best_support.color)) {
            found = 1;
            best_support = candidate_support;
        }
    }

    if (!found) {
        return 1;
    }

    *out_has_color = 1;
    *out_color = best_support.color;
    if (out_source) {
        *out_source = best_support.best_direct_partner_weight > 0 ? VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT
                                                                  : VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS;
    }
    if (out_partner_value_id) {
        *out_partner_value_id = best_support.best_direct_partner_weight > 0
            ? best_support.best_direct_partner_value_id
            : best_support.best_member_value_id;
    }
    return 1;
}

static int value_ssa_alloc_run_select_phase(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationSelectPhaseStepResult *out_result,
    ValueSsaError *error) {
    ValueSsaAllocationSelectQuery select_query;
    size_t item_index;
    ValueSsaAllocationRetryField retry_field;

    if (out_result) {
        value_ssa_alloc_select_phase_step_result_init(out_result);
    }
    if (!function || !prep || !interference || !plan || !result || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-188: invalid select-phase contract");
        return 0;
    }
    if (prep->value_count != result->value_count || interference->value_count != result->value_count ||
        plan->value_count != result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-074: invalid select-color contract");
        return 0;
    }

    value_ssa_alloc_select_query_init(&select_query);
    value_ssa_alloc_retry_field_init(&retry_field);
    if (!value_ssa_alloc_select_query_allocate(&select_query, color_budget, error)) {
        return 0;
    }
    if (!value_ssa_alloc_retry_field_prepare(&retry_field, color_budget, error)) {
        value_ssa_alloc_select_query_free(&select_query);
        return 0;
    }

    for (item_index = plan->item_count; item_index > 0; --item_index) {
        ValueSsaAllocationMainSelectStepResult step_result;

        value_ssa_alloc_main_select_step_result_init(&step_result);
        if (!value_ssa_alloc_run_main_select_step(prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                &plan->items[item_index - 1],
                color_budget,
                &select_query,
                &step_result,
                error)) {
            value_ssa_alloc_retry_field_free(&retry_field);
            value_ssa_alloc_select_query_free(&select_query);
            return 0;
        }
        if (step_result.status != VALUE_SSA_ALLOC_MAIN_SELECT_STEP_COMPLETED) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-187: main-select step ended in unknown state");
            value_ssa_alloc_retry_field_free(&retry_field);
            value_ssa_alloc_select_query_free(&select_query);
            return 0;
        }
        ++out_result->completed_main_select_steps;
    }

    for (;;) {
        ValueSsaAllocationRetryPhaseStepResult step_result;

        value_ssa_alloc_retry_phase_step_result_init(&step_result);
        if (!value_ssa_alloc_retry_run_phase_step(function,
                prep,
                coalesce_analysis,
                interference,
                plan,
                result,
                &retry_field,
                &step_result,
                error)) {
            value_ssa_alloc_retry_field_free(&retry_field);
            value_ssa_alloc_select_query_free(&select_query);
            return 0;
        }
        switch (step_result.status) {
        case VALUE_SSA_ALLOC_RETRY_PHASE_STEP_EXHAUSTED:
            value_ssa_alloc_retry_field_free(&retry_field);
            value_ssa_alloc_select_query_free(&select_query);
            out_result->status = VALUE_SSA_ALLOC_SELECT_PHASE_STEP_COMPLETED;
            return 1;
        case VALUE_SSA_ALLOC_RETRY_PHASE_STEP_COMPLETED:
            ++out_result->completed_retry_phase_steps;
            break;
        case VALUE_SSA_ALLOC_RETRY_PHASE_STEP_NONE:
        case VALUE_SSA_ALLOC_RETRY_PHASE_STEP_NO_PROGRESS:
        default:
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-185: retry phase-step ended in unknown state");
            value_ssa_alloc_retry_field_free(&retry_field);
            value_ssa_alloc_select_query_free(&select_query);
            return 0;
        }
    }
}

static int value_ssa_alloc_run_execute_step(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationExecuteStepResult *out_result,
    ValueSsaError *error) {
    if (out_result) {
        value_ssa_alloc_execute_step_result_init(out_result);
    }
    if (!function || !prep || !interference || !plan || !result || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-189: invalid execute-step contract");
        return 0;
    }

    if (!value_ssa_alloc_run_select_phase(
            function, prep, coalesce_analysis, interference, plan, color_budget, result, &out_result->select_phase, error)) {
        return 0;
    }
    if (out_result->select_phase.status != VALUE_SSA_ALLOC_SELECT_PHASE_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-190: select-phase ended in unknown state");
        return 0;
    }
    if (!value_ssa_alloc_assign_spill_slots(prep, interference, result, error)) {
        return 0;
    }

    out_result->spill_slots_assigned = 1u;
    out_result->status = VALUE_SSA_ALLOC_EXECUTE_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_result_find_preferred_color(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    int allow_analysis_only_class_color,
    int allow_blocked_colors,
    size_t value_id,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color,
    ValueSsaAllocationPreferredColorSource *out_source,
    size_t *out_partner_value_id,
    ValueSsaError *error) {
    size_t neighbor_count;
    size_t *neighbor_value_ids = NULL;
    size_t *weights = NULL;
    size_t neighbor_index;
    size_t best_color = 0;
    size_t best_weight = 0;
    int found = 0;

    if (!prep || !result || (!blocked_colors && !allow_blocked_colors) || !out_has_color || !out_color ||
        value_id >= prep->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-004: invalid preferred-color query");
        return 0;
    }

    *out_has_color = 0;
    *out_color = 0;
    if (out_source) {
        *out_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE;
    }
    if (out_partner_value_id) {
        *out_partner_value_id = (size_t)-1;
    }
    if (color_budget == 0) {
        return 1;
    }

    if (plan && plan->value_count == result->value_count) {
        if (!value_ssa_alloc_result_find_runtime_family_color(coalesce_analysis,
                plan,
                result,
                blocked_colors,
                allow_analysis_only_class_color,
                allow_blocked_colors,
                value_id,
                color_budget,
                &found,
                &best_color,
                out_source,
                out_partner_value_id,
                error)) {
            return 0;
        }
        if (found) {
            *out_has_color = 1;
            *out_color = best_color;
            return 1;
        }
    }

    neighbor_value_ids = (size_t *)malloc(color_budget * sizeof(size_t));
    weights = (size_t *)malloc(color_budget * sizeof(size_t));
    if (!neighbor_value_ids || !weights) {
        free(neighbor_value_ids);
        free(weights);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-005: out of memory querying affinity neighbors");
        return 0;
    }

    if (!value_ssa_allocation_prep_get_top_affinity_neighbors(
            prep, value_id, color_budget, neighbor_value_ids, weights, &neighbor_count)) {
        free(neighbor_value_ids);
        free(weights);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-006: failed affinity-neighbor query");
        return 0;
    }

    for (neighbor_index = 0; neighbor_index < neighbor_count; ++neighbor_index) {
        size_t neighbor_value_id = neighbor_value_ids[neighbor_index];

        if (neighbor_value_id >= result->value_count || !result->has_color[neighbor_value_id]) {
            continue;
        }
        if (result->colors[neighbor_value_id] >= color_budget ||
            (!allow_blocked_colors && blocked_colors[result->colors[neighbor_value_id]])) {
            continue;
        }

        if (!found || weights[neighbor_index] > best_weight ||
            (weights[neighbor_index] == best_weight && result->colors[neighbor_value_id] < best_color)) {
            found = 1;
            best_color = result->colors[neighbor_value_id];
            best_weight = weights[neighbor_index];
            if (out_source) {
                *out_source = VALUE_SSA_ALLOC_PREFERRED_COLOR_AFFINITY;
            }
            if (out_partner_value_id) {
                *out_partner_value_id = neighbor_value_id;
            }
        }
    }

    free(neighbor_value_ids);
    free(weights);
    if (found) {
        *out_has_color = 1;
        *out_color = best_color;
    }
    return 1;
}

static int value_ssa_alloc_choose_best_free_color(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    const unsigned char *blocked_colors,
    size_t value_id,
    size_t color_budget,
    int *out_has_color,
    size_t *out_color) {
    size_t color;
    size_t best_color = 0;
    size_t best_support_count = 0;
    int found = 0;

    if (out_has_color) {
        *out_has_color = 0;
    }
    if (out_color) {
        *out_color = 0;
    }
    if (!plan || !result || !blocked_colors || !out_has_color || !out_color || color_budget == 0 ||
        value_id >= result->value_count || plan->value_count != result->value_count) {
        return 1;
    }

    if (!value_ssa_alloc_value_has_explicit_family_color_authority(
            plan, coalesce_analysis, result->value_count, value_id)) {
        for (color = 0; color < color_budget; ++color) {
            if (blocked_colors[color]) {
                continue;
            }
            *out_has_color = 1;
            *out_color = color;
            return 1;
        }
        return 1;
    }

    for (color = 0; color < color_budget; ++color) {
        size_t support_count = 0;

        if (blocked_colors[color]) {
            continue;
        }
        if (!value_ssa_alloc_count_runtime_family_members_on_color(
                plan, coalesce_analysis, result, value_id, color, &support_count)) {
            support_count = 0;
        }
        if (!found || support_count > best_support_count ||
            (support_count == best_support_count && color < best_color)) {
            found = 1;
            best_color = color;
            best_support_count = support_count;
        }
    }

    if (!found) {
        return 1;
    }
    *out_has_color = 1;
    *out_color = best_color;
    return 1;
}

static int value_ssa_alloc_resolve_main_select_claim(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    const ValueSsaAllocationSelectQuery *query,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error) {
    int has_option = 0;
    ValueSsaAllocationSelectOption option;

    if (!prep || !interference || !plan || !result || !query || !out_claim) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-135: invalid main-select claim query");
        return 0;
    }

    value_ssa_alloc_select_claim_init(out_claim);
    value_ssa_alloc_select_option_init(&option);
    if (!value_ssa_alloc_find_select_option_from_query(prep,
            coalesce_analysis,
            interference,
            plan,
            result,
            query,
            value_id,
            spill_priority,
            color_budget,
            &has_option,
            &option,
            error)) {
        return 0;
    }
    if (!has_option) {
        return 1;
    }

    return value_ssa_alloc_materialize_select_claim_from_option(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        value_id,
        spill_priority,
        color_budget,
        &option,
        VALUE_SSA_ALLOC_EVICTION_METADATA_GENERIC,
        0,
        out_claim,
        error);
}

static int value_ssa_alloc_run_main_select_step(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    const ValueSsaAllocatorPlanItem *plan_item,
    size_t color_budget,
    ValueSsaAllocationSelectQuery *select_query,
    ValueSsaAllocationMainSelectStepResult *out_result,
    ValueSsaError *error) {
    if (out_result) {
        value_ssa_alloc_main_select_step_result_init(out_result);
    }
    if (!prep || !interference || !plan || !result || !plan_item || !select_query || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-186: invalid main-select step contract");
        return 0;
    }
    if (plan_item->value_id >= result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-075: select-color value out of range");
        return 0;
    }

    out_result->value_id = plan_item->value_id;
    out_result->spill_priority = plan_item->priority;
    out_result->spill_intended = plan_item->removal_kind == VALUE_SSA_ALLOCATOR_REMOVE_SPILL ? 1u : 0u;
    result->spill_priorities[out_result->value_id] = out_result->spill_priority;
    value_ssa_alloc_reset_value_choice_metadata(result, out_result->value_id);
    if (out_result->spill_intended) {
        result->spill_intended_flags[out_result->value_id] = 1;
    }

    if (!value_ssa_alloc_prepare_select_query(prep,
            coalesce_analysis,
            interference,
            plan,
            result,
            0,
            out_result->value_id,
            color_budget,
            select_query,
            error)) {
        return 0;
    }
    if (!value_ssa_alloc_resolve_main_select_claim(prep,
            coalesce_analysis,
            interference,
            plan,
            result,
            select_query,
            out_result->value_id,
            out_result->spill_priority,
            color_budget,
            &out_result->claim,
            error)) {
        return 0;
    }

    if (out_result->claim.has_color) {
        value_ssa_alloc_apply_select_claim(result, out_result->value_id, &out_result->claim);
        out_result->colored = 1u;
    } else {
        result->spill_flags[out_result->value_id] = 1;
        result->spill_confirmed_flags[out_result->value_id] = 1;
        out_result->spilled = 1u;
    }

    out_result->status = VALUE_SSA_ALLOC_MAIN_SELECT_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_resolve_retry_recovery_claim(const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    size_t value_id,
    size_t spill_priority,
    size_t color_budget,
    const ValueSsaAllocationSelectOption *retry_option,
    ValueSsaAllocationSelectClaim *out_claim,
    ValueSsaError *error) {
    if (!prep || !interference || !plan || !result || !retry_option || !out_claim) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-136: invalid retry claim query");
        return 0;
    }

    value_ssa_alloc_select_claim_init(out_claim);
    if (retry_option->kind == VALUE_SSA_ALLOC_SELECT_OPTION_NONE) {
        return 1;
    }

    return value_ssa_alloc_materialize_select_claim_from_option(prep,
        coalesce_analysis,
        interference,
        plan,
        result,
        value_id,
        spill_priority,
        color_budget,
        retry_option,
        VALUE_SSA_ALLOC_EVICTION_METADATA_RETRY,
        1,
        out_claim,
        error);
}

#endif
