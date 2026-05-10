#include "value_ssa_alloc.h"

#include "value_ssa_pass.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} ValueSsaAllocStringBuilder;

typedef struct {
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
} ValueSsaAllocationFamilyRuntimeFacts;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_DRIVER_NONE = 0,
    VALUE_SSA_ALLOC_MOVE_ENGINE_PLAN_DRIVER_COMPLETED,
} ValueSsaAllocatorMoveEnginePlanDriverStatus;

typedef struct {
    ValueSsaAllocatorMoveEnginePlanDriverStatus status;
    size_t completed_plan_item_count;
    size_t completed_step_count;
} ValueSsaAllocatorMoveEnginePlanDriverResult;

typedef enum {
    VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_NONE = 0,
    VALUE_SSA_ALLOC_MOVE_ENGINE_RUN_COMPLETED,
} ValueSsaAllocatorMoveEngineRunStatus;

typedef struct {
    ValueSsaAllocatorMoveEngineRunStatus status;
    ValueSsaAllocatorMoveEnginePlanDriverResult plan_driver;
    size_t published_trace_step_count;
    size_t coalesce_step_count;
    size_t freeze_step_count;
    size_t simplify_remove_step_count;
    size_t spill_remove_step_count;
    size_t family_member_rebuild_count;
    size_t general_refresh_count;
    size_t coalesce_refresh_count;
    size_t refresh_affected_family_root_total;
    size_t refresh_affected_family_root_max;
    size_t phase_entry_rebuild_count;
    size_t simplify_candidate_scan_total;
    size_t freeze_candidate_scan_total;
    size_t spill_candidate_scan_total;
    size_t simplify_pair_scan_total;
    size_t freeze_pair_scan_total;
    unsigned char published_plan_artifacts;
    unsigned char published_trace_artifacts;
} ValueSsaAllocatorMoveEngineRunResult;

static void value_ssa_alloc_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...);
static int value_ssa_alloc_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
static int value_ssa_alloc_prepare_result(ValueSsaAllocationResult *result,
    size_t value_count,
    size_t color_budget,
    ValueSsaError *error);
static int value_ssa_clone_allocation_result(const ValueSsaAllocationResult *src,
    ValueSsaAllocationResult *dst,
    ValueSsaError *error);
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
    ValueSsaError *error);
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
    ValueSsaError *error);
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
    ValueSsaError *error);
static int value_ssa_alloc_insert_instruction(ValueSsaBasicBlock *block,
    size_t instruction_index,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error);
static int value_ssa_alloc_rewrite_program_rewriteable_spills(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const size_t *spill_local_floors,
    unsigned char *out_rewritten_functions,
    int *out_rewrote_any,
    ValueSsaError *error);
static int value_ssa_rewrite_program_block_local_spill_splits_with_change_map(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const size_t *spill_local_floors,
    unsigned char *out_rewritten_functions,
    int *out_rewrote_any,
    ValueSsaError *error);
static int value_ssa_alloc_run_program_rewrite_stage(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const size_t *spill_local_floors,
    unsigned char *out_rewritten_functions,
    int *out_split_rewrote_any,
    int *out_spill_rewrote_any,
    int *out_rewrote_any,
    int *out_canonicalized_after_rewrite,
    ValueSsaError *error);
static int value_ssa_alloc_trace_enabled(void);
static double value_ssa_alloc_now_s(void);
static void value_ssa_alloc_trace_timing(const char *stage, double elapsed_s);
static void value_ssa_alloc_trace_function_timing(const char *function_name,
    const char *stage,
    double elapsed_s);

static int value_ssa_alloc_trace_enabled(void) {
    const char *flag = getenv("VALUE_SSA_TRACE_TIMING");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static double value_ssa_alloc_now_s(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static void value_ssa_alloc_trace_timing(const char *stage, double elapsed_s) {
    if (!stage || !value_ssa_alloc_trace_enabled()) {
        return;
    }

    fprintf(stderr, "[value-ssa-alloc-timing] %s %.3f\n", stage, elapsed_s);
}

static void value_ssa_alloc_trace_function_timing(const char *function_name,
    const char *stage,
    double elapsed_s) {
    if (!stage || !value_ssa_alloc_trace_enabled()) {
        return;
    }

    fprintf(stderr,
        "[value-ssa-alloc-timing] fn=%s %s %.3f\n",
        function_name ? function_name : "<anon>",
        stage,
        elapsed_s);
}
int value_ssa_rewrite_program_block_local_spill_splits_with_change_flag(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    int *out_rewrote_any,
    ValueSsaError *error);
int value_ssa_rewrite_program_block_local_spill_splits(ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    ValueSsaError *error);
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
    ValueSsaError *error);
static void value_ssa_alloc_move_engine_run_result_init(ValueSsaAllocatorMoveEngineRunResult *result);
static int value_ssa_alloc_append_format(ValueSsaAllocStringBuilder *builder, const char *fmt, ...);
static void value_ssa_alloc_retry_family_agenda_entry_clear(ValueSsaAllocatorRetryFamilyAgendaItem *entry);
static void value_ssa_alloc_retry_phase_trace_item_clear(ValueSsaAllocatorRetryPhaseTraceItem *item);
static char *value_ssa_alloc_strdup(const char *text);

static char *value_ssa_alloc_strdup(const char *text) {
    char *copy;
    size_t length;

    if (!text) {
        return NULL;
    }

    length = strlen(text);
    copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void value_ssa_alloc_retry_family_agenda_entry_clear(ValueSsaAllocatorRetryFamilyAgendaItem *entry) {
    if (!entry) {
        return;
    }

    memset(entry, 0, sizeof(*entry));
    entry->root_value_id = (size_t)-1;
    entry->representative_value_id = (size_t)-1;
    entry->split_family_root_value_id = (size_t)-1;
    entry->preferred_partner_value_id = (size_t)-1;
    entry->blocker_value_id = (size_t)-1;
    entry->blocker_recolor_color = (size_t)-1;
}

static void value_ssa_alloc_retry_phase_trace_item_clear(ValueSsaAllocatorRetryPhaseTraceItem *item) {
    if (!item) {
        return;
    }

    memset(item, 0, sizeof(*item));
    item->phase_id = (size_t)-1;
    item->family_root_value_id = (size_t)-1;
    value_ssa_alloc_retry_family_agenda_entry_clear(&item->entry);
}

#define VALUE_SSA_ALLOC_SPLIT_AGGREGATOR
#include "value_ssa_alloc_core.inc"
#include "value_ssa_alloc_spill.inc"
#include "value_ssa_alloc_spill_cost.inc"
#include "value_ssa_alloc_coalesce.inc"
#include "value_ssa_alloc_move.inc"
#include "value_ssa_alloc_move_worklist.inc"
#include "value_ssa_alloc_coalesce_agenda.inc"
#include "value_ssa_alloc_plan.inc"
#include "value_ssa_alloc_move_engine.inc"
#include "value_ssa_alloc_move_transition.inc"
#include "value_ssa_alloc_select.inc"
#include "value_ssa_alloc_retry_agenda.inc"
#include "value_ssa_alloc_retry_phase_trace.inc"
#include "value_ssa_alloc_color.inc"
#include "value_ssa_alloc_rewrite.inc"
#include "value_ssa_alloc_layout.inc"
#include "value_ssa_alloc_layout_dump.inc"
#include "value_ssa_alloc_dump.inc"
