#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "ir.h"
#include "lexer.h"
#include "lower_ir.h"
#include "parser.h"
#include "semantic.h"
#include "value_ssa.h"
#include "value_ssa_alloc.h"
#include "value_ssa_pass.h"

#define COLOR_BUDGET 8u

static double now_s(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long n;
    char *b;

    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (char *)malloc((size_t)n + 1u);
    if (!b) {
        fclose(f);
        return NULL;
    }
    if (n > 0) {
        fread(b, 1, (size_t)n, f);
    }
    b[n] = '\0';
    fclose(f);
    return b;
}

int main(int argc, char **argv) {
    char *src = NULL;
    TokenArray tokens;
    AstProgram ast;
    ParserError pe;
    SemanticError se;
    SemanticOptions so;
    IrProgram ir;
    IrError ie;
    IrLowerOptions io;
    LowerIrProgram lower;
    LowerIrError le;
    LowerIrOptions lo;
    ValueSsaProgram value;
    ValueSsaFunction *function = NULL;
    ValueSsaDefUseAnalysis def_use;
    ValueSsaLivenessAnalysis liveness;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorMoveFamilyAnalysis move_family;
    ValueSsaAllocatorMoveWorklist move_worklist;
    ValueSsaAllocatorPlan plan;
    ValueSsaError ve;
    double t0;
    double t1;
    int ok = 0;

    if (argc != 2) {
        return 2;
    }

    lexer_init_tokens(&tokens);
    ast_program_init(&ast);
    ir_program_init(&ir);
    lower_ir_program_init(&lower);
    value_ssa_program_init(&value);
    value_ssa_def_use_analysis_init(&def_use);
    value_ssa_liveness_analysis_init(&liveness);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_move_family_analysis_init(&move_family);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    value_ssa_allocator_plan_init(&plan);
    memset(&pe, 0, sizeof(pe));
    memset(&se, 0, sizeof(se));
    memset(&so, 0, sizeof(so));
    memset(&ie, 0, sizeof(ie));
    memset(&io, 0, sizeof(io));
    memset(&le, 0, sizeof(le));
    memset(&lo, 0, sizeof(lo));
    memset(&ve, 0, sizeof(ve));

    so.skip_all_paths_return_check = 1;
    io.allow_implicit_fallthrough_return = 1;
    lo.allow_implicit_fallthrough_return = 1;

    src = read_file(argv[1]);
    if (!src) {
        ok = 3;
        goto cleanup;
    }

    if (!lexer_tokenize(src, &tokens) ||
        !parser_parse_translation_unit_ast(&tokens, &ast, &pe) ||
        !semantic_analyze_program_with_options(&ast, &so, &se) ||
        !ir_lower_program(&ast, &io, &ir, &ie) ||
        !lower_ir_lower_from_ir(&ir, &lo, &lower, &le) ||
        !value_ssa_build_default_from_lower_ir(&lower, &value, &ve)) {
        ok = 1;
        goto cleanup;
    }

    if (value.function_count == 0) {
        ok = 4;
        goto cleanup;
    }

    for (size_t function_index = 0; function_index < value.function_count; ++function_index) {
        if (value.functions[function_index].has_body && value.functions[function_index].block_count > 0) {
            function = &value.functions[function_index];
            break;
        }
    }
    if (!function) {
        ok = 5;
        goto cleanup;
    }

    t0 = now_s();
    if (!value_ssa_compute_def_use_analysis(function, &def_use, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("def_use %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_liveness_analysis(function, NULL, &liveness, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("liveness %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_interference_graph(function, NULL, &liveness, &interference, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("interference %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_copy_affinity_graph(function, &interference, &affinity, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("affinity %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_allocation_prep(function, &def_use, &liveness, &interference, &affinity, &prep, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("prep %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_allocation_worklist(function, &prep, &worklist, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("worklist %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_allocator_coalesce_analysis(
            function, &prep, &interference, &affinity, COLOR_BUDGET, &coalesce, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("coalesce %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_allocator_move_family_analysis(
            function, &affinity, &coalesce, NULL, COLOR_BUDGET, &move_family, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("move_family %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_allocator_move_worklist(
            function, &move_family, NULL, COLOR_BUDGET, &move_worklist, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("move_worklist %.3f\n", t1 - t0);

    t0 = now_s();
    if (!value_ssa_compute_allocator_plan(
            function, &prep, &worklist, &interference, &affinity, &coalesce, COLOR_BUDGET, &plan, &ve)) {
        ok = 1;
        goto cleanup;
    }
    t1 = now_s();
    printf("plan %.3f\n", t1 - t0);

    ok = 0;

cleanup:
    free(src);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_move_family_analysis_free(&move_family);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_liveness_analysis_free(&liveness);
    value_ssa_def_use_analysis_free(&def_use);
    value_ssa_program_free(&value);
    lower_ir_program_free(&lower);
    ir_program_free(&ir);
    ast_program_free(&ast);
    lexer_free_tokens(&tokens);
    return ok;
}
