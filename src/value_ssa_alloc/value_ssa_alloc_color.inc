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

    if (!value_ssa_compute_cfg_analysis(function, cfg, error)) {
        return 0;
    }
    out_result->built_cfg = 1u;

    if (!value_ssa_compute_liveness_analysis(function, cfg, liveness, error)) {
        return 0;
    }
    out_result->built_liveness = 1u;

    if (!value_ssa_compute_interference_graph(function, cfg, liveness, interference, error)) {
        return 0;
    }
    out_result->built_interference = 1u;

    if (!value_ssa_compute_copy_affinity_graph(function, interference, affinity, error)) {
        return 0;
    }
    out_result->built_affinity = 1u;

    if (!value_ssa_compute_allocation_prep(function, def_use, liveness, interference, affinity, prep, error)) {
        return 0;
    }
    out_result->built_prep = 1u;

    if (!value_ssa_compute_allocation_worklist(function, prep, worklist, error)) {
        return 0;
    }
    out_result->built_worklist = 1u;

    if (!value_ssa_compute_allocator_coalesce_analysis(
            function, prep, interference, affinity, color_budget, coalesce_analysis, error)) {
        return 0;
    }
    out_result->built_coalesce_analysis = 1u;

    out_result->status = VALUE_SSA_ALLOC_BUILD_STEP_COMPLETED;
    return 1;
}

static int value_ssa_alloc_run_full_step(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaAllocationOwnedArtifacts *artifacts,
    ValueSsaAllocationResult *result,
    ValueSsaAllocationFullStepResult *out_result,
    ValueSsaError *error) {
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

    out_result->status = VALUE_SSA_ALLOC_FULL_STEP_COMPLETED;
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
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        spill_local_floors[function_index] = program->functions[function_index].local_count;
    }

    if (!value_ssa_alloc_run_program_rewrite_stage(program,
            current_result,
            spill_local_floors,
            &split_rewrote_any,
            &spill_rewrote_any,
            &rewrote_any,
            &canonicalized_after_rewrite,
            error)) {
        return 0;
    }

    out_result->split_rewrote_any = split_rewrote_any ? 1u : 0u;
    out_result->spill_rewrote_any = spill_rewrote_any ? 1u : 0u;
    out_result->rewrote_any = rewrote_any ? 1u : 0u;
    out_result->canonicalized_after_rewrite = canonicalized_after_rewrite ? 1u : 0u;
    if (!rewrote_any) {
        out_result->status = VALUE_SSA_ALLOC_REWRITE_LOOP_STEP_CONVERGED;
        return 1;
    }

    if (out_stats) {
        ++out_stats->rewrite_rounds;
    }
    if (!value_ssa_allocate_program(program, color_budget, next_result, error)) {
        return 0;
    }
    if (out_stats) {
        ++out_stats->allocation_rounds;
    }

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

    if (!value_ssa_alloc_run_execute_step(
            function, prep, coalesce_analysis, interference, plan, color_budget, result, &out_result->execute_step, error)) {
        return 0;
    }
    if (out_result->execute_step.status != VALUE_SSA_ALLOC_EXECUTE_STEP_COMPLETED) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-191: execute-step ended in unknown state");
        return 0;
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
    int ok = 0;

    if (!function || !result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-007: invalid allocation contract");
        return 0;
    }

    value_ssa_alloc_owned_artifacts_init(&artifacts);
    value_ssa_alloc_full_step_result_init(&full_step);

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

    ok = 1;

cleanup:
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
