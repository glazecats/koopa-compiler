#ifndef VALUE_SSA_ALLOC_H
#define VALUE_SSA_ALLOC_H

#include "value_ssa.h"

typedef enum {
    VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE = 0,
    VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT,
    VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS,
    VALUE_SSA_ALLOC_PREFERRED_COLOR_AFFINITY,
} ValueSsaAllocationPreferredColorSource;

typedef struct ValueSsaAllocatorRetryFamilyAgendaItem ValueSsaAllocatorRetryFamilyAgendaItem;

typedef struct {
    char *function_name;
    size_t value_count;
    size_t color_budget;
    size_t spill_slot_count;
    size_t *colors;
    unsigned char *has_color;
    unsigned char *spill_flags;
    unsigned char *spill_intended_flags;
    unsigned char *spill_confirmed_flags;
    unsigned char *spill_recovered_flags;
    size_t *spill_recovery_orders;
    size_t *spill_recovery_phase_ids;
    size_t *spill_recovery_phase_member_orders;
    size_t *spill_recovery_phase_member_counts;
    ValueSsaAllocatorRetryFamilyAgendaItem *spill_recovery_phase_entries;
    size_t next_spill_recovery_order;
    size_t next_spill_recovery_phase_id;
    size_t *spill_priorities;
    size_t *spill_slots;
    unsigned char *used_preferred_color_flags;
    ValueSsaAllocationPreferredColorSource *preferred_color_sources;
    size_t *preferred_color_partner_value_ids;
    unsigned char *used_generic_eviction_flags;
    size_t *generic_eviction_blocker_value_ids;
    unsigned char *generic_eviction_blocker_recolored_flags;
    size_t *generic_eviction_blocker_new_colors;
    unsigned char *used_targeted_eviction_flags;
    size_t *targeted_eviction_blocker_value_ids;
    unsigned char *targeted_eviction_blocker_recolored_flags;
    size_t *targeted_eviction_blocker_new_colors;
    unsigned char *used_retry_eviction_flags;
    size_t *retry_eviction_blocker_value_ids;
    unsigned char *retry_eviction_blocker_recolored_flags;
    size_t *retry_eviction_blocker_new_colors;
} ValueSsaAllocationResult;

typedef struct {
    size_t function_count;
    ValueSsaAllocationResult *function_results;
} ValueSsaProgramAllocationResult;

typedef enum {
    VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_UNALLOCATED = 0,
    VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR,
    VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL,
} ValueSsaAllocatedValuePlacementKind;

typedef struct {
    size_t value_id;
    ValueSsaAllocatedValuePlacementKind kind;
    size_t color;
    size_t spill_slot;
    unsigned char spill_intended;
    unsigned char spill_confirmed;
    unsigned char spill_recovered;
} ValueSsaAllocatedValuePlacement;

typedef struct {
    size_t color;
    size_t value_count;
    size_t *value_ids;
} ValueSsaAllocationColorGroup;

typedef struct {
    size_t spill_slot;
    size_t value_count;
    size_t *value_ids;
} ValueSsaAllocationSpillSlotGroup;

typedef struct {
    char *function_name;
    size_t value_count;
    size_t color_budget;
    size_t spill_slot_count;
    size_t colored_count;
    size_t spilled_count;
    size_t optimistic_colored_count;
    size_t confirmed_spilled_count;
    size_t recovered_colored_count;
    size_t used_color_count;
    size_t color_group_count;
    size_t spill_slot_group_count;
    ValueSsaAllocatedValuePlacement *placements;
    size_t *colored_value_ids;
    size_t *spilled_value_ids;
    size_t *optimistic_colored_value_ids;
    size_t *confirmed_spilled_value_ids;
    size_t *recovered_colored_value_ids;
    size_t *used_colors;
    ValueSsaAllocationColorGroup *color_groups;
    ValueSsaAllocationSpillSlotGroup *spill_slot_groups;
} ValueSsaFunctionAllocationLayout;

typedef struct {
    size_t function_count;
    char **function_names;
    size_t total_value_count;
    size_t total_colored_count;
    size_t total_spilled_count;
    size_t total_optimistic_colored_count;
    size_t total_confirmed_spilled_count;
    size_t total_recovered_colored_count;
    size_t functions_with_colors_count;
    size_t functions_with_spills_count;
    size_t functions_with_confirmed_spills_count;
    size_t functions_with_optimistic_colors_count;
    size_t functions_with_recovered_colors_count;
    size_t max_color_budget;
    size_t max_spill_slot_count;
    ValueSsaFunctionAllocationLayout *function_layouts;
    size_t *function_indices_with_colors;
    size_t *function_indices_with_spills;
    size_t *function_indices_with_confirmed_spills;
    size_t *function_indices_with_optimistic_colors;
    size_t *function_indices_with_recovered_colors;
} ValueSsaProgramAllocationLayout;

typedef struct {
    size_t allocation_rounds;
    size_t rewrite_rounds;
} ValueSsaAllocateRewriteStats;

typedef struct {
    ValueSsaAllocateRewriteStats stats;
    ValueSsaProgramAllocationLayout layout;
} ValueSsaAllocateRewriteLayoutReport;

typedef struct {
    size_t colored_count;
    size_t spilled_count;
    size_t spill_intended_count;
    size_t spill_confirmed_count;
    size_t spill_recovered_count;
    size_t retry_eviction_recovered_count;
    size_t optimistic_direct_count;
    size_t optimistic_recovered_count;
    size_t optimistic_colored_count;
} ValueSsaAllocationSelectStats;

typedef struct {
    size_t lhs_value_id;
    size_t rhs_value_id;
    size_t weight;
    size_t merged_neighbor_count;
    unsigned char interferes;
    unsigned char can_coalesce;
} ValueSsaAllocatorCoalesceItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    ValueSsaAllocatorCoalesceItem *items;
    size_t *value_roots;
    size_t *class_sizes;
    size_t pair_index_capacity;
    size_t *pair_index_lhs;
    size_t *pair_index_rhs;
    size_t *pair_index_item_indices;
} ValueSsaAllocatorCoalesceAnalysis;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_FAMILY_INTERNAL = 0,
    VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED,
    VALUE_SSA_ALLOC_MOVE_FAMILY_FROZEN,
} ValueSsaAllocatorMoveFamilyState;

typedef struct {
    size_t root_value_id;
    size_t class_size;
    size_t external_neighbor_family_count;
    size_t coalesce_ready_neighbor_family_count;
    size_t coalesce_ready_affinity_weight_sum;
    size_t best_coalesce_ready_partner_root_value_id;
    size_t best_coalesce_ready_partner_affinity_weight;
    unsigned char best_coalesce_ready_partner_is_mutual;
    size_t external_affinity_weight_sum;
    size_t simplify_removed_count;
    size_t spill_removed_count;
    unsigned char initial_move_related;
    unsigned char was_frozen;
    ValueSsaAllocatorMoveFamilyState state;
} ValueSsaAllocatorMoveFamilyItem;

typedef struct {
    size_t value_count;
    size_t family_count;
    ValueSsaAllocatorMoveFamilyItem *items;
    size_t *value_family_indices;
    size_t *root_family_indices;
} ValueSsaAllocatorMoveFamilyAnalysis;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY = 0,
    VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING,
    VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
    VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
} ValueSsaAllocatorMoveWorkClass;

typedef struct {
    size_t root_value_id;
    ValueSsaAllocatorMoveWorkClass work_class;
    size_t priority;
    size_t class_size;
    size_t external_neighbor_family_count;
    size_t coalesce_ready_neighbor_family_count;
    size_t coalesce_ready_affinity_weight_sum;
    size_t best_coalesce_ready_partner_root_value_id;
    size_t best_coalesce_ready_partner_affinity_weight;
    unsigned char best_coalesce_ready_partner_is_mutual;
    size_t external_affinity_weight_sum;
    size_t simplify_removed_count;
    size_t spill_removed_count;
} ValueSsaAllocatorMoveWorkItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    ValueSsaAllocatorMoveWorkItem *items;
    size_t *value_work_indices;
    size_t *root_work_indices;
} ValueSsaAllocatorMoveWorklist;

typedef struct {
    size_t lhs_root_value_id;
    size_t rhs_root_value_id;
    unsigned char lhs_prefers_rhs;
    unsigned char rhs_prefers_lhs;
    unsigned char mutual_best;
    size_t lhs_preference_weight;
    size_t rhs_preference_weight;
    size_t total_preference_weight;
    ValueSsaAllocatorMoveWorkClass lhs_work_class;
    ValueSsaAllocatorMoveWorkClass rhs_work_class;
    size_t lhs_move_work_priority;
    size_t rhs_move_work_priority;
    size_t priority;
} ValueSsaAllocatorCoalesceOpportunityItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    ValueSsaAllocatorCoalesceOpportunityItem *items;
} ValueSsaAllocatorCoalesceOpportunityAgenda;

typedef enum {
    VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE = 0,
    VALUE_SSA_ALLOCATOR_RETRY_ENTRY_PREFERRED_EVICT,
    VALUE_SSA_ALLOCATOR_RETRY_ENTRY_GENERIC_EVICT,
} ValueSsaAllocatorRetryFamilyEntryKind;

struct ValueSsaAllocatorRetryFamilyAgendaItem {
    size_t root_value_id;
    size_t representative_value_id;
    size_t recoverable_member_count;
    size_t recoverable_intended_count;
    size_t split_family_root_value_id;
    size_t split_family_member_count;
    size_t split_family_intended_count;
    size_t representative_spill_priority;
    size_t representative_spill_cost;
    unsigned char representative_split_child;
    size_t representative_split_child_depth;
    ValueSsaAllocatorRetryFamilyEntryKind entry_kind;
    ValueSsaAllocationPreferredColorSource preferred_source;
    size_t preferred_class_size;
    size_t preferred_partner_value_id;
    size_t color;
    unsigned char requires_eviction;
    size_t blocker_value_id;
    unsigned char blocker_recolorable;
    size_t blocker_recolor_color;
};

typedef struct {
    size_t value_count;
    size_t item_count;
    ValueSsaAllocatorRetryFamilyAgendaItem *items;
} ValueSsaAllocatorRetryFamilyAgenda;

typedef struct {
    size_t phase_id;
    size_t family_root_value_id;
    ValueSsaAllocatorRetryFamilyAgendaItem entry;
    size_t recovered_member_count;
    size_t first_recovery_order;
    size_t last_recovery_order;
} ValueSsaAllocatorRetryPhaseTraceItem;

typedef struct {
    size_t value_count;
    size_t phase_count;
    ValueSsaAllocatorRetryPhaseTraceItem *items;
} ValueSsaAllocatorRetryPhaseTrace;

typedef enum {
    VALUE_SSA_ALLOCATOR_PHASE_FREEZE = 0,
    VALUE_SSA_ALLOCATOR_PHASE_SPILL,
    VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY,
} ValueSsaAllocatorPhase;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_EVENT_FREEZE = 0,
    VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE,
    VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE,
} ValueSsaAllocatorMoveTransitionEventKind;

typedef enum {
    VALUE_SSA_ALLOCATOR_REMOVE_NONE = 0,
    VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
    VALUE_SSA_ALLOCATOR_REMOVE_SPILL,
} ValueSsaAllocatorRemovalKind;

typedef struct {
    size_t step_index;
    size_t value_id;
    size_t root_value_id;
    size_t partner_value_id;
    size_t partner_root_value_id;
    ValueSsaAllocatorMoveTransitionEventKind event_kind;
    ValueSsaAllocatorPhase phase;
    ValueSsaAllocatorRemovalKind removal_kind;
    ValueSsaAllocatorMoveWorkClass move_work_class_before;
    ValueSsaAllocatorMoveWorkClass move_work_class_after;
    size_t family_active_member_count_before;
    size_t family_active_member_count_after;
    size_t family_external_neighbor_count_before;
    size_t family_coalesce_ready_neighbor_count_before;
    size_t family_coalesce_ready_affinity_weight_sum_before;
    size_t family_best_coalesce_ready_partner_root_value_id_before;
    size_t family_best_coalesce_ready_partner_affinity_weight_before;
    unsigned char family_best_coalesce_ready_partner_is_mutual_before;
    size_t family_external_neighbor_count_after;
    size_t family_coalesce_ready_neighbor_count_after;
    size_t family_coalesce_ready_affinity_weight_sum_after;
    size_t family_best_coalesce_ready_partner_root_value_id_after;
    size_t family_best_coalesce_ready_partner_affinity_weight_after;
    unsigned char family_best_coalesce_ready_partner_is_mutual_after;
    size_t family_external_affinity_weight_sum_before;
    size_t family_external_affinity_weight_sum_after;
    ValueSsaAllocatorMoveWorkClass partner_move_work_class_before;
    ValueSsaAllocatorMoveWorkClass partner_move_work_class_after;
    size_t partner_family_active_member_count_before;
    size_t partner_family_active_member_count_after;
    size_t partner_family_external_neighbor_count_before;
    size_t partner_family_coalesce_ready_neighbor_count_before;
    size_t partner_family_coalesce_ready_affinity_weight_sum_before;
    size_t partner_family_best_coalesce_ready_partner_root_value_id_before;
    size_t partner_family_best_coalesce_ready_partner_affinity_weight_before;
    unsigned char partner_family_best_coalesce_ready_partner_is_mutual_before;
    size_t partner_family_external_neighbor_count_after;
    size_t partner_family_coalesce_ready_neighbor_count_after;
    size_t partner_family_coalesce_ready_affinity_weight_sum_after;
    size_t partner_family_best_coalesce_ready_partner_root_value_id_after;
    size_t partner_family_best_coalesce_ready_partner_affinity_weight_after;
    unsigned char partner_family_best_coalesce_ready_partner_is_mutual_after;
    size_t partner_family_external_affinity_weight_sum_before;
    size_t partner_family_external_affinity_weight_sum_after;
    size_t move_work_priority;
} ValueSsaAllocatorMoveTransitionStep;

typedef struct {
    size_t value_count;
    size_t step_count;
    ValueSsaAllocatorMoveTransitionStep *steps;
} ValueSsaAllocatorMoveTransitionTrace;

typedef struct {
    size_t value_id;
    ValueSsaAllocationWorkClass work_class;
    ValueSsaAllocatorPhase phase;
    ValueSsaAllocatorRemovalKind removal_kind;
    size_t coalesce_root_value_id;
    size_t coalesce_class_size;
    size_t priority;
    size_t spill_cost;
    size_t spill_use_cost;
    size_t spill_use_loop_depth_cost;
    size_t spill_use_call_density_cost;
    size_t spill_live_range_cost;
    size_t spill_live_block_cost;
    size_t spill_loop_depth_cost;
    size_t spill_call_density_cost;
    size_t spill_call_crossing_cost;
    size_t spill_cross_block_cost;
    size_t spill_affinity_cost;
    unsigned char rematerializable;
    unsigned char split_child_value;
    size_t split_child_depth;
    size_t initial_degree;
    size_t active_degree;
    size_t initial_effective_degree;
    size_t active_effective_degree;
    ValueSsaAllocatorMoveWorkClass move_work_class;
    ValueSsaAllocatorMoveWorkClass move_work_next_class;
    size_t move_work_priority;
    size_t family_spill_removed_count;
    size_t family_active_member_count;
    size_t family_active_member_count_after;
    size_t family_external_neighbor_count;
    size_t family_coalesce_ready_neighbor_count;
    size_t family_coalesce_ready_affinity_weight_sum;
    size_t family_best_coalesce_ready_partner_root_value_id;
    size_t family_best_coalesce_ready_partner_affinity_weight;
    unsigned char family_best_coalesce_ready_partner_is_mutual;
    size_t family_external_affinity_weight_sum;
    size_t family_external_neighbor_count_after;
    size_t family_coalesce_ready_neighbor_count_after;
    size_t family_coalesce_ready_affinity_weight_sum_after;
    size_t family_best_coalesce_ready_partner_root_value_id_after;
    size_t family_best_coalesce_ready_partner_affinity_weight_after;
    unsigned char family_best_coalesce_ready_partner_is_mutual_after;
    size_t family_external_affinity_weight_sum_after;
    unsigned char initial_move_related;
    unsigned char active_move_related;
    unsigned char initial_effective_move_related;
    unsigned char active_effective_move_related;
    unsigned char was_frozen;
} ValueSsaAllocatorPlanItem;

typedef struct {
    size_t value_count;
    size_t item_count;
    ValueSsaAllocatorPlanItem *items;
    size_t *final_runtime_coalesce_roots;
    size_t *final_runtime_coalesce_class_sizes;
    size_t *value_item_indices;
} ValueSsaAllocatorPlan;

void value_ssa_allocation_result_init(ValueSsaAllocationResult *result);
void value_ssa_allocation_result_free(ValueSsaAllocationResult *result);
void value_ssa_program_allocation_result_init(ValueSsaProgramAllocationResult *result);
void value_ssa_program_allocation_result_free(ValueSsaProgramAllocationResult *result);
void value_ssa_function_allocation_layout_init(ValueSsaFunctionAllocationLayout *layout);
void value_ssa_function_allocation_layout_free(ValueSsaFunctionAllocationLayout *layout);
void value_ssa_program_allocation_layout_init(ValueSsaProgramAllocationLayout *layout);
void value_ssa_program_allocation_layout_free(ValueSsaProgramAllocationLayout *layout);
void value_ssa_allocate_rewrite_layout_report_init(ValueSsaAllocateRewriteLayoutReport *report);
void value_ssa_allocate_rewrite_layout_report_free(ValueSsaAllocateRewriteLayoutReport *report);
int value_ssa_allocate_rewrite_layout_report_set_stats(ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaAllocateRewriteStats *stats,
    ValueSsaError *error);
int value_ssa_allocate_rewrite_layout_report_set_program_layout(ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaProgramAllocationLayout *layout,
    ValueSsaError *error);
int value_ssa_build_allocate_rewrite_layout_report(const ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocateRewriteStats *stats,
    ValueSsaAllocateRewriteLayoutReport *out_report,
    ValueSsaError *error);
int value_ssa_allocate_rewrite_layout_report_attach_program_context(
    ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaProgram *program,
    ValueSsaError *error);
void value_ssa_allocator_plan_init(ValueSsaAllocatorPlan *plan);
void value_ssa_allocator_plan_free(ValueSsaAllocatorPlan *plan);
void value_ssa_allocation_select_stats_init(ValueSsaAllocationSelectStats *stats);
const char *value_ssa_allocation_preferred_color_source_name(ValueSsaAllocationPreferredColorSource source);
const char *value_ssa_allocated_value_placement_kind_name(ValueSsaAllocatedValuePlacementKind kind);
void value_ssa_allocator_coalesce_analysis_init(ValueSsaAllocatorCoalesceAnalysis *analysis);
void value_ssa_allocator_coalesce_analysis_free(ValueSsaAllocatorCoalesceAnalysis *analysis);
void value_ssa_allocator_move_family_analysis_init(ValueSsaAllocatorMoveFamilyAnalysis *analysis);
void value_ssa_allocator_move_family_analysis_free(ValueSsaAllocatorMoveFamilyAnalysis *analysis);
void value_ssa_allocator_move_worklist_init(ValueSsaAllocatorMoveWorklist *worklist);
void value_ssa_allocator_move_worklist_free(ValueSsaAllocatorMoveWorklist *worklist);
void value_ssa_allocator_coalesce_opportunity_agenda_init(ValueSsaAllocatorCoalesceOpportunityAgenda *agenda);
void value_ssa_allocator_coalesce_opportunity_agenda_free(ValueSsaAllocatorCoalesceOpportunityAgenda *agenda);
void value_ssa_allocator_retry_family_agenda_init(ValueSsaAllocatorRetryFamilyAgenda *agenda);
void value_ssa_allocator_retry_family_agenda_free(ValueSsaAllocatorRetryFamilyAgenda *agenda);
void value_ssa_allocator_retry_phase_trace_init(ValueSsaAllocatorRetryPhaseTrace *trace);
void value_ssa_allocator_retry_phase_trace_free(ValueSsaAllocatorRetryPhaseTrace *trace);
void value_ssa_allocator_move_transition_trace_init(ValueSsaAllocatorMoveTransitionTrace *trace);
void value_ssa_allocator_move_transition_trace_free(ValueSsaAllocatorMoveTransitionTrace *trace);
const char *value_ssa_allocator_move_family_state_name(ValueSsaAllocatorMoveFamilyState state);
const char *value_ssa_allocator_move_work_class_name(ValueSsaAllocatorMoveWorkClass work_class);
const char *value_ssa_allocator_move_transition_event_kind_name(
    ValueSsaAllocatorMoveTransitionEventKind event_kind);

int value_ssa_allocate_function(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaError *error);
int value_ssa_allocate_function_layout(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaFunctionAllocationLayout *out_layout,
    ValueSsaError *error);
int value_ssa_allocate_function_from_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaError *error);
int value_ssa_allocate_program(const ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
int value_ssa_allocate_program_layout(const ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *result,
    ValueSsaAllocateRewriteStats *out_stats,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_layout_with_stats(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaAllocateRewriteStats *out_stats,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_layout(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaError *error);
int value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaAllocateRewriteLayoutReport *out_report,
    ValueSsaError *error);
int value_ssa_rewrite_program_single_block_spills(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
int value_ssa_rewrite_program_single_block_spills_and_canonicalize(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
int value_ssa_rewrite_program_block_local_spill_splits_with_change_flag(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    int *out_rewrote_any,
    ValueSsaError *error);
int value_ssa_rewrite_program_block_local_spill_splits(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
int value_ssa_rewrite_program_block_local_spill_splits_and_canonicalize(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
int value_ssa_build_function_allocation_layout(const ValueSsaAllocationResult *result,
    ValueSsaFunctionAllocationLayout *out_layout,
    ValueSsaError *error);
int value_ssa_build_program_allocation_layout(const ValueSsaProgramAllocationResult *result,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaError *error);
int value_ssa_function_allocation_layout_attach_name(ValueSsaFunctionAllocationLayout *layout,
    const char *function_name,
    ValueSsaError *error);
int value_ssa_program_allocation_layout_attach_program_context(ValueSsaProgramAllocationLayout *layout,
    const ValueSsaProgram *program,
    ValueSsaError *error);
int value_ssa_function_allocation_layout_get_placement(const ValueSsaFunctionAllocationLayout *layout,
    size_t value_id,
    const ValueSsaAllocatedValuePlacement **out_placement);
int value_ssa_function_allocation_layout_get_summary(const ValueSsaFunctionAllocationLayout *layout,
    const char **out_function_name,
    size_t *out_value_count,
    size_t *out_color_budget,
    size_t *out_spill_slot_count,
    size_t *out_colored_count,
    size_t *out_spilled_count,
    size_t *out_optimistic_colored_count,
    size_t *out_confirmed_spilled_count,
    size_t *out_recovered_colored_count,
    size_t *out_used_color_count,
    size_t *out_color_group_count,
    size_t *out_spill_slot_group_count);
int value_ssa_function_allocation_layout_get_colored_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_allocation_layout_get_spilled_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_allocation_layout_get_optimistic_colored_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_allocation_layout_get_confirmed_spilled_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_allocation_layout_get_recovered_colored_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids);
int value_ssa_function_allocation_layout_get_used_colors(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_colors);
int value_ssa_function_allocation_layout_get_color_group(const ValueSsaFunctionAllocationLayout *layout,
    size_t color,
    const ValueSsaAllocationColorGroup **out_group);
int value_ssa_function_allocation_layout_get_color_group_at(const ValueSsaFunctionAllocationLayout *layout,
    size_t group_index,
    const ValueSsaAllocationColorGroup **out_group);
int value_ssa_function_allocation_layout_get_spill_slot_group(const ValueSsaFunctionAllocationLayout *layout,
    size_t spill_slot,
    const ValueSsaAllocationSpillSlotGroup **out_group);
int value_ssa_function_allocation_layout_get_spill_slot_group_at(const ValueSsaFunctionAllocationLayout *layout,
    size_t group_index,
    const ValueSsaAllocationSpillSlotGroup **out_group);
int value_ssa_program_allocation_layout_get_function_layout(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout);
int value_ssa_program_allocation_layout_get_function_layout_by_name(const ValueSsaProgramAllocationLayout *layout,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout);
int value_ssa_program_allocation_layout_get_function_name(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    const char **out_function_name);
int value_ssa_program_allocation_layout_find_function_index_by_name(const ValueSsaProgramAllocationLayout *layout,
    const char *function_name,
    size_t *out_function_index);
int value_ssa_program_allocation_layout_get_function_placement(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    size_t value_id,
    const ValueSsaAllocatedValuePlacement **out_placement);
int value_ssa_program_allocation_layout_get_function_summary(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    const char **out_function_name,
    size_t *out_value_count,
    size_t *out_color_budget,
    size_t *out_spill_slot_count,
    size_t *out_colored_count,
    size_t *out_spilled_count,
    size_t *out_optimistic_colored_count,
    size_t *out_confirmed_spilled_count,
    size_t *out_recovered_colored_count,
    size_t *out_used_color_count,
    size_t *out_color_group_count,
    size_t *out_spill_slot_group_count);
int value_ssa_program_allocation_layout_get_functions_with_colors(const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_allocation_layout_get_functions_with_spills(const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_allocation_layout_get_functions_with_confirmed_spills(
    const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_allocation_layout_get_functions_with_optimistic_colors(
    const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_allocation_layout_get_functions_with_recovered_colors(
    const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_program_allocation_layout_get_summary(const ValueSsaProgramAllocationLayout *layout,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_colored_count,
    size_t *out_total_spilled_count,
    size_t *out_total_optimistic_colored_count,
    size_t *out_total_confirmed_spilled_count,
    size_t *out_total_recovered_colored_count,
    size_t *out_max_color_budget,
    size_t *out_max_spill_slot_count);
int value_ssa_allocate_rewrite_layout_report_get_stats(const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds);
int value_ssa_allocate_rewrite_layout_report_get_program_layout(const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaProgramAllocationLayout **out_layout);
int value_ssa_allocate_rewrite_layout_report_get_function_layout(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout);
int value_ssa_allocate_rewrite_layout_report_get_function_layout_by_name(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout);
int value_ssa_allocate_rewrite_layout_report_get_function_name(const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    const char **out_function_name);
int value_ssa_allocate_rewrite_layout_report_find_function_index_by_name(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const char *function_name,
    size_t *out_function_index);
int value_ssa_allocate_rewrite_layout_report_get_function_placement(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    size_t value_id,
    const ValueSsaAllocatedValuePlacement **out_placement);
int value_ssa_allocate_rewrite_layout_report_get_function_summary(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    const char **out_function_name,
    size_t *out_value_count,
    size_t *out_color_budget,
    size_t *out_spill_slot_count,
    size_t *out_colored_count,
    size_t *out_spilled_count,
    size_t *out_optimistic_colored_count,
    size_t *out_confirmed_spilled_count,
    size_t *out_recovered_colored_count,
    size_t *out_used_color_count,
    size_t *out_color_group_count,
    size_t *out_spill_slot_group_count);
int value_ssa_allocate_rewrite_layout_report_get_functions_with_colors(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_allocate_rewrite_layout_report_get_functions_with_spills(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_allocate_rewrite_layout_report_get_functions_with_confirmed_spills(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_allocate_rewrite_layout_report_get_functions_with_optimistic_colors(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_allocate_rewrite_layout_report_get_functions_with_recovered_colors(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int value_ssa_allocate_rewrite_layout_report_get_summary(const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_colored_count,
    size_t *out_total_spilled_count,
    size_t *out_total_optimistic_colored_count,
    size_t *out_total_confirmed_spilled_count,
    size_t *out_total_recovered_colored_count,
    size_t *out_max_color_budget,
    size_t *out_max_spill_slot_count);
int value_ssa_function_allocation_layout_equivalent_ignoring_names(
    const ValueSsaFunctionAllocationLayout *lhs,
    const ValueSsaFunctionAllocationLayout *rhs);
int value_ssa_program_allocation_layout_equivalent_ignoring_names(
    const ValueSsaProgramAllocationLayout *lhs,
    const ValueSsaProgramAllocationLayout *rhs);
int value_ssa_allocate_rewrite_layout_report_equivalent_ignoring_names(
    const ValueSsaAllocateRewriteLayoutReport *lhs,
    const ValueSsaAllocateRewriteLayoutReport *rhs);
int value_ssa_dump_function_allocation_layout(const ValueSsaFunction *function,
    const ValueSsaFunctionAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_function_allocation_layout_artifact(const ValueSsaFunctionAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocation_result_artifact(const ValueSsaFunctionAllocationLayout *layout,
    const size_t *spill_priorities,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_allocation_layout(const ValueSsaProgram *program,
    const ValueSsaProgramAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_allocation_layout_artifact(const ValueSsaProgramAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocate_rewrite_layout_report_artifact(const ValueSsaAllocateRewriteLayoutReport *report,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocate_rewrite_layout_report(const ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocateRewriteStats *stats,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocation_result(const ValueSsaFunction *function,
    const ValueSsaAllocationResult *result,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_program_allocation_result(const ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocate_rewrite_stats(const ValueSsaAllocateRewriteStats *stats,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocation_select_stats(const ValueSsaAllocationResult *result,
    ValueSsaAllocationSelectStats *out_stats);
int value_ssa_dump_allocation_select_stats(const ValueSsaAllocationSelectStats *stats,
    char **out_text,
    ValueSsaError *error);
int value_ssa_dump_allocation_select_trace(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocator_coalesce_analysis(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    size_t color_budget,
    ValueSsaAllocatorCoalesceAnalysis *analysis,
    ValueSsaError *error);
int value_ssa_dump_allocator_coalesce_analysis(const ValueSsaFunction *function,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t color_budget,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocator_coalesce_analysis_find_pair(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t lhs_value_id,
    size_t rhs_value_id,
    size_t *out_index,
    const ValueSsaAllocatorCoalesceItem **out_item);
int value_ssa_allocator_coalesce_analysis_get_class(const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_id,
    size_t *out_root_value_id,
    size_t *out_class_size);
int value_ssa_compute_allocator_move_family_analysis(const ValueSsaFunction *function,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    ValueSsaError *error);
int value_ssa_dump_allocator_move_family_analysis(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocator_move_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveFamilyAnalysis *family_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocatorMoveWorklist *worklist,
    ValueSsaError *error);
int value_ssa_dump_allocator_move_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveWorklist *worklist,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocator_coalesce_opportunity_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveFamilyAnalysis *family_analysis,
    const ValueSsaAllocatorMoveWorklist *worklist,
    size_t color_budget,
    ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    ValueSsaError *error);
int value_ssa_dump_allocator_coalesce_opportunity_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocator_retry_family_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t color_budget,
    ValueSsaAllocatorRetryFamilyAgenda *agenda,
    ValueSsaError *error);
int value_ssa_dump_allocator_retry_family_agenda(const ValueSsaFunction *function,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocator_retry_phase_trace(const ValueSsaAllocationResult *result,
    ValueSsaAllocatorRetryPhaseTrace *trace,
    ValueSsaError *error);
int value_ssa_dump_allocator_retry_phase_trace(const ValueSsaAllocatorRetryPhaseTrace *trace,
    char **out_text,
    ValueSsaError *error);
int value_ssa_compute_allocator_move_transition_trace(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    const ValueSsaAllocatorPlan *plan,
    ValueSsaAllocatorMoveTransitionTrace *trace,
    ValueSsaError *error);
int value_ssa_dump_allocator_move_transition_trace(const ValueSsaFunction *function,
    const ValueSsaAllocatorMoveTransitionTrace *trace,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocator_move_family_analysis_find_root(const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveFamilyItem **out_item);
int value_ssa_allocator_move_family_analysis_get_value_family(const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveFamilyItem **out_item);
int value_ssa_allocator_move_worklist_find_root(const ValueSsaAllocatorMoveWorklist *worklist,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveWorkItem **out_item);
int value_ssa_allocator_move_worklist_get_value_work(const ValueSsaAllocatorMoveWorklist *worklist,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocatorMoveWorkItem **out_item);
int value_ssa_allocator_coalesce_opportunity_agenda_find_pair(
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorCoalesceOpportunityItem **out_item);
int value_ssa_allocator_coalesce_opportunity_agenda_find_root_best(
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorCoalesceOpportunityItem **out_item);
int value_ssa_allocator_retry_family_agenda_find_root(
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    size_t *out_index,
    const ValueSsaAllocatorRetryFamilyAgendaItem **out_item);
int value_ssa_allocator_retry_phase_trace_find_phase_id(
    const ValueSsaAllocatorRetryPhaseTrace *trace,
    size_t phase_id,
    size_t *out_index,
    const ValueSsaAllocatorRetryPhaseTraceItem **out_item);
int value_ssa_allocator_move_transition_trace_find_value_step(const ValueSsaAllocatorMoveTransitionTrace *trace,
    size_t value_id,
    ValueSsaAllocatorMoveTransitionEventKind event_kind,
    size_t *out_index,
    const ValueSsaAllocatorMoveTransitionStep **out_step);
const char *value_ssa_allocator_phase_name(ValueSsaAllocatorPhase phase);
const char *value_ssa_allocator_removal_kind_name(ValueSsaAllocatorRemovalKind removal_kind);
int value_ssa_compute_allocator_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaCopyAffinityGraph *affinity_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    ValueSsaAllocatorPlan *plan,
    ValueSsaError *error);
int value_ssa_dump_allocator_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    char **out_text,
    ValueSsaError *error);
int value_ssa_allocator_plan_find_value(const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocatorPlanItem **out_item);
int value_ssa_allocator_plan_get_spill_cost(const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t *out_total_cost,
    size_t *out_use_cost,
    size_t *out_live_range_cost,
    size_t *out_affinity_cost);
int value_ssa_allocator_plan_get_spill_cost_detail(const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t *out_use_loop_depth_cost,
    size_t *out_use_call_density_cost,
    size_t *out_live_block_cost,
    size_t *out_loop_depth_cost,
    size_t *out_call_density_cost,
    size_t *out_call_crossing_cost,
    size_t *out_cross_block_cost);
int value_ssa_allocation_result_get_color(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_color,
    size_t *out_color);
int value_ssa_allocation_result_is_spilled(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spilled);
int value_ssa_allocation_result_is_spill_intended(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spill_intended);
int value_ssa_allocation_result_is_spill_confirmed(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spill_confirmed);
int value_ssa_allocation_result_is_spill_recovered(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spill_recovered);
int value_ssa_allocation_result_get_spill_recovery_order(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_recovery_order,
    size_t *out_recovery_order);
int value_ssa_allocation_result_get_spill_recovery_phase(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_recovery_phase,
    size_t *out_recovery_phase_id,
    size_t *out_recovery_family_root_value_id);
int value_ssa_allocation_result_get_spill_recovery_phase_member_order(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_member_order,
    size_t *out_phase_member_order);
int value_ssa_allocation_result_get_spill_recovery_phase_member_count(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_member_count,
    size_t *out_phase_member_count);
int value_ssa_allocation_result_get_spill_recovery_phase_entry(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_entry,
    size_t *out_entry_value_id,
    size_t *out_entry_recoverable_count,
    size_t *out_entry_intended_count);
int value_ssa_allocation_result_get_spill_recovery_phase_entry_split_family(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_entry,
    size_t *out_split_family_root_value_id,
    size_t *out_split_family_member_count,
    size_t *out_split_family_intended_count);
int value_ssa_allocation_result_used_retry_eviction(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_retry_eviction,
    size_t *out_blocker_value_id);
int value_ssa_allocation_result_get_targeted_eviction_outcome(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_targeted_eviction,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color);
int value_ssa_allocation_result_get_generic_eviction_outcome(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_generic_eviction,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color);
int value_ssa_allocation_result_get_retry_eviction_outcome(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_retry_eviction,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color);
int value_ssa_allocation_result_get_spill_priority(const ValueSsaAllocationResult *result,
    size_t value_id,
    size_t *out_priority);
int value_ssa_allocation_result_get_spill_slot(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_spill_slot,
    size_t *out_spill_slot);
int value_ssa_allocation_result_count_spilled_values(const ValueSsaAllocationResult *result, size_t *out_count);
int value_ssa_allocation_result_count_colored_values(const ValueSsaAllocationResult *result, size_t *out_count);
void value_ssa_allocate_rewrite_stats_init(ValueSsaAllocateRewriteStats *stats);

#endif
