#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

typedef enum {
    VALUE_SSA_ALLOC_BUILD_STEP_NONE = 0,
    VALUE_SSA_ALLOC_BUILD_STEP_COMPLETED,
} ValueSsaAllocationBuildStepStatus;

typedef struct {
    ValueSsaDefUseAnalysis def_use;
    ValueSsaCfgAnalysis cfg;
    ValueSsaLivenessAnalysis liveness;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaAllocatorCoalesceAnalysis coalesce_analysis;
    ValueSsaAllocatorPlan plan;
} ValueSsaAllocationOwnedArtifacts;

typedef struct {
    ValueSsaAllocationBuildStepStatus status;
    unsigned char built_def_use;
    unsigned char built_cfg;
    unsigned char built_liveness;
    unsigned char built_interference;
    unsigned char built_affinity;
    unsigned char built_prep;
    unsigned char built_worklist;
    unsigned char built_coalesce_analysis;
} ValueSsaAllocationBuildStepResult;

typedef enum {
    VALUE_SSA_ALLOC_PIPELINE_STEP_NONE = 0,
    VALUE_SSA_ALLOC_PIPELINE_STEP_COMPLETED,
} ValueSsaAllocationPipelineStepStatus;

typedef struct {
    ValueSsaAllocationPipelineStepStatus status;
    unsigned char built_plan;
    unsigned char reused_plan;
    ValueSsaAllocationExecuteStepResult execute_step;
} ValueSsaAllocationPipelineStepResult;

typedef enum {
    VALUE_SSA_ALLOC_ENTRY_STEP_NONE = 0,
    VALUE_SSA_ALLOC_ENTRY_STEP_COMPLETED,
} ValueSsaAllocationEntryStepStatus;

typedef struct {
    ValueSsaAllocationEntryStepStatus status;
    ValueSsaAllocationPipelineStepResult pipeline_step;
} ValueSsaAllocationEntryStepResult;

typedef enum {
    VALUE_SSA_ALLOC_FULL_STEP_NONE = 0,
    VALUE_SSA_ALLOC_FULL_STEP_COMPLETED,
} ValueSsaAllocationFullStepStatus;

typedef struct {
    ValueSsaAllocationFullStepStatus status;
    ValueSsaAllocationBuildStepResult build_step;
    ValueSsaAllocationEntryStepResult entry_step;
} ValueSsaAllocationFullStepResult;

typedef enum {
    VALUE_SSA_ALLOC_PROGRAM_STEP_NONE = 0,
    VALUE_SSA_ALLOC_PROGRAM_STEP_COMPLETED,
} ValueSsaAllocationProgramStepStatus;

typedef struct {
    ValueSsaAllocationProgramStepStatus status;
    size_t allocated_function_result_count;
    size_t completed_function_count;
} ValueSsaAllocationProgramStepResult;

typedef enum {
    VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_NONE = 0,
    VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_COMPLETED,
    VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_CONVERGED,
} ValueSsaAllocationRewriteLoopStepStatus;

typedef struct {
    ValueSsaAllocationRewriteLoopStepStatus status;
    unsigned char split_rewrote_any;
    unsigned char spill_rewrote_any;
    unsigned char rewrote_any;
    unsigned char canonicalized_after_rewrite;
    unsigned char reallocated_after_rewrite;
    size_t completed_iteration_count;
} ValueSsaAllocationRewriteLoopStepResult;

static int value_ssa_alloc_entry_step_completed_or_cleanup(ValueSsaAllocationResult *result,
    const ValueSsaAllocationEntryStepResult *entry_step,
    ValueSsaError *error);
static int value_ssa_alloc_run_entry_step(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaCopyAffinityGraph *affinity,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    int plan_already_built,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationEntryStepResult *out_result,
    ValueSsaError *error);
static int value_ssa_alloc_program_step_completed_or_cleanup(ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocationProgramStepResult *program_step,
    ValueSsaError *error);
static void value_ssa_alloc_rewrite_loop_step_result_init(ValueSsaAllocationRewriteLoopStepResult *result);
static int value_ssa_alloc_run_rewrite_loop_step(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *current_result,
    ValueSsaProgramAllocationResult *next_result,
    size_t *spill_local_floors,
    size_t iteration_count,
    ValueSsaAllocateRewriteStats *out_stats,
    ValueSsaAllocationRewriteLoopStepResult *out_result,
    ValueSsaError *error);
static void value_ssa_alloc_mark_evicted_value_spilled(ValueSsaAllocationResult *result, size_t value_id);
static int value_ssa_alloc_assign_spill_slots(const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    ValueSsaAllocationResult *result,
    ValueSsaError *error);

static int value_ssa_alloc_function_spill_shape_differs(const ValueSsaAllocationResult *lhs,
    const ValueSsaAllocationResult *rhs) {
    size_t value_id;

    if (!lhs || !rhs || lhs->value_count != rhs->value_count) {
        return 1;
    }
    for (value_id = 0; value_id < lhs->value_count; ++value_id) {
        if ((lhs->spill_flags[value_id] ? 1u : 0u) != (rhs->spill_flags[value_id] ? 1u : 0u)) {
            return 1;
        }
        if (lhs->spill_flags[value_id] &&
            lhs->spill_slots[value_id] != rhs->spill_slots[value_id]) {
            return 1;
        }
    }
    return 0;
}

static int value_ssa_alloc_function_has_call_instruction(const ValueSsaFunction *function) {
    size_t block_index;

    if (!function || !function->has_body) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            if (block->instructions[instruction_index].kind == VALUE_SSA_INSTR_CALL) {
                return 1;
            }
        }
    }
    return 0;
}

static int value_ssa_alloc_function_has_new_spill_local_loads(const ValueSsaFunction *function,
    size_t spill_local_floor) {
    size_t block_index;

    if (!function || !function->has_body) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const ValueSsaBasicBlock *block = &function->blocks[block_index];
        size_t instruction_index;

        for (instruction_index = 0; instruction_index < block->instruction_count; ++instruction_index) {
            const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

            if (instruction->kind == VALUE_SSA_INSTR_LOAD_LOCAL &&
                instruction->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
                instruction->as.load_slot.id >= spill_local_floor) {
                return 1;
            }
        }
    }
    return 0;
}

static int value_ssa_alloc_result_has_interference_color_conflict(const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocationResult *result) {
    size_t lhs;

    if (!interference || !result || interference->value_count != result->value_count || !interference->interferes) {
        return 0;
    }
    for (lhs = 0; lhs < result->value_count; ++lhs) {
        size_t rhs;

        if (!result->has_color[lhs] || result->spill_flags[lhs]) {
            continue;
        }
        for (rhs = lhs + 1; rhs < result->value_count; ++rhs) {
            if (!result->has_color[rhs] || result->spill_flags[rhs]) {
                continue;
            }
            if (!interference->interferes[lhs * interference->value_count + rhs]) {
                continue;
            }
            if (result->colors[lhs] == result->colors[rhs]) {
                return 1;
            }
        }
    }
    return 0;
}

static int value_ssa_alloc_build_identity_coalesce_analysis(size_t value_count,
    ValueSsaAllocatorCoalesceAnalysis *analysis,
    ValueSsaError *error) {
    size_t value_id;

    if (!analysis) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-243: invalid identity-coalesce contract");
        return 0;
    }
    value_ssa_allocator_coalesce_analysis_free(analysis);
    analysis->value_count = value_count;
    if (value_count == 0u) {
        return 1;
    }
    analysis->value_roots = (size_t *)calloc(value_count, sizeof(size_t));
    analysis->class_sizes = (size_t *)calloc(value_count, sizeof(size_t));
    if (!analysis->value_roots || !analysis->class_sizes) {
        value_ssa_allocator_coalesce_analysis_free(analysis);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-244: out of memory building identity coalesce analysis");
        return 0;
    }
    for (value_id = 0; value_id < value_count; ++value_id) {
        analysis->value_roots[value_id] = value_id;
        analysis->class_sizes[value_id] = 1u;
    }
    return 1;
}

static int value_ssa_alloc_force_spill_interference_color_conflicts(const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference,
    ValueSsaAllocationResult *result,
    ValueSsaError *error) {
    int changed = 0;

    if (!interference || !result || interference->value_count != result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-246: invalid conflict-spill contract");
        return 0;
    }
    for (;;) {
        size_t lhs;
        int found = 0;

        for (lhs = 0; lhs < result->value_count && !found; ++lhs) {
            size_t rhs;

            if (!result->has_color[lhs] || result->spill_flags[lhs]) {
                continue;
            }
            for (rhs = lhs + 1; rhs < result->value_count; ++rhs) {
                size_t victim = lhs;

                if (!result->has_color[rhs] || result->spill_flags[rhs] ||
                    !interference->interferes[lhs * interference->value_count + rhs] ||
                    result->colors[lhs] != result->colors[rhs]) {
                    continue;
                }
                if (result->spill_priorities &&
                    result->spill_priorities[rhs] < result->spill_priorities[lhs]) {
                    victim = rhs;
                } else if (result->spill_priorities &&
                    result->spill_priorities[rhs] == result->spill_priorities[lhs] &&
                    rhs > lhs) {
                    victim = rhs;
                }
                value_ssa_alloc_mark_evicted_value_spilled(result, victim);
                changed = 1;
                found = 1;
                break;
            }
        }
        if (!found) {
            break;
        }
    }
    if (changed && !value_ssa_alloc_assign_spill_slots(prep, interference, result, error)) {
        return 0;
    }
    return 1;
}

static void value_ssa_alloc_pipeline_step_result_init(ValueSsaAllocationPipelineStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_PIPELINE_STEP_NONE;
    value_ssa_alloc_execute_step_result_init(&result->execute_step);
}

static void value_ssa_alloc_build_step_result_init(ValueSsaAllocationBuildStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_BUILD_STEP_NONE;
}

static void value_ssa_alloc_owned_artifacts_init(ValueSsaAllocationOwnedArtifacts *artifacts) {
    if (!artifacts) {
        return;
    }

    value_ssa_def_use_analysis_init(&artifacts->def_use);
    value_ssa_cfg_analysis_init(&artifacts->cfg);
    value_ssa_liveness_analysis_init(&artifacts->liveness);
    value_ssa_interference_graph_init(&artifacts->interference);
    value_ssa_copy_affinity_graph_init(&artifacts->affinity);
    value_ssa_allocation_prep_init(&artifacts->prep);
    value_ssa_allocation_worklist_init(&artifacts->worklist);
    value_ssa_allocator_coalesce_analysis_init(&artifacts->coalesce_analysis);
    value_ssa_allocator_plan_init(&artifacts->plan);
}

static void value_ssa_alloc_owned_artifacts_free(ValueSsaAllocationOwnedArtifacts *artifacts) {
    if (!artifacts) {
        return;
    }

    value_ssa_allocator_plan_free(&artifacts->plan);
    value_ssa_allocator_coalesce_analysis_free(&artifacts->coalesce_analysis);
    value_ssa_allocation_worklist_free(&artifacts->worklist);
    value_ssa_allocation_prep_free(&artifacts->prep);
    value_ssa_copy_affinity_graph_free(&artifacts->affinity);
    value_ssa_interference_graph_free(&artifacts->interference);
    value_ssa_liveness_analysis_free(&artifacts->liveness);
    value_ssa_cfg_analysis_free(&artifacts->cfg);
    value_ssa_def_use_analysis_free(&artifacts->def_use);
}

static void value_ssa_alloc_entry_step_result_init(ValueSsaAllocationEntryStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_ENTRY_STEP_NONE;
    value_ssa_alloc_pipeline_step_result_init(&result->pipeline_step);
}

static void value_ssa_alloc_full_step_result_init(ValueSsaAllocationFullStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_FULL_STEP_NONE;
    value_ssa_alloc_build_step_result_init(&result->build_step);
    value_ssa_alloc_entry_step_result_init(&result->entry_step);
}

static void value_ssa_alloc_program_step_result_init(ValueSsaAllocationProgramStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_PROGRAM_STEP_NONE;
}

static void value_ssa_alloc_rewrite_loop_step_result_init(ValueSsaAllocationRewriteLoopStepResult *result) {
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->status = VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_NONE;
}

static int value_ssa_alloc_run_build_step(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaDefUseAnalysis *def_use,
    ValueSsaCfgAnalysis *cfg,
    ValueSsaLivenessAnalysis *liveness,
    ValueSsaInterferenceGraph *interference,
    ValueSsaCopyAffinityGraph *affinity,
    ValueSsaAllocationPrep *prep,
    ValueSsaAllocationWorklist *worklist,
    ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    ValueSsaAllocationBuildStepResult *out_result,
    ValueSsaError *error) {
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (out_result) {
        value_ssa_alloc_build_step_result_init(out_result);
    }
    if (!function || !def_use || !cfg || !liveness || !interference || !affinity || !prep || !worklist ||
        !coalesce_analysis || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-198: invalid build-step contract");
        return 0;
    }

    if (!value_ssa_compute_def_use_analysis(function, def_use, error)) {
        return 0;
    }
    out_result->built_def_use = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step def-use", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_cfg_analysis(function, cfg, error)) {
        return 0;
    }
    out_result->built_cfg = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step cfg", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_liveness_analysis(function, cfg, liveness, error)) {
        return 0;
    }
    out_result->built_liveness = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step liveness", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_interference_graph(function, cfg, liveness, interference, error)) {
        return 0;
    }
    out_result->built_interference = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step interference", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_copy_affinity_graph(function, interference, affinity, error)) {
        return 0;
    }
    out_result->built_affinity = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step affinity", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_allocation_prep(function, def_use, liveness, interference, affinity, prep, error)) {
        return 0;
    }
    out_result->built_prep = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step prep", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_allocation_worklist(function, prep, worklist, error)) {
        return 0;
    }
    out_result->built_worklist = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "build-step worklist", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_compute_allocator_coalesce_analysis(
            function, prep, interference, affinity, color_budget, coalesce_analysis, error)) {
        return 0;
    }
    out_result->built_coalesce_analysis = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(
            function->name, "build-step coalesce-analysis", value_ssa_alloc_now_s() - stage_start);
        value_ssa_alloc_trace_function_timing(function->name, "build-step total", value_ssa_alloc_now_s() - total_start);
    }

    out_result->status = VALUE_SSA_ALLOC_BUILD_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_run_full_step(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaAllocationOwnedArtifacts *artifacts,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationFullStepResult *out_result,
    ValueSsaError *error) {
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (out_result) {
        value_ssa_alloc_full_step_result_init(out_result);
    }
    if (!function || !artifacts || !result || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-200: invalid full-step contract");
        return 0;
    }

    if (!value_ssa_alloc_run_build_step(function,
            color_budget,
            &artifacts->def_use,
            &artifacts->cfg,
            &artifacts->liveness,
            &artifacts->interference,
            &artifacts->affinity,
            &artifacts->prep,
            &artifacts->worklist,
            &artifacts->coalesce_analysis,
            &out_result->build_step,
            error)) {
        return 0;
    }
    if (out_result->build_step.status != VALUE_SSA_ALLOC_BUILD_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-199: build-step ended in unknown state");
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "full-step build", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_alloc_run_entry_step(function,
            &artifacts->prep,
            &artifacts->worklist,
            &artifacts->interference,
            &artifacts->affinity,
            &artifacts->coalesce_analysis,
            &artifacts->plan,
            color_budget,
            0,
            result,
            &out_result->entry_step,
            error)) {
        return 0;
    }
    if (!value_ssa_alloc_entry_step_completed_or_cleanup(NULL, &out_result->entry_step, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "full-step entry", value_ssa_alloc_now_s() - stage_start);
        value_ssa_alloc_trace_function_timing(function->name, "full-step total", value_ssa_alloc_now_s() - total_start);
    }

    out_result->status = VALUE_SSA_ALLOC_FULL_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_function_is_large_straight_line_chain_hotspot(const ValueSsaFunction *function) {
    const ValueSsaBasicBlock *block;
    size_t instruction_index;
    size_t load_count = 0u;

    if (!function || !function->has_body || function->block_count != 1u || function->next_value_id < 4096u) {
        return 0;
    }

    block = &function->blocks[0];
    if (block->phi_count != 0u || block->instruction_count < 4096u) {
        return 0;
    }
    switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            break;
        default:
            return 0;
    }

    for (instruction_index = 0u; instruction_index < block->instruction_count; ++instruction_index) {
        const ValueSsaInstruction *instruction = &block->instructions[instruction_index];

        switch (instruction->kind) {
            case VALUE_SSA_INSTR_LOAD_LOCAL:
            case VALUE_SSA_INSTR_LOAD_GLOBAL:
                ++load_count;
                break;
            case VALUE_SSA_INSTR_MOV:
            case VALUE_SSA_INSTR_BINARY:
                break;
            default:
                return 0;
        }
    }

    return load_count <= 32u;
}

static int value_ssa_alloc_try_large_straight_line_chain_fast_allocate(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *def_use,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaError *error) {
    size_t value_id;
    size_t next_spill_slot = 0u;

    if (!function || !def_use || !result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-248: invalid straight-line fast allocation contract");
        return 0;
    }
    if (!value_ssa_alloc_prepare_result(result, function->next_value_id, color_budget, error)) {
        return 0;
    }
    if (function->name) {
        result->function_name = value_ssa_alloc_strdup(function->name);
        if (!result->function_name) {
            value_ssa_allocation_result_free(result);
            value_ssa_alloc_set_error(
                error, 0, 0, "VALUE-SSA-ALLOC-249: out of memory copying straight-line fast allocation function name");
            return 0;
        }
    }

    for (value_id = 0u; value_id < function->next_value_id; ++value_id) {
        size_t use_count = def_use->use_counts ? def_use->use_counts[value_id] : 0u;

        if (use_count <= 1u && color_budget > 0u) {
            result->has_color[value_id] = 1u;
            result->colors[value_id] = 0u;
            result->spill_flags[value_id] = 0u;
            result->spill_confirmed_flags[value_id] = 0u;
            result->spill_intended_flags[value_id] = 0u;
            result->spill_slots[value_id] = (size_t)-1;
        } else {
            result->has_color[value_id] = 0u;
            result->colors[value_id] = (size_t)-1;
            result->spill_flags[value_id] = 1u;
            result->spill_confirmed_flags[value_id] = 1u;
            result->spill_intended_flags[value_id] = 1u;
            result->spill_slots[value_id] = next_spill_slot++;
        }
        result->spill_priorities[value_id] = use_count;
    }
    result->spill_slot_count = next_spill_slot;
    return 1;
}

static int value_ssa_alloc_entry_step_completed_or_cleanup(ValueSsaAllocationResult *result,
    const ValueSsaAllocationEntryStepResult *entry_step,
    ValueSsaError *error) {
    if (!entry_step || entry_step->status != VALUE_SSA_ALLOC_ENTRY_STEP_COMPLETED) {
        if (!error || error->message[0] == '\0') {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-197: entry-step ended in unknown state");
        }
        if (result) {
            value_ssa_allocation_result_free(result);
        }
        return 0;
    }
    return 1;
}

static int value_ssa_alloc_program_step_completed_or_cleanup(ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocationProgramStepResult *program_step,
    ValueSsaError *error) {
    if (!program_step || program_step->status != VALUE_SSA_ALLOC_PROGRAM_STEP_COMPLETED) {
        if (!error || error->message[0] == '\0') {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-204: program-step ended in unknown state");
        }
        if (result) {
            value_ssa_program_allocation_result_free(result);
        }
        return 0;
    }
    return 1;
}

static int value_ssa_alloc_run_rewrite_loop_step(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *current_result,
    ValueSsaProgramAllocationResult *next_result,
    size_t *spill_local_floors,
    size_t iteration_count,
    ValueSsaAllocateRewriteStats *out_stats,
    ValueSsaAllocationRewriteLoopStepResult *out_result,
    ValueSsaError *error) {
    enum { VALUE_SSA_ALLOC_MAX_REWRITE_ITERATIONS = 32 };
    size_t function_index;
    int split_rewrote_any = 0;
    int spill_rewrote_any = 0;
    int rewrote_any = 0;
    int canonicalized_after_rewrite = 0;
    unsigned char *rewritten_functions = NULL;
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (out_result) {
        value_ssa_alloc_rewrite_loop_step_result_init(out_result);
    }
    if (!program || !current_result || !next_result || !spill_local_floors || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-205: invalid allocate+rewrite loop-step contract");
        return 0;
    }
    if (iteration_count >= VALUE_SSA_ALLOC_MAX_REWRITE_ITERATIONS) {
        value_ssa_alloc_set_error(
            error, 0, 0, "VALUE-SSA-ALLOC-048: allocate+rewrite did not converge within %d iterations",
            VALUE_SSA_ALLOC_MAX_REWRITE_ITERATIONS);
        return 0;
    }

    out_result->completed_iteration_count = iteration_count + 1;
    rewritten_functions =
        (unsigned char *)calloc(program->function_count ? program->function_count : 1u, sizeof(unsigned char));
    if (!rewritten_functions) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-236: out of memory preparing rewrite-loop function map");
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        if (iteration_count == 0) {
            spill_local_floors[function_index] = program->functions[function_index].local_count;
        }
    }

    if (!value_ssa_alloc_run_program_rewrite_stage(program,
            current_result,
            spill_local_floors,
            rewritten_functions,
            &split_rewrote_any,
            &spill_rewrote_any,
            &rewrote_any,
            &canonicalized_after_rewrite,
            error)) {
        free(rewritten_functions);
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_timing("rewrite-loop rewrite-stage", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    out_result->split_rewrote_any = split_rewrote_any ? 1u : 0u;
    out_result->spill_rewrote_any = spill_rewrote_any ? 1u : 0u;
    out_result->rewrote_any = rewrote_any ? 1u : 0u;
    out_result->canonicalized_after_rewrite = canonicalized_after_rewrite ? 1u : 0u;
    if (!rewrote_any) {
        if (trace_timing) {
            value_ssa_alloc_trace_timing("rewrite-loop total", value_ssa_alloc_now_s() - total_start);
        }
        free(rewritten_functions);
        out_result->status = VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_CONVERGED;
        return 1;
    }

    if (out_stats) {
        ++out_stats->rewrite_rounds;
    }
    value_ssa_program_allocation_result_free(next_result);
    next_result->function_count = program->function_count;
    if (program->function_count > 0u) {
        next_result->function_results =
            (ValueSsaAllocationResult *)calloc(program->function_count, sizeof(ValueSsaAllocationResult));
        if (!next_result->function_results) {
            free(rewritten_functions);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-237: out of memory preparing incremental reallocate");
            return 0;
        }
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            value_ssa_allocation_result_init(&next_result->function_results[function_index]);
            if (!program->functions[function_index].has_body) {
                continue;
            }
            if (rewritten_functions[function_index]) {
                if (!value_ssa_allocate_function(&program->functions[function_index],
                        color_budget,
                        &next_result->function_results[function_index],
                        error)) {
                    free(rewritten_functions);
                    value_ssa_program_allocation_result_free(next_result);
                    return 0;
                }
            } else if (!value_ssa_clone_allocation_result(&current_result->function_results[function_index],
                           &next_result->function_results[function_index],
                           error)) {
                free(rewritten_functions);
                value_ssa_program_allocation_result_free(next_result);
                return 0;
            }
        }
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            ValueSsaFunction *function = &program->functions[function_index];

            if (!function->has_body || !rewritten_functions[function_index]) {
                continue;
            }
            if (value_ssa_alloc_skip_call_heavy_rewrite_rollback_enabled()) {
                continue;
            }
            if (!value_ssa_alloc_function_has_call_instruction(function)) {
                continue;
            }
            if (function->local_count <= spill_local_floors[function_index]) {
                continue;
            }
            if (!value_ssa_alloc_function_has_new_spill_local_loads(
                    function, spill_local_floors[function_index])) {
                continue;
            }
            if (current_result->function_results[function_index].value_count != function->next_value_id) {
                continue;
            }
            if (!value_ssa_alloc_function_spill_shape_differs(
                    &current_result->function_results[function_index],
                    &next_result->function_results[function_index])) {
                continue;
            }
            value_ssa_allocation_result_free(&next_result->function_results[function_index]);
            value_ssa_allocation_result_init(&next_result->function_results[function_index]);
            if (!value_ssa_clone_allocation_result(&current_result->function_results[function_index],
                    &next_result->function_results[function_index],
                    error)) {
                free(rewritten_functions);
                value_ssa_program_allocation_result_free(next_result);
                return 0;
            }
        }
    }
    if (trace_timing) {
        size_t reallocated_function_count = 0;
        for (function_index = 0; function_index < program->function_count; ++function_index) {
            if (rewritten_functions[function_index]) {
                ++reallocated_function_count;
            }
        }
        fprintf(stderr,
            "[value-ssa-alloc-timing] rewrite-loop reallocated-functions=%zu/%zu\n",
            reallocated_function_count,
            program->function_count);
        value_ssa_alloc_trace_timing("rewrite-loop reallocate", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }
    if (out_stats) {
        ++out_stats->allocation_rounds;
    }

    if (!spill_rewrote_any) {
        if (trace_timing) {
            value_ssa_alloc_trace_timing("rewrite-loop split-only-converge", value_ssa_alloc_now_s() - stage_start);
            value_ssa_alloc_trace_timing("rewrite-loop total", value_ssa_alloc_now_s() - total_start);
        }
        free(rewritten_functions);
        value_ssa_program_allocation_result_free(current_result);
        *current_result = *next_result;
        value_ssa_program_allocation_result_init(next_result);
        out_result->reallocated_after_rewrite = 1u;
        out_result->status = VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_CONVERGED;
        return 1;
    }

    if (value_ssa_program_allocation_result_equivalent_ignoring_names(current_result, next_result)) {
        if (trace_timing) {
            value_ssa_alloc_trace_timing("rewrite-loop equivalence-check", value_ssa_alloc_now_s() - stage_start);
            value_ssa_alloc_trace_timing("rewrite-loop total", value_ssa_alloc_now_s() - total_start);
        }
        free(rewritten_functions);
        value_ssa_program_allocation_result_free(current_result);
        *current_result = *next_result;
        value_ssa_program_allocation_result_init(next_result);
        out_result->reallocated_after_rewrite = 1u;
        out_result->status = VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_CONVERGED;
        return 1;
    }

    if (trace_timing) {
        value_ssa_alloc_trace_timing("rewrite-loop equivalence-check", value_ssa_alloc_now_s() - stage_start);
        value_ssa_alloc_trace_timing("rewrite-loop total", value_ssa_alloc_now_s() - total_start);
    }
    free(rewritten_functions);
    out_result->reallocated_after_rewrite = 1u;
    out_result->status = VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_run_pipeline_step(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaCopyAffinityGraph *affinity,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t color_budget,
    ValueSsaAllocatorPlan *plan,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationPipelineStepResult *out_result,
    ValueSsaError *error) {
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (out_result) {
        value_ssa_alloc_pipeline_step_result_init(out_result);
    }
    if (!function || !prep || !worklist || !interference || !affinity || !plan || !result || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-192: invalid pipeline-step contract");
        return 0;
    }

    if (!value_ssa_compute_allocator_plan(
            function, prep, worklist, interference, affinity, coalesce_analysis, color_budget, plan, error)) {
        return 0;
    }
    out_result->built_plan = 1u;
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "pipeline-step plan", value_ssa_alloc_now_s() - stage_start);
        stage_start = value_ssa_alloc_now_s();
    }

    if (!value_ssa_alloc_run_execute_step(
            function, prep, coalesce_analysis, interference, plan, color_budget, result, &out_result->execute_step, error)) {
        return 0;
    }
    if (out_result->execute_step.status != VALUE_SSA_ALLOC_EXECUTE_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-191: execute-step ended in unknown state");
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "pipeline-step execute", value_ssa_alloc_now_s() - stage_start);
        value_ssa_alloc_trace_function_timing(function->name, "pipeline-step total", value_ssa_alloc_now_s() - total_start);
    }

    out_result->status = VALUE_SSA_ALLOC_PIPELINE_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_run_pipeline_step_from_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationPipelineStepResult *out_result,
    ValueSsaError *error) {
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;

    if (out_result) {
        value_ssa_alloc_pipeline_step_result_init(out_result);
    }
    if (!function || !prep || !interference || !plan || !result || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-194: invalid pipeline-step-from-plan contract");
        return 0;
    }

    out_result->reused_plan = 1u;
    if (!value_ssa_alloc_run_execute_step(
            function, prep, coalesce_analysis, interference, plan, color_budget, result, &out_result->execute_step, error)) {
        return 0;
    }
    if (out_result->execute_step.status != VALUE_SSA_ALLOC_EXECUTE_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-191: execute-step ended in unknown state");
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_function_timing(function->name, "pipeline-step-from-plan total", value_ssa_alloc_now_s() - total_start);
    }

    out_result->status = VALUE_SSA_ALLOC_PIPELINE_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_run_entry_step(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocationWorklist *worklist,
    const ValueSsaInterferenceGraph *interference,
    const ValueSsaCopyAffinityGraph *affinity,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    int plan_already_built,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationEntryStepResult *out_result,
    ValueSsaError *error) {
    if (out_result) {
        value_ssa_alloc_entry_step_result_init(out_result);
    }
    if (!function || !prep || !interference || !result || !out_result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-195: invalid entry-step contract");
        return 0;
    }
    if (!value_ssa_alloc_prepare_result(result, function->next_value_id, color_budget, error)) {
        return 0;
    }

    if (plan_already_built) {
        if (!value_ssa_alloc_run_pipeline_step_from_plan(function,
                prep,
                interference,
                coalesce_analysis,
                plan,
                color_budget,
                result,
                &out_result->pipeline_step,
                error)) {
            value_ssa_allocation_result_free(result);
            return 0;
        }
    } else {
        if (!worklist || !affinity || !plan) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-196: missing artifacts for full entry-step");
            value_ssa_allocation_result_free(result);
            return 0;
        }
        if (!value_ssa_alloc_run_pipeline_step(function,
                prep,
                worklist,
                interference,
                affinity,
                coalesce_analysis,
                color_budget,
                (ValueSsaAllocatorPlan *)plan,
                result,
                &out_result->pipeline_step,
                error)) {
            value_ssa_allocation_result_free(result);
            return 0;
        }
    }

    if (out_result->pipeline_step.status != VALUE_SSA_ALLOC_PIPELINE_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-193: pipeline-step ended in unknown state");
        value_ssa_allocation_result_free(result);
        return 0;
    }

    out_result->status = VALUE_SSA_ALLOC_ENTRY_STEP_COMPLETED;
    return 1;
}

int value_ssa_allocate_function(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaError *error) {
    ValueSsaAllocationOwnedArtifacts artifacts;
    ValueSsaAllocationFullStepResult full_step;
    ValueSsaAllocationEntryStepResult fallback_entry;
    ValueSsaAllocatorCoalesceAnalysis identity_coalesce;
    int ok = 0;

    if (!function || !result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-007: invalid allocation contract");
        return 0;
    }

    value_ssa_alloc_owned_artifacts_init(&artifacts);
    value_ssa_alloc_full_step_result_init(&full_step);
    value_ssa_alloc_entry_step_result_init(&fallback_entry);
    value_ssa_allocator_coalesce_analysis_init(&identity_coalesce);

    if (value_ssa_alloc_function_is_large_straight_line_chain_hotspot(function)) {
        if (!value_ssa_compute_def_use_analysis(function, &artifacts.def_use, error) ||
            !value_ssa_alloc_try_large_straight_line_chain_fast_allocate(
                function, &artifacts.def_use, color_budget, result, error)) {
            goto cleanup;
        }
        ok = 1;
        goto cleanup;
    }

    if (!value_ssa_alloc_run_full_step(function,
            color_budget,
            &artifacts,
            result,
            &full_step,
            error)) {
        goto cleanup;
    }
    if (full_step.status != VALUE_SSA_ALLOC_FULL_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-201: full-step ended in unknown state");
        goto cleanup;
    }
    if (value_ssa_alloc_result_has_interference_color_conflict(&artifacts.interference, result)) {
        value_ssa_allocation_result_free(result);
        value_ssa_allocator_plan_free(&artifacts.plan);
        value_ssa_allocator_plan_init(&artifacts.plan);
        if (!value_ssa_alloc_build_identity_coalesce_analysis(
                function->next_value_id, &identity_coalesce, error) ||
            !value_ssa_alloc_run_entry_step(function,
                &artifacts.prep,
                &artifacts.worklist,
                &artifacts.interference,
                &artifacts.affinity,
                &identity_coalesce,
                &artifacts.plan,
                color_budget,
                0,
                result,
                &fallback_entry,
                error) ||
            !value_ssa_alloc_entry_step_completed_or_cleanup(NULL, &fallback_entry, error) ||
            value_ssa_alloc_result_has_interference_color_conflict(&artifacts.interference, result)) {
            if (!error || error->message[0] == '\0') {
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-245: allocation fallback still produced an interference color conflict");
            }
            goto cleanup;
        }
    }
    if (value_ssa_alloc_result_has_interference_color_conflict(&artifacts.interference, result)) {
        if (!value_ssa_alloc_force_spill_interference_color_conflicts(
                &artifacts.prep, &artifacts.interference, result, error) ||
            value_ssa_alloc_result_has_interference_color_conflict(&artifacts.interference, result)) {
            if (!error || error->message[0] == '\0') {
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-247: forced conflict spilling still left an interference color conflict");
            }
            goto cleanup;
        }
    }

    ok = 1;

cleanup:
    value_ssa_allocator_coalesce_analysis_free(&identity_coalesce);
    value_ssa_alloc_owned_artifacts_free(&artifacts);
    if (!ok) {
        value_ssa_allocation_result_free(result);
    }
    return ok;
}

int value_ssa_allocate_function_from_plan(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaInterferenceGraph *interference_graph,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    ValueSsaAllocationResult *result,
    ValueSsaError *error) {
    ValueSsaAllocationEntryStepResult entry_step;

    if (!function || !prep || !interference_graph || !plan || !result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-098: invalid artifact-driven allocation contract");
        return 0;
    }
    value_ssa_alloc_entry_step_result_init(&entry_step);
    if (!value_ssa_alloc_run_entry_step(function,
            prep,
            NULL,
            interference_graph,
            NULL,
            coalesce_analysis,
            plan,
            color_budget,
            1,
            result,
            &entry_step,
            error)) {
        return 0;
    }
    if (!value_ssa_alloc_entry_step_completed_or_cleanup(result, &entry_step, error)) {
        return 0;
    }
    return 1;
}

int value_ssa_allocate_program(const ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *result,
    ValueSsaError *error) {
    ValueSsaAllocationProgramStepResult program_step;
    size_t function_index;

    if (!program || !result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-013: invalid program allocation contract");
        return 0;
    }

    value_ssa_alloc_program_step_result_init(&program_step);
    value_ssa_program_allocation_result_free(result);
    result->function_count = program->function_count;
    if (program->function_count == 0) {
        program_step.status = VALUE_SSA_ALLOC_PROGRAM_STEP_COMPLETED;
        if (!value_ssa_alloc_program_step_completed_or_cleanup(result, &program_step, error)) {
            return 0;
        }
        return 1;
    }

    result->function_results =
        (ValueSsaAllocationResult *)calloc(program->function_count, sizeof(ValueSsaAllocationResult));
    if (!result->function_results) {
        value_ssa_program_allocation_result_init(result);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-014: out of memory preparing program allocation");
        return 0;
    }
    program_step.allocated_function_result_count = program->function_count;

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        value_ssa_allocation_result_init(&result->function_results[function_index]);
        if (program->functions[function_index].has_body &&
            !value_ssa_allocate_function(
                &program->functions[function_index], color_budget, &result->function_results[function_index], error)) {
            value_ssa_program_allocation_result_free(result);
            return 0;
        }
        ++program_step.completed_function_count;
    }

    program_step.status = VALUE_SSA_ALLOC_PROGRAM_STEP_COMPLETED;
    if (!value_ssa_alloc_program_step_completed_or_cleanup(result, &program_step, error)) {
        return 0;
    }
    return 1;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *result,
    ValueSsaAllocateRewriteStats *out_stats,
    ValueSsaError *error) {
    ValueSsaProgramAllocationResult current_result;
    ValueSsaProgramAllocationResult next_result;
    ValueSsaAllocationRewriteLoopStepResult loop_step;
    size_t *spill_local_floors = NULL;
    size_t iteration_count = 0;
    int trace_timing = value_ssa_alloc_trace_enabled();
    double total_start = trace_timing ? value_ssa_alloc_now_s() : 0.0;
    double stage_start = total_start;

    if (!program || !result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-043: invalid allocate+rewrite contract");
        return 0;
    }

    value_ssa_allocate_rewrite_stats_init(out_stats);
    value_ssa_program_allocation_result_init(&current_result);
    value_ssa_program_allocation_result_init(&next_result);
    value_ssa_alloc_rewrite_loop_step_result_init(&loop_step);
    if (!value_ssa_allocate_program(program, color_budget, &current_result, error)) {
        return 0;
    }
    if (trace_timing) {
        value_ssa_alloc_trace_timing("initial-allocate", value_ssa_alloc_now_s() - stage_start);
    }
    if (out_stats) {
        out_stats->allocation_rounds = 1;
    }
    spill_local_floors = (size_t *)malloc((program->function_count ? program->function_count : 1) * sizeof(size_t));
    if (!spill_local_floors) {
        value_ssa_program_allocation_result_free(&current_result);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-047: out of memory preparing allocate+rewrite loop");
        return 0;
    }

    for (;;) {
        if (!value_ssa_alloc_run_rewrite_loop_step(program,
                color_budget,
                &current_result,
                &next_result,
                spill_local_floors,
                iteration_count,
                out_stats,
                &loop_step,
                error)) {
            free(spill_local_floors);
            value_ssa_program_allocation_result_free(&next_result);
            value_ssa_program_allocation_result_free(&current_result);
            return 0;
        }
        iteration_count = loop_step.completed_iteration_count;
        if (loop_step.status == VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_CONVERGED) {
            break;
        }
        if (loop_step.status != VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_COMPLETED || !loop_step.reallocated_after_rewrite) {
            free(spill_local_floors);
            value_ssa_program_allocation_result_free(&next_result);
            value_ssa_program_allocation_result_free(&current_result);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-206: invalid allocate+rewrite loop-step result");
            return 0;
        }

        value_ssa_program_allocation_result_free(&current_result);
        current_result = next_result;
        value_ssa_program_allocation_result_init(&next_result);
    }

    free(spill_local_floors);
    value_ssa_program_allocation_result_free(result);
    *result = current_result;
    if (trace_timing) {
        value_ssa_alloc_trace_timing("total", value_ssa_alloc_now_s() - total_start);
    }
    return 1;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationResult *result,
    ValueSsaError *error) {
    return value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
        program, color_budget, result, NULL, error);
}

#endif
