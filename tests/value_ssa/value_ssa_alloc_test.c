#include "value_ssa_alloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int build_alloc_smoke_program(ValueSsaProgram *program, ValueSsaError *error);
static int build_alloc_spill_program(ValueSsaProgram *program, ValueSsaError *error);
static void clear_retry_phase_entry_item(ValueSsaAllocatorRetryFamilyAgendaItem *entry);

static int expect_alloc_dump(const char *case_id,
    const ValueSsaFunction *function,
    const ValueSsaAllocationResult *result,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocation_result(function, result, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_alloc_results_equal_by_dump(const char *case_id,
    const ValueSsaFunction *function,
    const ValueSsaAllocationResult *lhs,
    const ValueSsaAllocationResult *rhs) {
    char *lhs_text = NULL;
    char *rhs_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocation_result(function, lhs, &lhs_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s lhs dump failed: %s\n", case_id, error.message);
        return 0;
    }
    if (!value_ssa_dump_allocation_result(function, rhs, &rhs_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s rhs dump failed: %s\n", case_id, error.message);
        free(lhs_text);
        return 0;
    }

    if (strcmp(lhs_text, rhs_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocation results differ\nlhs:\n%s\nrhs:\n%s\n",
            case_id,
            lhs_text,
            rhs_text);
        ok = 0;
    }

    free(rhs_text);
    free(lhs_text);
    return ok;
}

static int expect_program_alloc_dump(const char *case_id,
    const ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_program_allocation_result(program, result, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s program dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s program dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_function_layout_dump(const char *case_id,
    const ValueSsaFunction *function,
    const ValueSsaFunctionAllocationLayout *layout,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_function_allocation_layout(function, layout, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s function layout dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s function layout dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_program_layout_dump(const char *case_id,
    const ValueSsaProgram *program,
    const ValueSsaProgramAllocationLayout *layout,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_program_allocation_layout(program, layout, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s program layout dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s program layout dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_allocate_rewrite_layout_report(const char *case_id,
    const ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocateRewriteStats *stats,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocate_rewrite_layout_report(program, result, stats, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocate+rewrite layout report failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocate+rewrite layout report mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_allocate_rewrite_layout_report_artifact(const char *case_id,
    const ValueSsaAllocateRewriteLayoutReport *report,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocate_rewrite_layout_report_artifact(report, &actual_text, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocate+rewrite layout report artifact dump failed: %s\n",
            case_id,
            error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocate+rewrite layout report artifact mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_program_layouts_equivalent_ignoring_names(const char *case_id,
    const ValueSsaProgramAllocationLayout *lhs,
    const ValueSsaProgramAllocationLayout *rhs) {
    if (!value_ssa_program_allocation_layout_equivalent_ignoring_names(lhs, rhs)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s program layout semantic equivalence failed\n", case_id);
        return 0;
    }

    return 1;
}

static int expect_allocate_rewrite_layout_reports_equivalent_ignoring_names(const char *case_id,
    const ValueSsaAllocateRewriteLayoutReport *lhs,
    const ValueSsaAllocateRewriteLayoutReport *rhs) {
    if (!value_ssa_allocate_rewrite_layout_report_equivalent_ignoring_names(lhs, rhs)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocate+rewrite layout report semantic equivalence failed\n",
            case_id);
        return 0;
    }

    return 1;
}

static int expect_function_layouts_equivalent_ignoring_names(const char *case_id,
    const ValueSsaFunctionAllocationLayout *lhs,
    const ValueSsaFunctionAllocationLayout *rhs) {
    if (!value_ssa_function_allocation_layout_equivalent_ignoring_names(lhs, rhs)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s function layout semantic equivalence failed\n", case_id);
        return 0;
    }

    return 1;
}

static int expect_layout_value_list(const char *case_id,
    const size_t *actual_values,
    size_t actual_count,
    const size_t *expected_values,
    size_t expected_count,
    const char *label) {
    size_t index;

    if (actual_count != expected_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s %s count mismatch expected=%zu actual=%zu\n",
            case_id,
            label,
            expected_count,
            actual_count);
        return 0;
    }
    for (index = 0; index < expected_count; ++index) {
        if (!actual_values || actual_values[index] != expected_values[index]) {
            fprintf(stderr,
                "[value-ssa-alloc] FAIL: %s %s[%zu] mismatch expected=%zu actual=%zu\n",
                case_id,
                label,
                index,
                expected_values[index],
                actual_values ? actual_values[index] : (size_t)-1);
            return 0;
        }
    }

    return 1;
}

static int expect_layout_color_group(const char *case_id,
    const ValueSsaFunctionAllocationLayout *layout,
    size_t color,
    const size_t *expected_values,
    size_t expected_count) {
    const ValueSsaAllocationColorGroup *group = NULL;

    if (!value_ssa_function_allocation_layout_get_color_group(layout, color, &group) || !group) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s missing color-group %zu\n", case_id, color);
        return 0;
    }
    return expect_layout_value_list(case_id, group->value_ids, group->value_count, expected_values, expected_count, "color-group");
}

static int expect_layout_spill_slot_group(const char *case_id,
    const ValueSsaFunctionAllocationLayout *layout,
    size_t spill_slot,
    const size_t *expected_values,
    size_t expected_count) {
    const ValueSsaAllocationSpillSlotGroup *group = NULL;

    if (!value_ssa_function_allocation_layout_get_spill_slot_group(layout, spill_slot, &group) || !group) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s missing spill-slot-group %zu\n", case_id, spill_slot);
        return 0;
    }
    return expect_layout_value_list(
        case_id, group->value_ids, group->value_count, expected_values, expected_count, "spill-slot-group");
}

static int expect_layout_color_group_at(const char *case_id,
    const ValueSsaFunctionAllocationLayout *layout,
    size_t group_index,
    size_t expected_color,
    const size_t *expected_values,
    size_t expected_count) {
    const ValueSsaAllocationColorGroup *group = NULL;

    if (!value_ssa_function_allocation_layout_get_color_group_at(layout, group_index, &group) || !group) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s missing color-group index %zu\n", case_id, group_index);
        return 0;
    }
    if (group->color != expected_color) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s color-group[%zu] expected color %zu got %zu\n",
            case_id,
            group_index,
            expected_color,
            group->color);
        return 0;
    }
    return expect_layout_value_list(case_id, group->value_ids, group->value_count, expected_values, expected_count, "color-group-at");
}

static int expect_layout_spill_slot_group_at(const char *case_id,
    const ValueSsaFunctionAllocationLayout *layout,
    size_t group_index,
    size_t expected_spill_slot,
    const size_t *expected_values,
    size_t expected_count) {
    const ValueSsaAllocationSpillSlotGroup *group = NULL;

    if (!value_ssa_function_allocation_layout_get_spill_slot_group_at(layout, group_index, &group) || !group) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s missing spill-slot-group index %zu\n", case_id, group_index);
        return 0;
    }
    if (group->spill_slot != expected_spill_slot) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill-slot-group[%zu] expected slot %zu got %zu\n",
            case_id,
            group_index,
            expected_spill_slot,
            group->spill_slot);
        return 0;
    }
    return expect_layout_value_list(
        case_id, group->value_ids, group->value_count, expected_values, expected_count, "spill-slot-group-at");
}

static int expect_program_function_index_list(const char *case_id,
    const size_t *actual_indices,
    size_t actual_count,
    const size_t *expected_indices,
    size_t expected_count,
    const char *label) {
    return expect_layout_value_list(case_id, actual_indices, actual_count, expected_indices, expected_count, label);
}

static int expect_layout_summary(const char *case_id,
    const ValueSsaProgramAllocationLayout *layout,
    size_t expected_function_count,
    size_t expected_total_value_count,
    size_t expected_total_colored_count,
    size_t expected_total_spilled_count,
    size_t expected_total_optimistic_colored_count,
    size_t expected_total_confirmed_spilled_count,
    size_t expected_total_recovered_colored_count,
    size_t expected_max_color_budget,
    size_t expected_max_spill_slot_count) {
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_colored_count = 0;
    size_t total_spilled_count = 0;
    size_t total_optimistic_colored_count = 0;
    size_t total_confirmed_spilled_count = 0;
    size_t total_recovered_colored_count = 0;
    size_t max_color_budget = 0;
    size_t max_spill_slot_count = 0;

    if (!value_ssa_program_allocation_layout_get_summary(layout,
            &function_count,
            &total_value_count,
            &total_colored_count,
            &total_spilled_count,
            &total_optimistic_colored_count,
            &total_confirmed_spilled_count,
            &total_recovered_colored_count,
            &max_color_budget,
            &max_spill_slot_count)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s program layout summary query failed\n", case_id);
        return 0;
    }
    if (function_count != expected_function_count || total_value_count != expected_total_value_count ||
        total_colored_count != expected_total_colored_count || total_spilled_count != expected_total_spilled_count ||
        total_optimistic_colored_count != expected_total_optimistic_colored_count ||
        total_confirmed_spilled_count != expected_total_confirmed_spilled_count ||
        total_recovered_colored_count != expected_total_recovered_colored_count ||
        max_color_budget != expected_max_color_budget || max_spill_slot_count != expected_max_spill_slot_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s program layout summary mismatch got=(%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu)\n",
            case_id,
            function_count,
            total_value_count,
            total_colored_count,
            total_spilled_count,
            total_optimistic_colored_count,
            total_confirmed_spilled_count,
            total_recovered_colored_count,
            max_color_budget,
            max_spill_slot_count);
        return 0;
    }

    return 1;
}

static int expect_report_summary(const char *case_id,
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t expected_allocation_rounds,
    size_t expected_rewrite_rounds,
    size_t expected_function_count,
    size_t expected_total_value_count,
    size_t expected_total_colored_count,
    size_t expected_total_spilled_count,
    size_t expected_total_optimistic_colored_count,
    size_t expected_total_confirmed_spilled_count,
    size_t expected_total_recovered_colored_count,
    size_t expected_max_color_budget,
    size_t expected_max_spill_slot_count) {
    size_t allocation_rounds = 0;
    size_t rewrite_rounds = 0;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_colored_count = 0;
    size_t total_spilled_count = 0;
    size_t total_optimistic_colored_count = 0;
    size_t total_confirmed_spilled_count = 0;
    size_t total_recovered_colored_count = 0;
    size_t max_color_budget = 0;
    size_t max_spill_slot_count = 0;

    if (!value_ssa_allocate_rewrite_layout_report_get_summary(report,
            &allocation_rounds,
            &rewrite_rounds,
            &function_count,
            &total_value_count,
            &total_colored_count,
            &total_spilled_count,
            &total_optimistic_colored_count,
            &total_confirmed_spilled_count,
            &total_recovered_colored_count,
            &max_color_budget,
            &max_spill_slot_count)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s layout report summary query failed\n", case_id);
        return 0;
    }
    if (allocation_rounds != expected_allocation_rounds || rewrite_rounds != expected_rewrite_rounds ||
        function_count != expected_function_count || total_value_count != expected_total_value_count ||
        total_colored_count != expected_total_colored_count || total_spilled_count != expected_total_spilled_count ||
        total_optimistic_colored_count != expected_total_optimistic_colored_count ||
        total_confirmed_spilled_count != expected_total_confirmed_spilled_count ||
        total_recovered_colored_count != expected_total_recovered_colored_count ||
        max_color_budget != expected_max_color_budget || max_spill_slot_count != expected_max_spill_slot_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s layout report summary mismatch got=(%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu)\n",
            case_id,
            allocation_rounds,
            rewrite_rounds,
            function_count,
            total_value_count,
            total_colored_count,
            total_spilled_count,
            total_optimistic_colored_count,
            total_confirmed_spilled_count,
            total_recovered_colored_count,
            max_color_budget,
            max_spill_slot_count);
        return 0;
    }

    return 1;
}

static int expect_report_function_index_list(const char *case_id,
    const size_t *actual_indices,
    size_t actual_count,
    const size_t *expected_indices,
    size_t expected_count,
    const char *label) {
    return expect_program_function_index_list(case_id, actual_indices, actual_count, expected_indices, expected_count, label);
}

static int expect_function_layout_summary(const char *case_id,
    const ValueSsaFunctionAllocationLayout *layout,
    const char *expected_function_name,
    size_t expected_value_count,
    size_t expected_color_budget,
    size_t expected_spill_slot_count,
    size_t expected_colored_count,
    size_t expected_spilled_count,
    size_t expected_optimistic_colored_count,
    size_t expected_confirmed_spilled_count,
    size_t expected_recovered_colored_count,
    size_t expected_used_color_count,
    size_t expected_color_group_count,
    size_t expected_spill_slot_group_count) {
    const char *function_name = NULL;
    size_t value_count = 0;
    size_t color_budget = 0;
    size_t spill_slot_count = 0;
    size_t colored_count = 0;
    size_t spilled_count = 0;
    size_t optimistic_colored_count = 0;
    size_t confirmed_spilled_count = 0;
    size_t recovered_colored_count = 0;
    size_t used_color_count = 0;
    size_t color_group_count = 0;
    size_t spill_slot_group_count = 0;

    if (!value_ssa_function_allocation_layout_get_summary(layout,
            &function_name,
            &value_count,
            &color_budget,
            &spill_slot_count,
            &colored_count,
            &spilled_count,
            &optimistic_colored_count,
            &confirmed_spilled_count,
            &recovered_colored_count,
            &used_color_count,
            &color_group_count,
            &spill_slot_group_count)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s function summary query failed\n", case_id);
        return 0;
    }
    if (((expected_function_name == NULL) != (function_name == NULL)) ||
        (expected_function_name && strcmp(expected_function_name, function_name) != 0) ||
        value_count != expected_value_count || color_budget != expected_color_budget ||
        spill_slot_count != expected_spill_slot_count || colored_count != expected_colored_count ||
        spilled_count != expected_spilled_count ||
        optimistic_colored_count != expected_optimistic_colored_count ||
        confirmed_spilled_count != expected_confirmed_spilled_count ||
        recovered_colored_count != expected_recovered_colored_count ||
        used_color_count != expected_used_color_count ||
        color_group_count != expected_color_group_count ||
        spill_slot_group_count != expected_spill_slot_group_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s function summary mismatch\n",
            case_id);
        return 0;
    }

    return 1;
}

static int build_alloc_two_function_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaProgram smoke_program;
    ValueSsaProgram spill_program;
    ValueSsaError local_error;

    value_ssa_program_init(program);
    value_ssa_program_init(&smoke_program);
    value_ssa_program_init(&spill_program);
    if (!build_alloc_smoke_program(&smoke_program, &local_error) ||
        !build_alloc_spill_program(&spill_program, &local_error)) {
        if (error) {
            *error = local_error;
        }
        value_ssa_program_free(&spill_program);
        value_ssa_program_free(&smoke_program);
        value_ssa_program_free(program);
        return 0;
    }

    *program = smoke_program;
    {
        ValueSsaFunction *grown_functions =
            (ValueSsaFunction *)realloc(program->functions, 2 * sizeof(ValueSsaFunction));
        if (!grown_functions) {
            value_ssa_program_free(&spill_program);
            value_ssa_program_free(program);
            return 0;
        }
        program->functions = grown_functions;
    }
    if (!program->functions) {
        value_ssa_program_free(&spill_program);
        value_ssa_program_free(program);
        return 0;
    }
    program->functions[1] = spill_program.functions[0];
    program->function_count = 2;
    program->function_capacity = 2;
    spill_program.functions = NULL;
    spill_program.function_count = 0;
    spill_program.function_capacity = 0;
    value_ssa_program_free(&spill_program);
    return 1;
}

static int prepare_manual_two_function_program_result(ValueSsaProgramAllocationResult *result,
    const ValueSsaProgram *program) {
    size_t function_index;
    size_t value_index;

    if (!result || !program || program->function_count != 2) {
        return 0;
    }

    value_ssa_program_allocation_result_free(result);
    result->function_count = 2;
    result->function_results = (ValueSsaAllocationResult *)calloc(2, sizeof(ValueSsaAllocationResult));
    if (!result->function_results) {
        return 0;
    }

    for (function_index = 0; function_index < 2; ++function_index) {
        size_t value_count = program->functions[function_index].next_value_id;

        value_ssa_allocation_result_init(&result->function_results[function_index]);
        result->function_results[function_index].value_count = value_count;
        result->function_results[function_index].color_budget = 2;
        result->function_results[function_index].colors = (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].has_color = (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_intended_flags =
            (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_confirmed_flags =
            (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_recovered_flags =
            (unsigned char *)calloc(value_count, sizeof(unsigned char));
        result->function_results[function_index].spill_recovery_orders =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_ids =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_member_orders =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_member_counts =
            (size_t *)malloc(value_count * sizeof(size_t));
        result->function_results[function_index].spill_recovery_phase_entries =
            (ValueSsaAllocatorRetryFamilyAgendaItem *)malloc(value_count * sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
        result->function_results[function_index].spill_priorities = (size_t *)calloc(value_count, sizeof(size_t));
        result->function_results[function_index].spill_slots = (size_t *)malloc(value_count * sizeof(size_t));
        if (!result->function_results[function_index].colors || !result->function_results[function_index].has_color ||
            !result->function_results[function_index].spill_flags ||
            !result->function_results[function_index].spill_intended_flags ||
            !result->function_results[function_index].spill_confirmed_flags ||
            !result->function_results[function_index].spill_recovered_flags ||
            !result->function_results[function_index].spill_recovery_orders ||
            !result->function_results[function_index].spill_recovery_phase_ids ||
            !result->function_results[function_index].spill_recovery_phase_member_orders ||
            !result->function_results[function_index].spill_recovery_phase_member_counts ||
            !result->function_results[function_index].spill_recovery_phase_entries ||
            !result->function_results[function_index].spill_priorities ||
            !result->function_results[function_index].spill_slots) {
            value_ssa_program_allocation_result_free(result);
            return 0;
        }

        for (value_index = 0; value_index < value_count; ++value_index) {
            result->function_results[function_index].colors[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_orders[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_phase_ids[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_phase_member_orders[value_index] = (size_t)-1;
            result->function_results[function_index].spill_recovery_phase_member_counts[value_index] = (size_t)-1;
            clear_retry_phase_entry_item(&result->function_results[function_index].spill_recovery_phase_entries[value_index]);
            result->function_results[function_index].spill_slots[value_index] = (size_t)-1;
        }
    }

    /* Function 0: all colored in color 0. */
    for (value_index = 0; value_index < result->function_results[0].value_count; ++value_index) {
        result->function_results[0].has_color[value_index] = 1;
        result->function_results[0].colors[value_index] = 0;
    }
    result->function_results[0].spill_slot_count = 0;

    /* Function 1: one confirmed spill, one optimistic colored, one recovered colored. */
    for (value_index = 1; value_index < result->function_results[1].value_count; ++value_index) {
        result->function_results[1].has_color[value_index] = 1;
        result->function_results[1].colors[value_index] = value_index == 2 ? 1 : 0;
    }
    result->function_results[1].spill_flags[0] = 1;
    result->function_results[1].spill_confirmed_flags[0] = 1;
    result->function_results[1].spill_slots[0] = 0;
    result->function_results[1].spill_slot_count = 1;
    result->function_results[1].spill_intended_flags[2] = 1;
    result->function_results[1].spill_intended_flags[4] = 1;
    result->function_results[1].spill_recovered_flags[4] = 1;

    return 1;
}

static int expect_allocate_rewrite_stats_dump(const char *case_id,
    const ValueSsaAllocateRewriteStats *stats,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocate_rewrite_stats(stats, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s stats dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s stats dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_select_stats_dump(const char *case_id,
    const ValueSsaAllocationSelectStats *stats,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocation_select_stats(stats, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s select stats dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s select stats dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_select_trace_contains(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    const char *needle_a,
    const char *needle_b) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocation_select_trace(plan, result, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s select trace dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if ((needle_a && strstr(actual_text, needle_a) == NULL) ||
        (needle_b && strstr(actual_text, needle_b) == NULL)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s select trace missing expected text\nneedles:\n%s\n%s\nactual:\n%s\n",
            case_id,
            needle_a ? needle_a : "<none>",
            needle_b ? needle_b : "<none>",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_allocator_plan_dump(const char *case_id,
    const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *prep,
    const ValueSsaAllocatorPlan *plan,
    size_t color_budget,
    const char *expected_text) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocator_plan(function, prep, plan, color_budget, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if (strcmp(actual_text, expected_text) != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocator-plan dump mismatch\nexpected:\n%s\nactual:\n%s\n",
            case_id,
            expected_text,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_coalesce_dump_contains(const char *case_id,
    const ValueSsaFunction *function,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t color_budget,
    const char *needle_a,
    const char *needle_b) {
    char *actual_text = NULL;
    ValueSsaError error;
    int ok = 1;

    if (!value_ssa_dump_allocator_coalesce_analysis(function, analysis, color_budget, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s coalesce dump failed: %s\n", case_id, error.message);
        return 0;
    }

    if ((needle_a && strstr(actual_text, needle_a) == NULL) ||
        (needle_b && strstr(actual_text, needle_b) == NULL)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s coalesce dump missing expected text\nneedles:\n%s\n%s\nactual:\n%s\n",
            case_id,
            needle_a ? needle_a : "<none>",
            needle_b ? needle_b : "<none>",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    return ok;
}

static int expect_has_color(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected) {
    int actual;

    if (!result || value_id >= result->value_count) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid color query\n", case_id);
        return 0;
    }

    actual = (int)result->has_color[value_id];
    if (actual != expected) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected has_color(ssa.%zu) = %d, got %d\n",
            case_id,
            value_id,
            expected,
            actual);
        return 0;
    }

    return 1;
}

static int expect_spill_flag(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected) {
    int actual;

    if (!result || value_id >= result->value_count) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid spill query\n", case_id);
        return 0;
    }

    actual = (int)result->spill_flags[value_id];
    if (actual != expected) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill(ssa.%zu) = %d, got %d\n",
            case_id,
            value_id,
            expected,
            actual);
        return 0;
    }

    return 1;
}

static int expect_spill_count_on_values(const char *case_id,
    const ValueSsaAllocationResult *result,
    const size_t *value_ids,
    size_t value_count,
    size_t expected_count) {
    size_t index;
    size_t actual_count = 0;

    if (!result || (value_count > 0 && !value_ids)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid spill-count query\n", case_id);
        return 0;
    }

    for (index = 0; index < value_count; ++index) {
        if (value_ids[index] >= result->value_count) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid spill-count value id\n", case_id);
            return 0;
        }
        if (result->spill_flags[value_ids[index]]) {
            ++actual_count;
        }
    }

    if (actual_count != expected_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill count %zu, got %zu\n",
            case_id,
            expected_count,
            actual_count);
        return 0;
    }

    return 1;
}

static int expect_query_color(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_color,
    size_t expected_color) {
    int has_color = 0;
    size_t color = 0;

    if (!value_ssa_allocation_result_get_color(result, value_id, &has_color, &color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s color query failed\n", case_id);
        return 0;
    }
    if (has_color != expected_has_color || (has_color && color != expected_color)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected color query (%d,%zu), got (%d,%zu)\n",
            case_id,
            expected_has_color,
            expected_color,
            has_color,
            color);
        return 0;
    }

    return 1;
}

static int expect_query_spill(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_is_spilled) {
    int is_spilled = 0;

    if (!value_ssa_allocation_result_is_spilled(result, value_id, &is_spilled)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s spill query failed\n", case_id);
        return 0;
    }
    if (is_spilled != expected_is_spilled) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill query %d, got %d\n",
            case_id,
            expected_is_spilled,
            is_spilled);
        return 0;
    }

    return 1;
}

static int expect_query_spill_intent(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_is_intended,
    int expected_is_confirmed) {
    int is_intended = 0;
    int is_confirmed = 0;

    if (!value_ssa_allocation_result_is_spill_intended(result, value_id, &is_intended) ||
        !value_ssa_allocation_result_is_spill_confirmed(result, value_id, &is_confirmed)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s spill intent query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (is_intended != expected_is_intended || is_confirmed != expected_is_confirmed) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill intent(ssa.%zu)=(%d,%d), got (%d,%d)\n",
            case_id,
            value_id,
            expected_is_intended,
            expected_is_confirmed,
            is_intended,
            is_confirmed);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovered(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_is_recovered) {
    int is_recovered = 0;

    if (!value_ssa_allocation_result_is_spill_recovered(result, value_id, &is_recovered)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s spill recovered query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (is_recovered != expected_is_recovered) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovered(ssa.%zu)=%d, got %d\n",
            case_id,
            value_id,
            expected_is_recovered,
            is_recovered);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovery_order(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_order,
    size_t expected_order) {
    int has_recovery_order = 0;
    size_t recovery_order = 0;

    if (!value_ssa_allocation_result_get_spill_recovery_order(
            result, value_id, &has_recovery_order, &recovery_order)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s spill recovery-order query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (has_recovery_order != expected_has_order || (expected_has_order && recovery_order != expected_order)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovery-order(ssa.%zu)=(%d,%zu), got (%d,%zu)\n",
            case_id,
            value_id,
            expected_has_order,
            expected_order,
            has_recovery_order,
            recovery_order);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovery_phase(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_phase,
    size_t expected_phase_id,
    size_t expected_family_root_value_id) {
    int has_recovery_phase = 0;
    size_t recovery_phase_id = 0;
    size_t recovery_family_root_value_id = 0;

    if (!value_ssa_allocation_result_get_spill_recovery_phase(result,
            value_id,
            &has_recovery_phase,
            &recovery_phase_id,
            &recovery_family_root_value_id)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s spill recovery-phase query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (has_recovery_phase != expected_has_phase ||
        (expected_has_phase &&
            (recovery_phase_id != expected_phase_id ||
                recovery_family_root_value_id != expected_family_root_value_id))) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovery-phase(ssa.%zu)=(%d,%zu,%zu), got (%d,%zu,%zu)\n",
            case_id,
            value_id,
            expected_has_phase,
            expected_phase_id,
            expected_family_root_value_id,
            has_recovery_phase,
            recovery_phase_id,
            recovery_family_root_value_id);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovery_phase_member_order(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_phase_member_order,
    size_t expected_phase_member_order) {
    int has_phase_member_order = 0;
    size_t phase_member_order = 0;

    if (!value_ssa_allocation_result_get_spill_recovery_phase_member_order(
            result, value_id, &has_phase_member_order, &phase_member_order)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill recovery-phase-member-order query failed for ssa.%zu\n",
            case_id,
            value_id);
        return 0;
    }
    if (has_phase_member_order != expected_has_phase_member_order ||
        (expected_has_phase_member_order && phase_member_order != expected_phase_member_order)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovery-phase-member-order(ssa.%zu)=(%d,%zu), got (%d,%zu)\n",
            case_id,
            value_id,
            expected_has_phase_member_order,
            expected_phase_member_order,
            has_phase_member_order,
            phase_member_order);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovery_phase_member_count(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_phase_member_count,
    size_t expected_phase_member_count) {
    int has_phase_member_count = 0;
    size_t phase_member_count = 0;

    if (!value_ssa_allocation_result_get_spill_recovery_phase_member_count(
            result, value_id, &has_phase_member_count, &phase_member_count)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill recovery-phase-member-count query failed for ssa.%zu\n",
            case_id,
            value_id);
        return 0;
    }
    if (has_phase_member_count != expected_has_phase_member_count ||
        (expected_has_phase_member_count && phase_member_count != expected_phase_member_count)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovery-phase-member-count(ssa.%zu)=(%d,%zu), got (%d,%zu)\n",
            case_id,
            value_id,
            expected_has_phase_member_count,
            expected_phase_member_count,
            has_phase_member_count,
            phase_member_count);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovery_phase_entry(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_phase_entry,
    size_t expected_entry_value_id,
    size_t expected_entry_recoverable_count,
    size_t expected_entry_intended_count) {
    int has_phase_entry = 0;
    size_t entry_value_id = 0;
    size_t entry_recoverable_count = 0;
    size_t entry_intended_count = 0;

    if (!value_ssa_allocation_result_get_spill_recovery_phase_entry(result,
            value_id,
            &has_phase_entry,
            &entry_value_id,
            &entry_recoverable_count,
            &entry_intended_count)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill recovery-phase-entry query failed for ssa.%zu\n",
            case_id,
            value_id);
        return 0;
    }
    if (has_phase_entry != expected_has_phase_entry ||
        (expected_has_phase_entry &&
            (entry_value_id != expected_entry_value_id ||
                entry_recoverable_count != expected_entry_recoverable_count ||
                entry_intended_count != expected_entry_intended_count))) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovery-phase-entry(ssa.%zu)=(%d,%zu,%zu,%zu), got (%d,%zu,%zu,%zu)\n",
            case_id,
            value_id,
            expected_has_phase_entry,
            expected_entry_value_id,
            expected_entry_recoverable_count,
            expected_entry_intended_count,
            has_phase_entry,
            entry_value_id,
            entry_recoverable_count,
            entry_intended_count);
        return 0;
    }

    return 1;
}

static int expect_query_spill_recovery_phase_entry_split_family(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_phase_entry,
    size_t expected_split_root_value_id,
    size_t expected_split_member_count,
    size_t expected_split_intended_count) {
    int has_phase_entry = 0;
    size_t split_root_value_id = 0;
    size_t split_member_count = 0;
    size_t split_intended_count = 0;

    if (!value_ssa_allocation_result_get_spill_recovery_phase_entry_split_family(result,
            value_id,
            &has_phase_entry,
            &split_root_value_id,
            &split_member_count,
            &split_intended_count)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill recovery-phase-entry split-family query failed for ssa.%zu\n",
            case_id,
            value_id);
        return 0;
    }
    if (has_phase_entry != expected_has_phase_entry ||
        (expected_has_phase_entry &&
            (split_root_value_id != expected_split_root_value_id ||
                split_member_count != expected_split_member_count ||
                split_intended_count != expected_split_intended_count))) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill recovery-phase-entry split-family(ssa.%zu)=(%d,ssa.%zu,%zu,%zu), got (%d,ssa.%zu,%zu,%zu)\n",
            case_id,
            value_id,
            expected_has_phase_entry,
            expected_split_root_value_id,
            expected_split_member_count,
            expected_split_intended_count,
            has_phase_entry,
            split_root_value_id,
            split_member_count,
            split_intended_count);
        return 0;
    }

    return 1;
}

static int expect_query_retry_eviction(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_used_retry_eviction,
    size_t expected_blocker_value_id) {
    int used_retry_eviction = 0;
    size_t blocker_value_id = 0;

    if (!value_ssa_allocation_result_used_retry_eviction(
            result, value_id, &used_retry_eviction, &blocker_value_id)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry eviction query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (used_retry_eviction != expected_used_retry_eviction ||
        (used_retry_eviction && blocker_value_id != expected_blocker_value_id)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry eviction(ssa.%zu)=(%d,%zu), got (%d,%zu)\n",
            case_id,
            value_id,
            expected_used_retry_eviction,
            expected_blocker_value_id,
            used_retry_eviction,
            blocker_value_id);
        return 0;
    }

    return 1;
}

static int expect_query_retry_eviction_outcome(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_used_retry_eviction,
    size_t expected_blocker_value_id,
    int expected_blocker_recolored,
    size_t expected_blocker_new_color) {
    int used_retry_eviction = 0;
    int blocker_recolored = 0;
    size_t blocker_value_id = 0;
    size_t blocker_new_color = 0;

    if (!value_ssa_allocation_result_get_retry_eviction_outcome(
            result,
            value_id,
            &used_retry_eviction,
            &blocker_value_id,
            &blocker_recolored,
            &blocker_new_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry eviction outcome query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (used_retry_eviction != expected_used_retry_eviction ||
        (used_retry_eviction && blocker_value_id != expected_blocker_value_id) ||
        (used_retry_eviction && blocker_recolored != expected_blocker_recolored) ||
        (used_retry_eviction && blocker_recolored && blocker_new_color != expected_blocker_new_color)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry eviction outcome(ssa.%zu)=(%d,%zu,%d,%zu), got (%d,%zu,%d,%zu)\n",
            case_id,
            value_id,
            expected_used_retry_eviction,
            expected_blocker_value_id,
            expected_blocker_recolored,
            expected_blocker_new_color,
            used_retry_eviction,
            blocker_value_id,
            blocker_recolored,
            blocker_new_color);
        return 0;
    }

    return 1;
}

static int expect_query_targeted_eviction_outcome(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_used_targeted_eviction,
    size_t expected_blocker_value_id,
    int expected_blocker_recolored,
    size_t expected_blocker_new_color) {
    int used_targeted_eviction = 0;
    int blocker_recolored = 0;
    size_t blocker_value_id = 0;
    size_t blocker_new_color = 0;

    if (!value_ssa_allocation_result_get_targeted_eviction_outcome(
            result,
            value_id,
            &used_targeted_eviction,
            &blocker_value_id,
            &blocker_recolored,
            &blocker_new_color)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s targeted eviction outcome query failed for ssa.%zu\n",
            case_id,
            value_id);
        return 0;
    }
    if (used_targeted_eviction != expected_used_targeted_eviction ||
        (used_targeted_eviction && blocker_value_id != expected_blocker_value_id) ||
        (used_targeted_eviction && blocker_recolored != expected_blocker_recolored) ||
        (used_targeted_eviction && blocker_recolored && blocker_new_color != expected_blocker_new_color)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected targeted eviction outcome(ssa.%zu)=(%d,%zu,%d,%zu), got (%d,%zu,%d,%zu)\n",
            case_id,
            value_id,
            expected_used_targeted_eviction,
            expected_blocker_value_id,
            expected_blocker_recolored,
            expected_blocker_new_color,
            used_targeted_eviction,
            blocker_value_id,
            blocker_recolored,
            blocker_new_color);
        return 0;
    }

    return 1;
}

static int expect_query_generic_eviction_outcome(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_used_generic_eviction,
    size_t expected_blocker_value_id,
    int expected_blocker_recolored,
    size_t expected_blocker_new_color) {
    int used_generic_eviction = 0;
    int blocker_recolored = 0;
    size_t blocker_value_id = 0;
    size_t blocker_new_color = 0;

    if (!value_ssa_allocation_result_get_generic_eviction_outcome(
            result,
            value_id,
            &used_generic_eviction,
            &blocker_value_id,
            &blocker_recolored,
            &blocker_new_color)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s generic eviction outcome query failed for ssa.%zu\n",
            case_id,
            value_id);
        return 0;
    }
    if (used_generic_eviction != expected_used_generic_eviction ||
        (used_generic_eviction && blocker_value_id != expected_blocker_value_id) ||
        (used_generic_eviction && blocker_recolored != expected_blocker_recolored) ||
        (used_generic_eviction && blocker_recolored && blocker_new_color != expected_blocker_new_color)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected generic eviction outcome(ssa.%zu)=(%d,%zu,%d,%zu), got (%d,%zu,%d,%zu)\n",
            case_id,
            value_id,
            expected_used_generic_eviction,
            expected_blocker_value_id,
            expected_blocker_recolored,
            expected_blocker_new_color,
            used_generic_eviction,
            blocker_value_id,
            blocker_recolored,
            blocker_new_color);
        return 0;
    }

    return 1;
}

static void clear_retry_phase_entry_item(ValueSsaAllocatorRetryFamilyAgendaItem *entry) {
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

static int expect_query_priority(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    size_t expected_priority) {
    size_t priority = 0;

    if (!value_ssa_allocation_result_get_spill_priority(result, value_id, &priority)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s priority query failed\n", case_id);
        return 0;
    }
    if (priority != expected_priority) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected priority %zu, got %zu\n",
            case_id,
            expected_priority,
            priority);
        return 0;
    }

    return 1;
}

static int expect_worklist_priority(const char *case_id,
    const ValueSsaAllocationWorklist *worklist,
    size_t value_id,
    size_t expected_priority) {
    size_t item_index;

    if (!worklist) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid worklist\n", case_id);
        return 0;
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        if (worklist->items[item_index].value_id != value_id) {
            continue;
        }
        if (worklist->items[item_index].priority != expected_priority) {
            fprintf(stderr,
                "[value-ssa-alloc] FAIL: %s expected worklist priority(ssa.%zu)=%zu, got %zu\n",
                case_id,
                value_id,
                expected_priority,
                worklist->items[item_index].priority);
            return 0;
        }
        return 1;
    }

    fprintf(stderr, "[value-ssa-alloc] FAIL: %s could not find ssa.%zu in worklist\n", case_id, value_id);
    return 0;
}

static int expect_worklist_index_less_than(const char *case_id,
    const ValueSsaAllocationWorklist *worklist,
    size_t earlier_value_id,
    size_t later_value_id) {
    size_t earlier_index = (size_t)-1;
    size_t later_index = (size_t)-1;
    size_t item_index;

    if (!worklist) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid worklist\n", case_id);
        return 0;
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        if (worklist->items[item_index].value_id == earlier_value_id) {
            earlier_index = item_index;
        }
        if (worklist->items[item_index].value_id == later_value_id) {
            later_index = item_index;
        }
    }

    if (earlier_index == (size_t)-1 || later_index == (size_t)-1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s worklist index lookup failed for ssa.%zu / ssa.%zu\n",
            case_id,
            earlier_value_id,
            later_value_id);
        return 0;
    }
    if (earlier_index >= later_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected ssa.%zu before ssa.%zu in worklist, got %zu >= %zu\n",
            case_id,
            earlier_value_id,
            later_value_id,
            earlier_index,
            later_index);
        return 0;
    }

    return 1;
}

static int expect_query_spill_slot(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    int expected_has_spill_slot,
    size_t expected_spill_slot) {
    int has_spill_slot = 0;
    size_t spill_slot = 0;

    if (!value_ssa_allocation_result_get_spill_slot(result, value_id, &has_spill_slot, &spill_slot)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s spill-slot query failed\n", case_id);
        return 0;
    }
    if (has_spill_slot != expected_has_spill_slot || (has_spill_slot && spill_slot != expected_spill_slot)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill-slot query (%d,%zu), got (%d,%zu)\n",
            case_id,
            expected_has_spill_slot,
            expected_spill_slot,
            has_spill_slot,
            spill_slot);
        return 0;
    }

    return 1;
}

static int expect_result_counts(const char *case_id,
    const ValueSsaAllocationResult *result,
    size_t expected_colored_count,
    size_t expected_spilled_count) {
    size_t colored_count = 0;
    size_t spilled_count = 0;

    if (!value_ssa_allocation_result_count_colored_values(result, &colored_count) ||
        !value_ssa_allocation_result_count_spilled_values(result, &spilled_count)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s count query failed\n", case_id);
        return 0;
    }
    if (colored_count != expected_colored_count || spilled_count != expected_spilled_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected counts (colored=%zu, spilled=%zu), got (%zu,%zu)\n",
            case_id,
            expected_colored_count,
            expected_spilled_count,
            colored_count,
            spilled_count);
        return 0;
    }

    return 1;
}

static int expect_plan_phase(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    ValueSsaAllocatorPhase expected_phase) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->phase != expected_phase) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected phase(ssa.%zu)=%s, got %s\n",
            case_id,
            value_id,
            value_ssa_allocator_phase_name(expected_phase),
            value_ssa_allocator_phase_name(item->phase));
        return 0;
    }

    return 1;
}

static int expect_plan_removal_kind(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    ValueSsaAllocatorRemovalKind expected_removal_kind) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->removal_kind != expected_removal_kind) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected removal(ssa.%zu)=%s, got %s\n",
            case_id,
            value_id,
            value_ssa_allocator_removal_kind_name(expected_removal_kind),
            value_ssa_allocator_removal_kind_name(item->removal_kind));
        return 0;
    }

    return 1;
}

static int expect_plan_frozen(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    int expected_was_frozen) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if ((int)item->was_frozen != expected_was_frozen) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected frozen(ssa.%zu)=%d, got %d\n",
            case_id,
            value_id,
            expected_was_frozen,
            (int)item->was_frozen);
        return 0;
    }

    return 1;
}

static int expect_plan_effective_move_related(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    int expected_initial_effective_move_related,
    int expected_active_effective_move_related) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if ((int)item->initial_effective_move_related != expected_initial_effective_move_related ||
        (int)item->active_effective_move_related != expected_active_effective_move_related) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected effective_move(ssa.%zu)=(%d,%d), got (%d,%d)\n",
            case_id,
            value_id,
            expected_initial_effective_move_related,
            expected_active_effective_move_related,
            (int)item->initial_effective_move_related,
            (int)item->active_effective_move_related);
        return 0;
    }

    return 1;
}

static int expect_plan_move_work_class(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    ValueSsaAllocatorMoveWorkClass expected_work_class) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->move_work_class != expected_work_class) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected move-work(ssa.%zu)=%s, got %s\n",
            case_id,
            value_id,
            value_ssa_allocator_move_work_class_name(expected_work_class),
            value_ssa_allocator_move_work_class_name(item->move_work_class));
        return 0;
    }

    return 1;
}

static int expect_plan_move_work_transition(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    ValueSsaAllocatorMoveWorkClass expected_before,
    ValueSsaAllocatorMoveWorkClass expected_after) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->move_work_class != expected_before || item->move_work_next_class != expected_after) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected move-work transition(ssa.%zu)=%s->%s, got %s->%s\n",
            case_id,
            value_id,
            value_ssa_allocator_move_work_class_name(expected_before),
            value_ssa_allocator_move_work_class_name(expected_after),
            value_ssa_allocator_move_work_class_name(item->move_work_class),
            value_ssa_allocator_move_work_class_name(item->move_work_next_class));
        return 0;
    }
    return 1;
}

static int expect_plan_move_work_priority_order(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t lower_priority_value_id,
    size_t higher_priority_value_id) {
    const ValueSsaAllocatorPlanItem *lower_item = NULL;
    const ValueSsaAllocatorPlanItem *higher_item = NULL;

    if (!value_ssa_allocator_plan_find_value(plan, lower_priority_value_id, NULL, &lower_item) || !lower_item ||
        !value_ssa_allocator_plan_find_value(plan, higher_priority_value_id, NULL, &higher_item) || !higher_item) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocator-plan move-work priority lookup failed for ssa.%zu / ssa.%zu\n",
            case_id,
            lower_priority_value_id,
            higher_priority_value_id);
        return 0;
    }
    if (lower_item->move_work_priority >= higher_item->move_work_priority) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected move-work priority ssa.%zu < ssa.%zu, got %zu >= %zu\n",
            case_id,
            lower_priority_value_id,
            higher_priority_value_id,
            lower_item->move_work_priority,
            higher_item->move_work_priority);
        return 0;
    }
    return 1;
}

static int expect_plan_index_less_than(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t earlier_value_id,
    size_t later_value_id) {
    size_t earlier_index = 0;
    size_t later_index = 0;

    if (!value_ssa_allocator_plan_find_value(plan, earlier_value_id, &earlier_index, NULL) ||
        !value_ssa_allocator_plan_find_value(plan, later_value_id, &later_index, NULL)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s allocator-plan index lookup failed for ssa.%zu / ssa.%zu\n",
            case_id,
            earlier_value_id,
            later_value_id);
        return 0;
    }
    if (earlier_index >= later_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected ssa.%zu before ssa.%zu in allocator-plan, got %zu >= %zu\n",
            case_id,
            earlier_value_id,
            later_value_id,
            earlier_index,
            later_index);
        return 0;
    }

    return 1;
}

static int expect_plan_spill_cost_components(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t expected_use_cost,
    size_t expected_live_range_cost,
    size_t expected_affinity_cost) {
    const ValueSsaAllocatorPlanItem *item = NULL;
    size_t expected_total;
    size_t query_total = 0;
    size_t query_use = 0;
    size_t query_live_range = 0;
    size_t query_affinity = 0;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    expected_total = expected_use_cost + item->spill_use_loop_depth_cost + item->spill_use_call_density_cost +
        expected_live_range_cost + expected_affinity_cost;
    if (item->spill_use_cost != expected_use_cost ||
        item->spill_live_range_cost != expected_live_range_cost ||
        item->spill_affinity_cost != expected_affinity_cost ||
        item->spill_cost != expected_total) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill-cost(ssa.%zu)=%zu(%zu,%zu,%zu), got %zu(%zu,%zu,%zu)\n",
            case_id,
            value_id,
            expected_total,
            expected_use_cost,
            expected_live_range_cost,
            expected_affinity_cost,
            item->spill_cost,
            item->spill_use_cost,
            item->spill_live_range_cost,
            item->spill_affinity_cost);
        return 0;
    }
    if (!value_ssa_allocator_plan_get_spill_cost(
            plan, value_id, &query_total, &query_use, &query_live_range, &query_affinity) ||
        query_total != expected_total ||
        query_use != expected_use_cost ||
        query_live_range != expected_live_range_cost ||
        query_affinity != expected_affinity_cost) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill-cost query mismatch for ssa.%zu, got %zu(%zu,%zu,%zu)\n",
            case_id,
            value_id,
            query_total,
            query_use,
            query_live_range,
            query_affinity);
        return 0;
    }

    return 1;
}

static int expect_plan_spill_cost_detail(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t expected_use_loop_depth_cost,
    size_t expected_use_call_density_cost,
    size_t expected_live_block_cost,
    size_t expected_loop_depth_cost,
    size_t expected_call_density_cost,
    size_t expected_call_crossing_cost,
    size_t expected_cross_block_cost) {
    const ValueSsaAllocatorPlanItem *item = NULL;
    size_t query_use_loop_depth = 0;
    size_t query_use_call_density = 0;
    size_t query_live_block = 0;
    size_t query_loop_depth = 0;
    size_t query_call_density = 0;
    size_t query_call_crossing = 0;
    size_t query_cross_block = 0;

    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s allocator-plan find failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->spill_use_loop_depth_cost != expected_use_loop_depth_cost ||
        item->spill_use_call_density_cost != expected_use_call_density_cost ||
        item->spill_live_block_cost != expected_live_block_cost ||
        item->spill_loop_depth_cost != expected_loop_depth_cost ||
        item->spill_call_density_cost != expected_call_density_cost ||
        item->spill_call_crossing_cost != expected_call_crossing_cost ||
        item->spill_cross_block_cost != expected_cross_block_cost) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected spill-cost-detail(ssa.%zu)=(%zu,%zu,%zu,%zu,%zu,%zu,%zu), got (%zu,%zu,%zu,%zu,%zu,%zu,%zu)\n",
            case_id,
            value_id,
            expected_use_loop_depth_cost,
            expected_use_call_density_cost,
            expected_live_block_cost,
            expected_loop_depth_cost,
            expected_call_density_cost,
            expected_call_crossing_cost,
            expected_cross_block_cost,
            item->spill_use_loop_depth_cost,
            item->spill_use_call_density_cost,
            item->spill_live_block_cost,
            item->spill_loop_depth_cost,
            item->spill_call_density_cost,
            item->spill_call_crossing_cost,
            item->spill_cross_block_cost);
        return 0;
    }
    if (!value_ssa_allocator_plan_get_spill_cost_detail(plan,
            value_id,
            &query_use_loop_depth,
            &query_use_call_density,
            &query_live_block,
            &query_loop_depth,
            &query_call_density,
            &query_call_crossing,
            &query_cross_block) ||
        query_use_loop_depth != expected_use_loop_depth_cost ||
        query_use_call_density != expected_use_call_density_cost ||
        query_live_block != expected_live_block_cost ||
        query_loop_depth != expected_loop_depth_cost ||
        query_call_density != expected_call_density_cost ||
        query_call_crossing != expected_call_crossing_cost ||
        query_cross_block != expected_cross_block_cost) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s spill-cost-detail query mismatch for ssa.%zu, got (%zu,%zu,%zu,%zu,%zu,%zu,%zu)\n",
            case_id,
            value_id,
            query_use_loop_depth,
            query_use_call_density,
            query_live_block,
            query_loop_depth,
            query_call_density,
            query_call_crossing,
            query_cross_block);
        return 0;
    }

    return 1;
}

static int expect_coalesce_pair(const char *case_id,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t lhs_value_id,
    size_t rhs_value_id,
    int expected_interferes,
    int expected_can_coalesce) {
    const ValueSsaAllocatorCoalesceItem *item = NULL;

    if (!value_ssa_allocator_coalesce_analysis_find_pair(analysis, lhs_value_id, rhs_value_id, NULL, &item) || !item) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s coalesce pair lookup failed for ssa.%zu / ssa.%zu\n",
            case_id,
            lhs_value_id,
            rhs_value_id);
        return 0;
    }
    if ((int)item->interferes != expected_interferes || (int)item->can_coalesce != expected_can_coalesce) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected coalesce(ssa.%zu,ssa.%zu)=(interferes=%d,can=%d), got (%d,%d)\n",
            case_id,
            lhs_value_id,
            rhs_value_id,
            expected_interferes,
            expected_can_coalesce,
            (int)item->interferes,
            (int)item->can_coalesce);
        return 0;
    }

    return 1;
}

static int expect_coalesce_pair_absent(const char *case_id,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t lhs_value_id,
    size_t rhs_value_id) {
    if (value_ssa_allocator_coalesce_analysis_find_pair(analysis, lhs_value_id, rhs_value_id, NULL, NULL)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected no coalesce pair for ssa.%zu / ssa.%zu\n",
            case_id,
            lhs_value_id,
            rhs_value_id);
        return 0;
    }

    return 1;
}

static int expect_coalesce_class(const char *case_id,
    const ValueSsaAllocatorCoalesceAnalysis *analysis,
    size_t value_id,
    size_t expected_root_value_id,
    size_t expected_class_size) {
    size_t root_value_id = 0;
    size_t class_size = 0;

    if (!value_ssa_allocator_coalesce_analysis_get_class(
            analysis, value_id, &root_value_id, &class_size)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s coalesce class query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (root_value_id != expected_root_value_id || class_size != expected_class_size) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected class(ssa.%zu)=(root=ssa.%zu,size=%zu), got (root=ssa.%zu,size=%zu)\n",
            case_id,
            value_id,
            expected_root_value_id,
            expected_class_size,
            root_value_id,
            class_size);
        return 0;
    }

    return 1;
}

static int expect_move_family(const char *case_id,
    const ValueSsaAllocatorMoveFamilyAnalysis *analysis,
    size_t value_id,
    size_t expected_root_value_id,
    ValueSsaAllocatorMoveFamilyState expected_state,
    size_t expected_class_size,
    size_t expected_external_neighbor_count,
    size_t expected_coalesce_ready_neighbor_count,
    size_t expected_external_affinity_sum,
    int expected_initial_move_related,
    int expected_was_frozen,
    size_t expected_simplify_removed_count,
    size_t expected_spill_removed_count) {
    const ValueSsaAllocatorMoveFamilyItem *item = NULL;

    if (!value_ssa_allocator_move_family_analysis_get_value_family(analysis, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s move-family query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->root_value_id != expected_root_value_id || item->state != expected_state ||
        item->class_size != expected_class_size ||
        item->external_neighbor_family_count != expected_external_neighbor_count ||
        item->coalesce_ready_neighbor_family_count != expected_coalesce_ready_neighbor_count ||
        item->external_affinity_weight_sum != expected_external_affinity_sum ||
        (int)item->initial_move_related != expected_initial_move_related ||
        (int)item->was_frozen != expected_was_frozen ||
        item->simplify_removed_count != expected_simplify_removed_count ||
        item->spill_removed_count != expected_spill_removed_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected move-family(ssa.%zu)=(root=ssa.%zu,state=%s,size=%zu,neighbors=%zu,ready=%zu,affinity=%zu,initial_move=%d,frozen=%d,removed=%zu/%zu), got (root=ssa.%zu,state=%s,size=%zu,neighbors=%zu,ready=%zu,affinity=%zu,initial_move=%d,frozen=%d,removed=%zu/%zu)\n",
            case_id,
            value_id,
            expected_root_value_id,
            value_ssa_allocator_move_family_state_name(expected_state),
            expected_class_size,
            expected_external_neighbor_count,
            expected_coalesce_ready_neighbor_count,
            expected_external_affinity_sum,
            expected_initial_move_related,
            expected_was_frozen,
            expected_simplify_removed_count,
            expected_spill_removed_count,
            item->root_value_id,
            value_ssa_allocator_move_family_state_name(item->state),
            item->class_size,
            item->external_neighbor_family_count,
            item->coalesce_ready_neighbor_family_count,
            item->external_affinity_weight_sum,
            (int)item->initial_move_related,
            (int)item->was_frozen,
            item->simplify_removed_count,
            item->spill_removed_count);
        return 0;
    }

    return 1;
}

static int expect_move_work(const char *case_id,
    const ValueSsaAllocatorMoveWorklist *worklist,
    size_t value_id,
    size_t expected_root_value_id,
    ValueSsaAllocatorMoveWorkClass expected_work_class,
    size_t expected_class_size,
    size_t expected_external_neighbor_count,
    size_t expected_coalesce_ready_neighbor_count,
    size_t expected_external_affinity_sum,
    size_t expected_simplify_removed_count,
    size_t expected_spill_removed_count) {
    const ValueSsaAllocatorMoveWorkItem *item = NULL;

    if (!value_ssa_allocator_move_worklist_get_value_work(worklist, value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s move-work query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (item->root_value_id != expected_root_value_id || item->work_class != expected_work_class ||
        item->class_size != expected_class_size ||
        item->external_neighbor_family_count != expected_external_neighbor_count ||
        item->coalesce_ready_neighbor_family_count != expected_coalesce_ready_neighbor_count ||
        item->external_affinity_weight_sum != expected_external_affinity_sum ||
        item->simplify_removed_count != expected_simplify_removed_count ||
        item->spill_removed_count != expected_spill_removed_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected move-work(ssa.%zu)=(root=ssa.%zu,class=%s,size=%zu,neighbors=%zu,ready=%zu,affinity=%zu,removed=%zu/%zu), got (root=ssa.%zu,class=%s,size=%zu,neighbors=%zu,ready=%zu,affinity=%zu,removed=%zu/%zu)\n",
            case_id,
            value_id,
            expected_root_value_id,
            value_ssa_allocator_move_work_class_name(expected_work_class),
            expected_class_size,
            expected_external_neighbor_count,
            expected_coalesce_ready_neighbor_count,
            expected_external_affinity_sum,
            expected_simplify_removed_count,
            expected_spill_removed_count,
            item->root_value_id,
            value_ssa_allocator_move_work_class_name(item->work_class),
            item->class_size,
            item->external_neighbor_family_count,
            item->coalesce_ready_neighbor_family_count,
            item->external_affinity_weight_sum,
            item->simplify_removed_count,
            item->spill_removed_count);
        return 0;
    }

    return 1;
}

static int expect_coalesce_opportunity(const char *case_id,
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t lhs_root_value_id,
    size_t rhs_root_value_id,
    int expected_mutual_best,
    int expected_lhs_prefers_rhs,
    size_t expected_lhs_weight,
    int expected_rhs_prefers_lhs,
    size_t expected_rhs_weight) {
    const ValueSsaAllocatorCoalesceOpportunityItem *item = NULL;

    if (!value_ssa_allocator_coalesce_opportunity_agenda_find_pair(
            agenda, lhs_root_value_id, rhs_root_value_id, NULL, &item) ||
        !item) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s coalesce-opportunity query failed for ssa.%zu / ssa.%zu\n",
            case_id,
            lhs_root_value_id,
            rhs_root_value_id);
        return 0;
    }
    if ((int)item->mutual_best != expected_mutual_best || (int)item->lhs_prefers_rhs != expected_lhs_prefers_rhs ||
        item->lhs_preference_weight != expected_lhs_weight ||
        (int)item->rhs_prefers_lhs != expected_rhs_prefers_lhs ||
        item->rhs_preference_weight != expected_rhs_weight) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected coalesce-opportunity(ssa.%zu,ssa.%zu)=(mutual=%d,lhs=%d@%zu,rhs=%d@%zu), got (mutual=%d,lhs=%d@%zu,rhs=%d@%zu)\n",
            case_id,
            lhs_root_value_id,
            rhs_root_value_id,
            expected_mutual_best,
            expected_lhs_prefers_rhs,
            expected_lhs_weight,
            expected_rhs_prefers_lhs,
            expected_rhs_weight,
            (int)item->mutual_best,
            (int)item->lhs_prefers_rhs,
            item->lhs_preference_weight,
            (int)item->rhs_prefers_lhs,
            item->rhs_preference_weight);
        return 0;
    }

    return 1;
}

static int expect_best_coalesce_opportunity_for_root(const char *case_id,
    const ValueSsaAllocatorCoalesceOpportunityAgenda *agenda,
    size_t root_value_id,
    size_t expected_lhs_root_value_id,
    size_t expected_rhs_root_value_id) {
    const ValueSsaAllocatorCoalesceOpportunityItem *item = NULL;

    if (!value_ssa_allocator_coalesce_opportunity_agenda_find_root_best(
            agenda, root_value_id, NULL, &item) ||
        !item) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s best coalesce-opportunity query failed for root ssa.%zu\n",
            case_id,
            root_value_id);
        return 0;
    }
    if (item->lhs_root_value_id != expected_lhs_root_value_id || item->rhs_root_value_id != expected_rhs_root_value_id) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected best coalesce-opportunity for root ssa.%zu to be (ssa.%zu,ssa.%zu), got (ssa.%zu,ssa.%zu)\n",
            case_id,
            root_value_id,
            expected_lhs_root_value_id,
            expected_rhs_root_value_id,
            item->lhs_root_value_id,
            item->rhs_root_value_id);
        return 0;
    }

    return 1;
}

static int expect_retry_family_agenda_root(const char *case_id,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    size_t expected_representative_value_id,
    size_t expected_recoverable_count,
    size_t expected_intended_count) {
    const ValueSsaAllocatorRetryFamilyAgendaItem *item = NULL;

    if (!value_ssa_allocator_retry_family_agenda_find_root(agenda, root_value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry family-agenda query failed for root ssa.%zu\n", case_id, root_value_id);
        return 0;
    }
    if (item->representative_value_id != expected_representative_value_id ||
        item->recoverable_member_count != expected_recoverable_count ||
        item->recoverable_intended_count != expected_intended_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry family-agenda(ssa.%zu)=(rep=ssa.%zu,recoverable=%zu,intended=%zu), got (rep=ssa.%zu,recoverable=%zu,intended=%zu)\n",
            case_id,
            root_value_id,
            expected_representative_value_id,
            expected_recoverable_count,
            expected_intended_count,
            item->representative_value_id,
            item->recoverable_member_count,
            item->recoverable_intended_count);
        return 0;
    }

    return 1;
}

static int expect_retry_family_agenda_entry_shape(const char *case_id,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    ValueSsaAllocatorRetryFamilyEntryKind expected_entry_kind,
    ValueSsaAllocationPreferredColorSource expected_preferred_source,
    size_t expected_preferred_partner_value_id,
    int expected_requires_eviction,
    size_t expected_blocker_value_id,
    int expected_blocker_recolorable) {
    const ValueSsaAllocatorRetryFamilyAgendaItem *item = NULL;

    if (!value_ssa_allocator_retry_family_agenda_find_root(agenda, root_value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry family-agenda shape query failed for root ssa.%zu\n", case_id, root_value_id);
        return 0;
    }
    if (item->entry_kind != expected_entry_kind || item->preferred_source != expected_preferred_source ||
        item->preferred_partner_value_id != expected_preferred_partner_value_id ||
        !!item->requires_eviction != !!expected_requires_eviction || item->blocker_value_id != expected_blocker_value_id ||
        !!item->blocker_recolorable != !!expected_blocker_recolorable) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry family-agenda shape(ssa.%zu)=(kind=%d,preferred=%d,partner=ssa.%zu,evict=%d,blocker=ssa.%zu,recolorable=%d), got (kind=%d,preferred=%d,partner=ssa.%zu,evict=%d,blocker=ssa.%zu,recolorable=%d)\n",
            case_id,
            root_value_id,
            (int)expected_entry_kind,
            (int)expected_preferred_source,
            expected_preferred_partner_value_id,
            expected_requires_eviction,
            expected_blocker_value_id,
            expected_blocker_recolorable,
            (int)item->entry_kind,
            (int)item->preferred_source,
            item->preferred_partner_value_id,
            item->requires_eviction ? 1 : 0,
            item->blocker_value_id,
            item->blocker_recolorable ? 1 : 0);
        return 0;
    }

    return 1;
}

static int expect_retry_family_agenda_split_child(const char *case_id,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    int expected_split_child) {
    const ValueSsaAllocatorRetryFamilyAgendaItem *item = NULL;

    if (!value_ssa_allocator_retry_family_agenda_find_root(agenda, root_value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry family-agenda split-child query failed for root ssa.%zu\n", case_id, root_value_id);
        return 0;
    }
    if (!!item->representative_split_child != !!expected_split_child) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry family-agenda split_child(ssa.%zu)=%d, got %d\n",
            case_id,
            root_value_id,
            expected_split_child,
            item->representative_split_child ? 1 : 0);
        return 0;
    }

    return 1;
}

static int expect_retry_family_agenda_split_child_depth(const char *case_id,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    size_t expected_depth) {
    const ValueSsaAllocatorRetryFamilyAgendaItem *item = NULL;

    if (!value_ssa_allocator_retry_family_agenda_find_root(agenda, root_value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry family-agenda split-depth query failed for root ssa.%zu\n", case_id, root_value_id);
        return 0;
    }
    if (item->representative_split_child_depth != expected_depth) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry family-agenda split_depth(ssa.%zu)=%zu, got %zu\n",
            case_id,
            root_value_id,
            expected_depth,
            item->representative_split_child_depth);
        return 0;
    }

    return 1;
}

static int expect_retry_family_agenda_split_family(const char *case_id,
    const ValueSsaAllocatorRetryFamilyAgenda *agenda,
    size_t root_value_id,
    size_t expected_split_root_value_id,
    size_t expected_split_member_count,
    size_t expected_split_intended_count) {
    const ValueSsaAllocatorRetryFamilyAgendaItem *item = NULL;

    if (!value_ssa_allocator_retry_family_agenda_find_root(agenda, root_value_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry family-agenda split-family query failed for root ssa.%zu\n", case_id, root_value_id);
        return 0;
    }
    if (item->split_family_root_value_id != expected_split_root_value_id ||
        item->split_family_member_count != expected_split_member_count ||
        item->split_family_intended_count != expected_split_intended_count) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry family-agenda split_family(ssa.%zu)=(root=ssa.%zu,members=%zu,intended=%zu), got (root=ssa.%zu,members=%zu,intended=%zu)\n",
            case_id,
            root_value_id,
            expected_split_root_value_id,
            expected_split_member_count,
            expected_split_intended_count,
            item->split_family_root_value_id,
            item->split_family_member_count,
            item->split_family_intended_count);
        return 0;
    }

    return 1;
}

static int expect_retry_phase_trace_item(const char *case_id,
    const ValueSsaAllocatorRetryPhaseTrace *trace,
    size_t phase_id,
    size_t expected_family_root_value_id,
    size_t expected_entry_value_id,
    size_t expected_entry_recoverable_count,
    size_t expected_entry_intended_count,
    size_t expected_recovered_member_count,
    size_t expected_first_recovery_order,
    size_t expected_last_recovery_order) {
    const ValueSsaAllocatorRetryPhaseTraceItem *item = NULL;

    if (!value_ssa_allocator_retry_phase_trace_find_phase_id(trace, phase_id, NULL, &item) || !item) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s retry phase-trace query failed for phase %zu\n", case_id, phase_id);
        return 0;
    }
    if (item->family_root_value_id != expected_family_root_value_id ||
        item->entry.representative_value_id != expected_entry_value_id ||
        item->entry.recoverable_member_count != expected_entry_recoverable_count ||
        item->entry.recoverable_intended_count != expected_entry_intended_count ||
        item->recovered_member_count != expected_recovered_member_count ||
        item->first_recovery_order != expected_first_recovery_order ||
        item->last_recovery_order != expected_last_recovery_order) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected retry phase-trace(%zu)=(root=ssa.%zu,entry=ssa.%zu,summary=%zu/%zu,recovered=%zu,orders=%zu..%zu), got (root=ssa.%zu,entry=ssa.%zu,summary=%zu/%zu,recovered=%zu,orders=%zu..%zu)\n",
            case_id,
            phase_id,
            expected_family_root_value_id,
            expected_entry_value_id,
            expected_entry_recoverable_count,
            expected_entry_intended_count,
            expected_recovered_member_count,
            expected_first_recovery_order,
            expected_last_recovery_order,
            item->family_root_value_id,
            item->entry.representative_value_id,
            item->entry.recoverable_member_count,
            item->entry.recoverable_intended_count,
            item->recovered_member_count,
            item->first_recovery_order,
            item->last_recovery_order);
        return 0;
    }

    return 1;
}

static int expect_move_transition(const char *case_id,
    const ValueSsaAllocatorMoveTransitionTrace *trace,
    size_t value_id,
    ValueSsaAllocatorMoveTransitionEventKind expected_event_kind,
    ValueSsaAllocatorRemovalKind expected_removal_kind,
    ValueSsaAllocatorMoveWorkClass expected_before,
    ValueSsaAllocatorMoveWorkClass expected_after,
    size_t expected_member_count_before,
    size_t expected_member_count_after,
    size_t expected_external_neighbor_count_before,
    size_t expected_external_neighbor_count_after) {
    const ValueSsaAllocatorMoveTransitionStep *step = NULL;

    if (!value_ssa_allocator_move_transition_trace_find_value_step(
            trace, value_id, expected_event_kind, NULL, &step) ||
        !step) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s move-transition query failed for ssa.%zu\n", case_id, value_id);
        return 0;
    }
    if (step->event_kind != expected_event_kind || step->removal_kind != expected_removal_kind ||
        step->move_work_class_before != expected_before ||
        step->move_work_class_after != expected_after ||
        step->family_active_member_count_before != expected_member_count_before ||
        step->family_active_member_count_after != expected_member_count_after ||
        step->family_external_neighbor_count_before != expected_external_neighbor_count_before ||
        step->family_external_neighbor_count_after != expected_external_neighbor_count_after) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected move-transition(ssa.%zu)=(event=%s,remove=%s,%s->%s,members=%zu->%zu,neighbors=%zu->%zu), got (event=%s,remove=%s,%s->%s,members=%zu->%zu,neighbors=%zu->%zu)\n",
            case_id,
            value_id,
            value_ssa_allocator_move_transition_event_kind_name(expected_event_kind),
            value_ssa_allocator_removal_kind_name(expected_removal_kind),
            value_ssa_allocator_move_work_class_name(expected_before),
            value_ssa_allocator_move_work_class_name(expected_after),
            expected_member_count_before,
            expected_member_count_after,
            expected_external_neighbor_count_before,
            expected_external_neighbor_count_after,
            value_ssa_allocator_move_transition_event_kind_name(step->event_kind),
            value_ssa_allocator_removal_kind_name(step->removal_kind),
            value_ssa_allocator_move_work_class_name(step->move_work_class_before),
            value_ssa_allocator_move_work_class_name(step->move_work_class_after),
            step->family_active_member_count_before,
            step->family_active_member_count_after,
            step->family_external_neighbor_count_before,
            step->family_external_neighbor_count_after);
        return 0;
    }
    return 1;
}

static int expect_plan_final_runtime_coalesce_class(const char *case_id,
    const ValueSsaAllocatorPlan *plan,
    size_t value_id,
    size_t expected_root_value_id,
    size_t expected_class_size) {
    if (!plan || !plan->final_runtime_coalesce_roots || !plan->final_runtime_coalesce_class_sizes ||
        value_id >= plan->value_count) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: %s invalid final-runtime-coalesce query\n", case_id);
        return 0;
    }
    if (plan->final_runtime_coalesce_roots[value_id] != expected_root_value_id ||
        plan->final_runtime_coalesce_class_sizes[expected_root_value_id] != expected_class_size) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: %s expected final runtime coalesce for ssa.%zu -> root=ssa.%zu size=%zu, got root=ssa.%zu size=%zu\n",
            case_id,
            value_id,
            expected_root_value_id,
            expected_class_size,
            plan->final_runtime_coalesce_roots[value_id],
            plan->final_runtime_coalesce_class_sizes[plan->final_runtime_coalesce_roots[value_id]]);
        return 0;
    }
    return 1;
}

static int prepare_manual_spill_result_for_values(ValueSsaProgramAllocationResult *result,
    const ValueSsaProgram *program,
    const size_t *value_ids,
    size_t spill_value_count,
    ValueSsaError *error) {
    size_t value_index;
    size_t spill_index;
    size_t value_count;

    if (!result || !program || !value_ids || spill_value_count == 0 || program->function_count != 1) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: manual spill-result invalid setup\n");
        return 0;
    }
    for (spill_index = 0; spill_index < spill_value_count; ++spill_index) {
        if (value_ids[spill_index] >= program->functions[0].next_value_id) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: manual spill-result invalid value id\n");
            return 0;
        }
    }

    value_ssa_program_allocation_result_free(result);
    result->function_count = 1;
    result->function_results = (ValueSsaAllocationResult *)calloc(1, sizeof(ValueSsaAllocationResult));
    if (!result->function_results) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: manual spill-result allocation failed\n");
        return 0;
    }

    value_ssa_allocation_result_init(&result->function_results[0]);
    value_count = program->functions[0].next_value_id;
    result->function_results[0].value_count = value_count;
    result->function_results[0].color_budget = 2;
    result->function_results[0].spill_slot_count = spill_value_count;
    result->function_results[0].colors = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[0].has_color = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[0].spill_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[0].spill_intended_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[0].spill_confirmed_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[0].spill_recovered_flags = (unsigned char *)calloc(value_count, sizeof(unsigned char));
    result->function_results[0].spill_recovery_orders = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[0].spill_recovery_phase_ids = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[0].spill_recovery_phase_member_orders = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[0].spill_recovery_phase_member_counts = (size_t *)malloc(value_count * sizeof(size_t));
    result->function_results[0].spill_recovery_phase_entries =
        (ValueSsaAllocatorRetryFamilyAgendaItem *)malloc(value_count * sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    result->function_results[0].spill_priorities = (size_t *)calloc(value_count, sizeof(size_t));
    result->function_results[0].spill_slots = (size_t *)malloc(value_count * sizeof(size_t));
    if (!result->function_results[0].colors || !result->function_results[0].has_color ||
        !result->function_results[0].spill_flags ||
        !result->function_results[0].spill_intended_flags ||
        !result->function_results[0].spill_confirmed_flags || !result->function_results[0].spill_recovered_flags ||
        !result->function_results[0].spill_recovery_orders ||
        !result->function_results[0].spill_recovery_phase_ids ||
        !result->function_results[0].spill_recovery_phase_member_orders ||
        !result->function_results[0].spill_recovery_phase_member_counts ||
        !result->function_results[0].spill_recovery_phase_entries ||
        !result->function_results[0].spill_priorities || !result->function_results[0].spill_slots) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: manual spill-result arrays failed\n");
        value_ssa_program_allocation_result_free(result);
        return 0;
    }

    for (value_index = 0; value_index < value_count; ++value_index) {
        result->function_results[0].colors[value_index] = (size_t)-1;
        result->function_results[0].spill_recovery_orders[value_index] = (size_t)-1;
        result->function_results[0].spill_recovery_phase_ids[value_index] = (size_t)-1;
        result->function_results[0].spill_recovery_phase_member_orders[value_index] = (size_t)-1;
        result->function_results[0].spill_recovery_phase_member_counts[value_index] = (size_t)-1;
        clear_retry_phase_entry_item(&result->function_results[0].spill_recovery_phase_entries[value_index]);
        result->function_results[0].spill_slots[value_index] = (size_t)-1;
    }
    result->function_results[0].next_spill_recovery_order = 0;
    result->function_results[0].next_spill_recovery_phase_id = 0;
    for (spill_index = 0; spill_index < spill_value_count; ++spill_index) {
        result->function_results[0].spill_flags[value_ids[spill_index]] = 1;
        result->function_results[0].spill_intended_flags[value_ids[spill_index]] = 1;
        result->function_results[0].spill_confirmed_flags[value_ids[spill_index]] = 1;
        result->function_results[0].spill_slots[value_ids[spill_index]] = spill_index;
    }

    (void)error;
    return 1;
}

static int prepare_manual_spill_result_for_value(ValueSsaProgramAllocationResult *result,
    const ValueSsaProgram *program,
    size_t value_id,
    ValueSsaError *error) {
    const size_t value_ids[1] = {value_id};

    return prepare_manual_spill_result_for_values(result, program, value_ids, 1, error);
}

static int build_alloc_smoke_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t joined_value;
    size_t moved_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "main", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    joined_value = value_ssa_function_allocate_value(function);
    moved_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 || else_value == (size_t)-1 ||
        joined_value == (size_t)-1 || moved_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(then_value);
        phi_inputs[1].predecessor_block_id = 2;
        phi_inputs[1].value = value_ssa_value_id(else_value);
        if (!value_ssa_block_append_phi(join_block, joined_value, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.result = value_ssa_value_id(moved_value);
    instruction.as.mov_value = value_ssa_value_id(joined_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(moved_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t sum0;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        sum0 == (size_t)-1 || sum1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_evict_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t move3;
    size_t sum0;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "evict", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    move3 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        move3 == (size_t)-1 || sum0 == (size_t)-1 || sum1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(move3);
    instruction.as.mov_value = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(move3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_affinity_weighted_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t then_value;
    size_t else_value;
    size_t then_move;
    size_t else_move0;
    size_t else_move1;
    size_t join_a;
    size_t join_b;
    size_t final_move;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "affinity", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    then_value = value_ssa_function_allocate_value(function);
    else_value = value_ssa_function_allocate_value(function);
    then_move = value_ssa_function_allocate_value(function);
    else_move0 = value_ssa_function_allocate_value(function);
    else_move1 = value_ssa_function_allocate_value(function);
    join_a = value_ssa_function_allocate_value(function);
    join_b = value_ssa_function_allocate_value(function);
    final_move = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || then_value == (size_t)-1 || else_value == (size_t)-1 ||
        then_move == (size_t)-1 || else_move0 == (size_t)-1 || else_move1 == (size_t)-1 ||
        join_a == (size_t)-1 || join_b == (size_t)-1 || final_move == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(then_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(then_move);
    instruction.as.mov_value = value_ssa_value_id(then_value);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_jump(then_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_value);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(else_move0);
    instruction.as.mov_value = value_ssa_value_id(else_value);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(else_move1);
    instruction.as.mov_value = value_ssa_value_id(else_move0);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(then_move);
        phi_inputs[1].predecessor_block_id = 2;
        phi_inputs[1].value = value_ssa_value_id(else_move1);
        if (!value_ssa_block_append_phi(join_block, join_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.result = value_ssa_value_id(join_b);
    instruction.as.mov_value = value_ssa_value_id(then_value);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(final_move);
    instruction.as.mov_value = value_ssa_value_id(join_a);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(join_b), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_affinity_downgrade_program(ValueSsaProgram *program,
    size_t *out_source_value,
    size_t *out_copy_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t source_value;
    size_t copy_value;
    size_t blocker_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "affinity_downgrade", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    source_value = value_ssa_function_allocate_value(function);
    copy_value = value_ssa_function_allocate_value(function);
    blocker_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (source_value == (size_t)-1 || copy_value == (size_t)-1 ||
        blocker_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(source_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(copy_value);
    instruction.as.mov_value = value_ssa_value_id(source_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(blocker_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(copy_value);
    instruction.as.binary.rhs = value_ssa_value_id(blocker_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_source_value) {
        *out_source_value = source_value;
    }
    if (out_copy_value) {
        *out_copy_value = copy_value;
    }
    return 1;
}

static int build_alloc_move_related_drop_program(ValueSsaProgram *program,
    size_t *out_root_value,
    size_t *out_mid_value,
    size_t *out_leaf_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t root_value;
    size_t mid_value;
    size_t leaf_value;
    size_t blocker_a;
    size_t blocker_b;
    size_t sum_a;
    size_t sum_b;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "move_related_drop", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    root_value = value_ssa_function_allocate_value(function);
    mid_value = value_ssa_function_allocate_value(function);
    leaf_value = value_ssa_function_allocate_value(function);
    blocker_a = value_ssa_function_allocate_value(function);
    blocker_b = value_ssa_function_allocate_value(function);
    sum_a = value_ssa_function_allocate_value(function);
    sum_b = value_ssa_function_allocate_value(function);
    if (root_value == (size_t)-1 || mid_value == (size_t)-1 || leaf_value == (size_t)-1 ||
        blocker_a == (size_t)-1 || blocker_b == (size_t)-1 || sum_a == (size_t)-1 || sum_b == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(root_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(mid_value);
    instruction.as.mov_value = value_ssa_value_id(root_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(leaf_value);
    instruction.as.mov_value = value_ssa_value_id(mid_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(blocker_a);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(blocker_b);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(mid_value);
    instruction.as.binary.rhs = value_ssa_value_id(blocker_a);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum_b);
    instruction.as.binary.lhs = value_ssa_value_id(leaf_value);
    instruction.as.binary.rhs = value_ssa_value_id(blocker_b);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum_b), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_root_value) {
        *out_root_value = root_value;
    }
    if (out_mid_value) {
        *out_mid_value = mid_value;
    }
    if (out_leaf_value) {
        *out_leaf_value = leaf_value;
    }
    return 1;
}

static int build_alloc_coalesce_safe_program(ValueSsaProgram *program,
    size_t *out_lhs_value,
    size_t *out_rhs_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t lhs_value;
    size_t rhs_value;
    size_t exit_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "coalesce_safe", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    lhs_value = value_ssa_function_allocate_value(function);
    rhs_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (lhs_value == (size_t)-1 || rhs_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(lhs_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(rhs_value);
    instruction.as.mov_value = value_ssa_value_id(lhs_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.mov_value = value_ssa_value_id(rhs_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_lhs_value) {
        *out_lhs_value = lhs_value;
    }
    if (out_rhs_value) {
        *out_rhs_value = rhs_value;
    }
    return 1;
}

static int build_alloc_coalesce_interfering_program(ValueSsaProgram *program,
    size_t *out_lhs_value,
    size_t *out_rhs_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t lhs_value;
    size_t rhs_value;
    size_t use_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "coalesce_interfering", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    lhs_value = value_ssa_function_allocate_value(function);
    rhs_value = value_ssa_function_allocate_value(function);
    use_value = value_ssa_function_allocate_value(function);
    if (lhs_value == (size_t)-1 || rhs_value == (size_t)-1 || use_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(lhs_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(rhs_value);
    instruction.as.mov_value = value_ssa_value_id(lhs_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(use_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(lhs_value);
    instruction.as.binary.rhs = value_ssa_value_id(rhs_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(use_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_lhs_value) {
        *out_lhs_value = lhs_value;
    }
    if (out_rhs_value) {
        *out_rhs_value = rhs_value;
    }
    return 1;
}

static int build_alloc_coalesce_budget_blocked_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "coalesce_budget_blocked", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->next_value_id = 4;
    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }
    return 1;
}

static int build_alloc_coalesce_george_unsafe_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "coalesce_george_unsafe", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    function->next_value_id = 6;
    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }
    return 1;
}

static int build_alloc_coalesce_chain_program(ValueSsaProgram *program,
    size_t *out_root_value,
    size_t *out_mid_value,
    size_t *out_leaf_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t root_value;
    size_t mid_value;
    size_t leaf_value;
    size_t exit_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "coalesce_chain", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    root_value = value_ssa_function_allocate_value(function);
    mid_value = value_ssa_function_allocate_value(function);
    leaf_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (root_value == (size_t)-1 || mid_value == (size_t)-1 || leaf_value == (size_t)-1 ||
        exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(root_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(mid_value);
    instruction.as.mov_value = value_ssa_value_id(root_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(leaf_value);
    instruction.as.mov_value = value_ssa_value_id(mid_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.mov_value = value_ssa_value_id(leaf_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_root_value) {
        *out_root_value = root_value;
    }
    if (out_mid_value) {
        *out_mid_value = mid_value;
    }
    if (out_leaf_value) {
        *out_leaf_value = leaf_value;
    }
    return 1;
}

static int build_alloc_unique_blocker_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t move3;
    size_t move4;
    size_t sum0;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "unique_blocker", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    move3 = value_ssa_function_allocate_value(function);
    move4 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 || move3 == (size_t)-1 ||
        move4 == (size_t)-1 || sum0 == (size_t)-1 || sum1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(move3);
    instruction.as.mov_value = value_ssa_value_id(value0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(move4);
    instruction.as.mov_value = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(move3);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(move4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_spill_cost_order_program(ValueSsaProgram *program,
    size_t *out_low_cost_value,
    size_t *out_high_cost_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t low_cost_value;
    size_t middle_value;
    size_t high_cost_value;
    size_t sum0;
    size_t sum1;
    size_t sum2;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_cost_order", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    low_cost_value = value_ssa_function_allocate_value(function);
    middle_value = value_ssa_function_allocate_value(function);
    high_cost_value = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    sum2 = value_ssa_function_allocate_value(function);
    if (low_cost_value == (size_t)-1 || middle_value == (size_t)-1 || high_cost_value == (size_t)-1 ||
        sum0 == (size_t)-1 || sum1 == (size_t)-1 || sum2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(low_cost_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(middle_value);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(high_cost_value);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(low_cost_value);
    instruction.as.binary.rhs = value_ssa_value_id(middle_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(high_cost_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum2);
    instruction.as.binary.lhs = value_ssa_value_id(sum1);
    instruction.as.binary.rhs = value_ssa_value_id(high_cost_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_low_cost_value) {
        *out_low_cost_value = low_cost_value;
    }
    if (out_high_cost_value) {
        *out_high_cost_value = high_cost_value;
    }
    return 1;
}

static int build_alloc_manual_spill_cost_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_spill_cost", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_runtime_family_color_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_runtime_family_color", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    for (value_id = 0; value_id < 5; ++value_id) {
        size_t allocated_value_id = value_ssa_function_allocate_value(function);

        if (allocated_value_id == (size_t)-1) {
            value_ssa_program_free(program);
            return 0;
        }
        instruction.result = value_ssa_value_id(allocated_value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_call_density_prep_program(ValueSsaProgram *program,
    size_t *out_call_live_value,
    size_t *out_post_call_value,
    ValueSsaError *error) {
    ValueSsaFunction *decl = NULL;
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *call_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaValueRef *call_args = NULL;
    size_t call_live_value;
    size_t post_call_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "sink", 1, &decl, error) ||
        !value_ssa_program_append_function(program, "call_density", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &call_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    call_live_value = value_ssa_function_allocate_value(function);
    post_call_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (call_live_value == (size_t)-1 || post_call_value == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }
    call_args = (ValueSsaValueRef *)malloc(sizeof(ValueSsaValueRef));
    if (!call_args) {
        value_ssa_program_free(program);
        return 0;
    }
    call_args[0] = value_ssa_value_id(call_live_value);

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(call_live_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, call_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_CALL;
    instruction.has_result = 0;
    instruction.as.call.callee_name = "sink";
    instruction.as.call.args = call_args;
    instruction.as.call.arg_count = 1;
    if (!value_ssa_block_append_instruction(call_block, &instruction, error) ||
        !value_ssa_block_set_jump(call_block, exit_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(post_call_value);
    instruction.as.mov_value = value_ssa_value_immediate(11);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(call_live_value);
    instruction.as.binary.rhs = value_ssa_value_id(post_call_value);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_call_live_value) {
        *out_call_live_value = call_live_value;
    }
    if (out_post_call_value) {
        *out_post_call_value = post_call_value;
    }
    return 1;
}

static int build_alloc_manual_eviction_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_eviction", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t value3;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    value3 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 || value3 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(value3);
    instruction.as.mov_value = value_ssa_value_immediate(4);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_evict_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry_evict", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 5; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_dual_evict_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry_dual_evict", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 6; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_blocker_family_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry_blocker_family", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 7; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_targeted_recolor_family_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_targeted_recolor_family", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 5; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_main_recolor_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_main_recolor", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 6; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_preferred_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry_preferred", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 6; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_class_size_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry_class_size", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 8; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_main_family_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_main_family", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 4; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_family_pressure_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_family_pressure", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 5; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_manual_retry_family_batch_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value_id;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "manual_retry_family_batch", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    for (value_id = 0; value_id < 8; ++value_id) {
        size_t allocated = value_ssa_function_allocate_value(function);

        if (allocated == (size_t)-1 || allocated != value_id) {
            value_ssa_program_free(program);
            return 0;
        }

        memset(&instruction, 0, sizeof(instruction));
        instruction.kind = VALUE_SSA_INSTR_MOV;
        instruction.has_result = 1;
        instruction.result = value_ssa_value_id(value_id);
        instruction.as.mov_value = value_ssa_value_immediate((int)(value_id + 1));
        if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_return(entry, value_ssa_value_immediate(0), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int setup_manual_eviction_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan,
    size_t blocker1_priority,
    ValueSsaAllocatorRemovalKind blocker1_removal_kind,
    size_t blocker2_priority,
    ValueSsaAllocatorRemovalKind blocker2_removal_kind,
    int blocker1_rematerializable,
    int blocker2_rematerializable) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 3) {
        return 0;
    }

    prep->value_count = 3;
    prep->use_counts = (size_t *)calloc(3, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(3, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(3, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(3, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(3, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(3, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(3, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(3, sizeof(unsigned char));
    interference->value_count = 3;
    interference->interferes = (unsigned char *)calloc(9, sizeof(unsigned char));
    plan->value_count = 3;
    plan->item_count = 3;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(3, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->interference_degrees[0] = 2;
    prep->interference_degrees[1] = 2;
    prep->interference_degrees[2] = 2;
    prep->rematerializable[1] = blocker1_rematerializable ? 1u : 0u;
    prep->rematerializable[2] = blocker2_rematerializable ? 1u : 0u;

    interference->interferes[0 * 3 + 1] = 1;
    interference->interferes[1 * 3 + 0] = 1;
    interference->interferes[0 * 3 + 2] = 1;
    interference->interferes[2 * 3 + 0] = 1;
    interference->interferes[1 * 3 + 2] = 1;
    interference->interferes[2 * 3 + 1] = 1;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 20;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[0].spill_use_cost = prep->use_counts[0];
    plan->items[0].spill_live_range_cost = prep->live_block_counts[0];
    plan->items[0].spill_affinity_cost = prep->affinity_sums[0];
    plan->items[0].spill_cost =
        plan->items[0].spill_use_cost + plan->items[0].spill_live_range_cost + plan->items[0].spill_affinity_cost;
    plan->items[1].value_id = 1;
    plan->items[1].priority = blocker1_priority;
    plan->items[1].removal_kind = blocker1_removal_kind;
    plan->items[1].spill_use_cost = prep->use_counts[1];
    plan->items[1].spill_live_range_cost = prep->live_block_counts[1];
    plan->items[1].spill_affinity_cost = prep->affinity_sums[1];
    plan->items[1].spill_cost =
        plan->items[1].spill_use_cost + plan->items[1].spill_live_range_cost + plan->items[1].spill_affinity_cost;
    plan->items[2].value_id = 2;
    plan->items[2].priority = blocker2_priority;
    plan->items[2].removal_kind = blocker2_removal_kind;
    plan->items[2].spill_use_cost = prep->use_counts[2];
    plan->items[2].spill_live_range_cost = prep->live_block_counts[2];
    plan->items[2].spill_affinity_cost = prep->affinity_sums[2];
    plan->items[2].spill_cost =
        plan->items[2].spill_use_cost + plan->items[2].spill_live_range_cost + plan->items[2].spill_affinity_cost;
    return 1;
}

static int setup_manual_retry_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 4) {
        return 0;
    }

    prep->value_count = 4;
    prep->use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(4, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(4, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(4, sizeof(unsigned char));
    interference->value_count = 4;
    interference->interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    plan->value_count = 4;
    plan->item_count = 4;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 3;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 2;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->interference_degrees[0] = 2;
    prep->interference_degrees[1] = 3;
    prep->interference_degrees[2] = 3;
    prep->interference_degrees[3] = 2;

    interference->interferes[0 * 4 + 1] = 1;
    interference->interferes[1 * 4 + 0] = 1;
    interference->interferes[0 * 4 + 2] = 1;
    interference->interferes[2 * 4 + 0] = 1;
    interference->interferes[1 * 4 + 2] = 1;
    interference->interferes[2 * 4 + 1] = 1;
    interference->interferes[1 * 4 + 3] = 1;
    interference->interferes[3 * 4 + 1] = 1;
    interference->interferes[2 * 4 + 3] = 1;
    interference->interferes[3 * 4 + 2] = 1;

    plan->items[0].value_id = 3;
    plan->items[0].priority = 18;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[0].spill_use_cost = prep->use_counts[3];
    plan->items[0].spill_live_range_cost = prep->live_block_counts[3];
    plan->items[0].spill_affinity_cost = prep->affinity_sums[3];
    plan->items[0].spill_cost =
        plan->items[0].spill_use_cost + plan->items[0].spill_live_range_cost + plan->items[0].spill_affinity_cost;
    plan->items[1].value_id = 0;
    plan->items[1].priority = 10;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].spill_use_cost = prep->use_counts[0];
    plan->items[1].spill_live_range_cost = prep->live_block_counts[0];
    plan->items[1].spill_affinity_cost = prep->affinity_sums[0];
    plan->items[1].spill_cost =
        plan->items[1].spill_use_cost + plan->items[1].spill_live_range_cost + plan->items[1].spill_affinity_cost;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 15;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].spill_use_cost = prep->use_counts[1];
    plan->items[2].spill_live_range_cost = prep->live_block_counts[1];
    plan->items[2].spill_affinity_cost = prep->affinity_sums[1];
    plan->items[2].spill_cost =
        plan->items[2].spill_use_cost + plan->items[2].spill_live_range_cost + plan->items[2].spill_affinity_cost;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 20;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].spill_use_cost = prep->use_counts[2];
    plan->items[3].spill_live_range_cost = prep->live_block_counts[2];
    plan->items[3].spill_affinity_cost = prep->affinity_sums[2];
    plan->items[3].spill_cost =
        plan->items[3].spill_use_cost + plan->items[3].spill_live_range_cost + plan->items[3].spill_affinity_cost;
    return 1;
}

static int setup_manual_retry_evict_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan,
    int block_retry_recolor) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    prep->value_count = 5;
    prep->use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference->value_count = 5;
    interference->interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    plan->value_count = 5;
    plan->item_count = 5;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 2;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->interference_degrees[0] = 3;
    prep->interference_degrees[1] = 3;
    prep->interference_degrees[2] = 2;
    prep->interference_degrees[3] = 1;
    prep->interference_degrees[4] = 1;

    interference->interferes[0 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 0] = 1;
    interference->interferes[1 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 1] = 1;
    interference->interferes[2 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 2] = 1;
    if (block_retry_recolor) {
        interference->interferes[2 * 5 + 4] = 1;
        interference->interferes[4 * 5 + 2] = 1;
    }

    plan->items[0].value_id = 3;
    plan->items[0].priority = 25;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 0;
    plan->items[1].priority = 15;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].value_id = 2;
    plan->items[2].priority = 20;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].value_id = 1;
    plan->items[3].priority = 20;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 4;
    plan->items[4].priority = 10;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 5; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_intended_bias_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    prep->value_count = 5;
    prep->use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference->value_count = 5;
    interference->interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    plan->value_count = 5;
    plan->item_count = 5;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 4;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->interference_degrees[0] = 3;
    prep->interference_degrees[1] = 3;
    prep->interference_degrees[2] = 4;
    prep->interference_degrees[3] = 4;
    prep->interference_degrees[4] = 2;

    interference->interferes[0 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 0] = 1;
    interference->interferes[1 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 1] = 1;
    interference->interferes[2 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 2] = 1;
    interference->interferes[3 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 3] = 1;

    plan->items[0].value_id = 4;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 0;
    plan->items[1].priority = 15;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 15;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].value_id = 3;
    plan->items[3].priority = 18;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 2;
    plan->items[4].priority = 20;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 5; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_main_recolor_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 6) {
        return 0;
    }

    prep->value_count = 6;
    prep->use_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(6, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(6, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(6, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(6, sizeof(unsigned char));
    interference->value_count = 6;
    interference->interferes = (unsigned char *)calloc(36, sizeof(unsigned char));
    plan->value_count = 6;
    plan->item_count = 6;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(6, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 2;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;

    interference->interferes[4 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 4] = 1;
    interference->interferes[3 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 3] = 1;
    interference->interferes[2 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 2] = 1;
    interference->interferes[1 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 1] = 1;
    interference->interferes[0 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 0] = 1;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].value_id = 1;
    plan->items[1].priority = 10;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].value_id = 2;
    plan->items[2].priority = 10;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].value_id = 3;
    plan->items[3].priority = 25;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 5;
    plan->items[4].priority = 15;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 4;
    plan->items[5].priority = 20;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 6; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_targeted_recolor_family_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *analysis,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !analysis || !plan || function->next_value_id != 5) {
        return 0;
    }

    prep->value_count = 5;
    prep->use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference->value_count = 5;
    interference->interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    analysis->value_count = 5;
    analysis->item_count = 1;
    analysis->items = (ValueSsaAllocatorCoalesceItem *)calloc(1, sizeof(ValueSsaAllocatorCoalesceItem));
    analysis->value_roots = (size_t *)calloc(5, sizeof(size_t));
    analysis->class_sizes = (size_t *)calloc(5, sizeof(size_t));
    plan->value_count = 5;
    plan->item_count = 5;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !analysis->items || !analysis->value_roots || !analysis->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;

    interference->interferes[2 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 2] = 1;
    interference->interferes[3 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 3] = 1;
    interference->interferes[0 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 0] = 1;

    analysis->items[0].lhs_value_id = 0;
    analysis->items[0].rhs_value_id = 4;
    analysis->items[0].weight = 1;
    analysis->items[0].can_coalesce = 1;
    analysis->value_roots[0] = 0;
    analysis->value_roots[1] = 1;
    analysis->value_roots[2] = 1;
    analysis->value_roots[3] = 3;
    analysis->value_roots[4] = 0;
    analysis->class_sizes[0] = 2;
    analysis->class_sizes[1] = 2;
    analysis->class_sizes[3] = 1;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 50;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[0].coalesce_root_value_id = 0;
    plan->items[0].coalesce_class_size = 2;
    plan->items[1].value_id = 2;
    plan->items[1].priority = 10;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].coalesce_root_value_id = 1;
    plan->items[1].coalesce_class_size = 2;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 15;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].coalesce_root_value_id = 1;
    plan->items[2].coalesce_class_size = 2;
    plan->items[3].value_id = 3;
    plan->items[3].priority = 20;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].coalesce_root_value_id = 3;
    plan->items[3].coalesce_class_size = 1;
    plan->items[4].value_id = 4;
    plan->items[4].priority = 25;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].coalesce_root_value_id = 0;
    plan->items[4].coalesce_class_size = 2;

    {
        size_t item_index;
        for (item_index = 0; item_index < 5; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_weighted_targeted_recolor_family_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *analysis,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !analysis || !plan || function->next_value_id != 5) {
        return 0;
    }

    prep->value_count = 5;
    prep->use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference->value_count = 5;
    interference->interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    analysis->value_count = 5;
    analysis->item_count = 3;
    analysis->items = (ValueSsaAllocatorCoalesceItem *)calloc(3, sizeof(ValueSsaAllocatorCoalesceItem));
    analysis->value_roots = (size_t *)calloc(5, sizeof(size_t));
    analysis->class_sizes = (size_t *)calloc(5, sizeof(size_t));
    plan->value_count = 5;
    plan->item_count = 5;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !analysis->items || !analysis->value_roots || !analysis->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;

    interference->interferes[0 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 0] = 1;
    interference->interferes[2 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 2] = 1;
    interference->interferes[3 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 3] = 1;

    analysis->items[0].lhs_value_id = 0;
    analysis->items[0].rhs_value_id = 4;
    analysis->items[0].weight = 1;
    analysis->items[0].can_coalesce = 1;
    analysis->items[1].lhs_value_id = 1;
    analysis->items[1].rhs_value_id = 2;
    analysis->items[1].weight = 1;
    analysis->items[1].can_coalesce = 1;
    analysis->items[2].lhs_value_id = 1;
    analysis->items[2].rhs_value_id = 3;
    analysis->items[2].weight = 5;
    analysis->items[2].can_coalesce = 1;

    analysis->value_roots[0] = 0;
    analysis->value_roots[1] = 1;
    analysis->value_roots[2] = 1;
    analysis->value_roots[3] = 1;
    analysis->value_roots[4] = 0;
    analysis->class_sizes[0] = 2;
    analysis->class_sizes[1] = 3;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 50;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[0].coalesce_root_value_id = 0;
    plan->items[0].coalesce_class_size = 2;
    plan->items[1].value_id = 1;
    plan->items[1].priority = 15;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].coalesce_root_value_id = 1;
    plan->items[1].coalesce_class_size = 3;
    plan->items[2].value_id = 3;
    plan->items[2].priority = 20;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].coalesce_root_value_id = 1;
    plan->items[2].coalesce_class_size = 3;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 10;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].coalesce_root_value_id = 1;
    plan->items[3].coalesce_class_size = 3;
    plan->items[4].value_id = 4;
    plan->items[4].priority = 25;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].coalesce_root_value_id = 0;
    plan->items[4].coalesce_class_size = 2;

    {
        size_t item_index;
        for (item_index = 0; item_index < 5; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_preferred_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 6) {
        return 0;
    }

    prep->value_count = 6;
    prep->use_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(6, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(6, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(6, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(6, sizeof(unsigned char));
    interference->value_count = 6;
    interference->interferes = (unsigned char *)calloc(36, sizeof(unsigned char));
    coalesce->value_count = 6;
    coalesce->item_count = 1;
    coalesce->items = (ValueSsaAllocatorCoalesceItem *)calloc(1, sizeof(ValueSsaAllocatorCoalesceItem));
    coalesce->value_roots = (size_t *)calloc(6, sizeof(size_t));
    coalesce->class_sizes = (size_t *)calloc(6, sizeof(size_t));
    plan->value_count = 6;
    plan->item_count = 6;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(6, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !coalesce->items || !coalesce->value_roots || !coalesce->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;

    interference->interferes[0 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 0] = 1;
    interference->interferes[2 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 2] = 1;
    interference->interferes[1 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 1] = 1;
    interference->interferes[2 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 2] = 1;
    interference->interferes[3 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 3] = 1;

    coalesce->items[0].lhs_value_id = 1;
    coalesce->items[0].rhs_value_id = 4;
    coalesce->items[0].weight = 1;
    coalesce->items[0].can_coalesce = 1;
    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 3;
    coalesce->value_roots[4] = 1;
    coalesce->value_roots[5] = 5;
    coalesce->class_sizes[0] = 1;
    coalesce->class_sizes[1] = 2;
    coalesce->class_sizes[2] = 1;
    coalesce->class_sizes[3] = 1;
    coalesce->class_sizes[5] = 1;

    plan->items[0].value_id = 5;
    plan->items[0].priority = 20;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 1;
    plan->items[1].priority = 15;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].value_id = 0;
    plan->items[2].priority = 15;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 17;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 3;
    plan->items[4].priority = 25;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 4;
    plan->items[5].priority = 30;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 6; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_class_size_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 8) {
        return 0;
    }

    prep->value_count = 8;
    prep->use_counts = (size_t *)calloc(8, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(8, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(8, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(8, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(8, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(8, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(8, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(8, sizeof(unsigned char));
    interference->value_count = 8;
    interference->interferes = (unsigned char *)calloc(64, sizeof(unsigned char));
    coalesce->value_count = 8;
    coalesce->item_count = 0;
    coalesce->items = NULL;
    coalesce->value_roots = (size_t *)calloc(8, sizeof(size_t));
    coalesce->class_sizes = (size_t *)calloc(8, sizeof(size_t));
    plan->value_count = 8;
    plan->item_count = 8;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(8, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !coalesce->value_roots || !coalesce->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->use_counts[6] = 1;
    prep->use_counts[7] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;
    prep->live_block_counts[6] = 1;
    prep->live_block_counts[7] = 1;

    interference->interferes[0 * 8 + 1] = 1;
    interference->interferes[1 * 8 + 0] = 1;
    interference->interferes[0 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 0] = 1;
    interference->interferes[2 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 2] = 1;
    interference->interferes[0 * 8 + 4] = 1;
    interference->interferes[4 * 8 + 0] = 1;
    interference->interferes[0 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 0] = 1;
    interference->interferes[0 * 8 + 6] = 1;
    interference->interferes[6 * 8 + 0] = 1;
    interference->interferes[1 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 1] = 1;
    interference->interferes[1 * 8 + 6] = 1;
    interference->interferes[6 * 8 + 1] = 1;
    interference->interferes[3 * 8 + 6] = 1;
    interference->interferes[6 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 4] = 1;
    interference->interferes[4 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 7] = 1;
    interference->interferes[7 * 8 + 3] = 1;
    interference->interferes[6 * 8 + 7] = 1;
    interference->interferes[7 * 8 + 6] = 1;

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 0;
    coalesce->value_roots[3] = 3;
    coalesce->value_roots[4] = 1;
    coalesce->value_roots[5] = 1;
    coalesce->value_roots[6] = 6;
    coalesce->value_roots[7] = 7;
    coalesce->class_sizes[0] = 2;
    coalesce->class_sizes[1] = 3;
    coalesce->class_sizes[3] = 1;
    coalesce->class_sizes[6] = 1;
    coalesce->class_sizes[7] = 1;

    plan->items[0].value_id = 7;
    plan->items[0].priority = 20;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 1;
    plan->items[1].priority = 15;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].value_id = 0;
    plan->items[2].priority = 15;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[3].value_id = 6;
    plan->items[3].priority = 17;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 5;
    plan->items[4].priority = 22;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 4;
    plan->items[5].priority = 24;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[6].value_id = 3;
    plan->items[6].priority = 25;
    plan->items[6].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[7].value_id = 2;
    plan->items[7].priority = 30;
    plan->items[7].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 8; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_main_family_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 4) {
        return 0;
    }

    prep->value_count = 4;
    prep->use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(4, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(4, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(4, sizeof(unsigned char));
    interference->value_count = 4;
    interference->interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    coalesce->value_count = 4;
    coalesce->item_count = 0;
    coalesce->items = NULL;
    coalesce->value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce->class_sizes = (size_t *)calloc(4, sizeof(size_t));
    plan->value_count = 4;
    plan->item_count = 4;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !coalesce->value_roots || !coalesce->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 2;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;

    interference->interferes[0 * 4 + 1] = 1;
    interference->interferes[1 * 4 + 0] = 1;
    interference->interferes[0 * 4 + 2] = 1;
    interference->interferes[2 * 4 + 0] = 1;
    interference->interferes[1 * 4 + 2] = 1;
    interference->interferes[2 * 4 + 1] = 1;

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 1;
    coalesce->class_sizes[0] = 1;
    coalesce->class_sizes[1] = 2;
    coalesce->class_sizes[2] = 1;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].value_id = 1;
    plan->items[1].priority = 10;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].value_id = 2;
    plan->items[2].priority = 10;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].value_id = 3;
    plan->items[3].priority = 5;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 4; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_main_family_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 5) {
        return 0;
    }

    prep->value_count = 5;
    prep->use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference->value_count = 5;
    interference->interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    coalesce->value_count = 5;
    coalesce->item_count = 0;
    coalesce->items = NULL;
    coalesce->value_roots = (size_t *)calloc(5, sizeof(size_t));
    coalesce->class_sizes = (size_t *)calloc(5, sizeof(size_t));
    plan->value_count = 5;
    plan->item_count = 5;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !coalesce->value_roots || !coalesce->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 2;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;

    interference->interferes[0 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 0] = 1;
    interference->interferes[1 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 1] = 1;

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 1;
    coalesce->value_roots[4] = 2;
    coalesce->class_sizes[0] = 1;
    coalesce->class_sizes[1] = 2;
    coalesce->class_sizes[2] = 2;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].value_id = 1;
    plan->items[1].priority = 10;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[1].family_external_neighbor_count = 1;
    plan->items[1].family_external_affinity_weight_sum = 4;
    plan->items[2].value_id = 2;
    plan->items[2].priority = 10;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[2].family_external_neighbor_count = 1;
    plan->items[2].family_external_affinity_weight_sum = 1;
    plan->items[3].value_id = 3;
    plan->items[3].priority = 5;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[3].family_external_neighbor_count = 1;
    plan->items[3].family_external_affinity_weight_sum = 4;
    plan->items[4].value_id = 4;
    plan->items[4].priority = 5;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[4].family_external_neighbor_count = 1;
    plan->items[4].family_external_affinity_weight_sum = 1;

    {
        size_t item_index;
        for (item_index = 0; item_index < 5; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_main_family_neighbor_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 5) {
        return 0;
    }

    if (!setup_manual_main_family_pressure_artifacts(function, prep, interference, coalesce, plan)) {
        return 0;
    }

    plan->items[1].family_external_neighbor_count = 3;
    plan->items[1].family_external_affinity_weight_sum = 2;
    plan->items[2].family_external_neighbor_count = 1;
    plan->items[2].family_external_affinity_weight_sum = 2;
    plan->items[3].family_external_neighbor_count = 3;
    plan->items[3].family_external_affinity_weight_sum = 2;
    plan->items[4].family_external_neighbor_count = 1;
    plan->items[4].family_external_affinity_weight_sum = 2;
    return 1;
}

static int setup_manual_main_family_member_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 5) {
        return 0;
    }

    if (!setup_manual_main_family_pressure_artifacts(function, prep, interference, coalesce, plan)) {
        return 0;
    }

    plan->items[1].family_external_neighbor_count = 1;
    plan->items[1].family_external_affinity_weight_sum = 2;
    plan->items[1].family_active_member_count = 2;
    plan->items[2].family_external_neighbor_count = 1;
    plan->items[2].family_external_affinity_weight_sum = 2;
    plan->items[2].family_active_member_count = 1;
    plan->items[3].family_external_neighbor_count = 1;
    plan->items[3].family_external_affinity_weight_sum = 2;
    plan->items[3].family_active_member_count = 2;
    plan->items[4].family_external_neighbor_count = 1;
    plan->items[4].family_external_affinity_weight_sum = 2;
    plan->items[4].family_active_member_count = 1;
    return 1;
}

static int setup_manual_main_runtime_family_activity_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    ValueSsaAllocatorPlanItem item4;
    ValueSsaAllocatorPlanItem item0;
    ValueSsaAllocatorPlanItem item2;
    ValueSsaAllocatorPlanItem item1;
    ValueSsaAllocatorPlanItem item3;

    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 5) {
        return 0;
    }
    if (!setup_manual_main_family_member_pressure_artifacts(function, prep, interference, coalesce, plan)) {
        return 0;
    }

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 1;
    coalesce->value_roots[4] = 2;
    coalesce->class_sizes[0] = 1;
    coalesce->class_sizes[1] = 2;
    coalesce->class_sizes[2] = 2;

    item4 = plan->items[4];
    item0 = plan->items[0];
    item2 = plan->items[2];
    item1 = plan->items[1];
    item3 = plan->items[3];
    plan->items[0] = item4;
    plan->items[1] = item0;
    plan->items[2] = item2;
    plan->items[3] = item1;
    plan->items[4] = item3;

    {
        size_t item_index;
        for (item_index = 0; item_index < plan->item_count; ++item_index) {
            ValueSsaAllocatorPlanItem *item = &plan->items[item_index];

            item->move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
            item->family_external_neighbor_count = 1;
            item->family_external_affinity_weight_sum = 2;
            item->family_active_member_count = (item->value_id == 0) ? 1 : (item->value_id == 1 || item->value_id == 3 ||
                                                                              item->value_id == 2 || item->value_id == 4)
                    ? 2
                    : item->family_active_member_count;
            if (item->value_id == 0) {
                item->coalesce_root_value_id = 0;
                item->coalesce_class_size = 1;
            } else if (item->value_id == 1 || item->value_id == 3) {
                item->coalesce_root_value_id = 1;
                item->coalesce_class_size = 2;
            } else if (item->value_id == 2 || item->value_id == 4) {
                item->coalesce_root_value_id = 2;
                item->coalesce_class_size = 2;
            }
        }
    }

    return 1;
}

static int setup_manual_main_recolor_target_support_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 7) {
        return 0;
    }

    prep->value_count = 7;
    prep->use_counts = (size_t *)calloc(7, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(7, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(7, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(7, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(7, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(7, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(7, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(7, sizeof(unsigned char));
    interference->value_count = 7;
    interference->interferes = (unsigned char *)calloc(49, sizeof(unsigned char));
    coalesce->value_count = 7;
    coalesce->item_count = 0;
    coalesce->items = NULL;
    coalesce->value_roots = (size_t *)calloc(7, sizeof(size_t));
    coalesce->class_sizes = (size_t *)calloc(7, sizeof(size_t));
    plan->value_count = 7;
    plan->item_count = 7;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(7, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !coalesce->value_roots || !coalesce->class_sizes || !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 2;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->use_counts[6] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;
    prep->live_block_counts[6] = 1;
    prep->interference_degrees[0] = 2;
    prep->interference_degrees[1] = 2;
    prep->interference_degrees[2] = 2;
    prep->interference_degrees[3] = 2;
    prep->interference_degrees[4] = 1;
    prep->interference_degrees[5] = 1;
    prep->interference_degrees[6] = 0;

    interference->interferes[0 * 7 + 1] = 1;
    interference->interferes[1 * 7 + 0] = 1;
    interference->interferes[0 * 7 + 2] = 1;
    interference->interferes[2 * 7 + 0] = 1;
    interference->interferes[0 * 7 + 6] = 1;
    interference->interferes[6 * 7 + 0] = 1;
    interference->interferes[1 * 7 + 4] = 1;
    interference->interferes[4 * 7 + 1] = 1;
    interference->interferes[3 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 3] = 1;
    interference->interferes[3 * 7 + 4] = 1;
    interference->interferes[4 * 7 + 3] = 1;
    interference->interferes[4 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 4] = 1;
    interference->interferes[4 * 7 + 6] = 1;
    interference->interferes[6 * 7 + 4] = 1;
    interference->interferes[5 * 7 + 6] = 1;
    interference->interferes[6 * 7 + 5] = 1;

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 1;
    coalesce->value_roots[4] = 4;
    coalesce->value_roots[5] = 5;
    coalesce->value_roots[6] = 6;
    coalesce->class_sizes[0] = 1;
    coalesce->class_sizes[1] = 2;
    coalesce->class_sizes[2] = 1;
    coalesce->class_sizes[4] = 1;
    coalesce->class_sizes[5] = 1;
    coalesce->class_sizes[6] = 1;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].value_id = 3;
    plan->items[1].priority = 8;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 10;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 10;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 6;
    plan->items[4].priority = 40;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 5;
    plan->items[5].priority = 6;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[6].value_id = 4;
    plan->items[6].priority = 5;
    plan->items[6].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    plan->items[1].coalesce_root_value_id = 1;
    plan->items[1].coalesce_class_size = 2;
    plan->items[2].coalesce_root_value_id = 1;
    plan->items[2].coalesce_class_size = 2;
    plan->items[3].coalesce_root_value_id = 2;
    plan->items[3].coalesce_class_size = 1;

    {
        size_t item_index;
        for (item_index = 0; item_index < 7; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_weighted_main_recolor_target_support_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    ValueSsaAllocatorPlan *plan) {
    size_t item_index;

    if (!function || !prep || !interference || !coalesce || !plan || function->next_value_id != 7) {
        return 0;
    }
    if (!setup_manual_main_recolor_target_support_artifacts(function, prep, interference, coalesce, plan)) {
        return 0;
    }

    coalesce->item_count = 2;
    coalesce->items = (ValueSsaAllocatorCoalesceItem *)calloc(2, sizeof(ValueSsaAllocatorCoalesceItem));
    if (!coalesce->items) {
        return 0;
    }

    coalesce->items[0].lhs_value_id = 1;
    coalesce->items[0].rhs_value_id = 3;
    coalesce->items[0].weight = 1;
    coalesce->items[0].can_coalesce = 1;
    coalesce->items[1].lhs_value_id = 2;
    coalesce->items[1].rhs_value_id = 5;
    coalesce->items[1].weight = 3;
    coalesce->items[1].can_coalesce = 1;

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 1;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 1;
    coalesce->value_roots[4] = 4;
    coalesce->value_roots[5] = 2;
    coalesce->value_roots[6] = 6;
    coalesce->class_sizes[0] = 1;
    coalesce->class_sizes[1] = 2;
    coalesce->class_sizes[2] = 2;
    coalesce->class_sizes[4] = 1;
    coalesce->class_sizes[6] = 1;

    plan->items[0].value_id = 0;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].value_id = 3;
    plan->items[1].priority = 8;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 10;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].value_id = 6;
    plan->items[3].priority = 40;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 5;
    plan->items[4].priority = 6;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 2;
    plan->items[5].priority = 10;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[6].value_id = 4;
    plan->items[6].priority = 5;
    plan->items[6].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    for (item_index = 0; item_index < plan->item_count; ++item_index) {
        switch (plan->items[item_index].value_id) {
        case 1:
        case 3:
            plan->items[item_index].coalesce_root_value_id = 1;
            plan->items[item_index].coalesce_class_size = 2;
            break;
        case 2:
        case 5:
            plan->items[item_index].coalesce_root_value_id = 2;
            plan->items[item_index].coalesce_class_size = 2;
            break;
        default:
            break;
        }
    }

    return 1;
}

static int setup_manual_retry_family_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    prep->value_count = 5;
    prep->use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference->value_count = 5;
    interference->interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    plan->value_count = 5;
    plan->item_count = 5;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->interference_degrees[0] = 3;
    prep->interference_degrees[1] = 3;
    prep->interference_degrees[2] = 3;
    prep->interference_degrees[3] = 3;
    prep->interference_degrees[4] = 4;

    interference->interferes[0 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 0] = 1;
    interference->interferes[0 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 0] = 1;
    interference->interferes[1 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 1] = 1;
    interference->interferes[1 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 1] = 1;
    interference->interferes[2 * 5 + 3] = 1;
    interference->interferes[3 * 5 + 2] = 1;
    interference->interferes[2 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 2] = 1;
    interference->interferes[3 * 5 + 4] = 1;
    interference->interferes[4 * 5 + 3] = 1;

    plan->items[0].value_id = 3;
    plan->items[0].priority = 25;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 0;
    plan->items[1].priority = 15;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_RELEASED;
    plan->items[1].family_external_neighbor_count = 1;
    plan->items[1].family_external_affinity_weight_sum = 1;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 15;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[2].family_external_neighbor_count = 2;
    plan->items[2].family_external_affinity_weight_sum = 4;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 20;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 4;
    plan->items[4].priority = 30;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 5; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_member_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    if (!setup_manual_retry_family_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[1].family_external_neighbor_count = 1;
    plan->items[1].family_external_affinity_weight_sum = 2;
    plan->items[1].family_active_member_count = 1;
    plan->items[2].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[2].family_external_neighbor_count = 1;
    plan->items[2].family_external_affinity_weight_sum = 2;
    plan->items[2].family_active_member_count = 2;
    return 1;
}

static int setup_manual_retry_runtime_family_activity_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }
    if (!setup_manual_retry_member_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[2].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[1].family_external_neighbor_count = 1;
    plan->items[2].family_external_neighbor_count = 1;
    plan->items[1].family_external_affinity_weight_sum = 2;
    plan->items[2].family_external_affinity_weight_sum = 2;
    plan->items[1].family_active_member_count = 1;
    plan->items[2].family_active_member_count = 1;

    plan->items[0].coalesce_root_value_id = 1;
    plan->items[0].coalesce_class_size = 2;
    plan->items[1].coalesce_root_value_id = 0;
    plan->items[1].coalesce_class_size = 2;
    plan->items[2].coalesce_root_value_id = 1;
    plan->items[2].coalesce_class_size = 2;
    plan->items[3].coalesce_root_value_id = 0;
    plan->items[3].coalesce_class_size = 2;
    plan->items[4].coalesce_root_value_id = 4;
    plan->items[4].coalesce_class_size = 1;
    return 1;
}

static int setup_manual_retry_family_representative_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }
    if (!setup_manual_retry_family_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].priority = 15;
    plan->items[1].coalesce_root_value_id = 0;
    plan->items[1].coalesce_class_size = 2;
    plan->items[2].priority = 16;
    plan->items[2].coalesce_root_value_id = 0;
    plan->items[2].coalesce_class_size = 2;
    plan->items[0].coalesce_root_value_id = 3;
    plan->items[0].coalesce_class_size = 1;
    plan->items[3].coalesce_root_value_id = 2;
    plan->items[3].coalesce_class_size = 1;
    plan->items[4].coalesce_root_value_id = 4;
    plan->items[4].coalesce_class_size = 1;
    return 1;
}

static int setup_manual_retry_ready_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    if (!setup_manual_retry_member_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].family_active_member_count = 1;
    plan->items[2].family_active_member_count = 1;
    plan->items[1].family_coalesce_ready_neighbor_count = 1;
    plan->items[2].family_coalesce_ready_neighbor_count = 2;
    return 1;
}

static int setup_manual_retry_ready_affinity_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    if (!setup_manual_retry_ready_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].family_coalesce_ready_neighbor_count = 1;
    plan->items[2].family_coalesce_ready_neighbor_count = 1;
    plan->items[1].family_coalesce_ready_affinity_weight_sum = 1;
    plan->items[2].family_coalesce_ready_affinity_weight_sum = 3;
    return 1;
}

static int setup_manual_retry_mutual_ready_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 5) {
        return 0;
    }

    if (!setup_manual_retry_ready_affinity_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].family_best_coalesce_ready_partner_root_value_id = 4;
    plan->items[1].family_best_coalesce_ready_partner_affinity_weight = 3;
    plan->items[1].family_best_coalesce_ready_partner_is_mutual = 0;
    plan->items[2].family_best_coalesce_ready_partner_root_value_id = 4;
    plan->items[2].family_best_coalesce_ready_partner_affinity_weight = 3;
    plan->items[2].family_best_coalesce_ready_partner_is_mutual = 1;
    return 1;
}

static int setup_manual_retry_dual_evict_family_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 6) {
        return 0;
    }

    prep->value_count = 6;
    prep->use_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(6, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(6, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(6, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(6, sizeof(unsigned char));
    interference->value_count = 6;
    interference->interferes = (unsigned char *)calloc(36, sizeof(unsigned char));
    plan->value_count = 6;
    plan->item_count = 6;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(6, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;
    prep->interference_degrees[0] = 4;
    prep->interference_degrees[1] = 4;
    prep->interference_degrees[2] = 4;
    prep->interference_degrees[3] = 3;
    prep->interference_degrees[4] = 2;
    prep->interference_degrees[5] = 2;

    interference->interferes[0 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 0] = 1;
    interference->interferes[1 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 1] = 1;
    interference->interferes[2 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 2] = 1;
    interference->interferes[3 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 3] = 1;
    interference->interferes[4 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 4] = 1;
    interference->interferes[3 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 3] = 1;

    plan->items[0].value_id = 5;
    plan->items[0].priority = 20;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 0;
    plan->items[1].priority = 18;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[1].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_RELEASED;
    plan->items[1].family_external_neighbor_count = 1;
    plan->items[1].family_external_affinity_weight_sum = 1;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 18;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[2].family_external_neighbor_count = 2;
    plan->items[2].family_external_affinity_weight_sum = 4;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 10;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].value_id = 3;
    plan->items[4].priority = 25;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 4;
    plan->items[5].priority = 8;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 6; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_blocker_family_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 7) {
        return 0;
    }

    prep->value_count = 7;
    prep->use_counts = (size_t *)calloc(7, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(7, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(7, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(7, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(7, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(7, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(7, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(7, sizeof(unsigned char));
    interference->value_count = 7;
    interference->interferes = (unsigned char *)calloc(49, sizeof(unsigned char));
    plan->value_count = 7;
    plan->item_count = 7;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(7, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 3;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->use_counts[6] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;
    prep->live_block_counts[6] = 1;
    prep->interference_degrees[0] = 4;
    prep->interference_degrees[1] = 4;
    prep->interference_degrees[2] = 2;
    prep->interference_degrees[3] = 2;
    prep->interference_degrees[4] = 2;
    prep->interference_degrees[5] = 1;
    prep->interference_degrees[6] = 2;

    interference->interferes[0 * 7 + 2] = 1;
    interference->interferes[2 * 7 + 0] = 1;
    interference->interferes[0 * 7 + 4] = 1;
    interference->interferes[4 * 7 + 0] = 1;
    interference->interferes[0 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 0] = 1;
    interference->interferes[1 * 7 + 3] = 1;
    interference->interferes[3 * 7 + 1] = 1;
    interference->interferes[1 * 7 + 4] = 1;
    interference->interferes[4 * 7 + 1] = 1;
    interference->interferes[1 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 1] = 1;
    interference->interferes[2 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 2] = 1;
    interference->interferes[3 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 3] = 1;
    interference->interferes[4 * 7 + 5] = 1;
    interference->interferes[5 * 7 + 4] = 1;
    interference->interferes[4 * 7 + 6] = 1;
    interference->interferes[6 * 7 + 4] = 1;
    interference->interferes[5 * 7 + 6] = 1;
    interference->interferes[6 * 7 + 5] = 1;

    plan->items[0].value_id = 6;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 0;
    plan->items[1].priority = 18;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].value_id = 1;
    plan->items[2].priority = 18;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[3].value_id = 2;
    plan->items[3].priority = 12;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    plan->items[3].family_external_neighbor_count = 2;
    plan->items[3].family_external_affinity_weight_sum = 4;
    plan->items[4].value_id = 3;
    plan->items[4].priority = 12;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[4].move_work_class = VALUE_SSA_ALLOC_MOVE_WORK_RELEASED;
    plan->items[4].family_external_neighbor_count = 1;
    plan->items[4].family_external_affinity_weight_sum = 1;
    plan->items[5].value_id = 4;
    plan->items[5].priority = 10;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[6].value_id = 5;
    plan->items[6].priority = 20;
    plan->items[6].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    {
        size_t item_index;
        for (item_index = 0; item_index < 7; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_family_batch_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 8) {
        return 0;
    }

    prep->value_count = 8;
    prep->use_counts = (size_t *)calloc(8, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(8, sizeof(size_t));
    prep->loop_depth_sums = (size_t *)calloc(8, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(8, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(8, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(8, sizeof(unsigned char));
    prep->rematerializable = (unsigned char *)calloc(8, sizeof(unsigned char));
    prep->split_child_values = (unsigned char *)calloc(8, sizeof(unsigned char));
    interference->value_count = 8;
    interference->interferes = (unsigned char *)calloc(64, sizeof(unsigned char));
    plan->value_count = 8;
    plan->item_count = 8;
    plan->items = (ValueSsaAllocatorPlanItem *)calloc(8, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep->use_counts || !prep->live_block_counts || !prep->loop_depth_sums || !prep->interference_degrees ||
        !prep->affinity_sums || !prep->move_related || !prep->rematerializable || !prep->split_child_values ||
        !interference->interferes ||
        !plan->items) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 3;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->use_counts[6] = 1;
    prep->use_counts[7] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;
    prep->live_block_counts[6] = 1;
    prep->live_block_counts[7] = 1;
    prep->interference_degrees[0] = 3;
    prep->interference_degrees[1] = 3;
    prep->interference_degrees[2] = 2;
    prep->interference_degrees[3] = 2;
    prep->interference_degrees[4] = 3;
    prep->interference_degrees[5] = 5;
    prep->interference_degrees[6] = 2;
    prep->interference_degrees[7] = 3;

    interference->interferes[0 * 8 + 2] = 1;
    interference->interferes[2 * 8 + 0] = 1;
    interference->interferes[0 * 8 + 4] = 1;
    interference->interferes[4 * 8 + 0] = 1;
    interference->interferes[0 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 0] = 1;
    interference->interferes[1 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 1] = 1;
    interference->interferes[1 * 8 + 4] = 1;
    interference->interferes[4 * 8 + 1] = 1;
    interference->interferes[1 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 1] = 1;
    interference->interferes[2 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 2] = 1;
    interference->interferes[3 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 3] = 1;
    interference->interferes[4 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 4] = 1;
    interference->interferes[4 * 8 + 6] = 1;
    interference->interferes[6 * 8 + 4] = 1;
    interference->interferes[5 * 8 + 6] = 1;
    interference->interferes[6 * 8 + 5] = 1;
    interference->interferes[7 * 8 + 3] = 1;
    interference->interferes[3 * 8 + 7] = 1;
    interference->interferes[7 * 8 + 4] = 1;
    interference->interferes[4 * 8 + 7] = 1;
    interference->interferes[7 * 8 + 5] = 1;
    interference->interferes[5 * 8 + 7] = 1;

    plan->items[0].value_id = 6;
    plan->items[0].priority = 30;
    plan->items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[1].value_id = 7;
    plan->items[1].priority = 18;
    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[2].value_id = 0;
    plan->items[2].priority = 18;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[3].value_id = 1;
    plan->items[3].priority = 18;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan->items[4].value_id = 2;
    plan->items[4].priority = 12;
    plan->items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[5].value_id = 3;
    plan->items[5].priority = 12;
    plan->items[5].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[6].value_id = 4;
    plan->items[6].priority = 10;
    plan->items[6].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[7].value_id = 5;
    plan->items[7].priority = 20;
    plan->items[7].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    plan->items[1].coalesce_root_value_id = 0;
    plan->items[1].coalesce_class_size = 2;
    plan->items[2].coalesce_root_value_id = 0;
    plan->items[2].coalesce_class_size = 2;
    plan->items[3].coalesce_root_value_id = 1;
    plan->items[3].coalesce_class_size = 1;

    {
        size_t item_index;
        for (item_index = 0; item_index < 8; ++item_index) {
            size_t value_id = plan->items[item_index].value_id;

            plan->items[item_index].spill_use_cost = prep->use_counts[value_id];
            plan->items[item_index].spill_live_range_cost = prep->live_block_counts[value_id];
            plan->items[item_index].spill_affinity_cost = prep->affinity_sums[value_id];
            plan->items[item_index].spill_cost = plan->items[item_index].spill_use_cost +
                plan->items[item_index].spill_live_range_cost + plan->items[item_index].spill_affinity_cost;
        }
    }

    return 1;
}

static int setup_manual_retry_confirmed_family_batch_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 8) {
        return 0;
    }
    if (!setup_manual_retry_family_batch_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    plan->items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan->items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    return 1;
}

static void rollback_retry_family_batch_recoveries(ValueSsaAllocationResult *result) {
    const size_t recovered_value_ids[3] = {0, 7, 1};
    size_t index;

    if (!result || result->value_count < 8) {
        return;
    }

    for (index = 0; index < 3; ++index) {
        size_t value_id = recovered_value_ids[index];

        result->spill_flags[value_id] = 1;
        if (result->has_color) {
            result->has_color[value_id] = 0;
        }
        if (result->colors) {
            result->colors[value_id] = (size_t)-1;
        }
        if (result->spill_confirmed_flags) {
            result->spill_confirmed_flags[value_id] = 1;
        }
        if (result->spill_recovered_flags) {
            result->spill_recovered_flags[value_id] = 0;
        }
        if (result->spill_recovery_orders) {
            result->spill_recovery_orders[value_id] = (size_t)-1;
        }
        if (result->spill_recovery_phase_ids) {
            result->spill_recovery_phase_ids[value_id] = (size_t)-1;
        }
        if (result->spill_recovery_phase_member_orders) {
            result->spill_recovery_phase_member_orders[value_id] = (size_t)-1;
        }
        if (result->spill_recovery_phase_member_counts) {
            result->spill_recovery_phase_member_counts[value_id] = (size_t)-1;
        }
        if (result->spill_recovery_phase_entries) {
            clear_retry_phase_entry_item(&result->spill_recovery_phase_entries[value_id]);
        }
    }

    result->next_spill_recovery_order = 0;
    result->next_spill_recovery_phase_id = 0;
}

static void rollback_retry_recovery_value(ValueSsaAllocationResult *result, size_t value_id) {
    if (!result || value_id >= result->value_count) {
        return;
    }

    result->spill_flags[value_id] = 1;
    if (result->has_color) {
        result->has_color[value_id] = 0;
    }
    if (result->colors) {
        result->colors[value_id] = (size_t)-1;
    }
    if (result->spill_confirmed_flags) {
        result->spill_confirmed_flags[value_id] = 1;
    }
    if (result->spill_recovered_flags) {
        result->spill_recovered_flags[value_id] = 0;
    }
    if (result->spill_recovery_orders) {
        result->spill_recovery_orders[value_id] = (size_t)-1;
    }
    if (result->spill_recovery_phase_ids) {
        result->spill_recovery_phase_ids[value_id] = (size_t)-1;
    }
    if (result->spill_recovery_phase_member_orders) {
        result->spill_recovery_phase_member_orders[value_id] = (size_t)-1;
    }
    if (result->spill_recovery_phase_member_counts) {
        result->spill_recovery_phase_member_counts[value_id] = (size_t)-1;
    }
    if (result->spill_recovery_phase_entries) {
        clear_retry_phase_entry_item(&result->spill_recovery_phase_entries[value_id]);
    }
}

static int setup_manual_retry_family_blocker_representative_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaInterferenceGraph *interference,
    ValueSsaAllocatorPlan *plan) {
    if (!function || !prep || !interference || !plan || function->next_value_id != 7) {
        return 0;
    }
    if (!setup_manual_retry_blocker_family_pressure_artifacts(function, prep, interference, plan)) {
        return 0;
    }

    prep->use_counts[0] = 1;
    plan->items[1].spill_use_cost = prep->use_counts[0];
    plan->items[1].spill_cost = plan->items[1].spill_use_cost + plan->items[1].spill_live_range_cost +
        plan->items[1].spill_affinity_cost;

    plan->items[1].coalesce_root_value_id = 0;
    plan->items[1].coalesce_class_size = 2;
    plan->items[2].coalesce_root_value_id = 0;
    plan->items[2].coalesce_class_size = 2;
    plan->items[3].coalesce_root_value_id = 2;
    plan->items[3].coalesce_class_size = 1;
    plan->items[4].coalesce_root_value_id = 3;
    plan->items[4].coalesce_class_size = 1;
    plan->items[5].coalesce_root_value_id = 4;
    plan->items[5].coalesce_class_size = 1;
    plan->items[6].coalesce_root_value_id = 5;
    plan->items[6].coalesce_class_size = 1;
    plan->items[0].coalesce_root_value_id = 6;
    plan->items[0].coalesce_class_size = 1;
    return 1;
}

static int setup_manual_spill_family_pressure_artifacts(const ValueSsaFunction *function,
    ValueSsaAllocationPrep *prep,
    ValueSsaAllocationWorklist *worklist,
    ValueSsaInterferenceGraph *interference,
    ValueSsaCopyAffinityGraph *affinity,
    ValueSsaAllocatorCoalesceAnalysis *coalesce,
    int split_neighbor_pressure) {
    if (!function || !prep || !worklist || !interference || !affinity || !coalesce ||
        function->next_value_id != 6) {
        return 0;
    }

    prep->value_count = 6;
    prep->use_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->live_block_counts = (size_t *)calloc(6, sizeof(size_t));
    prep->interference_degrees = (size_t *)calloc(6, sizeof(size_t));
    prep->affinity_sums = (size_t *)calloc(6, sizeof(size_t));
    prep->move_related = (unsigned char *)calloc(6, sizeof(unsigned char));
    worklist->value_count = 6;
    worklist->item_count = 6;
    worklist->items = (ValueSsaAllocationWorkItem *)calloc(6, sizeof(ValueSsaAllocationWorkItem));
    interference->value_count = 6;
    interference->interferes = (unsigned char *)calloc(36, sizeof(unsigned char));
    affinity->value_count = 6;
    affinity->weights = (size_t *)calloc(36, sizeof(size_t));
    coalesce->value_count = 6;
    coalesce->item_count = 0;
    coalesce->items = NULL;
    coalesce->value_roots = (size_t *)calloc(6, sizeof(size_t));
    coalesce->class_sizes = (size_t *)calloc(6, sizeof(size_t));
    if (!prep->use_counts || !prep->live_block_counts || !prep->interference_degrees || !prep->affinity_sums ||
        !prep->move_related || !worklist->items || !interference->interferes || !affinity->weights ||
        !coalesce->value_roots || !coalesce->class_sizes) {
        return 0;
    }

    prep->use_counts[0] = 1;
    prep->use_counts[1] = 1;
    prep->use_counts[2] = 1;
    prep->use_counts[3] = 1;
    prep->use_counts[4] = 1;
    prep->use_counts[5] = 1;
    prep->live_block_counts[0] = 1;
    prep->live_block_counts[1] = 1;
    prep->live_block_counts[2] = 1;
    prep->live_block_counts[3] = 1;
    prep->live_block_counts[4] = 1;
    prep->live_block_counts[5] = 1;
    prep->interference_degrees[0] = 2;
    prep->interference_degrees[1] = 2;
    prep->interference_degrees[2] = 2;
    prep->interference_degrees[3] = 2;
    prep->interference_degrees[4] = 4;
    prep->interference_degrees[5] = 4;
    prep->move_related[0] = 1;
    prep->move_related[1] = 1;
    prep->move_related[2] = 1;
    prep->move_related[3] = 1;
    prep->move_related[4] = 1;
    prep->move_related[5] = 1;

    worklist->items[0].value_id = 0;
    worklist->items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist->items[0].priority = 10;
    worklist->items[1].value_id = 1;
    worklist->items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist->items[1].priority = 9;
    worklist->items[2].value_id = 2;
    worklist->items[2].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist->items[2].priority = 10;
    worklist->items[3].value_id = 3;
    worklist->items[3].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist->items[3].priority = 9;
    worklist->items[4].value_id = 4;
    worklist->items[4].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist->items[4].priority = 8;
    worklist->items[5].value_id = 5;
    worklist->items[5].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist->items[5].priority = 8;

    interference->interferes[0 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 0] = 1;
    interference->interferes[0 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 0] = 1;
    interference->interferes[1 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 1] = 1;
    interference->interferes[1 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 1] = 1;
    interference->interferes[2 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 2] = 1;
    interference->interferes[2 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 2] = 1;
    interference->interferes[3 * 6 + 4] = 1;
    interference->interferes[4 * 6 + 3] = 1;
    interference->interferes[3 * 6 + 5] = 1;
    interference->interferes[5 * 6 + 3] = 1;

    if (split_neighbor_pressure) {
        affinity->weights[0 * 6 + 4] = 2;
        affinity->weights[4 * 6 + 0] = 2;
        affinity->weights[2 * 6 + 4] = 1;
        affinity->weights[4 * 6 + 2] = 1;
        affinity->weights[3 * 6 + 5] = 1;
        affinity->weights[5 * 6 + 3] = 1;
    } else {
        affinity->weights[0 * 6 + 4] = 1;
        affinity->weights[4 * 6 + 0] = 1;
        affinity->weights[2 * 6 + 5] = 1;
        affinity->weights[5 * 6 + 2] = 1;
        affinity->weights[3 * 6 + 5] = 1;
        affinity->weights[5 * 6 + 3] = 1;
    }

    coalesce->value_roots[0] = 0;
    coalesce->value_roots[1] = 0;
    coalesce->value_roots[2] = 2;
    coalesce->value_roots[3] = 2;
    coalesce->value_roots[4] = 4;
    coalesce->value_roots[5] = 5;
    coalesce->class_sizes[0] = 2;
    coalesce->class_sizes[2] = 2;
    coalesce->class_sizes[4] = 1;
    coalesce->class_sizes[5] = 1;

    return 1;
}

static int build_alloc_spill_return_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t sum0;
    size_t sum1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_ret", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    sum1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        sum0 == (size_t)-1 || sum1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum1);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_same_block_multi_use_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t spilled_sum;
    size_t use0;
    size_t use1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_same_block_multi", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    spilled_sum = value_ssa_function_allocate_value(function);
    use0 = value_ssa_function_allocate_value(function);
    use1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        spilled_sum == (size_t)-1 || use0 == (size_t)-1 || use1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(use0);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(use1);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(use1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_unique_predecessor_reload_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t spilled_sum;
    size_t same_block_use;
    size_t cross_block_use;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_unique_pred_reload", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    spilled_sum = value_ssa_function_allocate_value(function);
    same_block_use = value_ssa_function_allocate_value(function);
    cross_block_use = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || spilled_sum == (size_t)-1 ||
        same_block_use == (size_t)-1 || cross_block_use == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(same_block_use);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(cross_block_use);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(cross_block_use), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_unique_predecessor_chain_reload_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *mid_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t value1;
    size_t spilled_sum;
    size_t same_block_use;
    size_t mid_use;
    size_t exit_use;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_unique_pred_chain", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &mid_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    spilled_sum = value_ssa_function_allocate_value(function);
    same_block_use = value_ssa_function_allocate_value(function);
    mid_use = value_ssa_function_allocate_value(function);
    exit_use = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || value1 == (size_t)-1 || spilled_sum == (size_t)-1 ||
        same_block_use == (size_t)-1 || mid_use == (size_t)-1 || exit_use == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_sum);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(same_block_use);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(mid_use);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(mid_block, &instruction, error) ||
        !value_ssa_block_set_jump(mid_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(exit_use);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_sum);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_use), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_cross_block_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t sum0;
    size_t then_sum;
    size_t else_sum;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_xblock", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    then_sum = value_ssa_function_allocate_value(function);
    else_sum = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        sum0 == (size_t)-1 || then_sum == (size_t)-1 || else_sum == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(then_sum);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_id(value2);
    if (!value_ssa_block_append_instruction(then_block, &instruction, error) ||
        !value_ssa_block_set_return(then_block, value_ssa_value_id(then_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(else_sum);
    instruction.as.binary.lhs = value_ssa_value_id(sum0);
    instruction.as.binary.rhs = value_ssa_value_immediate(5);
    if (!value_ssa_block_append_instruction(else_block, &instruction, error) ||
        !value_ssa_block_set_return(else_block, value_ssa_value_id(else_sum), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_block_local_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *use_block = NULL;
    ValueSsaInstruction instruction;
    size_t spilled_value;
    size_t use0;
    size_t use1;
    size_t use2;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "split_local", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &use_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    spilled_value = value_ssa_function_allocate_value(function);
    use0 = value_ssa_function_allocate_value(function);
    use1 = value_ssa_function_allocate_value(function);
    use2 = value_ssa_function_allocate_value(function);
    if (spilled_value == (size_t)-1 || use0 == (size_t)-1 || use1 == (size_t)-1 || use2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, use_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(use_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(use_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(use0);
    instruction.as.binary.rhs = value_ssa_value_id(use1);
    if (!value_ssa_block_append_instruction(use_block, &instruction, error) ||
        !value_ssa_block_set_return(use_block, value_ssa_value_id(use2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_block_local_split_term_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *use_block = NULL;
    ValueSsaInstruction instruction;
    size_t spilled_value;
    size_t cond_value;
    size_t use0;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "split_local_term", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &use_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    spilled_value = value_ssa_function_allocate_value(function);
    cond_value = value_ssa_function_allocate_value(function);
    use0 = value_ssa_function_allocate_value(function);
    if (spilled_value == (size_t)-1 || cond_value == (size_t)-1 || use0 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, use_block->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(use_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_NE;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(use_block, &instruction, error) ||
        !value_ssa_block_set_return(use_block, value_ssa_value_id(cond_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_multi_block_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *use1 = NULL;
    ValueSsaBasicBlock *use2 = NULL;
    ValueSsaInstruction instruction;
    size_t spilled_value;
    size_t use1_a;
    size_t use1_b;
    size_t use2_a;
    size_t use2_b;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "split_multi_block", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &use1, error) ||
        !value_ssa_function_append_block(function, NULL, &use2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    spilled_value = value_ssa_function_allocate_value(function);
    use1_a = value_ssa_function_allocate_value(function);
    use1_b = value_ssa_function_allocate_value(function);
    use2_a = value_ssa_function_allocate_value(function);
    use2_b = value_ssa_function_allocate_value(function);
    if (spilled_value == (size_t)-1 || use1_a == (size_t)-1 || use1_b == (size_t)-1 ||
        use2_a == (size_t)-1 || use2_b == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(9);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, use1->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(use1, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1_b);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(use1, &instruction, error) ||
        !value_ssa_block_set_jump(use1, use2->id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use2_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(use2, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use2_b);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_id(use2_a);
    if (!value_ssa_block_append_instruction(use2, &instruction, error) ||
        !value_ssa_block_set_return(use2, value_ssa_value_id(use2_b), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_def_block_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t input_local_id;
    size_t spilled_value;
    size_t use0;
    size_t use1;
    size_t use2;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "split_def_block", 1, &function, error) ||
        !value_ssa_function_append_local(function, "input", 0, &input_local_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    spilled_value = value_ssa_function_allocate_value(function);
    use0 = value_ssa_function_allocate_value(function);
    use1 = value_ssa_function_allocate_value(function);
    use2 = value_ssa_function_allocate_value(function);
    if (spilled_value == (size_t)-1 || use0 == (size_t)-1 || use1 == (size_t)-1 || use2 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.load_slot = value_ssa_slot_local(input_local_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(use0);
    instruction.as.binary.rhs = value_ssa_value_id(use1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(use2), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_split_chain_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t input_local_id;
    size_t root_value;
    size_t child_value;
    size_t grandchild_value;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "split_chain", 1, &function, error) ||
        !value_ssa_function_append_local(function, "input", 0, &input_local_id, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    root_value = value_ssa_function_allocate_value(function);
    child_value = value_ssa_function_allocate_value(function);
    grandchild_value = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (root_value == (size_t)-1 || child_value == (size_t)-1 || grandchild_value == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_LOAD_LOCAL;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(root_value);
    instruction.as.load_slot = value_ssa_slot_local(input_local_id);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(child_value);
    instruction.as.mov_value = value_ssa_value_id(root_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(grandchild_value);
    instruction.as.mov_value = value_ssa_value_id(child_value);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(grandchild_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_plan_program(ValueSsaProgram *program,
    size_t *out_invariant_value,
    size_t *out_init_value,
    size_t *out_phi_value,
    size_t *out_step_value,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t invariant_value;
    size_t init_value;
    size_t phi_value;
    size_t step_value;
    size_t exit_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "alloc_loop_plan", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    invariant_value = value_ssa_function_allocate_value(function);
    init_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    step_value = value_ssa_function_allocate_value(function);
    exit_value = value_ssa_function_allocate_value(function);
    if (invariant_value == (size_t)-1 || init_value == (size_t)-1 || phi_value == (size_t)-1 ||
        step_value == (size_t)-1 || exit_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(invariant_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(init_value);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(init_value);
    phi_inputs[1].predecessor_block_id = 1;
    phi_inputs[1].value = value_ssa_value_id(step_value);
    if (!value_ssa_block_append_phi(header, phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(step_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(exit_value);
    instruction.as.binary.lhs = value_ssa_value_id(invariant_value);
    instruction.as.binary.rhs = value_ssa_value_id(phi_value);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(invariant_value), 1, 2, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (out_invariant_value) {
        *out_invariant_value = invariant_value;
    }
    if (out_init_value) {
        *out_init_value = init_value;
    }
    if (out_phi_value) {
        *out_phi_value = phi_value;
    }
    if (out_step_value) {
        *out_step_value = step_value;
    }
    return 1;
}

static int build_alloc_phi_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t value0;
    size_t value1;
    size_t value2;
    size_t sum0;
    size_t joined_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_phi", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    value0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    value2 = value_ssa_function_allocate_value(function);
    sum0 = value_ssa_function_allocate_value(function);
    joined_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || value0 == (size_t)-1 || value1 == (size_t)-1 || value2 == (size_t)-1 ||
        sum0 == (size_t)-1 || joined_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(value2);
    instruction.as.mov_value = value_ssa_value_immediate(30);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_id(value1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error) ||
        !value_ssa_block_set_jump(then_block, 3, error) ||
        !value_ssa_block_set_jump(else_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(sum0);
        phi_inputs[1].predecessor_block_id = 2;
        phi_inputs[1].value = value_ssa_value_id(sum0);
        if (!value_ssa_block_append_phi(join_block, joined_value, phi_inputs, 2, error) ||
            !value_ssa_block_set_return(join_block, value_ssa_value_id(joined_value), error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    return 1;
}

static int build_alloc_phi_defined_multi_use_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *left_block = NULL;
    ValueSsaBasicBlock *right_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *use_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t left_value;
    size_t right_value;
    size_t joined_value;
    size_t same_block_use;
    size_t cross_block_use;
    size_t exit_phi;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_phi_multi", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &left_block, error) ||
        !value_ssa_function_append_block(function, NULL, &right_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &use_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    left_block = &function->blocks[1];
    right_block = &function->blocks[2];
    join_block = &function->blocks[3];
    use_block = &function->blocks[4];
    exit_block = &function->blocks[5];

    cond_value = value_ssa_function_allocate_value(function);
    left_value = value_ssa_function_allocate_value(function);
    right_value = value_ssa_function_allocate_value(function);
    joined_value = value_ssa_function_allocate_value(function);
    same_block_use = value_ssa_function_allocate_value(function);
    cross_block_use = value_ssa_function_allocate_value(function);
    exit_phi = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || left_value == (size_t)-1 || right_value == (size_t)-1 ||
        joined_value == (size_t)-1 || same_block_use == (size_t)-1 || cross_block_use == (size_t)-1 ||
        exit_phi == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(left_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(left_block, &instruction, error) ||
        !value_ssa_block_set_jump(left_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(right_value);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(right_block, &instruction, error) ||
        !value_ssa_block_set_jump(right_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(left_value);
        phi_inputs[1].predecessor_block_id = 2;
        phi_inputs[1].value = value_ssa_value_id(right_value);
        if (!value_ssa_block_append_phi(join_block, joined_value, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(same_block_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_branch(join_block, value_ssa_value_id(joined_value), 4, 5, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(cross_block_use);
    instruction.as.binary.lhs = value_ssa_value_id(joined_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(use_block, &instruction, error) ||
        !value_ssa_block_set_jump(use_block, 5, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 3;
        phi_inputs[0].value = value_ssa_value_id(joined_value);
        phi_inputs[1].predecessor_block_id = 4;
        phi_inputs[1].value = value_ssa_value_id(cross_block_use);
        if (!value_ssa_block_append_phi(exit_block, exit_phi, phi_inputs, 2, error) ||
            !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_phi), error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    return 1;
}

static int build_alloc_branch_predecessor_phi_input_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_value;
    size_t other_value;
    size_t joined_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_branch_phi_edge", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    other_value = value_ssa_function_allocate_value(function);
    joined_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_value == (size_t)-1 || other_value == (size_t)-1 ||
        joined_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(other_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_value);
        if (!value_ssa_block_append_phi(join_block, joined_value, phi_inputs, 2, error) ||
            !value_ssa_block_set_return(join_block, value_ssa_value_id(joined_value), error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    return 1;
}

static int build_alloc_branch_predecessor_two_phi_inputs_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_value;
    size_t other_a;
    size_t other_b;
    size_t joined_a;
    size_t joined_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_branch_two_phi_edges", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    other_a = value_ssa_function_allocate_value(function);
    other_b = value_ssa_function_allocate_value(function);
    joined_a = value_ssa_function_allocate_value(function);
    joined_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_value == (size_t)-1 || other_a == (size_t)-1 ||
        other_b == (size_t)-1 || joined_a == (size_t)-1 || joined_b == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(other_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_a);
        if (!value_ssa_block_append_phi(join_block, joined_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_b);
        if (!value_ssa_block_append_phi(join_block, joined_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_a);
    instruction.as.binary.rhs = value_ssa_value_id(joined_b);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_two_branch_edges_same_spilled_phi_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch0 = NULL;
    ValueSsaBasicBlock *join0 = NULL;
    ValueSsaBasicBlock *other0 = NULL;
    ValueSsaBasicBlock *branch1 = NULL;
    ValueSsaBasicBlock *join1 = NULL;
    ValueSsaBasicBlock *other1 = NULL;
    ValueSsaInstruction instruction;
    size_t cond0;
    size_t cond1;
    size_t spilled_value;
    size_t other0_value;
    size_t other0_value_b;
    size_t join0_value;
    size_t join0_value_b;
    size_t other1_value;
    size_t other1_value_b;
    size_t join1_value;
    size_t join1_value_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "two_branch_edges_same_spilled_phi", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch0, error) ||
        !value_ssa_function_append_block(function, NULL, &join0, error) ||
        !value_ssa_function_append_block(function, NULL, &other0, error) ||
        !value_ssa_function_append_block(function, NULL, &branch1, error) ||
        !value_ssa_function_append_block(function, NULL, &join1, error) ||
        !value_ssa_function_append_block(function, NULL, &other1, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    branch0 = &function->blocks[1];
    join0 = &function->blocks[2];
    other0 = &function->blocks[3];
    branch1 = &function->blocks[4];
    join1 = &function->blocks[5];
    other1 = &function->blocks[6];

    cond0 = value_ssa_function_allocate_value(function);
    cond1 = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    other0_value = value_ssa_function_allocate_value(function);
    other0_value_b = value_ssa_function_allocate_value(function);
    join0_value = value_ssa_function_allocate_value(function);
    join0_value_b = value_ssa_function_allocate_value(function);
    other1_value = value_ssa_function_allocate_value(function);
    other1_value_b = value_ssa_function_allocate_value(function);
    join1_value = value_ssa_function_allocate_value(function);
    join1_value_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond0 == (size_t)-1 || cond1 == (size_t)-1 || spilled_value == (size_t)-1 ||
        other0_value == (size_t)-1 || other0_value_b == (size_t)-1 || join0_value == (size_t)-1 ||
        join0_value_b == (size_t)-1 || other1_value == (size_t)-1 || other1_value_b == (size_t)-1 ||
        join1_value == (size_t)-1 || join1_value_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(branch0, value_ssa_value_id(cond0), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(other0_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other0, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other0_value_b);
    instruction.as.mov_value = value_ssa_value_immediate(17);
    if (!value_ssa_block_append_instruction(other0, &instruction, error) ||
        !value_ssa_block_set_jump(other0, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other0_value);
        if (!value_ssa_block_append_phi(join0, join0_value, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        phi_inputs[1].value = value_ssa_value_id(other0_value_b);
        if (!value_ssa_block_append_phi(join0, join0_value_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    if (!value_ssa_block_set_branch(join0, value_ssa_value_id(cond1), 4, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_branch(branch1, value_ssa_value_id(cond1), 5, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(other1_value);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other1, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other1_value_b);
    instruction.as.mov_value = value_ssa_value_immediate(18);
    if (!value_ssa_block_append_instruction(other1, &instruction, error) ||
        !value_ssa_block_set_jump(other1, 5, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 4;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 6;
        phi_inputs[1].value = value_ssa_value_id(other1_value);
        if (!value_ssa_block_append_phi(join1, join1_value, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        phi_inputs[1].value = value_ssa_value_id(other1_value_b);
        if (!value_ssa_block_append_phi(join1, join1_value_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(join0_value);
    instruction.as.binary.rhs = value_ssa_value_id(join1_value_b);
    if (!value_ssa_block_append_instruction(join1, &instruction, error) ||
        !value_ssa_block_set_return(join1, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_branch_predecessor_phi_and_tail_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_value;
    size_t tail_use;
    size_t other_a;
    size_t other_b;
    size_t joined_a;
    size_t joined_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_branch_phi_and_tail_split", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    tail_use = value_ssa_function_allocate_value(function);
    other_a = value_ssa_function_allocate_value(function);
    other_b = value_ssa_function_allocate_value(function);
    joined_a = value_ssa_function_allocate_value(function);
    joined_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_value == (size_t)-1 || tail_use == (size_t)-1 ||
        other_a == (size_t)-1 || other_b == (size_t)-1 || joined_a == (size_t)-1 ||
        joined_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(tail_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(other_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_a);
        if (!value_ssa_block_append_phi(join_block, joined_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].value = value_ssa_value_id(other_b);
        if (!value_ssa_block_append_phi(join_block, joined_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_a);
    instruction.as.binary.rhs = value_ssa_value_id(joined_b);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_branch_predecessor_phi_and_two_tails_same_value_split_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_value;
    size_t tail_use_a;
    size_t tail_use_b;
    size_t other_a;
    size_t other_b;
    size_t joined_a;
    size_t joined_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program,
            "spill_branch_phi_and_two_tails_same_value_split",
            1,
            &function,
            error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    tail_use_a = value_ssa_function_allocate_value(function);
    tail_use_b = value_ssa_function_allocate_value(function);
    other_a = value_ssa_function_allocate_value(function);
    other_b = value_ssa_function_allocate_value(function);
    joined_a = value_ssa_function_allocate_value(function);
    joined_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_value == (size_t)-1 || tail_use_a == (size_t)-1 ||
        tail_use_b == (size_t)-1 || other_a == (size_t)-1 || other_b == (size_t)-1 ||
        joined_a == (size_t)-1 || joined_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(tail_use_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(tail_use_b);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(other_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_a);
        if (!value_ssa_block_append_phi(join_block, joined_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        phi_inputs[0].value = value_ssa_value_id(spilled_value);
        phi_inputs[1].value = value_ssa_value_id(other_b);
        if (!value_ssa_block_append_phi(join_block, joined_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_a);
    instruction.as.binary.rhs = value_ssa_value_id(joined_b);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_branch_predecessor_two_spilled_phi_and_tail_split_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_a;
    size_t spilled_b;
    size_t tail_use;
    size_t other_a;
    size_t other_b;
    size_t joined_a;
    size_t joined_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program,
            "spill_branch_two_spilled_phi_and_tail_split",
            1,
            &function,
            error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spilled_a = value_ssa_function_allocate_value(function);
    spilled_b = value_ssa_function_allocate_value(function);
    tail_use = value_ssa_function_allocate_value(function);
    other_a = value_ssa_function_allocate_value(function);
    other_b = value_ssa_function_allocate_value(function);
    joined_a = value_ssa_function_allocate_value(function);
    joined_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_a == (size_t)-1 || spilled_b == (size_t)-1 ||
        tail_use == (size_t)-1 || other_a == (size_t)-1 || other_b == (size_t)-1 ||
        joined_a == (size_t)-1 || joined_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spilled_a);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spilled_b);
    instruction.as.mov_value = value_ssa_value_immediate(43);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(tail_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_a);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(other_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_a);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_a);
        if (!value_ssa_block_append_phi(join_block, joined_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        phi_inputs[0].value = value_ssa_value_id(spilled_b);
        phi_inputs[1].value = value_ssa_value_id(other_b);
        if (!value_ssa_block_append_phi(join_block, joined_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_a);
    instruction.as.binary.rhs = value_ssa_value_id(joined_b);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_branch_predecessor_two_spilled_phi_and_two_tails_split_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_a;
    size_t spilled_b;
    size_t tail_use_a;
    size_t tail_use_b;
    size_t other_a;
    size_t other_b;
    size_t joined_a;
    size_t joined_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program,
            "spill_branch_two_spilled_phi_and_two_tails_split",
            1,
            &function,
            error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spilled_a = value_ssa_function_allocate_value(function);
    spilled_b = value_ssa_function_allocate_value(function);
    tail_use_a = value_ssa_function_allocate_value(function);
    tail_use_b = value_ssa_function_allocate_value(function);
    other_a = value_ssa_function_allocate_value(function);
    other_b = value_ssa_function_allocate_value(function);
    joined_a = value_ssa_function_allocate_value(function);
    joined_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_a == (size_t)-1 || spilled_b == (size_t)-1 ||
        tail_use_a == (size_t)-1 || tail_use_b == (size_t)-1 || other_a == (size_t)-1 ||
        other_b == (size_t)-1 || joined_a == (size_t)-1 || joined_b == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spilled_a);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spilled_b);
    instruction.as.mov_value = value_ssa_value_immediate(43);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(tail_use_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_a);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(tail_use_b);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_b);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(other_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spilled_a);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_a);
        if (!value_ssa_block_append_phi(join_block, joined_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
        phi_inputs[0].value = value_ssa_value_id(spilled_b);
        phi_inputs[1].value = value_ssa_value_id(other_b);
        if (!value_ssa_block_append_phi(join_block, joined_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_a);
    instruction.as.binary.rhs = value_ssa_value_id(joined_b);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_branch_predecessor_two_distinct_spills_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *branch_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *other_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spill_a;
    size_t spill_b;
    size_t other_a;
    size_t other_b;
    size_t joined_a;
    size_t joined_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_branch_two_distinct", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &branch_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &other_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond_value = value_ssa_function_allocate_value(function);
    spill_a = value_ssa_function_allocate_value(function);
    spill_b = value_ssa_function_allocate_value(function);
    other_a = value_ssa_function_allocate_value(function);
    other_b = value_ssa_function_allocate_value(function);
    joined_a = value_ssa_function_allocate_value(function);
    joined_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spill_a == (size_t)-1 || spill_b == (size_t)-1 ||
        other_a == (size_t)-1 || other_b == (size_t)-1 || joined_a == (size_t)-1 ||
        joined_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(spill_a);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spill_b);
    instruction.as.mov_value = value_ssa_value_immediate(43);
    if (!value_ssa_block_append_instruction(branch_block, &instruction, error) ||
        !value_ssa_block_set_branch(branch_block, value_ssa_value_id(cond_value), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(other_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(other_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(other_block, &instruction, error) ||
        !value_ssa_block_set_jump(other_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    {
        ValueSsaPhiInput phi_inputs[2];

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spill_a);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_a);
        if (!value_ssa_block_append_phi(join_block, joined_a, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }

        phi_inputs[0].predecessor_block_id = 1;
        phi_inputs[0].value = value_ssa_value_id(spill_b);
        phi_inputs[1].predecessor_block_id = 3;
        phi_inputs[1].value = value_ssa_value_id(other_b);
        if (!value_ssa_block_append_phi(join_block, joined_b, phi_inputs, 2, error)) {
            value_ssa_program_free(program);
            return 0;
        }
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(joined_a);
    instruction.as.binary.rhs = value_ssa_value_id(joined_b);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_return(join_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_dominating_reload_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *left_block = NULL;
    ValueSsaBasicBlock *right_block = NULL;
    ValueSsaBasicBlock *join_block = NULL;
    ValueSsaBasicBlock *then_block = NULL;
    ValueSsaBasicBlock *else_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t cond_value;
    size_t spilled_value;
    size_t right_value;
    size_t join_use;
    size_t exit_use;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "dominating_reload_reuse", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &left_block, error) ||
        !value_ssa_function_append_block(function, NULL, &right_block, error) ||
        !value_ssa_function_append_block(function, NULL, &join_block, error) ||
        !value_ssa_function_append_block(function, NULL, &then_block, error) ||
        !value_ssa_function_append_block(function, NULL, &else_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    left_block = &function->blocks[1];
    right_block = &function->blocks[2];
    join_block = &function->blocks[3];
    then_block = &function->blocks[4];
    else_block = &function->blocks[5];
    exit_block = &function->blocks[6];

    cond_value = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    right_value = value_ssa_function_allocate_value(function);
    join_use = value_ssa_function_allocate_value(function);
    exit_use = value_ssa_function_allocate_value(function);
    if (cond_value == (size_t)-1 || spilled_value == (size_t)-1 || right_value == (size_t)-1 ||
        join_use == (size_t)-1 || exit_use == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_value);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond_value), 1, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(right_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(right_block, &instruction, error) ||
        !value_ssa_block_set_jump(right_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    if (!value_ssa_block_set_jump(left_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(join_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(join_block, &instruction, error) ||
        !value_ssa_block_set_branch(join_block, value_ssa_value_id(cond_value), 4, 5, error) ||
        !value_ssa_block_set_jump(then_block, 6, error) ||
        !value_ssa_block_set_jump(else_block, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(exit_use);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_use), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_dominating_reload_clobber_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *mid_block = NULL;
    ValueSsaBasicBlock *clobber_block = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t spill_local_id;
    size_t spilled_value;
    size_t clobber_value;
    size_t mid_use;
    size_t exit_use;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "dominating_reload_clobber", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &mid_block, error) ||
        !value_ssa_function_append_block(function, NULL, &clobber_block, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    mid_block = &function->blocks[1];
    clobber_block = &function->blocks[2];
    exit_block = &function->blocks[3];
    if (!value_ssa_function_append_local(function, "spill.0", 0, &spill_local_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    spilled_value = value_ssa_function_allocate_value(function);
    clobber_value = value_ssa_function_allocate_value(function);
    mid_use = value_ssa_function_allocate_value(function);
    exit_use = value_ssa_function_allocate_value(function);
    if (spilled_value == (size_t)-1 || clobber_value == (size_t)-1 ||
        mid_use == (size_t)-1 || exit_use == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(mid_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(mid_block, &instruction, error) ||
        !value_ssa_block_set_jump(mid_block, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(clobber_value);
    instruction.as.mov_value = value_ssa_value_immediate(99);
    instruction.kind = VALUE_SSA_INSTR_MOV;
    if (!value_ssa_block_append_instruction(clobber_block, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(spill_local_id);
    instruction.as.store.value = value_ssa_value_id(clobber_value);
    if (!value_ssa_block_append_instruction(clobber_block, &instruction, error) ||
        !value_ssa_block_set_jump(clobber_block, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_use), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_reload_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t phi_value;
    size_t step_value;
    size_t loop_cond;
    size_t header_use;
    size_t body_use;
    size_t exit_use;
    ValueSsaPhiInput phi_inputs[2];
    size_t seed_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_reload_reuse", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    seed_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    step_value = value_ssa_function_allocate_value(function);
    loop_cond = value_ssa_function_allocate_value(function);
    header_use = value_ssa_function_allocate_value(function);
    body_use = value_ssa_function_allocate_value(function);
    exit_use = value_ssa_function_allocate_value(function);
    if (seed_value == (size_t)-1 || phi_value == (size_t)-1 || step_value == (size_t)-1 ||
        loop_cond == (size_t)-1 || header_use == (size_t)-1 ||
        body_use == (size_t)-1 || exit_use == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(seed_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(step_value);
    if (!value_ssa_block_append_phi(header, phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(header_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(loop_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(loop_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(step_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(body_use);
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(exit_use);
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_use), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_reload_clobber_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    size_t spill_local_id;
    size_t seed_value;
    size_t phi_value;
    size_t step_value;
    size_t loop_cond;
    size_t header_use;
    size_t clobber_value;
    size_t after_clobber_use;
    size_t exit_use;
    ValueSsaPhiInput phi_inputs[2];

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_reload_clobber", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];
    if (!value_ssa_function_append_local(function, "spill.0", 0, &spill_local_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    seed_value = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    step_value = value_ssa_function_allocate_value(function);
    loop_cond = value_ssa_function_allocate_value(function);
    header_use = value_ssa_function_allocate_value(function);
    clobber_value = value_ssa_function_allocate_value(function);
    after_clobber_use = value_ssa_function_allocate_value(function);
    exit_use = value_ssa_function_allocate_value(function);
    if (seed_value == (size_t)-1 || phi_value == (size_t)-1 || step_value == (size_t)-1 ||
        loop_cond == (size_t)-1 || header_use == (size_t)-1 ||
        clobber_value == (size_t)-1 || after_clobber_use == (size_t)-1 || exit_use == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(seed_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(step_value);
    if (!value_ssa_block_append_phi(header, phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(header_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(loop_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(loop_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(step_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(clobber_value);
    instruction.as.mov_value = value_ssa_value_immediate(99);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(spill_local_id);
    instruction.as.store.value = value_ssa_value_id(clobber_value);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(after_clobber_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_jump(body, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(exit_use);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(exit_use), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_nested_reconverge_reload_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *left0 = NULL;
    ValueSsaBasicBlock *right0 = NULL;
    ValueSsaBasicBlock *join1 = NULL;
    ValueSsaBasicBlock *left1 = NULL;
    ValueSsaBasicBlock *right1 = NULL;
    ValueSsaBasicBlock *join2 = NULL;
    ValueSsaBasicBlock *left2 = NULL;
    ValueSsaBasicBlock *right2 = NULL;
    ValueSsaBasicBlock *join3 = NULL;
    ValueSsaInstruction instruction;
    size_t cond0;
    size_t cond1;
    size_t cond2;
    size_t spilled_value;
    size_t use1;
    size_t use2;
    size_t use3;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "nested_reconverge_reload_reuse", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &left0, error) ||
        !value_ssa_function_append_block(function, NULL, &right0, error) ||
        !value_ssa_function_append_block(function, NULL, &join1, error) ||
        !value_ssa_function_append_block(function, NULL, &left1, error) ||
        !value_ssa_function_append_block(function, NULL, &right1, error) ||
        !value_ssa_function_append_block(function, NULL, &join2, error) ||
        !value_ssa_function_append_block(function, NULL, &left2, error) ||
        !value_ssa_function_append_block(function, NULL, &right2, error) ||
        !value_ssa_function_append_block(function, NULL, &join3, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    left0 = &function->blocks[1];
    right0 = &function->blocks[2];
    join1 = &function->blocks[3];
    left1 = &function->blocks[4];
    right1 = &function->blocks[5];
    join2 = &function->blocks[6];
    left2 = &function->blocks[7];
    right2 = &function->blocks[8];
    join3 = &function->blocks[9];

    cond0 = value_ssa_function_allocate_value(function);
    cond1 = value_ssa_function_allocate_value(function);
    cond2 = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    use1 = value_ssa_function_allocate_value(function);
    use2 = value_ssa_function_allocate_value(function);
    use3 = value_ssa_function_allocate_value(function);
    if (cond0 == (size_t)-1 || cond1 == (size_t)-1 || cond2 == (size_t)-1 ||
        spilled_value == (size_t)-1 || use1 == (size_t)-1 || use2 == (size_t)-1 || use3 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond0), 1, 2, error) ||
        !value_ssa_block_set_jump(left0, 3, error) ||
        !value_ssa_block_set_jump(right0, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(join1, &instruction, error) ||
        !value_ssa_block_set_branch(join1, value_ssa_value_id(cond1), 4, 5, error) ||
        !value_ssa_block_set_jump(left1, 6, error) ||
        !value_ssa_block_set_jump(right1, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(use2);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(join2, &instruction, error) ||
        !value_ssa_block_set_branch(join2, value_ssa_value_id(cond2), 7, 8, error) ||
        !value_ssa_block_set_jump(left2, 9, error) ||
        !value_ssa_block_set_jump(right2, 9, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(use3);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(join3, &instruction, error) ||
        !value_ssa_block_set_return(join3, value_ssa_value_id(use3), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_nested_reconverge_clobber_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *left0 = NULL;
    ValueSsaBasicBlock *right0 = NULL;
    ValueSsaBasicBlock *join1 = NULL;
    ValueSsaBasicBlock *left1 = NULL;
    ValueSsaBasicBlock *right1 = NULL;
    ValueSsaBasicBlock *join2 = NULL;
    ValueSsaBasicBlock *left2 = NULL;
    ValueSsaBasicBlock *right2 = NULL;
    ValueSsaBasicBlock *join3 = NULL;
    ValueSsaInstruction instruction;
    size_t spill_local_id;
    size_t cond0;
    size_t cond1;
    size_t cond2;
    size_t spilled_value;
    size_t use1;
    size_t clobber_value;
    size_t use2;
    size_t use3;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "nested_reconverge_clobber", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &left0, error) ||
        !value_ssa_function_append_block(function, NULL, &right0, error) ||
        !value_ssa_function_append_block(function, NULL, &join1, error) ||
        !value_ssa_function_append_block(function, NULL, &left1, error) ||
        !value_ssa_function_append_block(function, NULL, &right1, error) ||
        !value_ssa_function_append_block(function, NULL, &join2, error) ||
        !value_ssa_function_append_block(function, NULL, &left2, error) ||
        !value_ssa_function_append_block(function, NULL, &right2, error) ||
        !value_ssa_function_append_block(function, NULL, &join3, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    left0 = &function->blocks[1];
    right0 = &function->blocks[2];
    join1 = &function->blocks[3];
    left1 = &function->blocks[4];
    right1 = &function->blocks[5];
    join2 = &function->blocks[6];
    left2 = &function->blocks[7];
    right2 = &function->blocks[8];
    join3 = &function->blocks[9];
    if (!value_ssa_function_append_local(function, "spill.0", 0, &spill_local_id, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    cond0 = value_ssa_function_allocate_value(function);
    cond1 = value_ssa_function_allocate_value(function);
    cond2 = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    use1 = value_ssa_function_allocate_value(function);
    clobber_value = value_ssa_function_allocate_value(function);
    use2 = value_ssa_function_allocate_value(function);
    use3 = value_ssa_function_allocate_value(function);
    if (cond0 == (size_t)-1 || cond1 == (size_t)-1 || cond2 == (size_t)-1 ||
        spilled_value == (size_t)-1 || use1 == (size_t)-1 || clobber_value == (size_t)-1 ||
        use2 == (size_t)-1 || use3 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond0);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_branch(entry, value_ssa_value_id(cond0), 1, 2, error) ||
        !value_ssa_block_set_jump(left0, 3, error) ||
        !value_ssa_block_set_jump(right0, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(join1, &instruction, error) ||
        !value_ssa_block_set_branch(join1, value_ssa_value_id(cond1), 4, 5, error) ||
        !value_ssa_block_set_jump(left1, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(clobber_value);
    instruction.as.mov_value = value_ssa_value_immediate(99);
    instruction.kind = VALUE_SSA_INSTR_MOV;
    if (!value_ssa_block_append_instruction(right1, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_STORE_LOCAL;
    instruction.as.store.slot = value_ssa_slot_local(spill_local_id);
    instruction.as.store.value = value_ssa_value_id(clobber_value);
    if (!value_ssa_block_append_instruction(right1, &instruction, error) ||
        !value_ssa_block_set_jump(right1, 6, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use2);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(join2, &instruction, error) ||
        !value_ssa_block_set_branch(join2, value_ssa_value_id(cond2), 7, 8, error) ||
        !value_ssa_block_set_jump(left2, 9, error) ||
        !value_ssa_block_set_jump(right2, 9, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(use3);
    instruction.as.binary.lhs = value_ssa_value_id(spilled_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(join3, &instruction, error) ||
        !value_ssa_block_set_return(join3, value_ssa_value_id(use3), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_backedge_phi_input_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_value;
    size_t spilled_value;
    size_t loop_phi;
    size_t result_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_backedge_phi_spill", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_value = value_ssa_function_allocate_value(function);
    spilled_value = value_ssa_function_allocate_value(function);
    loop_phi = value_ssa_function_allocate_value(function);
    result_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_value == (size_t)-1 ||
        spilled_value == (size_t)-1 || loop_phi == (size_t)-1 || result_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spilled_value);
    if (!value_ssa_block_append_phi(header, loop_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(spilled_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(result_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(loop_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(result_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_backedge_two_phi_inputs_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_a;
    size_t seed_b;
    size_t spill_a;
    size_t spill_b;
    size_t phi_a;
    size_t phi_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_backedge_two_phi_spills", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_a = value_ssa_function_allocate_value(function);
    seed_b = value_ssa_function_allocate_value(function);
    spill_a = value_ssa_function_allocate_value(function);
    spill_b = value_ssa_function_allocate_value(function);
    phi_a = value_ssa_function_allocate_value(function);
    phi_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_a == (size_t)-1 || seed_b == (size_t)-1 ||
        spill_a == (size_t)-1 || spill_b == (size_t)-1 || phi_a == (size_t)-1 ||
        phi_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_a);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_a);
    if (!value_ssa_block_append_phi(header, phi_a, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_b);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_b);
    if (!value_ssa_block_append_phi(header, phi_b, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(spill_a);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spill_b);
    instruction.as.mov_value = value_ssa_value_immediate(43);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_a);
    instruction.as.binary.rhs = value_ssa_value_id(phi_b);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_backedge_shared_phi_input_spill_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_a;
    size_t seed_b;
    size_t spill_value;
    size_t phi_a;
    size_t phi_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_backedge_shared_phi_spill", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_a = value_ssa_function_allocate_value(function);
    seed_b = value_ssa_function_allocate_value(function);
    spill_value = value_ssa_function_allocate_value(function);
    phi_a = value_ssa_function_allocate_value(function);
    phi_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_a == (size_t)-1 ||
        seed_b == (size_t)-1 || spill_value == (size_t)-1 || phi_a == (size_t)-1 ||
        phi_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_a);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_value);
    if (!value_ssa_block_append_phi(header, phi_a, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_b);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_value);
    if (!value_ssa_block_append_phi(header, phi_b, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(spill_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_a);
    instruction.as.binary.rhs = value_ssa_value_id(phi_b);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_backedge_shared_phi_and_tail_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_a;
    size_t seed_b;
    size_t spill_value;
    size_t tail_use_a;
    size_t tail_use_b;
    size_t phi_a;
    size_t phi_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_backedge_shared_phi_and_tail_split", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_a = value_ssa_function_allocate_value(function);
    seed_b = value_ssa_function_allocate_value(function);
    spill_value = value_ssa_function_allocate_value(function);
    tail_use_a = value_ssa_function_allocate_value(function);
    tail_use_b = value_ssa_function_allocate_value(function);
    phi_a = value_ssa_function_allocate_value(function);
    phi_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_a == (size_t)-1 ||
        seed_b == (size_t)-1 || spill_value == (size_t)-1 || tail_use_a == (size_t)-1 ||
        tail_use_b == (size_t)-1 || phi_a == (size_t)-1 || phi_b == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_a);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_value);
    if (!value_ssa_block_append_phi(header, phi_a, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_b);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_value);
    if (!value_ssa_block_append_phi(header, phi_b, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(spill_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(tail_use_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spill_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(tail_use_b);
    instruction.as.binary.lhs = value_ssa_value_id(spill_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_a);
    instruction.as.binary.rhs = value_ssa_value_id(phi_b);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_backedge_single_phi_and_tail_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_value;
    size_t spill_value;
    size_t tail_use_a;
    size_t tail_use_b;
    size_t phi_value;
    size_t result_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_backedge_single_phi_and_tail_split", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_value = value_ssa_function_allocate_value(function);
    spill_value = value_ssa_function_allocate_value(function);
    tail_use_a = value_ssa_function_allocate_value(function);
    tail_use_b = value_ssa_function_allocate_value(function);
    phi_value = value_ssa_function_allocate_value(function);
    result_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_value == (size_t)-1 ||
        spill_value == (size_t)-1 || tail_use_a == (size_t)-1 || tail_use_b == (size_t)-1 ||
        phi_value == (size_t)-1 || result_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_value);
    if (!value_ssa_block_append_phi(header, phi_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(spill_value);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(tail_use_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(spill_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(tail_use_b);
    instruction.as.binary.lhs = value_ssa_value_id(spill_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(result_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(3);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(result_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_backedge_two_spilled_shared_phi_input_program(ValueSsaProgram *program,
    ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_a;
    size_t seed_b;
    size_t spill_a;
    size_t spill_b;
    size_t phi_a;
    size_t phi_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_backedge_two_spilled_shared_phi", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_a = value_ssa_function_allocate_value(function);
    seed_b = value_ssa_function_allocate_value(function);
    spill_a = value_ssa_function_allocate_value(function);
    spill_b = value_ssa_function_allocate_value(function);
    phi_a = value_ssa_function_allocate_value(function);
    phi_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_a == (size_t)-1 ||
        seed_b == (size_t)-1 || spill_a == (size_t)-1 || spill_b == (size_t)-1 ||
        phi_a == (size_t)-1 || phi_b == (size_t)-1 || sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_a);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_b);
    instruction.as.mov_value = value_ssa_value_immediate(8);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_a);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_a);
    if (!value_ssa_block_append_phi(header, phi_a, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_b);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(spill_b);
    if (!value_ssa_block_append_phi(header, phi_b, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.result = value_ssa_value_id(spill_a);
    instruction.as.mov_value = value_ssa_value_immediate(42);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(spill_b);
    instruction.as.mov_value = value_ssa_value_immediate(43);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(phi_a);
    instruction.as.binary.rhs = value_ssa_value_id(phi_b);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_loop_body_local_split_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaBasicBlock *header = NULL;
    ValueSsaBasicBlock *body = NULL;
    ValueSsaBasicBlock *exit_block = NULL;
    ValueSsaInstruction instruction;
    ValueSsaPhiInput phi_inputs[2];
    size_t cond_seed;
    size_t cond_phi;
    size_t cond_step;
    size_t header_cond;
    size_t body_cond;
    size_t seed_value;
    size_t carried_value;
    size_t use_a;
    size_t use_b;
    size_t sum_value;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "loop_body_local_split", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error) ||
        !value_ssa_function_append_block(function, NULL, &header, error) ||
        !value_ssa_function_append_block(function, NULL, &body, error) ||
        !value_ssa_function_append_block(function, NULL, &exit_block, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    entry = &function->blocks[0];
    header = &function->blocks[1];
    body = &function->blocks[2];
    exit_block = &function->blocks[3];

    cond_seed = value_ssa_function_allocate_value(function);
    cond_phi = value_ssa_function_allocate_value(function);
    cond_step = value_ssa_function_allocate_value(function);
    header_cond = value_ssa_function_allocate_value(function);
    body_cond = value_ssa_function_allocate_value(function);
    seed_value = value_ssa_function_allocate_value(function);
    carried_value = value_ssa_function_allocate_value(function);
    use_a = value_ssa_function_allocate_value(function);
    use_b = value_ssa_function_allocate_value(function);
    sum_value = value_ssa_function_allocate_value(function);
    if (cond_seed == (size_t)-1 || cond_phi == (size_t)-1 || cond_step == (size_t)-1 ||
        header_cond == (size_t)-1 || body_cond == (size_t)-1 || seed_value == (size_t)-1 ||
        carried_value == (size_t)-1 || use_a == (size_t)-1 || use_b == (size_t)-1 ||
        sum_value == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(cond_seed);
    instruction.as.mov_value = value_ssa_value_immediate(0);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(seed_value);
    instruction.as.mov_value = value_ssa_value_immediate(7);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_jump(entry, 1, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(cond_seed);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(cond_step);
    if (!value_ssa_block_append_phi(header, cond_phi, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    phi_inputs[0].predecessor_block_id = 0;
    phi_inputs[0].value = value_ssa_value_id(seed_value);
    phi_inputs[1].predecessor_block_id = 2;
    phi_inputs[1].value = value_ssa_value_id(carried_value);
    if (!value_ssa_block_append_phi(header, carried_value, phi_inputs, 2, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(use_a);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(carried_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(use_b);
    instruction.as.binary.lhs = value_ssa_value_id(carried_value);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(header, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(header_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(header, &instruction, error) ||
        !value_ssa_block_set_branch(header, value_ssa_value_id(header_cond), 2, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.result = value_ssa_value_id(cond_step);
    instruction.as.mov_value = value_ssa_value_immediate(1);
    instruction.kind = VALUE_SSA_INSTR_MOV;
    if (!value_ssa_block_append_instruction(body, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }
    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.result = value_ssa_value_id(body_cond);
    instruction.as.binary.op = VALUE_SSA_BINARY_LT;
    instruction.as.binary.lhs = value_ssa_value_id(cond_phi);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(body, &instruction, error) ||
        !value_ssa_block_set_branch(body, value_ssa_value_id(body_cond), 1, 3, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.result = value_ssa_value_id(sum_value);
    instruction.as.binary.lhs = value_ssa_value_id(use_a);
    instruction.as.binary.rhs = value_ssa_value_id(use_b);
    if (!value_ssa_block_append_instruction(exit_block, &instruction, error) ||
        !value_ssa_block_set_return(exit_block, value_ssa_value_id(sum_value), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int build_alloc_noninterfering_spill_reuse_program(ValueSsaProgram *program, ValueSsaError *error) {
    ValueSsaFunction *function = NULL;
    ValueSsaBasicBlock *entry = NULL;
    ValueSsaInstruction instruction;
    size_t value0;
    size_t use0;
    size_t value1;
    size_t use1;

    value_ssa_program_init(program);
    if (!value_ssa_program_append_function(program, "spill_reuse", 1, &function, error) ||
        !value_ssa_function_append_block(function, NULL, &entry, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    value0 = value_ssa_function_allocate_value(function);
    use0 = value_ssa_function_allocate_value(function);
    value1 = value_ssa_function_allocate_value(function);
    use1 = value_ssa_function_allocate_value(function);
    if (value0 == (size_t)-1 || use0 == (size_t)-1 || value1 == (size_t)-1 || use1 == (size_t)-1) {
        value_ssa_program_free(program);
        return 0;
    }

    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value0);
    instruction.as.mov_value = value_ssa_value_immediate(10);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use0);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value0);
    instruction.as.binary.rhs = value_ssa_value_immediate(1);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_MOV;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(value1);
    instruction.as.mov_value = value_ssa_value_immediate(20);
    if (!value_ssa_block_append_instruction(entry, &instruction, error)) {
        value_ssa_program_free(program);
        return 0;
    }

    instruction.kind = VALUE_SSA_INSTR_BINARY;
    instruction.has_result = 1;
    instruction.result = value_ssa_value_id(use1);
    instruction.as.binary.op = VALUE_SSA_BINARY_ADD;
    instruction.as.binary.lhs = value_ssa_value_id(value1);
    instruction.as.binary.rhs = value_ssa_value_immediate(2);
    if (!value_ssa_block_append_instruction(entry, &instruction, error) ||
        !value_ssa_block_set_return(entry, value_ssa_value_id(use1), error)) {
        value_ssa_program_free(program);
        return 0;
    }

    return 1;
}

static int test_value_ssa_allocate_function_smoke(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    int ok;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: smoke setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: smoke allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_alloc_dump("VALUE-SSA-ALLOC-SMOKE",
        &program.functions[0],
        &result,
        "alloc func main colors=2 values=5\n"
        "ssa.0 color=0 priority=3\n"
        "ssa.1 color=0 priority=3\n"
        "ssa.2 color=0 priority=3\n"
        "ssa.3 color=0 priority=11\n"
        "ssa.4 color=0 priority=11\n");

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_function_allocation_layout_smoke(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    ValueSsaFunctionAllocationLayout layout;
    const ValueSsaAllocatedValuePlacement *placement = NULL;
    const size_t *colored_values = NULL;
    const size_t *spilled_values = NULL;
    const size_t *used_colors = NULL;
    size_t colored_count = 0;
    size_t spilled_count = 0;
    size_t used_color_count = 0;
    const size_t expected_colored_values[] = {0, 1, 2, 3, 4};
    const size_t expected_used_colors[] = {0};
    const size_t expected_color0_group[] = {0, 1, 2, 3, 4};
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-smoke setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    value_ssa_function_allocation_layout_init(&layout);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error) ||
        !value_ssa_build_function_allocation_layout(&result, &layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-smoke build failed: %s\n", error.message);
        value_ssa_function_allocation_layout_free(&layout);
        value_ssa_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_function_layout_summary("VALUE-SSA-ALLOC-LAYOUT-SMOKE-SUMMARY",
        &layout,
        NULL,
        5,
        2,
        0,
        5,
        0,
        0,
        0,
        0,
        1,
        1,
        0);
    if (!value_ssa_function_allocation_layout_get_placement(&layout, 4, &placement) || !placement ||
        placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR || placement->color != 0 ||
        placement->spill_intended || placement->spill_confirmed || placement->spill_recovered) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-smoke expected ssa.4 colored in color 0\n");
        ok = 0;
    }
    if (!value_ssa_function_allocation_layout_get_colored_values(&layout, &colored_count, &colored_values) ||
        !value_ssa_function_allocation_layout_get_spilled_values(&layout, &spilled_count, &spilled_values) ||
        !value_ssa_function_allocation_layout_get_used_colors(&layout, &used_color_count, &used_colors)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-smoke grouped queries failed\n");
        ok = 0;
    } else {
        ok &= expect_layout_value_list("VALUE-SSA-ALLOC-LAYOUT-SMOKE",
            colored_values,
            colored_count,
            expected_colored_values,
            sizeof(expected_colored_values) / sizeof(expected_colored_values[0]),
            "colored-values");
        ok &= expect_layout_value_list(
            "VALUE-SSA-ALLOC-LAYOUT-SMOKE", spilled_values, spilled_count, NULL, 0, "spilled-values");
        ok &= expect_layout_value_list("VALUE-SSA-ALLOC-LAYOUT-SMOKE",
            used_colors,
            used_color_count,
            expected_used_colors,
            sizeof(expected_used_colors) / sizeof(expected_used_colors[0]),
            "used-colors");
        ok &= expect_layout_color_group("VALUE-SSA-ALLOC-LAYOUT-SMOKE",
            &layout,
            0,
            expected_color0_group,
            sizeof(expected_color0_group) / sizeof(expected_color0_group[0]));
        ok &= expect_layout_color_group_at("VALUE-SSA-ALLOC-LAYOUT-SMOKE",
            &layout,
            0,
            0,
            expected_color0_group,
            sizeof(expected_color0_group) / sizeof(expected_color0_group[0]));
    }

    ok &= expect_function_layout_dump("VALUE-SSA-ALLOC-LAYOUT-SMOKE",
        &program.functions[0],
        &layout,
        "alloc-layout func main colors=2 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    value_ssa_function_allocation_layout_free(&layout);
    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_layout_matches_result_projection(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    ValueSsaFunctionAllocationLayout direct_layout;
    ValueSsaFunctionAllocationLayout projected_layout;
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: function-layout-entry setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    value_ssa_function_allocation_layout_init(&direct_layout);
    value_ssa_function_allocation_layout_init(&projected_layout);
    if (!value_ssa_allocate_function_layout(&program.functions[0], 2, &direct_layout, &error) ||
        !value_ssa_allocate_function(&program.functions[0], 2, &result, &error) ||
        !value_ssa_build_function_allocation_layout(&result, &projected_layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: function-layout-entry build failed: %s\n", error.message);
        value_ssa_function_allocation_layout_free(&projected_layout);
        value_ssa_function_allocation_layout_free(&direct_layout);
        value_ssa_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_function_layout_dump("VALUE-SSA-ALLOC-FUNCTION-LAYOUT-ENTRY",
        &program.functions[0],
        &direct_layout,
        "alloc-layout func spill colors=2 values=5 colored=4 spilled=1 optimistic_colored=1 confirmed_spilled=1 recovered_colored=0 spill_slots=1\n"
        "used-colors: 0 1\n"
        "colored-values: ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values: ssa.0\n"
        "optimistic-colored-values: ssa.2\n"
        "confirmed-spilled-values: ssa.0\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.1 ssa.3 ssa.4\n"
        "color-group 1: ssa.2\n"
        "spill-slot-group 0: ssa.0\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 color=0\n"
        "ssa.2 color=1 spill-intended\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");
    ok &= expect_function_layout_dump("VALUE-SSA-ALLOC-FUNCTION-LAYOUT-ENTRY-PROJECTED",
        &program.functions[0],
        &projected_layout,
        "alloc-layout func spill colors=2 values=5 colored=4 spilled=1 optimistic_colored=1 confirmed_spilled=1 recovered_colored=0 spill_slots=1\n"
        "used-colors: 0 1\n"
        "colored-values: ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values: ssa.0\n"
        "optimistic-colored-values: ssa.2\n"
        "confirmed-spilled-values: ssa.0\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.1 ssa.3 ssa.4\n"
        "color-group 1: ssa.2\n"
        "spill-slot-group 0: ssa.0\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 color=0\n"
        "ssa.2 color=1 spill-intended\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    value_ssa_function_allocation_layout_free(&projected_layout);
    value_ssa_function_allocation_layout_free(&direct_layout);
    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_matches_from_plan_pipeline(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaDefUseAnalysis def_use;
    ValueSsaCfgAnalysis cfg;
    ValueSsaLivenessAnalysis liveness;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaAllocatorCoalesceAnalysis coalesce_analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult direct_result;
    ValueSsaAllocationResult from_plan_result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_def_use_analysis_init(&def_use);
    value_ssa_cfg_analysis_init(&cfg);
    value_ssa_liveness_analysis_init(&liveness);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_allocator_coalesce_analysis_init(&coalesce_analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&direct_result);
    value_ssa_allocation_result_init(&from_plan_result);

    if (!build_alloc_evict_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: pipeline-match setup failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_compute_def_use_analysis(&program.functions[0], &def_use, &error) ||
        !value_ssa_compute_cfg_analysis(&program.functions[0], &cfg, &error) ||
        !value_ssa_compute_liveness_analysis(&program.functions[0], &cfg, &liveness, &error) ||
        !value_ssa_compute_interference_graph(&program.functions[0], &cfg, &liveness, &interference, &error) ||
        !value_ssa_compute_copy_affinity_graph(&program.functions[0], &interference, &affinity, &error) ||
        !value_ssa_compute_allocation_prep(
            &program.functions[0], &def_use, &liveness, &interference, &affinity, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], &prep, &interference, &affinity, 2, &coalesce_analysis, &error) ||
        !value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce_analysis, 2, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: pipeline-match artifact build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function(&program.functions[0], 2, &direct_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: pipeline-match direct allocate failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0],
            &prep,
            &interference,
            &coalesce_analysis,
            &plan,
            2,
            &from_plan_result,
            &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: pipeline-match from-plan allocate failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_alloc_results_equal_by_dump(
        "VALUE-SSA-ALLOC-PIPELINE-MATCH", &program.functions[0], &direct_result, &from_plan_result);

cleanup:
    value_ssa_allocation_result_free(&from_plan_result);
    value_ssa_allocation_result_free(&direct_result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce_analysis);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_liveness_analysis_free(&liveness);
    value_ssa_cfg_analysis_free(&cfg);
    value_ssa_def_use_analysis_free(&def_use);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_smoke(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_allocator_plan_init(&plan);

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-smoke setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, NULL, NULL, NULL, 2, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-smoke build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 3, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 3, 0);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 4, 0);
    ok &= expect_plan_effective_move_related("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 3, 0, 0);
    ok &= expect_plan_effective_move_related("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 4, 0, 0);
    ok &= expect_plan_move_work_class("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 3, VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL);
    ok &= expect_plan_move_work_class("VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 0, VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL);
    ok &= expect_plan_move_work_transition(
        "VALUE-SSA-ALLOC-PLAN-SMOKE", &plan, 3, VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL, VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL);
    ok &= expect_allocator_plan_dump("VALUE-SSA-ALLOC-PLAN-SMOKE",
        &program.functions[0],
        &prep,
        &plan,
        2,
        "allocator-plan main colors=2:\n"
        "  0: ssa.3 phase=simplify remove=simplify-remove class=move-hint coalesce=ssa.3/2 priority=11 spill_cost=3(uses=1,blocks=1,affinity=1) detail=(use_loops=0,use_calls=0,live=1,loops=0,calls=0,call_cross=0,xblock=0) split_child=no depth=0 degree=0->0 effective_degree=0->0 move_work=internal->internal:15 family_members=2->1 family_pressure=(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0)->(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0) move_related=yes->yes effective_move=no->no frozen=no\n"
        "  1: ssa.4 phase=simplify remove=simplify-remove class=move-hint coalesce=ssa.3/2 priority=11 spill_cost=3(uses=1,blocks=1,affinity=1) detail=(use_loops=0,use_calls=0,live=1,loops=0,calls=0,call_cross=0,xblock=0) split_child=yes depth=1 degree=0->0 effective_degree=0->0 move_work=internal->internal:13 family_members=1->0 family_pressure=(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0)->(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0) move_related=yes->no effective_move=no->no frozen=no\n"
        "  2: ssa.0 phase=simplify remove=simplify-remove class=isolated coalesce=ssa.0/1 priority=3 spill_cost=2(uses=1,blocks=1,affinity=0) detail=(use_loops=0,use_calls=0,live=1,loops=0,calls=0,call_cross=0,xblock=0) split_child=no depth=0 degree=0->0 effective_degree=0->0 move_work=internal->internal:5 family_members=1->0 family_pressure=(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0)->(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0) move_related=no->no effective_move=no->no frozen=no\n"
        "  3: ssa.1 phase=simplify remove=simplify-remove class=isolated coalesce=ssa.1/1 priority=3 spill_cost=2(uses=1,blocks=1,affinity=0) detail=(use_loops=0,use_calls=0,live=1,loops=0,calls=0,call_cross=0,xblock=0) split_child=no depth=0 degree=0->0 effective_degree=0->0 move_work=internal->internal:5 family_members=1->0 family_pressure=(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0)->(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0) move_related=no->no effective_move=no->no frozen=no\n"
        "  4: ssa.2 phase=simplify remove=simplify-remove class=isolated coalesce=ssa.2/1 priority=3 spill_cost=2(uses=1,blocks=1,affinity=0) detail=(use_loops=0,use_calls=0,live=1,loops=0,calls=0,call_cross=0,xblock=0) split_child=no depth=0 degree=0->0 effective_degree=0->0 move_work=internal->internal:5 family_members=1->0 family_pressure=(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0)->(neighbors=0,ready=0,ready_affinity=0,best_ready=-0@0,mutual=no,affinity=0) move_related=no->no effective_move=no->no frozen=no\n");

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_evicts_only_unique_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    int ok;

    if (!build_alloc_unique_blocker_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-blocker setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-blocker allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = 1;
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-UNIQUE-BLOCKER", &result, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-UNIQUE-BLOCKER", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-UNIQUE-BLOCKER", &result, 2, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-UNIQUE-BLOCKER", &result, 3, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-UNIQUE-BLOCKER", &result, 4, 0);

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_prefers_highest_affinity_color(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    int ok;

    if (!build_alloc_affinity_weighted_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: affinity setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: affinity allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = 1;
    ok &= expect_query_color("VALUE-SSA-ALLOC-AFFINITY", &result, 3, 1, result.colors[3]);
    ok &= expect_query_color("VALUE-SSA-ALLOC-AFFINITY", &result, 5, 1, result.colors[5]);
    ok &= expect_query_color("VALUE-SSA-ALLOC-AFFINITY", &result, 6, 1, result.colors[5]);

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_prefers_coalescible_partner_color(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    size_t lhs_value;
    size_t rhs_value;
    int lhs_has_color = 0;
    int rhs_has_color = 0;
    size_t lhs_color = 0;
    size_t rhs_color = 0;
    int ok = 1;

    if (!build_alloc_coalesce_safe_program(&program, &lhs_value, &rhs_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-color setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-color allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_allocation_result_get_color(&result, lhs_value, &lhs_has_color, &lhs_color) ||
        !value_ssa_allocation_result_get_color(&result, rhs_value, &rhs_has_color, &rhs_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-color query failed\n");
        ok = 0;
    } else if (!lhs_has_color || !rhs_has_color || lhs_color != rhs_color) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-color expected ssa.%zu and ssa.%zu to share color, got (%d,%zu) and (%d,%zu)\n",
            lhs_value,
            rhs_value,
            lhs_has_color,
            lhs_color,
            rhs_has_color,
            rhs_color);
        ok = 0;
    }
    if (!result.used_preferred_color_flags || !result.preferred_color_sources ||
        !result.preferred_color_partner_value_ids ||
        !result.used_preferred_color_flags[rhs_value] ||
        result.preferred_color_sources[rhs_value] != VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT ||
        result.preferred_color_partner_value_ids[rhs_value] != lhs_value) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-color expected ssa.%zu preferred source to be coalesce-direct(ssa.%zu)\n",
            rhs_value,
            lhs_value);
        ok = 0;
    }

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_prefers_coalesced_class_color(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    size_t root_value;
    size_t mid_value;
    size_t leaf_value;
    int root_has_color = 0;
    int mid_has_color = 0;
    int leaf_has_color = 0;
    size_t root_color = 0;
    size_t mid_color = 0;
    size_t leaf_color = 0;
    int ok = 1;

    if (!build_alloc_coalesce_chain_program(&program, &root_value, &mid_value, &leaf_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-class-color setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-class-color allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_allocation_result_get_color(&result, root_value, &root_has_color, &root_color) ||
        !value_ssa_allocation_result_get_color(&result, mid_value, &mid_has_color, &mid_color) ||
        !value_ssa_allocation_result_get_color(&result, leaf_value, &leaf_has_color, &leaf_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-class-color query failed\n");
        ok = 0;
    } else if (!root_has_color || !mid_has_color || !leaf_has_color || root_color != mid_color ||
        root_color != leaf_color) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-class-color expected ssa.%zu/ssa.%zu/ssa.%zu to share color, got (%d,%zu), (%d,%zu), (%d,%zu)\n",
            root_value,
            mid_value,
            leaf_value,
            root_has_color,
            root_color,
            mid_has_color,
            mid_color,
            leaf_has_color,
            leaf_color);
        ok = 0;
    }

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_preserves_preferred_coalesce_color_on_targeted_eviction(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int partner_has_color = 0;
    int target_has_color = 0;
    size_t partner_color = 0;
    size_t target_color = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-coalesce-evict setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    analysis.value_count = 4;
    analysis.item_count = 1;
    analysis.items = (ValueSsaAllocatorCoalesceItem *)calloc(1, sizeof(ValueSsaAllocatorCoalesceItem));
    analysis.value_roots = (size_t *)calloc(4, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    plan.value_count = 4;
    plan.item_count = 4;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !interference.interferes || !analysis.items || !analysis.value_roots ||
        !analysis.class_sizes || !plan.items) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-coalesce-evict manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    interference.interferes[1 * 4 + 2] = 1;
    interference.interferes[2 * 4 + 1] = 1;
    interference.interferes[1 * 4 + 3] = 1;
    interference.interferes[3 * 4 + 1] = 1;
    interference.interferes[0 * 4 + 3] = 1;
    interference.interferes[3 * 4 + 0] = 1;

    analysis.items[0].lhs_value_id = 0;
    analysis.items[0].rhs_value_id = 1;
    analysis.items[0].weight = 1;
    analysis.items[0].can_coalesce = 1;
    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 0;
    analysis.value_roots[2] = 2;
    analysis.value_roots[3] = 3;
    analysis.class_sizes[0] = 2;
    analysis.class_sizes[2] = 1;
    analysis.class_sizes[3] = 1;

    plan.items[0].value_id = 1;
    plan.items[0].priority = 50;
    plan.items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SPILL;
    plan.items[1].value_id = 2;
    plan.items[1].priority = 5;
    plan.items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[2].value_id = 3;
    plan.items[2].priority = 4;
    plan.items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[3].value_id = 0;
    plan.items[3].priority = 10;
    plan.items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &analysis, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-coalesce-evict allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_color(&result, 0, &partner_has_color, &partner_color) ||
        !value_ssa_allocation_result_get_color(&result, 1, &target_has_color, &target_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: targeted-coalesce-evict color query failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!partner_has_color || !target_has_color || partner_color != target_color) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-coalesce-evict expected ssa.0 and ssa.1 to share color, got (%d,%zu) and (%d,%zu)\n",
            partner_has_color,
            partner_color,
            target_has_color,
            target_color);
        ok = 0;
    }
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT", &result, 2, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT", &result, 3, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT", &result, 1, 1, 0);
    ok &= expect_query_targeted_eviction_outcome(
        "VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT", &result, 1, 1, 2, 1, 1);
    if (!result.used_preferred_color_flags || !result.preferred_color_sources ||
        !result.preferred_color_partner_value_ids ||
        !result.used_preferred_color_flags[1] ||
        result.preferred_color_sources[1] != VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT ||
        result.preferred_color_partner_value_ids[1] != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-coalesce-evict expected ssa.1 to record preferred=coalesce-direct(ssa.0)\n");
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT",
        &plan,
        &result,
        "preferred=coalesce-direct(ssa.0)",
        "targeted-evict=ssa.2->color1");
    ok &= expect_has_color("VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT", &result, 2, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-TARGETED-COALESCE-EVICT", &result, 2, 0);

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_family_supported_targeted_recolor(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int target_has_color = 0;
    int blocker_has_color = 0;
    int sibling_has_color = 0;
    size_t target_color = 0;
    size_t blocker_color = 0;
    size_t sibling_color = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_targeted_recolor_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-family-recolor setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_targeted_recolor_family_artifacts(
            &program.functions[0], &prep, &interference, &analysis, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-family-recolor manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &analysis, &plan, 3, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-family-recolor allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_color(&result, 0, &target_has_color, &target_color) ||
        !value_ssa_allocation_result_get_color(&result, 1, &blocker_has_color, &blocker_color) ||
        !value_ssa_allocation_result_get_color(&result, 2, &sibling_has_color, &sibling_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: targeted-family-recolor color query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!target_has_color || !blocker_has_color || !sibling_has_color ||
        target_color != 0 || sibling_color != 2 || blocker_color != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: targeted-family-recolor expected target/blocker/sibling colors (0,2,2), got (%d,%zu), (%d,%zu), (%d,%zu)\n",
            target_has_color,
            target_color,
            blocker_has_color,
            blocker_color,
            sibling_has_color,
            sibling_color);
        ok = 0;
    }
    ok &= expect_query_targeted_eviction_outcome(
        "VALUE-SSA-ALLOC-TARGETED-FAMILY-RECOLOR", &result, 0, 1, 1, 1, 2);
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-TARGETED-FAMILY-RECOLOR",
        &plan,
        &result,
        "preferred=coalesce-direct(ssa.4)",
        "targeted-evict=ssa.1->color2");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_weighted_family_targeted_recolor(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int target_has_color = 0;
    int blocker_has_color = 0;
    int light_partner_has_color = 0;
    int heavy_partner_has_color = 0;
    size_t target_color = 0;
    size_t blocker_color = 0;
    size_t light_partner_color = 0;
    size_t heavy_partner_color = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_targeted_recolor_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-targeted-family-recolor setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_weighted_targeted_recolor_family_artifacts(
            &program.functions[0], &prep, &interference, &analysis, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-targeted-family-recolor manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &analysis, &plan, 3, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-targeted-family-recolor allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_color(&result, 0, &target_has_color, &target_color) ||
        !value_ssa_allocation_result_get_color(&result, 1, &blocker_has_color, &blocker_color) ||
        !value_ssa_allocation_result_get_color(&result, 2, &light_partner_has_color, &light_partner_color) ||
        !value_ssa_allocation_result_get_color(&result, 3, &heavy_partner_has_color, &heavy_partner_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: weighted-targeted-family-recolor color query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!target_has_color || !blocker_has_color || !light_partner_has_color || !heavy_partner_has_color ||
        target_color != 0 || light_partner_color != 1 || heavy_partner_color != 2 || blocker_color != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-targeted-family-recolor expected target/light/heavy/blocker colors "
            "(0,1,2,2), got (%d,%zu), (%d,%zu), (%d,%zu), (%d,%zu)\n",
            target_has_color,
            target_color,
            light_partner_has_color,
            light_partner_color,
            heavy_partner_has_color,
            heavy_partner_color,
            blocker_has_color,
            blocker_color);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-WEIGHTED-TARGETED-FAMILY-RECOLOR",
        &plan,
        &result,
        "preferred=coalesce-direct(ssa.4)",
        "ssa.1 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored preferred=coalesce-direct(ssa.3) color=2");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_uses_runtime_merged_family_alias(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int partner_has_color = 0;
    int target_has_color = 0;
    size_t partner_color = 0;
    size_t target_color = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-merged-alias setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    analysis.value_count = 4;
    analysis.item_count = 0;
    analysis.items = NULL;
    analysis.value_roots = (size_t *)calloc(4, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    plan.value_count = 4;
    plan.item_count = 4;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    plan.final_runtime_coalesce_roots = (size_t *)calloc(4, sizeof(size_t));
    plan.final_runtime_coalesce_class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !interference.interferes || !analysis.value_roots || !analysis.class_sizes ||
        !plan.items || !plan.final_runtime_coalesce_roots || !plan.final_runtime_coalesce_class_sizes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-merged-alias manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 1;
    analysis.value_roots[2] = 2;
    analysis.value_roots[3] = 3;
    analysis.class_sizes[0] = 1;
    analysis.class_sizes[1] = 1;
    analysis.class_sizes[2] = 1;
    analysis.class_sizes[3] = 1;

    plan.final_runtime_coalesce_roots[0] = 0;
    plan.final_runtime_coalesce_roots[1] = 0;
    plan.final_runtime_coalesce_roots[2] = 2;
    plan.final_runtime_coalesce_roots[3] = 3;
    plan.final_runtime_coalesce_class_sizes[0] = 2;
    plan.final_runtime_coalesce_class_sizes[1] = 0;
    plan.final_runtime_coalesce_class_sizes[2] = 1;
    plan.final_runtime_coalesce_class_sizes[3] = 1;

    plan.items[0].value_id = 1;
    plan.items[0].priority = 50;
    plan.items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[1].value_id = 2;
    plan.items[1].priority = 5;
    plan.items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[2].value_id = 3;
    plan.items[2].priority = 4;
    plan.items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[3].value_id = 0;
    plan.items[3].priority = 10;
    plan.items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &analysis, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-merged-alias allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_color(&result, 0, &partner_has_color, &partner_color) ||
        !value_ssa_allocation_result_get_color(&result, 1, &target_has_color, &target_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: runtime-merged-alias color query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!partner_has_color || !target_has_color || partner_color != target_color) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-merged-alias expected ssa.0 and ssa.1 to share color, got (%d,%zu) and (%d,%zu)\n",
            partner_has_color,
            partner_color,
            target_has_color,
            target_color);
        ok = 0;
    }
    if (!result.used_preferred_color_flags || !result.preferred_color_sources ||
        !result.preferred_color_partner_value_ids ||
        !result.used_preferred_color_flags[1] ||
        result.preferred_color_sources[1] != VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS ||
        result.preferred_color_partner_value_ids[1] != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-merged-alias expected ssa.1 to record preferred=coalesce-class(ssa.0)\n");
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RUNTIME-MERGED-ALIAS",
        &plan,
        &result,
        "preferred=coalesce-class(ssa.0)",
        "outcome=colored");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_runtime_family_dominant_color(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int family_seed_has_color = 0;
    int family_target_has_color = 0;
    size_t family_seed_color = 0;
    size_t family_target_color = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_runtime_family_color_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-family-dominant-color setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 5;
    prep.use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference.value_count = 5;
    interference.interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    analysis.value_count = 5;
    analysis.item_count = 0;
    analysis.items = NULL;
    analysis.value_roots = (size_t *)calloc(5, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(5, sizeof(size_t));
    plan.value_count = 5;
    plan.item_count = 5;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    plan.final_runtime_coalesce_roots = (size_t *)calloc(5, sizeof(size_t));
    plan.final_runtime_coalesce_class_sizes = (size_t *)calloc(5, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !interference.interferes || !analysis.value_roots || !analysis.class_sizes ||
        !plan.items || !plan.final_runtime_coalesce_roots || !plan.final_runtime_coalesce_class_sizes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-family-dominant-color manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 1;
    analysis.value_roots[2] = 2;
    analysis.value_roots[3] = 3;
    analysis.value_roots[4] = 4;
    analysis.class_sizes[0] = 1;
    analysis.class_sizes[1] = 1;
    analysis.class_sizes[2] = 1;
    analysis.class_sizes[3] = 1;
    analysis.class_sizes[4] = 1;

    plan.final_runtime_coalesce_roots[0] = 0;
    plan.final_runtime_coalesce_roots[1] = 0;
    plan.final_runtime_coalesce_roots[2] = 0;
    plan.final_runtime_coalesce_roots[3] = 3;
    plan.final_runtime_coalesce_roots[4] = 0;
    plan.final_runtime_coalesce_class_sizes[0] = 4;
    plan.final_runtime_coalesce_class_sizes[1] = 0;
    plan.final_runtime_coalesce_class_sizes[2] = 0;
    plan.final_runtime_coalesce_class_sizes[3] = 1;
    plan.final_runtime_coalesce_class_sizes[4] = 0;

    interference.interferes[1 * 5 + 3] = 1u;
    interference.interferes[3 * 5 + 1] = 1u;
    interference.interferes[4 * 5 + 3] = 1u;
    interference.interferes[3 * 5 + 4] = 1u;

    plan.items[0].value_id = 2;
    plan.items[0].priority = 60;
    plan.items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[1].value_id = 4;
    plan.items[1].priority = 70;
    plan.items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[2].value_id = 1;
    plan.items[2].priority = 80;
    plan.items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[3].value_id = 0;
    plan.items[3].priority = 90;
    plan.items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[4].value_id = 3;
    plan.items[4].priority = 100;
    plan.items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &analysis, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-family-dominant-color allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_color(&result, 1, &family_seed_has_color, &family_seed_color) ||
        !value_ssa_allocation_result_get_color(&result, 2, &family_target_has_color, &family_target_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: runtime-family-dominant-color color query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!family_seed_has_color || !family_target_has_color || family_seed_color != 1 || family_target_color != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-family-dominant-color expected ssa.1 and ssa.2 to use dominant family color 1, got (%d,%zu) and (%d,%zu)\n",
            family_seed_has_color,
            family_seed_color,
            family_target_has_color,
            family_target_color);
        ok = 0;
    }
    if (!result.used_preferred_color_flags || !result.preferred_color_sources ||
        !result.preferred_color_partner_value_ids ||
        !result.used_preferred_color_flags[2] ||
        result.preferred_color_sources[2] != VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS ||
        result.preferred_color_partner_value_ids[2] != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: runtime-family-dominant-color expected ssa.2 preferred=coalesce-class(ssa.1)\n");
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RUNTIME-FAMILY-DOMINANT-COLOR",
        &plan,
        &result,
        "ssa.2",
        "preferred=coalesce-class(ssa.1) color=1");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_uses_plan_family_color_without_analysis(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int family_seed_has_color = 0;
    int family_target_has_color = 0;
    size_t family_seed_color = 0;
    size_t family_target_color = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_runtime_family_color_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: plan-family-color-without-analysis setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 5;
    prep.use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    interference.value_count = 5;
    interference.interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    plan.value_count = 5;
    plan.item_count = 5;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(5, sizeof(ValueSsaAllocatorPlanItem));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !interference.interferes || !plan.items) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: plan-family-color-without-analysis manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    interference.interferes[1 * 5 + 3] = 1u;
    interference.interferes[3 * 5 + 1] = 1u;
    interference.interferes[4 * 5 + 3] = 1u;
    interference.interferes[3 * 5 + 4] = 1u;

    plan.items[0].value_id = 2;
    plan.items[0].priority = 60;
    plan.items[0].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[0].coalesce_root_value_id = 0;
    plan.items[0].coalesce_class_size = 4;
    plan.items[1].value_id = 4;
    plan.items[1].priority = 70;
    plan.items[1].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[1].coalesce_root_value_id = 0;
    plan.items[1].coalesce_class_size = 4;
    plan.items[2].value_id = 1;
    plan.items[2].priority = 80;
    plan.items[2].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[2].coalesce_root_value_id = 0;
    plan.items[2].coalesce_class_size = 4;
    plan.items[3].value_id = 0;
    plan.items[3].priority = 90;
    plan.items[3].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[3].coalesce_root_value_id = 0;
    plan.items[3].coalesce_class_size = 4;
    plan.items[4].value_id = 3;
    plan.items[4].priority = 100;
    plan.items[4].removal_kind = VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY;
    plan.items[4].coalesce_root_value_id = 3;
    plan.items[4].coalesce_class_size = 1;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: plan-family-color-without-analysis allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_color(&result, 1, &family_seed_has_color, &family_seed_color) ||
        !value_ssa_allocation_result_get_color(&result, 2, &family_target_has_color, &family_target_color)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: plan-family-color-without-analysis color query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!family_seed_has_color || !family_target_has_color || family_seed_color != 1 || family_target_color != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: plan-family-color-without-analysis expected ssa.1 and ssa.2 to use family color 1, got (%d,%zu) and (%d,%zu)\n",
            family_seed_has_color,
            family_seed_color,
            family_target_has_color,
            family_target_color);
        ok = 0;
    }
    if (!result.used_preferred_color_flags || !result.preferred_color_sources ||
        !result.preferred_color_partner_value_ids ||
        !result.used_preferred_color_flags[2] ||
        result.preferred_color_sources[2] != VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS ||
        result.preferred_color_partner_value_ids[2] != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: plan-family-color-without-analysis expected ssa.2 preferred=coalesce-class(ssa.1)\n");
        ok = 0;
    }

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_spill(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    static const size_t clique_values[] = {0, 1, 2};
    int ok;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = 1;
    ok &= expect_spill_count_on_values("VALUE-SSA-ALLOC-SPILL", &result, clique_values, 3, 1);
    ok &= expect_has_color("VALUE-SSA-ALLOC-SPILL", &result, 3, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-SPILL", &result, 3, 0);
    ok &= expect_result_counts("VALUE-SSA-ALLOC-SPILL", &result, 4, 1);
    ok &= expect_has_color("VALUE-SSA-ALLOC-SPILL", &result, 2, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-SPILL", &result, 2, 1, 0);
    ok &= expect_query_spill_slot("VALUE-SSA-ALLOC-SPILL", &result, 0, result.spill_flags[0], 0);
    ok &= expect_query_spill_slot("VALUE-SSA-ALLOC-SPILL", &result, 1, result.spill_flags[1], 0);
    ok &= expect_query_spill_slot("VALUE-SSA-ALLOC-SPILL", &result, 2, result.spill_flags[2], 0);

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_marks_evicted_blocker_spill_confirmed(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_eviction_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: confirmed-eviction setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_eviction_artifacts(&program.functions[0],
            &prep,
            &interference,
            &plan,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            0,
            0)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: confirmed-eviction manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: confirmed-eviction allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_spill_flag("VALUE-SSA-ALLOC-CONFIRMED-EVICTION", &result, 1, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-CONFIRMED-EVICTION", &result, 1, 0, 1);
    ok &= expect_select_trace_contains(
        "VALUE-SSA-ALLOC-CONFIRMED-EVICTION", &plan, &result, "ssa.1", "outcome=spill-confirmed");
    ok &= expect_alloc_dump("VALUE-SSA-ALLOC-CONFIRMED-EVICTION",
        &program.functions[0],
        &result,
        "alloc func manual_eviction colors=2 values=3\n"
        "ssa.0 color=1 priority=20\n"
        "ssa.1 spill slot=0 priority=10 spill-confirmed\n"
        "ssa.2 color=0 priority=10\n");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_evicting_spill_intended_blocker_on_priority_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_eviction_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: intended-eviction setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_eviction_artifacts(&program.functions[0],
            &prep,
            &interference,
            &plan,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SPILL,
            0,
            0)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: intended-eviction manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: intended-eviction allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_spill_flag("VALUE-SSA-ALLOC-INTENDED-EVICTION-TIE", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-INTENDED-EVICTION-TIE", &result, 2, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-INTENDED-EVICTION-TIE", &result, 2, 1, 1);
    ok &= expect_select_trace_contains(
        "VALUE-SSA-ALLOC-INTENDED-EVICTION-TIE", &plan, &result, "ssa.2", "outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_evicting_rematerializable_blocker_on_priority_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_eviction_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: remat-eviction setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_eviction_artifacts(&program.functions[0],
            &prep,
            &interference,
            &plan,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            0,
            1)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: remat-eviction manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: remat-eviction allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_spill_flag("VALUE-SSA-ALLOC-REMAT-EVICTION-TIE", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-REMAT-EVICTION-TIE", &result, 2, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-REMAT-EVICTION-TIE", &result, 2, 0, 1);
    ok &= expect_select_trace_contains(
        "VALUE-SSA-ALLOC-REMAT-EVICTION-TIE", &plan, &result, "ssa.2", "outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_evicting_single_block_blocker_on_cost_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_eviction_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: single-block-eviction setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_eviction_artifacts(&program.functions[0],
            &prep,
            &interference,
            &plan,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            0,
            0)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: single-block-eviction manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.single_block_live_ranges = (unsigned char *)calloc(3, sizeof(unsigned char));
    if (!prep.single_block_live_ranges) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: single-block-eviction single-block facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.single_block_live_ranges[1] = 1;
    prep.single_block_live_ranges[2] = 0;
    plan.items[2].spill_live_range_cost += 1;
    plan.items[2].spill_cost += 1;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: single-block-eviction allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_spill_flag("VALUE-SSA-ALLOC-SINGLE-BLOCK-EVICTION-TIE", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-SINGLE-BLOCK-EVICTION-TIE", &result, 2, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-SINGLE-BLOCK-EVICTION-TIE", &result, 1, 0, 1);
    ok &= expect_select_trace_contains(
        "VALUE-SSA-ALLOC-SINGLE-BLOCK-EVICTION-TIE", &plan, &result, "ssa.1", "outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_evicting_split_child_blocker_on_cost_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_eviction_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-child-eviction setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_eviction_artifacts(&program.functions[0],
            &prep,
            &interference,
            &plan,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            10,
            VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
            0,
            0)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-child-eviction manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.split_child_values[1] = 1;
    prep.split_child_values[2] = 1;
    prep.split_child_depths = (size_t *)calloc(3, sizeof(size_t));
    if (!prep.split_child_depths) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-eviction split depths allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.split_child_depths[1] = 1;
    prep.split_child_depths[2] = 2;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-child-eviction allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_spill_flag("VALUE-SSA-ALLOC-SPLIT-CHILD-EVICTION-TIE", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-SPLIT-CHILD-EVICTION-TIE", &result, 2, 1);
    ok &= expect_select_trace_contains(
        "VALUE-SSA-ALLOC-SPLIT-CHILD-EVICTION-TIE", &plan, &result, "ssa.2", "outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_retries_spilled_value_after_late_eviction(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-after-eviction setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-after-eviction manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-after-eviction allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-after-eviction stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 0, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 0, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 0, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 0, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 0, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 1, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 1, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION", &result, 1, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 ||
        stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-after-eviction stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION",
        &plan,
        &result,
        "0: ssa.2 remove=simplify-remove",
        "1: ssa.1 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-AFTER-EVICTION",
        &plan,
        &result,
        "2: ssa.0 remove=spill-remove spill_cost=4(uses=3,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.3 remove=simplify-remove");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_retries_with_late_retry_eviction(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_evict_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-with-evict setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_evict_artifacts(&program.functions[0], &prep, &interference, &plan, 1)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-with-evict manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-with-evict allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-with-evict stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 0, 1, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 0, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 0, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 0, 1, 4);
    ok &= expect_query_retry_eviction_outcome("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 0, 1, 4, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 1, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 1, 0, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 4, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-WITH-EVICT", &result, 4, 0, 1);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 1 ||
        stats.optimistic_direct_count != 0 ||
        stats.optimistic_recovered_count != 1 || stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-with-evict stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-WITH-EVICT",
        &plan,
        &result,
        "0: ssa.4 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed",
        "3: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-recovered color=0 retry-step=0 retry-phase=0@ssa.0 retry-phase-step=0 retry-phase-size=1 retry-entry=ssa.0[1/1] retry-evict=ssa.4");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_recolors_retry_blocker_instead_of_spilling(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_evict_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-recolor setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_evict_artifacts(&program.functions[0], &prep, &interference, &plan, 0)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-recolor manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-recolor allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-recolor stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-RECOLOR", &result, 0, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-RECOLOR", &result, 0, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-RECOLOR", &result, 0, 1, 4);
    ok &= expect_query_retry_eviction_outcome("VALUE-SSA-ALLOC-RETRY-RECOLOR", &result, 0, 1, 4, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-RECOLOR", &result, 4, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-RECOLOR", &result, 4, 0);
    if (stats.colored_count != 4 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 1 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-recolor stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-RECOLOR",
        &plan,
        &result,
        "0: ssa.4 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=1",
        "3: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-recovered color=0 retry-step=0 retry-phase=0@ssa.0 retry-phase-step=0 retry-phase-size=1 retry-entry=ssa.0[1/1] retry-evict=ssa.4->color1");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_spill_intended_candidate(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_evict_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-spill-intended setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_intended_bias_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-spill-intended manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-spill-intended allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-spill-intended stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 0, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 0, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 0, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 0, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 1, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 1, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 3, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 3, 0, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 4, 1, 1);
    ok &= expect_query_generic_eviction_outcome("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED", &result, 4, 1, 3, 0, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 ||
        stats.optimistic_recovered_count != 1 || stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-spill-intended stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-SPILL-INTENDED",
        &plan,
        &result,
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "4: ssa.4 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=1 main-evict=ssa.3");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_split_child_candidate(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_evict_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-child setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_intended_bias_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-child manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.split_child_values[0] = 1;
    prep.split_child_values[1] = 1;
    prep.split_family_root_value_ids = (size_t *)calloc(5, sizeof(size_t));
    prep.split_child_depths = (size_t *)calloc(5, sizeof(size_t));
    if (!prep.split_family_root_value_ids || !prep.split_child_depths) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-split-child split family facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.split_family_root_value_ids[0] = 0;
    prep.split_family_root_value_ids[1] = 0;
    prep.split_family_root_value_ids[2] = 2;
    prep.split_family_root_value_ids[3] = 3;
    prep.split_family_root_value_ids[4] = 4;
    prep.split_child_depths[0] = 2;
    prep.split_child_depths[1] = 1;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-child allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-split-child stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD", &result, 0, 1, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD", &result, 0, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD", &result, 1, 1);
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD",
        &plan,
        &result,
        "ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "retry-step=0");
    rollback_retry_recovery_value(&result, 0);
    result.next_spill_recovery_order = 0;
    result.next_spill_recovery_phase_id = 0;
    {
        ValueSsaAllocatorRetryFamilyAgenda agenda;

        value_ssa_allocator_retry_family_agenda_init(&agenda);
        if (!value_ssa_compute_allocator_retry_family_agenda(
                &program.functions[0], &prep, &interference, NULL, &plan, &result, 2, &agenda, &error)) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: retry-split-child agenda failed: %s\n", error.message);
            value_ssa_allocator_retry_family_agenda_free(&agenda);
            ok = 0;
            goto cleanup;
        }
        ok &= expect_retry_family_agenda_split_child("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD", &agenda, 0, 1);
        ok &= expect_retry_family_agenda_split_child_depth("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD", &agenda, 0, 2);
        ok &= expect_retry_family_agenda_split_family("VALUE-SSA-ALLOC-RETRY-SPLIT-CHILD", &agenda, 0, 0, 1, 1);
        value_ssa_allocator_retry_family_agenda_free(&agenda);
    }

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_retry_family_agenda_split_summary_tracks_representative_root(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocatorRetryFamilyAgenda agenda;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocator_retry_family_agenda_init(&agenda);

    if (!build_alloc_manual_retry_evict_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-summary-representative setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_intended_bias_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-summary-representative manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.split_child_values[0] = 1;
    prep.split_child_values[1] = 1;
    prep.split_family_root_value_ids = (size_t *)calloc(5, sizeof(size_t));
    prep.split_child_depths = (size_t *)calloc(5, sizeof(size_t));
    if (!prep.split_family_root_value_ids || !prep.split_child_depths) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-summary-representative split family facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.split_family_root_value_ids[0] = 0;
    prep.split_family_root_value_ids[1] = 1;
    prep.split_family_root_value_ids[2] = 2;
    prep.split_family_root_value_ids[3] = 3;
    prep.split_family_root_value_ids[4] = 4;
    prep.split_child_depths[0] = 2;
    prep.split_child_depths[1] = 1;

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-summary-representative allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    rollback_retry_recovery_value(&result, 0);
    result.next_spill_recovery_order = 0;
    result.next_spill_recovery_phase_id = 0;

    if (!value_ssa_compute_allocator_retry_family_agenda(
            &program.functions[0], &prep, &interference, NULL, &plan, &result, 2, &agenda, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-split-summary-representative agenda failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_retry_family_agenda_root(
        "VALUE-SSA-ALLOC-RETRY-SPLIT-SUMMARY-REPRESENTATIVE", &agenda, 0, 0, 1, 1);
    ok &= expect_retry_family_agenda_split_family(
        "VALUE-SSA-ALLOC-RETRY-SPLIT-SUMMARY-REPRESENTATIVE", &agenda, 0, 0, 1, 1);

cleanup:
    value_ssa_allocator_retry_family_agenda_free(&agenda);
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_select_recolorable_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_main_recolor_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_recolor_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 3, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-recolor stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR", &result, 0, 1, 0);
    ok &= expect_query_generic_eviction_outcome("VALUE-SSA-ALLOC-MAIN-RECOLOR", &result, 0, 1, 1, 1, 2);
    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR", &result, 1, 1, 2);
    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR", &result, 2, 1, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR", &result, 3, 1, 2);
    ok &= expect_spill_count_on_values("VALUE-SSA-ALLOC-MAIN-RECOLOR", &result, (size_t[]){1, 2, 3, 4, 5}, 5, 0);
    if (stats.colored_count != 6 || stats.spilled_count != 0 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 0 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-RECOLOR",
        &plan,
        &result,
        "0: ssa.4 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=0",
        "5: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=0 main-evict=ssa.1->color2");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_preferred_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocatorRetryFamilyAgenda agenda;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocator_retry_family_agenda_init(&agenda);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_preferred_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-preferred setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_preferred_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-preferred manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-preferred allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-preferred stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-PREFERRED", &result, 1, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-PREFERRED", &result, 1, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-PREFERRED", &result, 0, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-PREFERRED", &result, 0, 1);
    if (stats.colored_count != 4 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-preferred stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-PREFERRED",
        &plan,
        &result,
        "4: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered preferred=coalesce-direct(ssa.4) color=0",
        "5: ssa.5 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=0");

    rollback_retry_recovery_value(&result, 1);
    result.next_spill_recovery_order = 0;
    result.next_spill_recovery_phase_id = 0;
    if (!value_ssa_compute_allocator_retry_family_agenda(&program.functions[0],
            &prep,
            &interference,
            &coalesce,
            &plan,
            &result,
            2,
            &agenda,
            &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-preferred agenda failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    ok &= expect_retry_family_agenda_root("VALUE-SSA-ALLOC-RETRY-PREFERRED", &agenda, 1, 1, 1, 1);
    ok &= expect_retry_family_agenda_split_child("VALUE-SSA-ALLOC-RETRY-PREFERRED", &agenda, 1, 0);
    ok &= expect_retry_family_agenda_entry_shape("VALUE-SSA-ALLOC-RETRY-PREFERRED",
        &agenda,
        1,
        VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE,
        VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT,
        4,
        0,
        (size_t)-1,
        0);

cleanup:
    value_ssa_allocator_retry_family_agenda_free(&agenda);
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_larger_coalesce_class(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_class_size_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-class-size setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_class_size_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-class-size manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-class-size allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-class-size stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-CLASS-SIZE", &result, 1, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-CLASS-SIZE", &result, 1, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-CLASS-SIZE", &result, 0, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-CLASS-SIZE", &result, 0, 1);
    if (stats.colored_count != 6 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-class-size stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-CLASS-SIZE",
        &plan,
        &result,
        "6: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered preferred=coalesce-class(ssa.4) color=0",
        "7: ssa.7 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=0");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_smaller_family_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_main_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_family_artifacts(&program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-family stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-FAMILY", &result, 0, 1, 0);
    ok &= expect_query_generic_eviction_outcome("VALUE-SSA-ALLOC-MAIN-FAMILY", &result, 0, 1, 2, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY", &result, 2, 1);
    if (stats.colored_count != 3 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-FAMILY",
        &plan,
        &result,
        "1: ssa.2 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed",
        "3: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=0");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_lighter_family_pressure_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_family_pressure_artifacts(&program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-family-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-FAMILY-PRESSURE", &result, 0, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY-PRESSURE", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY-PRESSURE", &result, 2, 1);
    if (stats.colored_count != 4 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-FAMILY-PRESSURE",
        &plan,
        &result,
        "2: ssa.2 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed",
        "4: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=0");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_fewer_family_neighbors_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-neighbor-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_family_neighbor_pressure_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-neighbor-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-neighbor-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-family-neighbor-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-FAMILY-NEIGHBORS", &result, 0, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY-NEIGHBORS", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY-NEIGHBORS", &result, 2, 1);
    if (stats.colored_count != 4 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-neighbor-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-FAMILY-NEIGHBORS",
        &plan,
        &result,
        "2: ssa.2 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed",
        "4: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=0");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_lighter_family_members_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-member-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_family_member_pressure_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-member-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-member-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-family-member-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-FAMILY-MEMBERS", &result, 0, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY-MEMBERS", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-FAMILY-MEMBERS", &result, 2, 1);
    if (stats.colored_count != 4 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-family-member-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-FAMILY-MEMBERS",
        &plan,
        &result,
        "2: ssa.2 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed",
        "4: ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=0");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_lighter_runtime_family_activity_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-runtime-family-activity setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_runtime_family_activity_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-runtime-family-activity manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-runtime-family-activity allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-runtime-family-activity stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RUNTIME-FAMILY-ACTIVITY", &result, 0, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-RUNTIME-FAMILY-ACTIVITY", &result, 1, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-MAIN-RUNTIME-FAMILY-ACTIVITY", &result, 2, 1);
    if (stats.colored_count != 4 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-runtime-family-activity stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-RUNTIME-FAMILY-ACTIVITY",
        &plan,
        &result,
        "2: ssa.2 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed",
        "1: ssa.1 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored preferred=coalesce-class(ssa.3) color=0");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_main_recolor_target_supported_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_blocker_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor-target-support setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_main_recolor_target_support_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor-target-support manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 3, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor-target-support allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: main-recolor-target-support stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT", &result, 0, 1, 1);
    ok &= expect_query_generic_eviction_outcome(
        "VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT", &result, 0, 1, 1, 1, 2);
    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT", &result, 1, 1, 2);
    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT", &result, 2, 1, 0);
    ok &= expect_query_color("VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT", &result, 3, 1, 2);
    ok &= expect_spill_count_on_values(
        "VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT", &result, (size_t[]){1, 2, 3, 4, 5, 6}, 6, 0);
    if (stats.colored_count != 7 || stats.spilled_count != 0 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 0 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: main-recolor-target-support stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-MAIN-RECOLOR-TARGET-SUPPORT",
        &plan,
        &result,
        "ssa.1 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=2",
        "ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=1 main-evict=ssa.1->color2");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_weighted_main_recolor_target_supported_blocker(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_blocker_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-main-recolor-target-support setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_weighted_main_recolor_target_support_artifacts(
            &program.functions[0], &prep, &interference, &coalesce, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-main-recolor-target-support manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, &coalesce, &plan, 3, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-main-recolor-target-support allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: weighted-main-recolor-target-support stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, 0, 1, 0);
    ok &= expect_query_generic_eviction_outcome(
        "VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, 0, 1, 2, 1, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, 1, 1, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, 2, 1, 1);
    ok &= expect_query_color("VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, 3, 1, 2);
    ok &= expect_query_color("VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, 5, 1, 1);
    ok &= expect_spill_count_on_values(
        "VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT", &result, (size_t[]){1, 2, 3, 4, 5, 6}, 6, 0);
    if (stats.colored_count != 7 || stats.spilled_count != 0 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 0 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 || stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: weighted-main-recolor-target-support stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-WEIGHTED-MAIN-RECOLOR-TARGET-SUPPORT",
        &plan,
        &result,
        "ssa.2 remove=simplify-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=colored color=1",
        "ssa.0 remove=spill-remove spill_cost=3(uses=2,blocks=1,affinity=0) outcome=optimistic-colored color=0 main-evict=ssa.2->color1");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_family_pressure_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_family_pressure_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 1, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 1, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 1, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE", &result, 2, 1);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-PRESSURE",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_family_members_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-member-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_member_pressure_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-member-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-member-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-member-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-MEMBERS", &result, 1, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-MEMBERS", &result, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-MEMBERS", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-MEMBERS", &result, 0, 1);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-member-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-MEMBERS",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_runtime_family_activity_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-runtime-family-activity setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_runtime_family_activity_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-runtime-family-activity manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-runtime-family-activity allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-runtime-family-activity stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-RUNTIME-FAMILY-ACTIVITY", &result, 1, 1, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-RUNTIME-FAMILY-ACTIVITY", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-RUNTIME-FAMILY-ACTIVITY", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-RUNTIME-FAMILY-ACTIVITY", &result, 0, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-runtime-family-activity stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-RUNTIME-FAMILY-ACTIVITY",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered preferred=coalesce-class(ssa.3) color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_picks_retry_family_representative(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-representative setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_family_representative_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-representative manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-representative allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-representative stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-REPRESENTATIVE", &result, 1, 1, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-REPRESENTATIVE", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-REPRESENTATIVE", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-REPRESENTATIVE", &result, 0, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-representative stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-REPRESENTATIVE",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_picks_retry_family_representative_by_blocker_aftermath(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_manual_retry_blocker_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-blocker-representative setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_family_blocker_representative_artifacts(
            &program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-blocker-representative manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-blocker-representative allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 1, 1, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 1, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 1, 1, 3);
    ok &= expect_query_retry_eviction_outcome(
        "VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 1, 1, 3, 0, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 0, 1);
    ok &= expect_query_spill_recovery_order(
        "VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 1, 1, 0);
    ok &= expect_query_spill_recovery_order(
        "VALUE-SSA-ALLOC-RETRY-FAMILY-BLOCKER-REPRESENTATIVE", &result, 0, 1, 1);

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_more_ready_family_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_ready_pressure_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-ready-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-READY", &result, 1, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-READY", &result, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-READY", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-READY", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-READY", &result, 0, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-READY",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_ready_affinity_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-affinity setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_ready_affinity_pressure_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-affinity manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-affinity allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-ready-affinity stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-AFFINITY", &result, 1, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-AFFINITY", &result, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-AFFINITY", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-AFFINITY", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-AFFINITY", &result, 0, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-affinity stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-AFFINITY",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_mutual_best_ready_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_family_pressure_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-mutual setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_mutual_ready_pressure_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-mutual manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-mutual allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-ready-mutual stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-MUTUAL", &result, 1, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-MUTUAL", &result, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-MUTUAL", &result, 1, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-MUTUAL", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-MUTUAL", &result, 0, 0);
    if (stats.colored_count != 3 || stats.spilled_count != 2 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 2 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-ready-mutual stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-READY-MUTUAL",
        &plan,
        &result,
        "2: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1",
        "3: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_batches_retry_provisional_family_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocatorRetryFamilyAgenda agenda;
    ValueSsaAllocatorRetryPhaseTrace trace;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocator_retry_family_agenda_init(&agenda);
    value_ssa_allocator_retry_phase_trace_init(&trace);

    if (!build_alloc_manual_retry_family_batch_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-batch setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_family_batch_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-batch manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-batch allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1, 0);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1, 1);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1, 2);
    ok &= expect_query_spill_recovery_phase("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1, 0, 0);
    ok &= expect_query_spill_recovery_phase("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1, 0, 0);
    ok &= expect_query_spill_recovery_phase("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1, 1, 1);
    ok &= expect_query_spill_recovery_phase_member_order("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1, 0);
    ok &= expect_query_spill_recovery_phase_member_order("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1, 1);
    ok &= expect_query_spill_recovery_phase_member_order("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1, 0);
    ok &= expect_query_spill_recovery_phase_member_count("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1, 2);
    ok &= expect_query_spill_recovery_phase_member_count("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1, 2);
    ok &= expect_query_spill_recovery_phase_member_count("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1, 1);
    ok &= expect_query_spill_recovery_phase_entry("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1, 0, 2, 2);
    ok &= expect_query_spill_recovery_phase_entry("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1, 0, 2, 2);
    ok &= expect_query_spill_recovery_phase_entry("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1, 1, 1, 1);
    ok &= expect_query_spill_recovery_phase_entry_split_family(
        "VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 0, 1, 0, 1, 1);
    ok &= expect_query_spill_recovery_phase_entry_split_family(
        "VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 7, 1, 0, 1, 1);
    ok &= expect_query_spill_recovery_phase_entry_split_family(
        "VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &result, 1, 1, 1, 1, 1);
    if (!value_ssa_compute_allocator_retry_phase_trace(&result, &trace, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-batch phase-trace failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (trace.phase_count != 2) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-batch expected phase_count=2, got %zu\n", trace.phase_count);
        ok = 0;
    }
    ok &= expect_retry_phase_trace_item("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &trace, 0, 0, 0, 2, 2, 2, 0, 1);
    ok &= expect_retry_phase_trace_item("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &trace, 1, 1, 1, 1, 1, 1, 2, 2);
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH",
        &plan,
        &result,
        "retry-phase-step=1 retry-phase-size=2 retry-entry=ssa.0[2/2]",
        "retry-entry=ssa.1[1/1]");

    rollback_retry_family_batch_recoveries(&result);
    if (!value_ssa_compute_allocator_retry_family_agenda(&program.functions[0],
            &prep,
            &interference,
            NULL,
            &plan,
            &result,
            2,
            &agenda,
            &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-family-batch agenda failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (agenda.item_count < 2 || agenda.items[0].root_value_id != 0 || agenda.items[1].root_value_id != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-family-batch agenda order mismatch (count=%zu, first=%zu, second=%zu)\n",
            agenda.item_count,
            agenda.item_count > 0 ? agenda.items[0].root_value_id : (size_t)-1,
            agenda.item_count > 1 ? agenda.items[1].root_value_id : (size_t)-1);
        ok = 0;
    }
    ok &= expect_retry_family_agenda_root("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &agenda, 0, 0, 2, 2);
    ok &= expect_retry_family_agenda_root("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &agenda, 1, 1, 1, 1);
    ok &= expect_retry_family_agenda_split_family("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &agenda, 0, 0, 1, 1);
    ok &= expect_retry_family_agenda_split_family("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH", &agenda, 1, 1, 1, 1);
    ok &= expect_retry_family_agenda_entry_shape("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH",
        &agenda,
        0,
        VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE,
        VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE,
        (size_t)-1,
        0,
        (size_t)-1,
        0);
    ok &= expect_retry_family_agenda_entry_shape("VALUE-SSA-ALLOC-RETRY-FAMILY-BATCH",
        &agenda,
        1,
        VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE,
        VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE,
        (size_t)-1,
        0,
        (size_t)-1,
        0);

cleanup:
    value_ssa_allocator_retry_phase_trace_free(&trace);
    value_ssa_allocator_retry_family_agenda_free(&agenda);
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_batches_retry_confirmed_family_recovery(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocatorRetryFamilyAgenda agenda;
    ValueSsaAllocatorRetryPhaseTrace trace;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocator_retry_family_agenda_init(&agenda);
    value_ssa_allocator_retry_phase_trace_init(&trace);

    if (!build_alloc_manual_retry_family_batch_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-confirmed-family-batch setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_confirmed_family_batch_artifacts(&program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-confirmed-family-batch manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-confirmed-family-batch allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 0, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 0, 0);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 0, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1, 0);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1, 1);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1, 2);
    ok &= expect_query_spill_recovery_phase(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1, 0, 0);
    ok &= expect_query_spill_recovery_phase(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1, 0, 0);
    ok &= expect_query_spill_recovery_phase(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1, 1, 1);
    ok &= expect_query_spill_recovery_phase_member_order(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1, 0);
    ok &= expect_query_spill_recovery_phase_member_order(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1, 1);
    ok &= expect_query_spill_recovery_phase_member_order(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1, 0);
    ok &= expect_query_spill_recovery_phase_member_count(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1, 2);
    ok &= expect_query_spill_recovery_phase_member_count(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1, 2);
    ok &= expect_query_spill_recovery_phase_member_count(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1, 1);
    ok &= expect_query_spill_recovery_phase_entry(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1, 0, 2, 0);
    ok &= expect_query_spill_recovery_phase_entry(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1, 0, 2, 0);
    ok &= expect_query_spill_recovery_phase_entry(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1, 1, 1, 0);
    ok &= expect_query_spill_recovery_phase_entry_split_family(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 0, 1, 0, 1, 0);
    ok &= expect_query_spill_recovery_phase_entry_split_family(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 7, 1, 0, 1, 0);
    ok &= expect_query_spill_recovery_phase_entry_split_family(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &result, 1, 1, 1, 1, 0);
    if (!value_ssa_compute_allocator_retry_phase_trace(&result, &trace, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-confirmed-family-batch phase-trace failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (trace.phase_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-confirmed-family-batch expected phase_count=2, got %zu\n",
            trace.phase_count);
        ok = 0;
    }
    ok &= expect_retry_phase_trace_item(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &trace, 0, 0, 0, 2, 0, 2, 0, 1);
    ok &= expect_retry_phase_trace_item(
        "VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &trace, 1, 1, 1, 1, 0, 1, 2, 2);
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH",
        &plan,
        &result,
        "retry-phase-step=1 retry-phase-size=2 retry-entry=ssa.0[2/0]",
        "retry-entry=ssa.1[1/0]");

    rollback_retry_family_batch_recoveries(&result);
    if (!value_ssa_compute_allocator_retry_family_agenda(&program.functions[0],
            &prep,
            &interference,
            NULL,
            &plan,
            &result,
            2,
            &agenda,
            &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-confirmed-family-batch agenda failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (agenda.item_count < 2 || agenda.items[0].root_value_id != 0 || agenda.items[1].root_value_id != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-confirmed-family-batch agenda order mismatch (count=%zu, first=%zu, second=%zu)\n",
            agenda.item_count,
            agenda.item_count > 0 ? agenda.items[0].root_value_id : (size_t)-1,
            agenda.item_count > 1 ? agenda.items[1].root_value_id : (size_t)-1);
        ok = 0;
    }
    ok &= expect_retry_family_agenda_root("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &agenda, 0, 0, 2, 0);
    ok &= expect_retry_family_agenda_root("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &agenda, 1, 1, 1, 0);
    ok &= expect_retry_family_agenda_split_family("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &agenda, 0, 0, 1, 0);
    ok &= expect_retry_family_agenda_split_family("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH", &agenda, 1, 1, 1, 0);
    ok &= expect_retry_family_agenda_entry_shape("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH",
        &agenda,
        0,
        VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE,
        VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE,
        (size_t)-1,
        0,
        (size_t)-1,
        0);
    ok &= expect_retry_family_agenda_entry_shape("VALUE-SSA-ALLOC-RETRY-CONFIRMED-FAMILY-BATCH",
        &agenda,
        1,
        VALUE_SSA_ALLOCATOR_RETRY_ENTRY_FREE,
        VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE,
        (size_t)-1,
        0,
        (size_t)-1,
        0);

cleanup:
    value_ssa_allocator_retry_phase_trace_free(&trace);
    value_ssa_allocator_retry_family_agenda_free(&agenda);
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_family_pressure_with_eviction(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_dual_evict_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-dual-evict-family-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_dual_evict_family_pressure_artifacts(
            &program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-dual-evict-family-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-dual-evict-family-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-dual-evict-family-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 1, 1, 0);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 1, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 1, 1, 2);
    ok &= expect_query_retry_eviction_outcome(
        "VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 1, 1, 2, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 0, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 2, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE", &result, 4, 1);
    if (stats.colored_count != 3 || stats.spilled_count != 3 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 3 ||
        stats.spill_recovered_count != 1 || stats.retry_eviction_recovered_count != 1 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 1 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-dual-evict-family-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-DUAL-EVICT-FAMILY-PRESSURE",
        &plan,
        &result,
        "3: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=0 retry-step=0 retry-phase=0@ssa.1 retry-phase-step=0 retry-phase-size=1 retry-entry=ssa.1[1/1] retry-evict=ssa.2",
        "4: ssa.0 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=spill-confirmed");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_from_plan_prefers_retry_lighter_blocker_family_with_eviction(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);

    if (!build_alloc_manual_retry_blocker_family_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-blocker-family-pressure setup failed: %s\n",
            error.message);
        return 0;
    }
    if (!setup_manual_retry_blocker_family_pressure_artifacts(
            &program.functions[0], &prep, &interference, &plan)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-blocker-family-pressure manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocate_function_from_plan(
            &program.functions[0], &prep, &interference, NULL, &plan, 2, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-blocker-family-pressure allocate failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: retry-blocker-family-pressure stats failed\n");
        ok = 0;
        goto cleanup;
    }

    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 1, 1, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 1, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 1, 1, 3);
    ok &= expect_query_retry_eviction_outcome(
        "VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 1, 1, 3, 0, 0);
    ok &= expect_query_color("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 0, 1, 1);
    ok &= expect_query_spill_recovered("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 0, 1);
    ok &= expect_query_retry_eviction("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 0, 1, 2);
    ok &= expect_query_retry_eviction_outcome(
        "VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 0, 1, 2, 0, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 3, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 4, 1);
    if (stats.colored_count != 4 || stats.spilled_count != 3 ||
        stats.spill_intended_count != 2 || stats.spill_confirmed_count != 3 ||
        stats.spill_recovered_count != 2 || stats.retry_eviction_recovered_count != 2 ||
        stats.optimistic_direct_count != 0 || stats.optimistic_recovered_count != 2 ||
        stats.optimistic_colored_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: retry-blocker-family-pressure stats mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE",
        &plan,
        &result,
        "4: ssa.1 remove=spill-remove spill_cost=2(uses=1,blocks=1,affinity=0) outcome=optimistic-recovered color=1 retry-step=0 retry-phase=0@ssa.1 retry-phase-step=0 retry-phase-size=1 retry-entry=ssa.1[1/1] retry-evict=ssa.3",
        "5: ssa.0 remove=spill-remove spill_cost=4(uses=3,blocks=1,affinity=0) outcome=optimistic-recovered color=1 retry-step=1 retry-phase=1@ssa.0 retry-phase-step=0 retry-phase-size=1 retry-entry=ssa.0[1/1] retry-evict=ssa.2");
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 1, 1, 0);
    ok &= expect_query_spill_recovery_order("VALUE-SSA-ALLOC-RETRY-BLOCKER-FAMILY-PRESSURE", &result, 0, 1, 1);

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_select_stats(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    ValueSsaAllocationSelectStats stats;
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: select-stats setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    value_ssa_allocation_select_stats_init(&stats);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: select-stats allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_compute_allocation_select_stats(&result, &stats)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: select-stats compute failed\n");
        value_ssa_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (stats.colored_count != 4 || stats.spilled_count != 1 ||
        stats.spill_intended_count != 1 || stats.spill_confirmed_count != 1 ||
        stats.spill_recovered_count != 0 || stats.retry_eviction_recovered_count != 0 ||
        stats.optimistic_direct_count != 1 ||
        stats.optimistic_recovered_count != 0 ||
        stats.optimistic_colored_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: select-stats values mismatch "
            "(colored=%zu spilled=%zu intended=%zu confirmed=%zu recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic=%zu)\n",
            stats.colored_count,
            stats.spilled_count,
            stats.spill_intended_count,
            stats.spill_confirmed_count,
            stats.spill_recovered_count,
            stats.retry_eviction_recovered_count,
            stats.optimistic_direct_count,
            stats.optimistic_recovered_count,
            stats.optimistic_colored_count);
        ok = 0;
    }
    ok &= expect_select_stats_dump("VALUE-SSA-ALLOC-SELECT-STATS",
        &stats,
        "select-stats colored=4 spilled=1 spill_intended=1 spill_confirmed=1 spill_recovered=0 retry_eviction_recovered=0 optimistic_direct=1 optimistic_recovered=0 optimistic_colored=1\n");

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_select_trace(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: select-trace setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_interference_graph(&program.functions[0], NULL, NULL, &interference, &error) ||
        !value_ssa_compute_copy_affinity_graph(&program.functions[0], &interference, &affinity, &error) ||
        !value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, &interference, &affinity, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, &affinity, NULL, 2, &plan, &error) ||
        !value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: select-trace build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_select_trace_contains("VALUE-SSA-ALLOC-SELECT-TRACE",
        &plan,
        &result,
        "outcome=optimistic-colored",
        "outcome=spill");

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_spill_dump_has_slot(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    int ok;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill dump setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill dump allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    {
        char *actual_text = NULL;
        if (!value_ssa_dump_allocation_result(&program.functions[0], &result, &actual_text, &error)) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: spill dump failed: %s\n", error.message);
            value_ssa_allocation_result_free(&result);
            value_ssa_program_free(&program);
            return 0;
        }
        ok = strstr(actual_text, "spill slot=0") != NULL &&
             strstr(actual_text, "spill-intended") != NULL;
        if (!ok) {
            fprintf(stderr,
                "[value-ssa-alloc] FAIL: spill dump expected explicit spill slot and optimistic spill-intended color, got:\n%s\n",
                actual_text);
        }
        free(actual_text);
    }

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_function_allocation_layout_spill_mix(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    ValueSsaFunctionAllocationLayout layout;
    const ValueSsaAllocatedValuePlacement *spill_placement = NULL;
    const ValueSsaAllocatedValuePlacement *color_placement = NULL;
    const size_t *colored_values = NULL;
    const size_t *spilled_values = NULL;
    const size_t *used_colors = NULL;
    size_t colored_count = 0;
    size_t spilled_count = 0;
    size_t used_color_count = 0;
    const size_t expected_colored_values[] = {1, 2, 3, 4};
    const size_t expected_spilled_values[] = {0};
    const size_t expected_used_colors[] = {0, 1};
    const size_t expected_color0_group[] = {1, 3, 4};
    const size_t expected_color1_group[] = {2};
    const size_t expected_spill_slot0_group[] = {0};
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    value_ssa_function_allocation_layout_init(&layout);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error) ||
        !value_ssa_build_function_allocation_layout(&result, &layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-spill build failed: %s\n", error.message);
        value_ssa_function_allocation_layout_free(&layout);
        value_ssa_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_function_layout_summary("VALUE-SSA-ALLOC-LAYOUT-SPILL-SUMMARY",
        &layout,
        NULL,
        5,
        2,
        1,
        4,
        1,
        1,
        1,
        0,
        2,
        2,
        1);
    if (!value_ssa_function_allocation_layout_get_placement(&layout, 0, &spill_placement) || !spill_placement ||
        spill_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL || spill_placement->spill_slot != 0 ||
        spill_placement->spill_intended || !spill_placement->spill_confirmed || spill_placement->spill_recovered) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-spill expected ssa.0 to be confirmed spill in slot 0\n");
        ok = 0;
    }
    if (!value_ssa_function_allocation_layout_get_placement(&layout, 2, &color_placement) || !color_placement ||
        color_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR || color_placement->color != 1 ||
        !color_placement->spill_intended || color_placement->spill_confirmed || color_placement->spill_recovered) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-spill expected ssa.2 to stay optimistic-colored\n");
        ok = 0;
    }
    if (!value_ssa_function_allocation_layout_get_colored_values(&layout, &colored_count, &colored_values) ||
        !value_ssa_function_allocation_layout_get_spilled_values(&layout, &spilled_count, &spilled_values) ||
        !value_ssa_function_allocation_layout_get_used_colors(&layout, &used_color_count, &used_colors)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-spill grouped queries failed\n");
        ok = 0;
    } else {
        ok &= expect_layout_value_list("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            colored_values,
            colored_count,
            expected_colored_values,
            sizeof(expected_colored_values) / sizeof(expected_colored_values[0]),
            "colored-values");
        ok &= expect_layout_value_list("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            spilled_values,
            spilled_count,
            expected_spilled_values,
            sizeof(expected_spilled_values) / sizeof(expected_spilled_values[0]),
            "spilled-values");
        ok &= expect_layout_value_list("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            used_colors,
            used_color_count,
            expected_used_colors,
            sizeof(expected_used_colors) / sizeof(expected_used_colors[0]),
            "used-colors");
        ok &= expect_layout_color_group("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            &layout,
            0,
            expected_color0_group,
            sizeof(expected_color0_group) / sizeof(expected_color0_group[0]));
        ok &= expect_layout_color_group("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            &layout,
            1,
            expected_color1_group,
            sizeof(expected_color1_group) / sizeof(expected_color1_group[0]));
        ok &= expect_layout_spill_slot_group("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            &layout,
            0,
            expected_spill_slot0_group,
            sizeof(expected_spill_slot0_group) / sizeof(expected_spill_slot0_group[0]));
        ok &= expect_layout_color_group_at("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            &layout,
            0,
            0,
            expected_color0_group,
            sizeof(expected_color0_group) / sizeof(expected_color0_group[0]));
        ok &= expect_layout_color_group_at("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            &layout,
            1,
            1,
            expected_color1_group,
            sizeof(expected_color1_group) / sizeof(expected_color1_group[0]));
        ok &= expect_layout_spill_slot_group_at("VALUE-SSA-ALLOC-LAYOUT-SPILL",
            &layout,
            0,
            0,
            expected_spill_slot0_group,
            sizeof(expected_spill_slot0_group) / sizeof(expected_spill_slot0_group[0]));
    }

    ok &= expect_function_layout_dump("VALUE-SSA-ALLOC-LAYOUT-SPILL",
        &program.functions[0],
        &layout,
        "alloc-layout func spill colors=2 values=5 colored=4 spilled=1 optimistic_colored=1 confirmed_spilled=1 recovered_colored=0 spill_slots=1\n"
        "used-colors: 0 1\n"
        "colored-values: ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values: ssa.0\n"
        "optimistic-colored-values: ssa.2\n"
        "confirmed-spilled-values: ssa.0\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.1 ssa.3 ssa.4\n"
        "color-group 1: ssa.2\n"
        "spill-slot-group 0: ssa.0\n"
        "ssa.0 spill=0 spill-confirmed\n"
        "ssa.1 color=0\n"
        "ssa.2 color=1 spill-intended\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    value_ssa_function_allocation_layout_free(&layout);
    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_reuses_spill_slot_for_noninterfering_values(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    int has_slot0 = 0;
    int has_slot2 = 0;
    size_t slot0 = 0;
    size_t slot2 = 0;
    int ok = 1;

    if (!build_alloc_noninterfering_spill_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-slot-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 0, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-slot-reuse allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_allocation_result_get_spill_slot(&result, 0, &has_slot0, &slot0) ||
        !value_ssa_allocation_result_get_spill_slot(&result, 2, &has_slot2, &slot2)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-slot-reuse query failed\n");
        ok = 0;
    } else if (!has_slot0 || !has_slot2 || slot0 != slot2 || result.spill_slot_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: spill-slot-reuse expected shared slot, got has0=%d slot0=%zu has2=%d slot2=%zu count=%zu\n",
            has_slot0,
            slot0,
            has_slot2,
            slot2,
            result.spill_slot_count);
        ok = 0;
    }

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_reuses_parent_spill_slot_for_split_child(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult split_result;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationResult result;
    ValueSsaError error;
    int has_slot0 = 0;
    int has_slot4 = 0;
    size_t slot0 = 0;
    size_t slot4 = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_program_allocation_result_init(&split_result);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_result_init(&result);

    if (!build_alloc_def_block_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-slot setup failed: %s\n", error.message);
        return 0;
    }
    if (!prepare_manual_spill_result_for_value(&split_result, &program, 0, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-slot split-result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &split_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-slot split rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&split_result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-slot prep failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (!prep.split_child_values || !prep.split_family_root_value_ids ||
        !prep.split_child_values[4] || prep.split_family_root_value_ids[4] != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-child-slot expected ssa.4 to have split root ssa.0, got split=%d root=%zu\n",
            prep.split_child_values ? (int)prep.split_child_values[4] : -1,
            prep.split_family_root_value_ids ? prep.split_family_root_value_ids[4] : (size_t)-1);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_allocate_function(&program.functions[0], 0, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-slot allocate failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocation_result_get_spill_slot(&result, 0, &has_slot0, &slot0) ||
        !value_ssa_allocation_result_get_spill_slot(&result, 4, &has_slot4, &slot4)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-slot query failed\n");
        ok = 0;
        goto cleanup;
    }
    if (!has_slot0 || !has_slot4 || slot0 != slot4) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-child-slot expected parent/child shared slot, got has0=%d slot0=%zu has4=%d slot4=%zu\n",
            has_slot0,
            slot0,
            has_slot4,
            slot4);
        ok = 0;
    }

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_allocation_result_free(&split_result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_single_block_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_program(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite run failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "store_local spill.") == NULL || strstr(actual_text, "load_local spill.") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: rewrite expected spill load/store pattern, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_spill_local_for_shared_slot(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    static const size_t spilled_values[] = {0, 2};
    int ok = 1;

    if (!build_alloc_noninterfering_spill_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-local-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 2, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }
    result.function_results[0].spill_slots[0] = 0;
    result.function_results[0].spill_slots[2] = 0;
    result.function_results[0].spill_slot_count = 1;

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-local-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-local-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "spill.0.0") == NULL || strstr(actual_text, "spill.1.1") != NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: spill-local-reuse expected one shared spill local, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_single_block_spill_return(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_spill_return_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-return setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_program(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-return allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-return rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: spill-return dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "ret ssa.") == NULL || strstr(actual_text, "load_local spill.") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: spill-return expected reload before return, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_same_block_reload(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    size_t load_count = 0;
    const char *cursor;
    int ok = 1;

    if (!build_alloc_same_block_multi_use_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: same-block-reload-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 3, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: same-block-reload-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: same-block-reload-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++load_count;
    }

    if (strstr(actual_text, "store_local spill.0.0, ssa.3") == NULL ||
        strstr(actual_text, "ssa.4 = add ssa.6, 1") == NULL ||
        strstr(actual_text, "ssa.5 = add ssa.6, ssa.2") == NULL ||
        load_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: same-block-reload-reuse expected one shared reload for later same-block uses, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_cross_block_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: cross-block setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_program(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: cross-block allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: cross-block rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: cross-block dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.1:\n    ssa.") == NULL || strstr(actual_text, "load_local spill.") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: cross-block rewrite expected reload in successor block, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {0};

    if (!build_alloc_block_local_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.1:\n    ssa.4 = mov ssa.0\n    ssa.1 = add ssa.4, 1\n    ssa.2 = add ssa.4, 2\n") ==
        NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: block-local-split expected block-local split mov and rewritten uses, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits_term_use(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {0};

    if (!build_alloc_block_local_split_term_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-term setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-term manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-term rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-term dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.1:\n    ssa.3 = mov ssa.0\n    ssa.2 = add ssa.3, 1\n    ssa.1 = ne ssa.3, 0\n    ret ssa.1\n") ==
        NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: block-local-split-term expected shared split across instr and terminator uses, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits_multiple_blocks(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {0};

    if (!build_alloc_multi_block_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-multi-block setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-multi-block manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-multi-block rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-multi-block dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.1:\n    ssa.5 = mov ssa.0\n    ssa.1 = add ssa.5, 1\n    ssa.2 = add ssa.5, 2\n    jmp bb.2\n") ==
            NULL ||
        strstr(actual_text, "bb.2:\n    ssa.6 = mov ssa.0\n    ssa.3 = add ssa.6, 3\n    ssa.4 = add ssa.6, ssa.3\n    ret ssa.4\n") ==
            NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: block-local-split-multi-block expected independent local splits in both use blocks, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits_def_block(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {0};

    if (!build_alloc_def_block_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-def-block setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-def-block manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-def-block rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-def-block dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.0:\n    ssa.0 = load_local input.0\n    ssa.4 = mov ssa.0\n    ssa.1 = add ssa.4, 1\n    ssa.2 = add ssa.4, 2\n    ssa.3 = add ssa.1, ssa.2\n    ret ssa.3\n") ==
        NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: block-local-split-def-block expected defining-block local split and rewritten uses, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits_reports_change_flag(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    int rewrote_any = 0;
    static const size_t spilled_values[] = {0};

    if (!build_alloc_block_local_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-change-flag setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-change-flag manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits_with_change_flag(
            &program, &result, &rewrote_any, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-change-flag rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!rewrote_any) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-change-flag expected rewrote_any=1\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits_reports_no_change_flag(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    int rewrote_any = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-no-change-flag setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 0, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-no-change-flag manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits_with_change_flag(
            &program, &result, &rewrote_any, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-no-change-flag rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (rewrote_any) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-no-change-flag expected rewrote_any=0\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return 1;
}

static int test_value_ssa_rewrite_program_edge_phi_spill_splits_branch_edge(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {1};

    if (!build_alloc_branch_predecessor_two_phi_inputs_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.4 = phi [bb.4: ssa.7], [bb.3: ssa.2]\n"
            "    ssa.5 = phi [bb.4: ssa.7], [bb.3: ssa.3]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.7 = mov ssa.1\n"
            "    jmp bb.2\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-branch expected one edge-shaped split child on the branch edge, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_edge_phi_spill_splits_loop_backedge(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {7};

    if (!build_alloc_loop_backedge_shared_phi_input_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.7 = mov 42\n"
            "    ssa.2 = mov 1\n"
            "    ssa.4 = lt ssa.1, 1\n"
            "    br ssa.4, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.4: ssa.2]\n"
            "    ssa.8 = phi [bb.0: ssa.5], [bb.4: ssa.11]\n"
            "    ssa.9 = phi [bb.0: ssa.6], [bb.4: ssa.11]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.11 = mov ssa.7\n"
            "    jmp bb.1\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-loop expected one edge-shaped split child on the loop backedge, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_edge_phi_spill_splits_branch_tail_and_phi_together(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {1};

    if (!build_alloc_branch_predecessor_phi_and_tail_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-tail-split-branch setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-tail-split-branch manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-tail-split-branch rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-tail-split-branch dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    ssa.2 = add ssa.1, 1\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.5 = phi [bb.4: ssa.8], [bb.3: ssa.3]\n"
            "    ssa.6 = phi [bb.4: ssa.8], [bb.3: ssa.4]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.8 = mov ssa.1\n"
            "    jmp bb.2\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-tail-split-branch expected predecessor tail use to stay local while phi-edge uses share one edge child, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_edge_phi_spill_splits_reuse_one_branch_split_block_across_values(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    size_t split_block_count = 0;
    const char *cursor;
    int ok = 1;
    static const size_t spilled_values[] = {1, 2};

    if (!build_alloc_branch_predecessor_two_spilled_phi_and_tail_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch-multi-value setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 2, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-branch-multi-value manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch-multi-value rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch-multi-value dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "jmp bb.2\n")) != NULL; ++cursor) {
        ++split_block_count;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    ssa.2 = mov 43\n"
            "    ssa.3 = add ssa.1, 1\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.6 = phi [bb.4: ssa.9], [bb.3: ssa.4]\n"
            "    ssa.7 = phi [bb.4: ssa.10], [bb.3: ssa.5]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.9 = mov ssa.1\n"
            "    ssa.10 = mov ssa.2\n"
            "    jmp bb.2\n") == NULL ||
        split_block_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-branch-multi-value expected one reused split block carrying two edge children, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_edge_phi_spill_splits_reuse_one_loop_backedge_block_across_values(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {7, 8};

    if (!build_alloc_loop_backedge_two_spilled_shared_phi_input_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop-multi-value setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 2, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop-multi-value manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop-multi-value rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-loop-multi-value dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.7 = mov 42\n"
            "    ssa.8 = mov 43\n"
            "    ssa.2 = mov 1\n"
            "    ssa.4 = lt ssa.1, 1\n"
            "    br ssa.4, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.4: ssa.2]\n"
            "    ssa.9 = phi [bb.0: ssa.5], [bb.4: ssa.12]\n"
            "    ssa.10 = phi [bb.0: ssa.6], [bb.4: ssa.13]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.12 = mov ssa.7\n"
            "    ssa.13 = mov ssa.8\n"
            "    jmp bb.1\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-loop-multi-value expected one reused backedge block carrying two edge children, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_edge_phi_spill_splits_reuse_one_branch_split_block_with_two_tail_values(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    size_t split_block_count = 0;
    const char *cursor;
    int ok = 1;
    static const size_t spilled_values[] = {1, 2};

    if (!build_alloc_branch_predecessor_two_spilled_phi_and_two_tails_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch-two-tail-values setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 2, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-branch-two-tail-values manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-branch-two-tail-values rewrite failed: %s\n",
            error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: edge-phi-split-branch-two-tail-values dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "jmp bb.2\n")) != NULL; ++cursor) {
        ++split_block_count;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    ssa.2 = mov 43\n"
            "    ssa.3 = add ssa.1, 1\n"
            "    ssa.4 = add ssa.2, 2\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.7 = phi [bb.4: ssa.10], [bb.3: ssa.5]\n"
            "    ssa.8 = phi [bb.4: ssa.11], [bb.3: ssa.6]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.10 = mov ssa.1\n"
            "    ssa.11 = mov ssa.2\n"
            "    jmp bb.2\n") == NULL ||
        split_block_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: edge-phi-split-branch-two-tail-values expected one reused split block while predecessor tail uses stay local, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_block_local_spill_splits_phi_defined_header_uses(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {6};

    if (!build_alloc_loop_body_local_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-loop-header setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-loop-header manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-loop-header rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: block-local-split-loop-header dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
            "    ssa.6 = phi [bb.0: ssa.5], [bb.2: ssa.6]\n"
            "    ssa.10 = mov ssa.6\n"
            "    ssa.7 = add ssa.10, 1\n"
            "    ssa.8 = add ssa.10, 2\n"
            "    ssa.3 = lt ssa.1, 1\n"
            "    br ssa.3, bb.2, bb.3\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: block-local-split-loop-header expected phi-defined carried value to get one header-local child for repeated local uses, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_composes_local_and_edge_splits_for_same_value(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {1};

    if (!build_alloc_branch_predecessor_phi_and_two_tails_same_value_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: local+edge-split-same-value setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: local+edge-split-same-value manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: local+edge-split-same-value rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: local+edge-split-same-value dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    ssa.9 = mov ssa.1\n"
            "    ssa.2 = add ssa.9, 1\n"
            "    ssa.3 = add ssa.9, 2\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.6 = phi [bb.4: ssa.10], [bb.3: ssa.4]\n"
            "    ssa.7 = phi [bb.4: ssa.10], [bb.3: ssa.5]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.10 = mov ssa.1\n"
            "    jmp bb.2\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: local+edge-split-same-value expected one local child for tail uses plus one edge child for phi uses, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_processes_two_edge_split_families_for_same_value(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {2};

    if (!build_alloc_two_branch_edges_same_spilled_phi_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: two-edge-split-same-value setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: two-edge-split-same-value manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: two-edge-split-same-value rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: two-edge-split-same-value dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.5 = phi [bb.7: ssa.12], [bb.3: ssa.3]\n"
            "    ssa.6 = phi [bb.7: ssa.12], [bb.3: ssa.4]\n"
            "    br ssa.1, bb.4, bb.6\n") == NULL ||
        strstr(actual_text,
            "bb.5:\n"
            "    ssa.9 = phi [bb.8: ssa.13], [bb.6: ssa.7]\n"
            "    ssa.10 = phi [bb.8: ssa.13], [bb.6: ssa.8]\n"
            "    ssa.11 = add ssa.5, ssa.10\n"
            "    ret ssa.11\n") == NULL ||
        strstr(actual_text,
            "bb.7:\n"
            "    ssa.12 = mov ssa.2\n"
            "    jmp bb.2\n") == NULL ||
        strstr(actual_text,
            "bb.8:\n"
            "    ssa.13 = mov ssa.2\n"
            "    jmp bb.5\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: two-edge-split-same-value expected one split child on each eligible edge for the same spilled value, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_composes_jump_edge_phi_and_local_tail_splits(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {7};

    if (!build_alloc_loop_backedge_shared_phi_and_tail_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-local+phi-split setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-local+phi-split manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-local+phi-split rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-local+phi-split dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.7 = mov 42\n"
            "    ssa.13 = mov ssa.7\n"
            "    ssa.8 = add ssa.13, 1\n"
            "    ssa.9 = add ssa.13, 2\n"
            "    ssa.2 = mov 1\n"
            "    ssa.4 = lt ssa.1, 1\n"
            "    br ssa.4, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.4: ssa.2]\n"
            "    ssa.10 = phi [bb.0: ssa.5], [bb.4: ssa.14]\n"
            "    ssa.11 = phi [bb.0: ssa.6], [bb.4: ssa.14]\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.14 = mov ssa.7\n"
            "    jmp bb.1\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: jump-edge-local+phi-split expected one local child in jump predecessor plus one edge child for backedge phis, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_jump_edge_single_phi_pair_can_follow_local_split(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaError error;
    char *actual_text = NULL;
    int ok = 1;
    static const size_t spilled_values[] = {7};

    if (!build_alloc_loop_backedge_single_phi_and_tail_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-single-phi-after-local setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-single-phi-after-local manual spill result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-single-phi-after-local rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: jump-edge-single-phi-after-local dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.6 = mov 42\n"
            "    ssa.7 = add ssa.6, 1\n"
            "    ssa.8 = add ssa.6, 2\n"
            "    ssa.2 = mov 1\n"
            "    ssa.4 = lt ssa.1, 1\n"
            "    br ssa.4, bb.1, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
            "    ssa.9 = phi [bb.0: ssa.5], [bb.2: ssa.6]\n") == NULL ||
        strstr(actual_text, "bb.4:") != NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: jump-edge-single-phi-after-local expected the predecessor local split child to satisfy the lone phi consumer without opening a separate split edge block, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_split_child_spill_reuses_parent_spill_family(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult split_result;
    ValueSsaProgramAllocationResult spill_result;
    ValueSsaError error;
    char *actual_text = NULL;
    static const size_t split_values[] = {0};
    static const size_t spilled_values[] = {0, 4};
    int ok = 1;

    if (!build_alloc_def_block_split_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-parent-family setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&split_result);
    value_ssa_program_allocation_result_init(&spill_result);
    if (!prepare_manual_spill_result_for_values(&split_result, &program, split_values, 1, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-parent-family split-result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_rewrite_program_block_local_spill_splits(&program, &split_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-parent-family split rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&split_result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!prepare_manual_spill_result_for_values(&spill_result, &program, spilled_values, 2, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-parent-family spill-result failed\n");
        value_ssa_program_allocation_result_free(&split_result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_rewrite_program_single_block_spills(&program, &spill_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-parent-family spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&spill_result);
        value_ssa_program_allocation_result_free(&split_result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-child-parent-family dump failed\n");
        value_ssa_program_allocation_result_free(&spill_result);
        value_ssa_program_allocation_result_free(&split_result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "store_local spill.1.2") != NULL ||
        strstr(actual_text, "load_local spill.1.2") != NULL ||
        strstr(actual_text, "store_local spill.0.1") == NULL ||
        strstr(actual_text, "ssa.4 = load_local spill.0.1") == NULL ||
        strstr(actual_text, "ssa.0 = load_local input.0") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-child-parent-family expected split child to reuse parent spill family, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&spill_result);
    value_ssa_program_allocation_result_free(&split_result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_split_chain_spill_reuses_root_family(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult spill_result;
    ValueSsaError error;
    char *actual_text = NULL;
    static const size_t spilled_values[] = {0, 1, 2};
    int ok = 1;

    if (!build_alloc_split_chain_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-chain-family setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&spill_result);
    if (!prepare_manual_spill_result_for_values(&spill_result, &program, spilled_values, 3, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-chain-family spill-result failed\n");
        value_ssa_program_free(&program);
        return 0;
    }
    spill_result.function_results[0].spill_slots[0] = 0;
    spill_result.function_results[0].spill_slots[1] = 1;
    spill_result.function_results[0].spill_slots[2] = 2;
    spill_result.function_results[0].spill_slot_count = 3;

    if (!value_ssa_rewrite_program_single_block_spills(&program, &spill_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-chain-family spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&spill_result);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: split-chain-family dump failed\n");
        value_ssa_program_allocation_result_free(&spill_result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "spill.1.") != NULL ||
        strstr(actual_text, "spill.2.") != NULL ||
        strstr(actual_text, "store_local spill.0.1, ssa.0") == NULL ||
        strstr(actual_text, "ssa.1 = load_local spill.0.1") == NULL ||
        strstr(actual_text, "ssa.2 = mov ssa.1") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: split-chain-family expected chained split children to reuse root spill family, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&spill_result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_unique_predecessor_reload(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    size_t load_count = 0;
    const char *cursor;
    int ok = 1;

    if (!build_alloc_unique_predecessor_reload_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-pred-reload-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 2, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-pred-reload-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-pred-reload-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++load_count;
    }

    if (strstr(actual_text, "store_local spill.0.0, ssa.2") == NULL ||
        strstr(actual_text, "bb.0:\n    ssa.0 = mov 10\n    ssa.1 = mov 20\n    ssa.2 = add ssa.0, ssa.1\n    store_local spill.0.0, ssa.2\n    ssa.5 = load_local spill.0.0\n    ssa.3 = add ssa.5, 1\n    jmp bb.1\n") == NULL ||
        strstr(actual_text, "bb.1:\n    ssa.4 = add ssa.5, 2\n    ret ssa.4\n") == NULL ||
        load_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: unique-pred-reload-reuse expected successor reuse of predecessor reload, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_unique_predecessor_chain_reload(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    size_t load_count = 0;
    const char *cursor;
    int ok = 1;

    if (!build_alloc_unique_predecessor_chain_reload_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-pred-chain-reload-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 2, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-pred-chain-reload-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: unique-pred-chain-reload-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++load_count;
    }

    if (strstr(actual_text, "store_local spill.0.0, ssa.2") == NULL ||
        strstr(actual_text, "bb.0:\n    ssa.0 = mov 10\n    ssa.1 = mov 20\n    ssa.2 = add ssa.0, ssa.1\n    store_local spill.0.0, ssa.2\n    ssa.6 = load_local spill.0.0\n    ssa.3 = add ssa.6, 1\n    jmp bb.1\n") == NULL ||
        strstr(actual_text, "bb.1:\n    ssa.4 = add ssa.6, 2\n    jmp bb.2\n") == NULL ||
        strstr(actual_text, "bb.2:\n    ssa.5 = add ssa.6, 3\n    ret ssa.5\n") == NULL ||
        load_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: unique-pred-chain-reload-reuse expected chain reuse of one predecessor reload, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_dominating_reload_after_reconvergence(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    const char *cursor;
    size_t reload_count = 0;
    int ok = 1;

    if (!build_alloc_dominating_reload_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: dominating-reload-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 1, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: dominating-reload-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: dominating-reload-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++reload_count;
    }

    if (strstr(actual_text,
            "bb.3:\n"
            "    ssa.5 = load_local spill.0.0\n"
            "    ssa.3 = add ssa.5, 1\n"
            "    br ssa.0, bb.4, bb.5\n") == NULL ||
        strstr(actual_text, "bb.6:\n    ssa.4 = add ssa.5, 2\n    ret ssa.4\n") == NULL ||
        reload_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: dominating-reload-reuse expected one join reload reused through reconvergence, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_does_not_reuse_dominating_reload_past_clobber(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    const char *cursor;
    size_t reload_count = 0;
    int ok = 1;

    if (!build_alloc_dominating_reload_clobber_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: dominating-reload-clobber setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 0, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: dominating-reload-clobber rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: dominating-reload-clobber dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++reload_count;
    }

    if (strstr(actual_text, "bb.1:\n    ssa.4 = load_local spill.0.0\n    ssa.2 = add ssa.4, 1\n    jmp bb.2\n") == NULL ||
        strstr(actual_text, "bb.2:\n    ssa.1 = mov 99\n    store_local spill.0.0, ssa.1\n    jmp bb.3\n") == NULL ||
        strstr(actual_text, "bb.3:\n    ssa.5 = load_local spill.0.0\n    ssa.3 = add ssa.5, 2\n    ret ssa.3\n") == NULL ||
        reload_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: dominating-reload-clobber expected clobber to block dominating reload reuse, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_reload_through_loop_header(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    const char *cursor;
    size_t reload_count = 0;
    int ok = 1;

    if (!build_alloc_loop_reload_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-reload-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 1, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-reload-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-reload-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++reload_count;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
            "    store_local spill.0.0, ssa.1\n"
            "    ssa.7 = load_local spill.0.0\n"
            "    ssa.4 = add ssa.7, 1\n"
            "    ssa.3 = lt ssa.7, 1\n"
            "    br ssa.3, bb.2, bb.3\n") == NULL ||
        strstr(actual_text, "bb.2:\n    ssa.2 = add ssa.7, 1\n    ssa.5 = add ssa.7, 2\n    jmp bb.1\n") == NULL ||
        strstr(actual_text, "bb.3:\n    ssa.6 = add ssa.7, 3\n    ret ssa.6\n") == NULL ||
        reload_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: loop-reload-reuse expected one header reload reused by body and exit, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_does_not_reuse_reload_past_loop_clobber(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    const char *cursor;
    size_t reload_count = 0;
    int ok = 1;

    if (!build_alloc_loop_reload_clobber_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-reload-clobber setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 1, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-reload-clobber rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-reload-clobber dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++reload_count;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.2: ssa.2]\n"
            "    store_local spill.0.0, ssa.1\n"
            "    ssa.8 = load_local spill.0.0\n"
            "    ssa.4 = add ssa.8, 1\n"
            "    ssa.3 = lt ssa.8, 1\n"
            "    br ssa.3, bb.2, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.2 = add ssa.8, 1\n"
            "    ssa.5 = mov 99\n"
            "    store_local spill.0.0, ssa.5\n"
            "    ssa.9 = load_local spill.0.0\n"
            "    ssa.6 = add ssa.9, 2\n"
            "    jmp bb.1\n") == NULL ||
        strstr(actual_text, "bb.3:\n    ssa.7 = add ssa.8, 3\n    ret ssa.7\n") == NULL ||
        reload_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: loop-reload-clobber expected loop-body clobber to force a fresh post-store reload, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_reload_through_nested_reconvergence(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    const char *cursor;
    size_t reload_count = 0;
    int ok = 1;

    if (!build_alloc_nested_reconverge_reload_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: nested-reconverge-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 3, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: nested-reconverge-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: nested-reconverge-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++reload_count;
    }

    if (strstr(actual_text,
            "bb.3:\n"
            "    ssa.7 = load_local spill.0.0\n"
            "    ssa.4 = add ssa.7, 1\n"
            "    br ssa.1, bb.4, bb.5\n") == NULL ||
        strstr(actual_text, "bb.6:\n    ssa.5 = add ssa.7, 2\n    br ssa.2, bb.7, bb.8\n") == NULL ||
        strstr(actual_text, "bb.9:\n    ssa.6 = add ssa.7, 3\n    ret ssa.6\n") == NULL ||
        reload_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: nested-reconverge-reuse expected one reused reload through two joins, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_does_not_reuse_reload_past_nested_clobber(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    const char *cursor;
    size_t reload_count = 0;
    int ok = 1;

    if (!build_alloc_nested_reconverge_clobber_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: nested-reconverge-clobber setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 3, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: nested-reconverge-clobber rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: nested-reconverge-clobber dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++reload_count;
    }

    if (strstr(actual_text,
            "bb.3:\n"
            "    ssa.8 = load_local spill.0.0\n"
            "    ssa.4 = add ssa.8, 1\n"
            "    br ssa.1, bb.4, bb.5\n") == NULL ||
        strstr(actual_text,
            "bb.5:\n"
            "    ssa.5 = mov 99\n"
            "    store_local spill.0.0, ssa.5\n"
            "    jmp bb.6\n") == NULL ||
        strstr(actual_text, "bb.6:\n    ssa.9 = load_local spill.0.0\n    ssa.6 = add ssa.9, 2\n    br ssa.2, bb.7, bb.8\n") == NULL ||
        strstr(actual_text, "bb.9:\n    ssa.7 = add ssa.9, 3\n    ret ssa.7\n") == NULL ||
        reload_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: nested-reconverge-clobber expected fresh reload after nested-path clobber, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_splits_loop_backedge_for_phi_input(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_loop_backedge_phi_input_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-backedge-phi-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 2, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-backedge-phi-spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-backedge-phi-spill dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.6 = mov 42\n"
            "    ssa.2 = mov 1\n"
            "    store_local spill.0.0, ssa.2\n"
            "    ssa.4 = lt ssa.1, 1\n"
            "    br ssa.4, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.4: ssa.9]\n"
            "    ssa.7 = phi [bb.0: ssa.5], [bb.4: ssa.10]\n"
            "    ssa.3 = lt ssa.1, 1\n"
            "    br ssa.3, bb.2, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.9 = load_local spill.0.0\n"
            "    ssa.10 = mov ssa.6\n"
            "    jmp bb.1\n") == NULL ||
        strstr(actual_text, "bb.3:\n    ssa.8 = add ssa.7, 1\n    ret ssa.8\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: loop-backedge-phi-spill expected split reload only on backedge-to-header edge, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_loop_backedge_split_for_two_phi_inputs(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_loop_backedge_two_phi_inputs_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-backedge-two-phi setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(
            &result, &program, (const size_t[]){7, 8}, 2, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-backedge-two-phi rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: loop-backedge-two-phi dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.2:\n"
            "    ssa.7 = mov 42\n"
            "    store_local spill.0.0, ssa.7\n"
            "    ssa.8 = mov 43\n"
            "    store_local spill.1.1, ssa.8\n"
            "    ssa.2 = mov 1\n"
            "    ssa.4 = lt ssa.1, 1\n"
            "    br ssa.4, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = phi [bb.0: ssa.0], [bb.4: ssa.13]\n"
            "    ssa.9 = phi [bb.0: ssa.5], [bb.4: ssa.12]\n"
            "    ssa.10 = phi [bb.0: ssa.6], [bb.4: ssa.14]\n"
            "    ssa.3 = lt ssa.1, 1\n"
            "    br ssa.3, bb.2, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.12 = load_local spill.0.0\n"
            "    ssa.13 = mov ssa.2\n"
            "    ssa.14 = load_local spill.1.1\n"
            "    jmp bb.1\n") == NULL ||
        strstr(actual_text, "bb.3:\n    ssa.11 = add ssa.9, ssa.10\n    ret ssa.11\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: loop-backedge-two-phi expected one reused split block on loop backedge, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_spills_and_canonicalizes(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    int ok;

    if (!build_alloc_cross_block_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite-canon setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_program(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite-canon allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills_and_canonicalize(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite-canon run failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = value_ssa_verify_program(&program, &error);
    if (!ok) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: rewrite-canon verify failed: %s\n", error.message);
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_phi_input_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_phi_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 4, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-spill dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.1:\n    ssa.") == NULL || strstr(actual_text, "bb.2:\n    ssa.") == NULL ||
        strstr(actual_text, "phi [bb.1: ssa.") == NULL || strstr(actual_text, "[bb.2: ssa.") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: phi-spill expected predecessor reloads feeding phi, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_phi_defined_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_phi_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 5, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-spill dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text, "bb.3:\n    ssa.5 = phi") == NULL ||
        strstr(actual_text, "store_local spill.0.0, ssa.5") == NULL ||
        strstr(actual_text, "load_local spill.0.0") == NULL ||
        strstr(actual_text, "ret ssa.6") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: phi-def-spill expected join-block store and reload, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_phi_defined_multi_use_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaDefUseAnalysis def_use;
    char *actual_text = NULL;
    size_t load_count = 0;
    const char *cursor;
    int ok = 1;

    if (!build_alloc_phi_defined_multi_use_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-multi-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_def_use_analysis_init(&def_use);
    if (!value_ssa_compute_def_use_analysis(&program.functions[0], &def_use, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-multi-spill def-use failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }
    if (def_use.value_count <= 3 || !def_use.has_def[3] || def_use.def_phi_indices[3] != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: phi-def-multi-spill expected ssa.3 phi def before rewrite "
            "(count=%zu has=%d block=%zu phi=%zu instr=%zu)\n",
            def_use.value_count,
            def_use.value_count > 3 ? (int)def_use.has_def[3] : 0,
            def_use.value_count > 3 ? def_use.def_block_ids[3] : (size_t)-1,
            def_use.value_count > 3 ? def_use.def_phi_indices[3] : (size_t)-1,
            def_use.value_count > 3 ? def_use.def_instruction_indices[3] : (size_t)-1);
        value_ssa_def_use_analysis_free(&def_use);
        value_ssa_program_free(&program);
        return 0;
    }
    value_ssa_def_use_analysis_free(&def_use);

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 3, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-multi-spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: phi-def-multi-spill dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "load_local spill.0.0")) != NULL; ++cursor) {
        ++load_count;
    }

    if (strstr(actual_text, "store_local spill.0.0, ssa.3") == NULL ||
        strstr(actual_text, "ssa.7 = load_local spill.0.0") == NULL ||
        strstr(actual_text, "ssa.4 = add ssa.7, 1") == NULL ||
        strstr(actual_text, "br ssa.7, bb.4, bb.5") == NULL ||
        strstr(actual_text, "bb.4:\n    ssa.5 = add ssa.7, 2") == NULL ||
        strstr(actual_text, "ssa.6 = phi [bb.3: ssa.7], [bb.4: ssa.5]") == NULL ||
        load_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: phi-def-multi-spill expected same-block and unique-predecessor reload reuse, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_branch_predecessor_phi_input_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_branch_predecessor_phi_input_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-pred-phi-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 1, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-pred-phi-spill rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-pred-phi-spill dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    store_local spill.0.0, ssa.1\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text, "ssa.3 = phi [bb.4: ssa.4], [bb.3: ssa.2]") == NULL ||
        strstr(actual_text, "bb.3:\n    ssa.2 = mov 7\n    jmp bb.2\n") == NULL ||
        strstr(actual_text, "bb.4:\n    ssa.4 = load_local spill.0.0\n    jmp bb.2\n") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: branch-pred-phi-spill expected split-edge reload and one-edge phi rewrite, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_branch_split_for_two_phi_inputs(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    size_t split_block_count = 0;
    const char *cursor;
    int ok = 1;

    if (!build_alloc_branch_predecessor_two_phi_inputs_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-split-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_value(&result, &program, 1, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-split-reuse rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-split-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "jmp bb.2\n")) != NULL; ++cursor) {
        ++split_block_count;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    store_local spill.0.0, ssa.1\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.4 = phi [bb.4: ssa.7], [bb.3: ssa.2]\n"
            "    ssa.5 = phi [bb.4: ssa.7], [bb.3: ssa.3]") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.7 = load_local spill.0.0\n"
            "    jmp bb.2\n") == NULL ||
        split_block_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: branch-split-reuse expected one reused split block and one shared reload, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_rewrite_program_reuses_branch_split_for_distinct_spill_slots(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    size_t split_block_count = 0;
    const char *cursor;
    static const size_t spilled_values[] = {1, 2};
    int ok = 1;

    if (!build_alloc_branch_predecessor_two_distinct_spills_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-split-distinct setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!prepare_manual_spill_result_for_values(&result, &program, spilled_values, 2, &error)) {
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_rewrite_program_single_block_spills(&program, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-split-distinct rewrite failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: branch-split-distinct dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "jmp bb.2\n")) != NULL; ++cursor) {
        ++split_block_count;
    }

    if (strstr(actual_text,
            "bb.1:\n"
            "    ssa.1 = mov 42\n"
            "    store_local spill.0.0, ssa.1\n"
            "    ssa.2 = mov 43\n"
            "    store_local spill.1.1, ssa.2\n"
            "    br ssa.0, bb.4, bb.3\n") == NULL ||
        strstr(actual_text,
            "bb.2:\n"
            "    ssa.5 = phi [bb.4: ssa.8], [bb.3: ssa.3]\n"
            "    ssa.6 = phi [bb.4: ssa.9], [bb.3: ssa.4]") == NULL ||
        strstr(actual_text,
            "bb.4:\n"
            "    ssa.8 = load_local spill.0.0\n"
            "    ssa.9 = load_local spill.1.1\n"
            "    jmp bb.2\n") == NULL ||
        split_block_count != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: branch-split-distinct expected one reused split block across distinct spill slots, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_evicts_lower_priority_neighbor(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    static const size_t earlier_values[] = {0, 1};
    int ok;

    if (!build_alloc_evict_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: evict setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: evict allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = 1;
    ok &= expect_spill_count_on_values("VALUE-SSA-ALLOC-EVICT", &result, earlier_values, 2, 1);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-EVICT", &result, 2, 0);
    ok &= expect_spill_flag("VALUE-SSA-ALLOC-EVICT", &result, 3, 0);

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_coalesce_analysis_safe_pair(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    size_t lhs_value;
    size_t rhs_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    if (!build_alloc_coalesce_safe_program(&program, &lhs_value, &rhs_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-safe setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], NULL, NULL, NULL, 2, &analysis, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-safe analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_pair("VALUE-SSA-ALLOC-COALESCE-SAFE", &analysis, lhs_value, rhs_value, 0, 1);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-COALESCE-SAFE", &analysis, lhs_value, lhs_value, 3);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-COALESCE-SAFE", &analysis, rhs_value, lhs_value, 3);
    ok &= expect_coalesce_dump_contains("VALUE-SSA-ALLOC-COALESCE-SAFE",
        &program.functions[0],
        &analysis,
        2,
        "interferes=no",
        "can_coalesce=yes");

cleanup:
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_coalesce_analysis_merges_chain_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    size_t root_value;
    size_t mid_value;
    size_t leaf_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    if (!build_alloc_coalesce_chain_program(&program, &root_value, &mid_value, &leaf_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-chain setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], NULL, NULL, NULL, 2, &analysis, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-chain analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_pair("VALUE-SSA-ALLOC-COALESCE-CHAIN", &analysis, root_value, mid_value, 0, 1);
    ok &= expect_coalesce_pair("VALUE-SSA-ALLOC-COALESCE-CHAIN", &analysis, mid_value, leaf_value, 0, 1);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-COALESCE-CHAIN", &analysis, root_value, root_value, 4);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-COALESCE-CHAIN", &analysis, mid_value, root_value, 4);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-COALESCE-CHAIN", &analysis, leaf_value, root_value, 4);
    ok &= expect_coalesce_dump_contains("VALUE-SSA-ALLOC-COALESCE-CHAIN",
        &program.functions[0],
        &analysis,
        2,
        "class ssa.2 -> root=ssa.0 size=4",
        "can_coalesce=yes");

cleanup:
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_coalesce_analysis_blocks_interfering_pair(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    size_t lhs_value;
    size_t rhs_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    if (!build_alloc_coalesce_interfering_program(&program, &lhs_value, &rhs_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-interfering setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], NULL, NULL, NULL, 2, &analysis, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-interfering analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_pair_absent("VALUE-SSA-ALLOC-COALESCE-INTERFERING",
        &analysis,
        lhs_value,
        rhs_value);
    ok &= expect_coalesce_dump_contains("VALUE-SSA-ALLOC-COALESCE-INTERFERING",
        &program.functions[0],
        &analysis,
        2,
        "candidates=0",
        NULL);

cleanup:
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_coalesce_analysis_blocks_budget_heavy_merge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-budget-blocked setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.candidate_count = 1;
    prep.candidates = (ValueSsaCopyAffinityCandidate *)calloc(1, sizeof(ValueSsaCopyAffinityCandidate));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    if (!prep.candidates || !interference.interferes || !affinity.weights) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-budget-blocked manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.candidates[0].lhs = 0;
    prep.candidates[0].rhs = 1;
    prep.candidates[0].weight = 1;
    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    interference.interferes[0 * 4 + 2] = 1;
    interference.interferes[2 * 4 + 0] = 1;
    interference.interferes[1 * 4 + 3] = 1;
    interference.interferes[3 * 4 + 1] = 1;

    if (!value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], &prep, &interference, &affinity, 2, &analysis, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-budget-blocked analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_pair("VALUE-SSA-ALLOC-COALESCE-BUDGET", &analysis, 0, 1, 0, 1);
    ok &= expect_coalesce_dump_contains("VALUE-SSA-ALLOC-COALESCE-BUDGET",
        &program.functions[0],
        &analysis,
        2,
        "merged_neighbors=2",
        "can_coalesce=yes");

cleanup:
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_coalesce_analysis_blocks_george_unsafe_budget_heavy_merge(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    if (!build_alloc_coalesce_george_unsafe_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-george-unsafe setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 6;
    prep.candidate_count = 1;
    prep.candidates = (ValueSsaCopyAffinityCandidate *)calloc(1, sizeof(ValueSsaCopyAffinityCandidate));
    interference.value_count = 6;
    interference.interferes = (unsigned char *)calloc(36, sizeof(unsigned char));
    affinity.value_count = 6;
    affinity.weights = (size_t *)calloc(36, sizeof(size_t));
    if (!prep.candidates || !interference.interferes || !affinity.weights) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-george-unsafe manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.candidates[0].lhs = 0;
    prep.candidates[0].rhs = 1;
    prep.candidates[0].weight = 1;
    affinity.weights[0 * 6 + 1] = 1;
    affinity.weights[1 * 6 + 0] = 1;

    interference.interferes[0 * 6 + 2] = 1;
    interference.interferes[2 * 6 + 0] = 1;
    interference.interferes[1 * 6 + 3] = 1;
    interference.interferes[3 * 6 + 1] = 1;
    interference.interferes[2 * 6 + 4] = 1;
    interference.interferes[4 * 6 + 2] = 1;
    interference.interferes[3 * 6 + 5] = 1;
    interference.interferes[5 * 6 + 3] = 1;

    if (!value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], &prep, &interference, &affinity, 2, &analysis, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: coalesce-george-unsafe analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_pair("VALUE-SSA-ALLOC-COALESCE-GEORGE-UNSAFE", &analysis, 0, 1, 0, 0);
    ok &= expect_coalesce_dump_contains("VALUE-SSA-ALLOC-COALESCE-GEORGE-UNSAFE",
        &program.functions[0],
        &analysis,
        2,
        "merged_neighbors=2",
        "can_coalesce=no");

cleanup:
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_uses_coalesced_effective_pressure(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    size_t lhs_value;
    size_t rhs_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_safe_program(&program, &lhs_value, &rhs_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-coalesce-pressure setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_compute_interference_graph(&program.functions[0], NULL, NULL, &interference, &error) ||
        !value_ssa_compute_copy_affinity_graph(&program.functions[0], &interference, &affinity, &error) ||
        !value_ssa_compute_allocator_coalesce_analysis(
            &program.functions[0], &prep, &interference, &affinity, 2, &analysis, &error) ||
        !value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-coalesce-pressure build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_pair("VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &analysis, lhs_value, rhs_value, 0, 1);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &plan, lhs_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &plan, rhs_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &plan, lhs_value, 0);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &plan, rhs_value, 0);
    ok &= expect_plan_effective_move_related(
        "VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &plan, lhs_value, 0, 0);
    ok &= expect_plan_effective_move_related(
        "VALUE-SSA-ALLOC-PLAN-COALESCE-PRESSURE", &plan, rhs_value, 0, 0);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_propagates_family_external_move_and_freeze(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-family-freeze setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    analysis.value_count = 4;
    analysis.value_roots = (size_t *)calloc(4, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !analysis.value_roots || !analysis.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-family-freeze manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;

    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 0;
    analysis.value_roots[2] = 0;
    analysis.value_roots[3] = 3;
    analysis.class_sizes[0] = 3;
    analysis.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-family-freeze build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &analysis, 0, 0, 3);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &analysis, 1, 0, 3);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &analysis, 2, 0, 3);
    ok &= expect_coalesce_class("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &analysis, 3, 3, 1);
    ok &= expect_plan_effective_move_related(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 0, 1, 0);
    ok &= expect_plan_effective_move_related(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 1, 1, 0);
    ok &= expect_plan_effective_move_related(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 2, 1, 0);
    ok &= expect_plan_effective_move_related(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 3, 1, 0);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 0, 0);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 1, 0);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 2, 0);
    ok &= expect_plan_frozen("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 3, 1);
    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 0, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 3, VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING);
    ok &= expect_plan_move_work_transition(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 0, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_move_work_transition(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 3, VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING,
        VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL);
    ok &= expect_plan_move_work_priority_order("VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 3, 0);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-FAMILY-FREEZE", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_prefers_released_simplify_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-released-simplify setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    analysis.value_count = 4;
    analysis.value_roots = (size_t *)calloc(4, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !analysis.value_roots || !analysis.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-released-simplify manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist.items[3].priority = 10;

    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 1;
    analysis.value_roots[2] = 1;
    analysis.value_roots[3] = 1;
    analysis.class_sizes[0] = 1;
    analysis.class_sizes[1] = 3;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-released-simplify build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-SIMPLIFY", &plan, 1, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-SIMPLIFY", &plan, 0, VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-SIMPLIFY", &plan, 1, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-SIMPLIFY", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-RELEASED-SIMPLIFY", &plan, 1, 0);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_prefers_mutual_best_ready_family_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-mutual-ready setup failed: %s\n",
            error.message);
        return 0;
    }

    program.functions[0].next_value_id = 6;
    prep.value_count = 6;
    prep.use_counts = (size_t *)calloc(6, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(6, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(6, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(6, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(6, sizeof(unsigned char));
    worklist.value_count = 6;
    worklist.item_count = 6;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(6, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 6;
    interference.interferes = (unsigned char *)calloc(36, sizeof(unsigned char));
    affinity.value_count = 6;
    affinity.weights = (size_t *)calloc(36, sizeof(size_t));
    analysis.value_count = 6;
    analysis.value_roots = (size_t *)calloc(6, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(6, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !analysis.value_roots || !analysis.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-mutual-ready manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.move_related[4] = 1;
    prep.move_related[5] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;
    worklist.items[4].value_id = 4;
    worklist.items[4].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[4].priority = 20;
    worklist.items[5].value_id = 5;
    worklist.items[5].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[5].priority = 20;

    affinity.weights[0 * 6 + 2] = 2;
    affinity.weights[2 * 6 + 0] = 2;
    affinity.weights[2 * 6 + 4] = 2;
    affinity.weights[4 * 6 + 2] = 2;

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 0;
    analysis.value_roots[2] = 2;
    analysis.value_roots[3] = 2;
    analysis.value_roots[4] = 4;
    analysis.value_roots[5] = 4;
    analysis.class_sizes[0] = 2;
    analysis.class_sizes[2] = 2;
    analysis.class_sizes[4] = 2;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-mutual-ready build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-MUTUAL-READY", &plan, 0, VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING);
    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-MUTUAL-READY", &plan, 4, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-MUTUAL-READY", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_phase(
        "VALUE-SSA-ALLOC-PLAN-MUTUAL-READY", &plan, 4, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-MUTUAL-READY", &plan, 0, 4);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_prefers_higher_priority_released_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-released-priority setup failed: %s\n",
            error.message);
        return 0;
    }

    program.functions[0].next_value_id = 5;
    prep.value_count = 5;
    prep.use_counts = (size_t *)calloc(5, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(5, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(5, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(5, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(5, sizeof(unsigned char));
    worklist.value_count = 5;
    worklist.item_count = 5;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(5, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 5;
    interference.interferes = (unsigned char *)calloc(25, sizeof(unsigned char));
    affinity.value_count = 5;
    affinity.weights = (size_t *)calloc(25, sizeof(size_t));
    analysis.value_count = 5;
    analysis.value_roots = (size_t *)calloc(5, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(5, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !analysis.value_roots || !analysis.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-released-priority manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[4] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 1;
    prep.affinity_sums[4] = 2;
    prep.interference_degrees[0] = 3;
    prep.interference_degrees[1] = 1;
    prep.interference_degrees[2] = 1;
    prep.interference_degrees[3] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_GLOBAL;
    worklist.items[3].priority = 10;
    worklist.items[4].value_id = 4;
    worklist.items[4].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[4].priority = 0;

    interference.interferes[0 * 5 + 1] = 1;
    interference.interferes[0 * 5 + 2] = 1;
    interference.interferes[0 * 5 + 3] = 1;
    interference.interferes[1 * 5 + 0] = 1;
    interference.interferes[2 * 5 + 0] = 1;
    interference.interferes[3 * 5 + 0] = 1;

    affinity.weights[0 * 5 + 4] = 1;
    affinity.weights[4 * 5 + 0] = 1;
    affinity.weights[1 * 5 + 4] = 1;
    affinity.weights[4 * 5 + 1] = 1;

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 1;
    analysis.value_roots[2] = 1;
    analysis.value_roots[3] = 1;
    analysis.value_roots[4] = 4;
    analysis.class_sizes[0] = 1;
    analysis.class_sizes[1] = 3;
    analysis.class_sizes[4] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-released-priority build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-PRIORITY", &plan, 0, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-PRIORITY", &plan, 1, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_move_work_priority_order("VALUE-SSA-ALLOC-PLAN-RELEASED-PRIORITY", &plan, 0, 1);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-RELEASED-PRIORITY", &plan, 1, 0);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_picks_best_member_within_released_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-released-member setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    analysis.value_count = 4;
    analysis.value_roots = (size_t *)calloc(4, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !analysis.value_roots || !analysis.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-released-member manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 8;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 14;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;

    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 0;
    analysis.value_roots[2] = 0;
    analysis.value_roots[3] = 3;
    analysis.class_sizes[0] = 3;
    analysis.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-released-member build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_move_work_class(
        "VALUE-SSA-ALLOC-PLAN-RELEASED-MEMBER", &plan, 1, VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-RELEASED-MEMBER", &plan, 1, 0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-RELEASED-MEMBER", &plan, 1, 2);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_picks_best_member_within_interleaved_released_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis analysis;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&analysis);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-interleaved-released-member setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    analysis.value_count = 4;
    analysis.value_roots = (size_t *)calloc(4, sizeof(size_t));
    analysis.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !analysis.value_roots || !analysis.class_sizes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-interleaved-released-member manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 8;
    worklist.items[1].value_id = 3;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 1;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 14;
    worklist.items[3].value_id = 2;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;

    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;

    analysis.value_roots[0] = 0;
    analysis.value_roots[1] = 0;
    analysis.value_roots[2] = 0;
    analysis.value_roots[3] = 3;
    analysis.class_sizes[0] = 3;
    analysis.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &analysis, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-interleaved-released-member build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_move_work_class("VALUE-SSA-ALLOC-PLAN-INTERLEAVED-RELEASED-MEMBER",
        &plan,
        1,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-INTERLEAVED-RELEASED-MEMBER", &plan, 1, 0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-INTERLEAVED-RELEASED-MEMBER", &plan, 1, 2);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-INTERLEAVED-RELEASED-MEMBER", &plan, 1, 3);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&analysis);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_family_analysis_internal_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveFamilyAnalysis analysis;
    size_t lhs_value;
    size_t rhs_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_family_analysis_init(&analysis);
    if (!build_alloc_coalesce_safe_program(&program, &lhs_value, &rhs_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-family-internal setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_move_family_analysis(
            &program.functions[0], NULL, NULL, NULL, 2, &analysis, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-family-internal analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_family("VALUE-SSA-ALLOC-MOVE-FAMILY-INTERNAL",
        &analysis,
        lhs_value,
        lhs_value,
        VALUE_SSA_ALLOC_MOVE_FAMILY_INTERNAL,
        3,
        0,
        0,
        0,
        0,
        0,
        3,
        0);
    ok &= expect_move_family("VALUE-SSA-ALLOC-MOVE-FAMILY-INTERNAL",
        &analysis,
        rhs_value,
        lhs_value,
        VALUE_SSA_ALLOC_MOVE_FAMILY_INTERNAL,
        3,
        0,
        0,
        0,
        0,
        0,
        3,
        0);

cleanup:
    value_ssa_allocator_move_family_analysis_free(&analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_family_analysis_frozen_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveFamilyAnalysis analysis;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_family_analysis_init(&analysis);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-family-frozen setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    coalesce.value_count = 4;
    coalesce.value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-family-frozen manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;
    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;
    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;
    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 0;
    coalesce.value_roots[2] = 0;
    coalesce.value_roots[3] = 3;
    coalesce.class_sizes[0] = 3;
    coalesce.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error) ||
        !value_ssa_compute_allocator_move_family_analysis(
            &program.functions[0], &affinity, &coalesce, &plan, 2, &analysis, &error) ||
        !value_ssa_dump_allocator_move_family_analysis(&program.functions[0], &analysis, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-family-frozen analysis failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_family("VALUE-SSA-ALLOC-MOVE-FAMILY-FROZEN",
        &analysis,
        0,
        0,
        VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED,
        3,
        1,
        0,
        1,
        1,
        0,
        3,
        0);
    ok &= expect_move_family("VALUE-SSA-ALLOC-MOVE-FAMILY-FROZEN",
        &analysis,
        3,
        3,
        VALUE_SSA_ALLOC_MOVE_FAMILY_FROZEN,
        1,
        1,
        0,
        1,
        1,
        1,
        1,
        0);
    if (!actual_text || strstr(actual_text, "state=frozen") == NULL || strstr(actual_text, "state=released") == NULL ||
        strstr(actual_text, "ready_neighbors=0") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-family-frozen expected dump to mention frozen, released, and ready_neighbors=0, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_move_family_analysis_free(&analysis);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_worklist_internal_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveWorklist worklist;
    size_t lhs_value;
    size_t rhs_value;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_worklist_init(&worklist);
    if (!build_alloc_coalesce_safe_program(&program, &lhs_value, &rhs_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-internal setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_move_worklist(
            &program.functions[0], NULL, NULL, 2, &worklist, &error) ||
        !value_ssa_dump_allocator_move_worklist(&program.functions[0], &worklist, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-internal build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-INTERNAL",
        &worklist,
        lhs_value,
        lhs_value,
        VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
        3,
        0,
        0,
        0,
        3,
        0);
    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-INTERNAL",
        &worklist,
        rhs_value,
        lhs_value,
        VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
        3,
        0,
        0,
        0,
        3,
        0);
    if (!actual_text || strstr(actual_text, "class=internal") == NULL ||
        strstr(actual_text, "ready_neighbors=0") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-internal expected dump to mention internal class and ready_neighbors=0, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_move_worklist_free(&worklist);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_worklist_manual_coalesce_ready_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveFamilyAnalysis family_analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveWorklist move_worklist;
    char *actual_text = NULL;
    size_t root_zero_index = 0;
    size_t root_two_index = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_family_analysis_init(&family_analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-ready setup failed: %s\n", error.message);
        return 0;
    }

    family_analysis.value_count = 4;
    family_analysis.family_count = 2;
    family_analysis.items =
        (ValueSsaAllocatorMoveFamilyItem *)calloc(2, sizeof(ValueSsaAllocatorMoveFamilyItem));
    family_analysis.value_family_indices = (size_t *)malloc(4 * sizeof(size_t));
    plan.value_count = 4;
    plan.item_count = 4;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!family_analysis.items || !family_analysis.value_family_indices || !plan.items) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-ready manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    family_analysis.items[0].root_value_id = 0;
    family_analysis.items[0].class_size = 2;
    family_analysis.items[0].external_neighbor_family_count = 1;
    family_analysis.items[0].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[0].external_affinity_weight_sum = 3;
    family_analysis.items[0].simplify_removed_count = 2;
    family_analysis.items[0].initial_move_related = 1;
    family_analysis.items[0].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.items[1].root_value_id = 2;
    family_analysis.items[1].class_size = 2;
    family_analysis.items[1].external_neighbor_family_count = 1;
    family_analysis.items[1].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[1].external_affinity_weight_sum = 2;
    family_analysis.items[1].simplify_removed_count = 2;
    family_analysis.items[1].initial_move_related = 1;
    family_analysis.items[1].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.value_family_indices[0] = 0;
    family_analysis.value_family_indices[1] = 0;
    family_analysis.value_family_indices[2] = 1;
    family_analysis.value_family_indices[3] = 1;

    plan.items[0].value_id = 0;
    plan.items[0].coalesce_root_value_id = 0;
    plan.items[0].priority = 7;
    plan.items[1].value_id = 1;
    plan.items[1].coalesce_root_value_id = 0;
    plan.items[1].priority = 11;
    plan.items[2].value_id = 2;
    plan.items[2].coalesce_root_value_id = 2;
    plan.items[2].priority = 6;
    plan.items[3].value_id = 3;
    plan.items[3].coalesce_root_value_id = 2;
    plan.items[3].priority = 9;

    if (!value_ssa_compute_allocator_move_worklist(
            &program.functions[0], &family_analysis, &plan, 2, &move_worklist, &error) ||
        !value_ssa_dump_allocator_move_worklist(&program.functions[0], &move_worklist, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-ready build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-READY",
        &move_worklist,
        0,
        0,
        VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY,
        2,
        1,
        1,
        3,
        2,
        0);
    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-READY",
        &move_worklist,
        2,
        2,
        VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY,
        2,
        1,
        1,
        2,
        2,
        0);
    if (!value_ssa_allocator_move_worklist_find_root(&move_worklist, 0, &root_zero_index, NULL) ||
        !value_ssa_allocator_move_worklist_find_root(&move_worklist, 2, &root_two_index, NULL) ||
        root_two_index <= root_zero_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready expected higher-priority ready family first, got root0=%zu root2=%zu\n",
            root_zero_index,
            root_two_index);
        ok = 0;
    }
    if (!actual_text || strstr(actual_text, "class=coalesce-ready") == NULL ||
        strstr(actual_text, "ready_neighbors=1") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready expected dump to mention coalesce-ready and ready_neighbors=1, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_move_family_analysis_free(&family_analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_worklist_prefers_more_ready_neighbors_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveFamilyAnalysis family_analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveWorklist move_worklist;
    size_t root_zero_index = 0;
    size_t root_two_index = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_family_analysis_init(&family_analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-ready-priority setup failed: %s\n", error.message);
        return 0;
    }

    family_analysis.value_count = 4;
    family_analysis.family_count = 2;
    family_analysis.items =
        (ValueSsaAllocatorMoveFamilyItem *)calloc(2, sizeof(ValueSsaAllocatorMoveFamilyItem));
    family_analysis.value_family_indices = (size_t *)malloc(4 * sizeof(size_t));
    plan.value_count = 4;
    plan.item_count = 4;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!family_analysis.items || !family_analysis.value_family_indices || !plan.items) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-ready-priority manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    family_analysis.items[0].root_value_id = 0;
    family_analysis.items[0].class_size = 2;
    family_analysis.items[0].external_neighbor_family_count = 1;
    family_analysis.items[0].coalesce_ready_neighbor_family_count = 2;
    family_analysis.items[0].external_affinity_weight_sum = 1;
    family_analysis.items[0].simplify_removed_count = 2;
    family_analysis.items[0].initial_move_related = 1;
    family_analysis.items[0].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.items[1].root_value_id = 2;
    family_analysis.items[1].class_size = 2;
    family_analysis.items[1].external_neighbor_family_count = 1;
    family_analysis.items[1].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[1].external_affinity_weight_sum = 1;
    family_analysis.items[1].simplify_removed_count = 2;
    family_analysis.items[1].initial_move_related = 1;
    family_analysis.items[1].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.value_family_indices[0] = 0;
    family_analysis.value_family_indices[1] = 0;
    family_analysis.value_family_indices[2] = 1;
    family_analysis.value_family_indices[3] = 1;

    plan.items[0].value_id = 0;
    plan.items[0].coalesce_root_value_id = 0;
    plan.items[0].priority = 10;
    plan.items[1].value_id = 1;
    plan.items[1].coalesce_root_value_id = 0;
    plan.items[1].priority = 10;
    plan.items[2].value_id = 2;
    plan.items[2].coalesce_root_value_id = 2;
    plan.items[2].priority = 10;
    plan.items[3].value_id = 3;
    plan.items[3].coalesce_root_value_id = 2;
    plan.items[3].priority = 10;

    if (!value_ssa_compute_allocator_move_worklist(
            &program.functions[0], &family_analysis, &plan, 2, &move_worklist, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-ready-priority build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-READY-PRIORITY",
        &move_worklist,
        0,
        0,
        VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY,
        2,
        1,
        2,
        1,
        2,
        0);
    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-READY-PRIORITY",
        &move_worklist,
        2,
        2,
        VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY,
        2,
        1,
        1,
        1,
        2,
        0);
    if (!value_ssa_allocator_move_worklist_find_root(&move_worklist, 0, &root_zero_index, NULL) ||
        !value_ssa_allocator_move_worklist_find_root(&move_worklist, 2, &root_two_index, NULL) ||
        root_zero_index >= root_two_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-priority expected higher-ready family first, got root0=%zu root2=%zu\n",
            root_zero_index,
            root_two_index);
        ok = 0;
    }

cleanup:
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_move_family_analysis_free(&family_analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_worklist_prefers_heavier_ready_affinity_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveFamilyAnalysis family_analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveWorklist move_worklist;
    size_t root_zero_index = 0;
    size_t root_two_index = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_family_analysis_init(&family_analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-affinity-priority setup failed: %s\n",
            error.message);
        return 0;
    }

    family_analysis.value_count = 4;
    family_analysis.family_count = 2;
    family_analysis.items =
        (ValueSsaAllocatorMoveFamilyItem *)calloc(2, sizeof(ValueSsaAllocatorMoveFamilyItem));
    family_analysis.value_family_indices = (size_t *)malloc(4 * sizeof(size_t));
    plan.value_count = 4;
    plan.item_count = 4;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!family_analysis.items || !family_analysis.value_family_indices || !plan.items) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-affinity-priority manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    family_analysis.items[0].root_value_id = 0;
    family_analysis.items[0].class_size = 2;
    family_analysis.items[0].external_neighbor_family_count = 1;
    family_analysis.items[0].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[0].coalesce_ready_affinity_weight_sum = 3;
    family_analysis.items[0].external_affinity_weight_sum = 3;
    family_analysis.items[0].simplify_removed_count = 2;
    family_analysis.items[0].initial_move_related = 1;
    family_analysis.items[0].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.items[1].root_value_id = 2;
    family_analysis.items[1].class_size = 2;
    family_analysis.items[1].external_neighbor_family_count = 1;
    family_analysis.items[1].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[1].coalesce_ready_affinity_weight_sum = 1;
    family_analysis.items[1].external_affinity_weight_sum = 3;
    family_analysis.items[1].simplify_removed_count = 2;
    family_analysis.items[1].initial_move_related = 1;
    family_analysis.items[1].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.value_family_indices[0] = 0;
    family_analysis.value_family_indices[1] = 0;
    family_analysis.value_family_indices[2] = 1;
    family_analysis.value_family_indices[3] = 1;

    plan.items[0].value_id = 0;
    plan.items[0].coalesce_root_value_id = 0;
    plan.items[0].priority = 10;
    plan.items[1].value_id = 1;
    plan.items[1].coalesce_root_value_id = 0;
    plan.items[1].priority = 10;
    plan.items[2].value_id = 2;
    plan.items[2].coalesce_root_value_id = 2;
    plan.items[2].priority = 10;
    plan.items[3].value_id = 3;
    plan.items[3].coalesce_root_value_id = 2;
    plan.items[3].priority = 10;

    if (!value_ssa_compute_allocator_move_worklist(
            &program.functions[0], &family_analysis, &plan, 2, &move_worklist, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-affinity-priority build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocator_move_worklist_find_root(&move_worklist, 0, &root_zero_index, NULL) ||
        !value_ssa_allocator_move_worklist_find_root(&move_worklist, 2, &root_two_index, NULL) ||
        root_zero_index >= root_two_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-affinity-priority expected heavier ready-affinity family first, got root0=%zu root2=%zu\n",
            root_zero_index,
            root_two_index);
        ok = 0;
    }

cleanup:
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_move_family_analysis_free(&family_analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_worklist_prefers_mutual_best_ready_partner_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveFamilyAnalysis family_analysis;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveWorklist move_worklist;
    size_t root_zero_index = 0;
    size_t root_two_index = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_family_analysis_init(&family_analysis);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-mutual-priority setup failed: %s\n",
            error.message);
        return 0;
    }

    family_analysis.value_count = 4;
    family_analysis.family_count = 2;
    family_analysis.items =
        (ValueSsaAllocatorMoveFamilyItem *)calloc(2, sizeof(ValueSsaAllocatorMoveFamilyItem));
    family_analysis.value_family_indices = (size_t *)malloc(4 * sizeof(size_t));
    plan.value_count = 4;
    plan.item_count = 4;
    plan.items = (ValueSsaAllocatorPlanItem *)calloc(4, sizeof(ValueSsaAllocatorPlanItem));
    if (!family_analysis.items || !family_analysis.value_family_indices || !plan.items) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-mutual-priority manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    family_analysis.items[0].root_value_id = 0;
    family_analysis.items[0].class_size = 2;
    family_analysis.items[0].external_neighbor_family_count = 1;
    family_analysis.items[0].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[0].coalesce_ready_affinity_weight_sum = 2;
    family_analysis.items[0].best_coalesce_ready_partner_root_value_id = 2;
    family_analysis.items[0].best_coalesce_ready_partner_affinity_weight = 2;
    family_analysis.items[0].best_coalesce_ready_partner_is_mutual = 1;
    family_analysis.items[0].external_affinity_weight_sum = 2;
    family_analysis.items[0].simplify_removed_count = 2;
    family_analysis.items[0].initial_move_related = 1;
    family_analysis.items[0].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.items[1].root_value_id = 2;
    family_analysis.items[1].class_size = 2;
    family_analysis.items[1].external_neighbor_family_count = 1;
    family_analysis.items[1].coalesce_ready_neighbor_family_count = 1;
    family_analysis.items[1].coalesce_ready_affinity_weight_sum = 2;
    family_analysis.items[1].best_coalesce_ready_partner_root_value_id = 0;
    family_analysis.items[1].best_coalesce_ready_partner_affinity_weight = 2;
    family_analysis.items[1].best_coalesce_ready_partner_is_mutual = 0;
    family_analysis.items[1].external_affinity_weight_sum = 2;
    family_analysis.items[1].simplify_removed_count = 2;
    family_analysis.items[1].initial_move_related = 1;
    family_analysis.items[1].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;

    family_analysis.value_family_indices[0] = 0;
    family_analysis.value_family_indices[1] = 0;
    family_analysis.value_family_indices[2] = 1;
    family_analysis.value_family_indices[3] = 1;

    plan.items[0].value_id = 0;
    plan.items[0].coalesce_root_value_id = 0;
    plan.items[0].priority = 10;
    plan.items[1].value_id = 1;
    plan.items[1].coalesce_root_value_id = 0;
    plan.items[1].priority = 10;
    plan.items[2].value_id = 2;
    plan.items[2].coalesce_root_value_id = 2;
    plan.items[2].priority = 10;
    plan.items[3].value_id = 3;
    plan.items[3].coalesce_root_value_id = 2;
    plan.items[3].priority = 10;

    if (!value_ssa_compute_allocator_move_worklist(
            &program.functions[0], &family_analysis, &plan, 2, &move_worklist, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-mutual-priority build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocator_move_worklist_find_root(&move_worklist, 0, &root_zero_index, NULL) ||
        !value_ssa_allocator_move_worklist_find_root(&move_worklist, 2, &root_two_index, NULL) ||
        root_zero_index >= root_two_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-ready-mutual-priority expected mutual best-ready family first, got root0=%zu root2=%zu\n",
            root_zero_index,
            root_two_index);
        ok = 0;
    }

cleanup:
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_move_family_analysis_free(&family_analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_coalesce_opportunity_agenda_manual_pairs(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorMoveFamilyAnalysis family_analysis;
    ValueSsaAllocatorMoveWorklist move_worklist;
    ValueSsaAllocatorCoalesceOpportunityAgenda agenda;
    char *actual_text = NULL;
    size_t first_pair_index = 0;
    size_t second_pair_index = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_move_family_analysis_init(&family_analysis);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    value_ssa_allocator_coalesce_opportunity_agenda_init(&agenda);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-opportunity-agenda setup failed: %s\n",
            error.message);
        return 0;
    }

    family_analysis.value_count = 6;
    family_analysis.family_count = 3;
    family_analysis.items =
        (ValueSsaAllocatorMoveFamilyItem *)calloc(3, sizeof(ValueSsaAllocatorMoveFamilyItem));
    family_analysis.value_family_indices = (size_t *)malloc(6 * sizeof(size_t));
    move_worklist.value_count = 6;
    move_worklist.item_count = 3;
    move_worklist.items = (ValueSsaAllocatorMoveWorkItem *)calloc(3, sizeof(ValueSsaAllocatorMoveWorkItem));
    move_worklist.value_work_indices = (size_t *)malloc(6 * sizeof(size_t));
    if (!family_analysis.items || !family_analysis.value_family_indices || !move_worklist.items ||
        !move_worklist.value_work_indices) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-opportunity-agenda manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    family_analysis.items[0].root_value_id = 0;
    family_analysis.items[0].class_size = 2;
    family_analysis.items[0].initial_move_related = 1;
    family_analysis.items[0].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;
    family_analysis.items[0].best_coalesce_ready_partner_root_value_id = 2;
    family_analysis.items[0].best_coalesce_ready_partner_affinity_weight = 2;

    family_analysis.items[1].root_value_id = 2;
    family_analysis.items[1].class_size = 2;
    family_analysis.items[1].initial_move_related = 1;
    family_analysis.items[1].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;
    family_analysis.items[1].best_coalesce_ready_partner_root_value_id = 0;
    family_analysis.items[1].best_coalesce_ready_partner_affinity_weight = 2;

    family_analysis.items[2].root_value_id = 4;
    family_analysis.items[2].class_size = 2;
    family_analysis.items[2].initial_move_related = 1;
    family_analysis.items[2].state = VALUE_SSA_ALLOC_MOVE_FAMILY_RELEASED;
    family_analysis.items[2].best_coalesce_ready_partner_root_value_id = 2;
    family_analysis.items[2].best_coalesce_ready_partner_affinity_weight = 5;

    family_analysis.value_family_indices[0] = 0;
    family_analysis.value_family_indices[1] = 0;
    family_analysis.value_family_indices[2] = 1;
    family_analysis.value_family_indices[3] = 1;
    family_analysis.value_family_indices[4] = 2;
    family_analysis.value_family_indices[5] = 2;

    move_worklist.items[0].root_value_id = 0;
    move_worklist.items[0].work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    move_worklist.items[0].priority = 20;
    move_worklist.items[1].root_value_id = 2;
    move_worklist.items[1].work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    move_worklist.items[1].priority = 18;
    move_worklist.items[2].root_value_id = 4;
    move_worklist.items[2].work_class = VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY;
    move_worklist.items[2].priority = 12;
    move_worklist.value_work_indices[0] = 0;
    move_worklist.value_work_indices[1] = 0;
    move_worklist.value_work_indices[2] = 1;
    move_worklist.value_work_indices[3] = 1;
    move_worklist.value_work_indices[4] = 2;
    move_worklist.value_work_indices[5] = 2;

    if (!value_ssa_compute_allocator_coalesce_opportunity_agenda(
            &program.functions[0], &family_analysis, &move_worklist, 2, &agenda, &error) ||
        !value_ssa_dump_allocator_coalesce_opportunity_agenda(&program.functions[0], &agenda, &actual_text, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-opportunity-agenda build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_coalesce_opportunity(
        "VALUE-SSA-ALLOC-COALESCE-OPPORTUNITY", &agenda, 0, 2, 1, 1, 2, 1, 2);
    ok &= expect_coalesce_opportunity(
        "VALUE-SSA-ALLOC-COALESCE-OPPORTUNITY", &agenda, 2, 4, 0, 0, 0, 1, 5);
    ok &= expect_best_coalesce_opportunity_for_root(
        "VALUE-SSA-ALLOC-COALESCE-OPPORTUNITY", &agenda, 0, 0, 2);
    ok &= expect_best_coalesce_opportunity_for_root(
        "VALUE-SSA-ALLOC-COALESCE-OPPORTUNITY", &agenda, 4, 2, 4);
    if (!value_ssa_allocator_coalesce_opportunity_agenda_find_pair(&agenda, 0, 2, &first_pair_index, NULL) ||
        !value_ssa_allocator_coalesce_opportunity_agenda_find_pair(&agenda, 2, 4, &second_pair_index, NULL) ||
        first_pair_index >= second_pair_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-opportunity-agenda expected mutual pair before unilateral pair, got first=%zu second=%zu\n",
            first_pair_index,
            second_pair_index);
        ok = 0;
    }
    if (!actual_text || strstr(actual_text, "pair=ssa.0<->ssa.2") == NULL ||
        strstr(actual_text, "mutual=yes") == NULL ||
        strstr(actual_text, "pair=ssa.2<->ssa.4") == NULL ||
        strstr(actual_text, "mutual=no") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: coalesce-opportunity-agenda expected dump to mention mutual and unilateral pairs, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_coalesce_opportunity_agenda_free(&agenda);
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_move_family_analysis_free(&family_analysis);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_worklist_frozen_and_released_families(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveFamilyAnalysis family_analysis;
    ValueSsaAllocatorMoveWorklist move_worklist;
    char *actual_text = NULL;
    size_t root_index = 0;
    size_t leaf_index = 0;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_family_analysis_init(&family_analysis);
    value_ssa_allocator_move_worklist_init(&move_worklist);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-frozen setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    coalesce.value_count = 4;
    coalesce.value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-frozen manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;
    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;
    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;
    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 0;
    coalesce.value_roots[2] = 0;
    coalesce.value_roots[3] = 3;
    coalesce.class_sizes[0] = 3;
    coalesce.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error) ||
        !value_ssa_compute_allocator_move_family_analysis(
            &program.functions[0], &affinity, &coalesce, &plan, 2, &family_analysis, &error) ||
        !value_ssa_compute_allocator_move_worklist(
            &program.functions[0], &family_analysis, &plan, 2, &move_worklist, &error) ||
        !value_ssa_dump_allocator_move_worklist(&program.functions[0], &move_worklist, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-work-frozen build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-FROZEN",
        &move_worklist,
        0,
        0,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        3,
        1,
        0,
        1,
        3,
        0);
    ok &= expect_move_work("VALUE-SSA-ALLOC-MOVE-WORK-FROZEN",
        &move_worklist,
        3,
        3,
        VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING,
        1,
        1,
        0,
        1,
        1,
        0);
    if (!value_ssa_allocator_move_worklist_find_root(&move_worklist, 0, &root_index, NULL) ||
        !value_ssa_allocator_move_worklist_find_root(&move_worklist, 3, &leaf_index, NULL) ||
        leaf_index >= root_index) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-frozen expected freeze-pending family before released sibling, got root_index=%zu leaf_index=%zu\n",
            root_index,
            leaf_index);
        ok = 0;
    }
    if (!actual_text || strstr(actual_text, "class=freeze-pending") == NULL ||
        strstr(actual_text, "class=released") == NULL ||
        strstr(actual_text, "ready_neighbors=0") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-work-frozen expected dump to mention freeze-pending, released, and ready_neighbors=0, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_move_worklist_free(&move_worklist);
    value_ssa_allocator_move_family_analysis_free(&family_analysis);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_transition_trace_family_progression(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveTransitionTrace trace;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_transition_trace_init(&trace);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    coalesce.value_count = 4;
    coalesce.value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 2;
    prep.affinity_sums[2] = 2;
    prep.affinity_sums[3] = 1;
    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 10;
    affinity.weights[0 * 4 + 1] = 1;
    affinity.weights[1 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 1] = 1;
    affinity.weights[2 * 4 + 3] = 1;
    affinity.weights[3 * 4 + 2] = 1;
    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 0;
    coalesce.value_roots[2] = 0;
    coalesce.value_roots[3] = 3;
    coalesce.class_sizes[0] = 3;
    coalesce.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error) ||
        !value_ssa_compute_allocator_move_transition_trace(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &trace, &error) ||
        !value_ssa_dump_allocator_move_transition_trace(&program.functions[0], &trace, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_transition("VALUE-SSA-ALLOC-MOVE-TRANSITION",
        &trace,
        3,
        VALUE_SSA_ALLOC_MOVE_EVENT_FREEZE,
        VALUE_SSA_ALLOCATOR_REMOVE_NONE,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING,
        1,
        1,
        1,
        0);
    ok &= expect_move_transition("VALUE-SSA-ALLOC-MOVE-TRANSITION",
        &trace,
        0,
        VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE,
        VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        3,
        2,
        0,
        0);
    ok &= expect_move_transition("VALUE-SSA-ALLOC-MOVE-TRANSITION",
        &trace,
        3,
        VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE,
        VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
        VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING,
        VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
        1,
        0,
        0,
        0);
    if (!actual_text || strstr(actual_text, "event=freeze phase=freeze ssa.3") == NULL ||
        strstr(actual_text, "remove=none") == NULL ||
        strstr(actual_text, "move_work=released->freeze-pending") == NULL ||
        strstr(actual_text, "move_work=released->released") == NULL ||
        strstr(actual_text, "move_work=freeze-pending->internal") == NULL ||
        strstr(actual_text, "family_members=1->0") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition expected dump to mention freeze and remove transitions, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }
    {
        size_t item_index;

        for (item_index = 0; item_index < plan.item_count; ++item_index) {
            const ValueSsaAllocatorPlanItem *plan_item = &plan.items[item_index];
            const ValueSsaAllocatorMoveTransitionStep *trace_step = NULL;

            if (!value_ssa_allocator_move_transition_trace_find_value_step(
                    &trace, plan_item->value_id, VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE, NULL, &trace_step) ||
                !trace_step) {
                fprintf(stderr,
                    "[value-ssa-alloc] FAIL: move-transition expected remove step for plan item ssa.%zu\n",
                    plan_item->value_id);
                ok = 0;
                continue;
            }

            if (trace_step->removal_kind != plan_item->removal_kind ||
                trace_step->move_work_class_before != plan_item->move_work_class ||
                trace_step->move_work_class_after != plan_item->move_work_next_class ||
                trace_step->family_active_member_count_before != plan_item->family_active_member_count ||
                trace_step->family_active_member_count_after != plan_item->family_active_member_count_after ||
                trace_step->family_external_neighbor_count_before != plan_item->family_external_neighbor_count ||
                trace_step->family_external_neighbor_count_after != plan_item->family_external_neighbor_count_after ||
                trace_step->family_external_affinity_weight_sum_before !=
                    plan_item->family_external_affinity_weight_sum ||
                trace_step->family_external_affinity_weight_sum_after !=
                    plan_item->family_external_affinity_weight_sum_after ||
                trace_step->move_work_priority != plan_item->move_work_priority) {
                fprintf(stderr,
                    "[value-ssa-alloc] FAIL: move-transition remove step diverged from plan item for ssa.%zu\n",
                    plan_item->value_id);
                ok = 0;
            }
        }
    }
    ok &= expect_plan_move_work_priority_order("VALUE-SSA-ALLOC-MOVE-TRANSITION", &plan, 1, 0);
    ok &= expect_plan_move_work_priority_order("VALUE-SSA-ALLOC-MOVE-TRANSITION", &plan, 2, 1);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-MOVE-TRANSITION", &plan, 0, 1);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-MOVE-TRANSITION", &plan, 1, 2);

cleanup:
    free(actual_text);
    value_ssa_allocator_move_transition_trace_free(&trace);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_transition_trace_explicit_coalesce_action(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveTransitionTrace trace;
    const ValueSsaAllocatorMoveTransitionStep *coalesce_step = NULL;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_transition_trace_init(&trace);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    coalesce.value_count = 4;
    coalesce.item_count = 1;
    coalesce.items = (ValueSsaAllocatorCoalesceItem *)calloc(1, sizeof(ValueSsaAllocatorCoalesceItem));
    coalesce.value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights || !coalesce.items ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[2] = 1;
    prep.affinity_sums[0] = 3;
    prep.affinity_sums[2] = 3;
    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 20;
    worklist.items[1].value_id = 2;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 18;
    worklist.items[2].value_id = 1;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_ISOLATED;
    worklist.items[2].priority = 10;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_ISOLATED;
    worklist.items[3].priority = 8;
    affinity.weights[0 * 4 + 2] = 3;
    affinity.weights[2 * 4 + 0] = 3;
    coalesce.items[0].lhs_value_id = 0;
    coalesce.items[0].rhs_value_id = 2;
    coalesce.items[0].weight = 3;
    coalesce.items[0].can_coalesce = 1;
    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 1;
    coalesce.value_roots[2] = 2;
    coalesce.value_roots[3] = 3;
    coalesce.class_sizes[0] = 1;
    coalesce.class_sizes[1] = 1;
    coalesce.class_sizes[2] = 1;
    coalesce.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error) ||
        !value_ssa_compute_allocator_move_transition_trace(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &trace, &error) ||
        !value_ssa_dump_allocator_move_transition_trace(&program.functions[0], &trace, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_move_transition("VALUE-SSA-ALLOC-MOVE-TRANSITION-COALESCE",
        &trace,
        0,
        VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE,
        VALUE_SSA_ALLOCATOR_REMOVE_NONE,
        VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        1,
        2,
        1,
        0);
    if (!value_ssa_allocator_move_transition_trace_find_value_step(
            &trace, 0, VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE, NULL, &coalesce_step) ||
        !coalesce_step) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce expected explicit coalesce step\n");
        ok = 0;
        goto cleanup;
    }
    if (coalesce_step->phase != VALUE_SSA_ALLOCATOR_PHASE_FREEZE ||
        coalesce_step->partner_value_id != 2 ||
        coalesce_step->partner_root_value_id != 2 ||
        coalesce_step->partner_move_work_class_before != VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY ||
        coalesce_step->partner_move_work_class_after != VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL ||
        coalesce_step->partner_family_active_member_count_before != 1 ||
        coalesce_step->partner_family_active_member_count_after != 0 ||
        coalesce_step->family_best_coalesce_ready_partner_root_value_id_after != (size_t)-1 ||
        coalesce_step->family_best_coalesce_ready_partner_affinity_weight_after != 0 ||
        coalesce_step->family_best_coalesce_ready_partner_is_mutual_after != 0 ||
        coalesce_step->family_external_neighbor_count_after != 0 ||
        coalesce_step->family_external_affinity_weight_sum_after != 0 ||
        coalesce_step->partner_family_coalesce_ready_neighbor_count_before != 1 ||
        coalesce_step->partner_family_coalesce_ready_neighbor_count_after != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-coalesce explicit pair step missing expected partner transition details\n");
        ok = 0;
    }
    if (trace.step_count < 5 || trace.steps[0].event_kind != VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-coalesce expected coalesce event before removals, got count=%zu first=%s\n",
            trace.step_count,
            trace.step_count > 0
                ? value_ssa_allocator_move_transition_event_kind_name(trace.steps[0].event_kind)
                : "<none>");
        ok = 0;
    }
    if (!actual_text || strstr(actual_text, "event=coalesce phase=freeze ssa.0 root=ssa.0") == NULL ||
        strstr(actual_text, "partner=ssa.2/ssa.2") == NULL ||
        strstr(actual_text, "move_work=coalesce-ready->released") == NULL ||
        strstr(actual_text, "partner_move_work=coalesce-ready->internal") == NULL ||
        strstr(actual_text, "family_members=1->2") == NULL ||
        strstr(actual_text, "partner_family_members=1->0") == NULL ||
        strstr(actual_text, "event=remove phase=simplify ssa.2 root=ssa.2") != NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-coalesce expected dump to use merged representative after coalesce, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }
    ok &= expect_move_transition("VALUE-SSA-ALLOC-MOVE-TRANSITION-COALESCE",
        &trace,
        2,
        VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE,
        VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED,
        VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL,
        1,
        0,
        0,
        0);
    {
        const ValueSsaAllocatorMoveTransitionStep *partner_remove_step = NULL;
        const ValueSsaAllocatorPlanItem *merged_plan_item = NULL;

        if (!value_ssa_allocator_move_transition_trace_find_value_step(
                &trace, 2, VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE, NULL, &partner_remove_step) ||
            !partner_remove_step || partner_remove_step->root_value_id != 0) {
            fprintf(stderr,
                "[value-ssa-alloc] FAIL: move-transition-coalesce expected ssa.2 remove step to run under merged root ssa.0\n");
            ok = 0;
        }
        if (!value_ssa_allocator_plan_find_value(&plan, 2, NULL, &merged_plan_item) || !merged_plan_item ||
            merged_plan_item->coalesce_root_value_id != 0 || merged_plan_item->coalesce_class_size != 2) {
            fprintf(stderr,
                "[value-ssa-alloc] FAIL: move-transition-coalesce expected plan item for ssa.2 to rebuild under merged family root ssa.0/2\n");
            ok = 0;
        }
    }
    ok &= expect_plan_final_runtime_coalesce_class(
        "VALUE-SSA-ALLOC-MOVE-TRANSITION-COALESCE", &plan, 0, 0, 2);
    ok &= expect_plan_final_runtime_coalesce_class(
        "VALUE-SSA-ALLOC-MOVE-TRANSITION-COALESCE", &plan, 2, 0, 2);
    ok &= expect_plan_final_runtime_coalesce_class(
        "VALUE-SSA-ALLOC-MOVE-TRANSITION-COALESCE", &plan, 1, 1, 1);
    ok &= expect_plan_final_runtime_coalesce_class(
        "VALUE-SSA-ALLOC-MOVE-TRANSITION-COALESCE", &plan, 3, 3, 1);

cleanup:
    free(actual_text);
    value_ssa_allocator_move_transition_trace_free(&trace);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_transition_trace_defers_weaker_coalesce_to_stronger_simplify(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveTransitionTrace trace;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_transition_trace_init(&trace);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce-vs-simplify setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    coalesce.value_count = 4;
    coalesce.item_count = 1;
    coalesce.items = (ValueSsaAllocatorCoalesceItem *)calloc(1, sizeof(ValueSsaAllocatorCoalesceItem));
    coalesce.value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights || !coalesce.items ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce-vs-simplify manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[2] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[2] = 1;
    worklist.items[0].value_id = 1;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_ISOLATED;
    worklist.items[0].priority = 120;
    worklist.items[1].value_id = 0;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 9;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_ISOLATED;
    worklist.items[3].priority = 8;
    affinity.weights[0 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 0] = 1;
    coalesce.items[0].lhs_value_id = 0;
    coalesce.items[0].rhs_value_id = 2;
    coalesce.items[0].weight = 1;
    coalesce.items[0].can_coalesce = 1;
    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 1;
    coalesce.value_roots[2] = 2;
    coalesce.value_roots[3] = 3;
    coalesce.class_sizes[0] = 1;
    coalesce.class_sizes[1] = 1;
    coalesce.class_sizes[2] = 1;
    coalesce.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error) ||
        !value_ssa_compute_allocator_move_transition_trace(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &trace, &error) ||
        !value_ssa_dump_allocator_move_transition_trace(&program.functions[0], &trace, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-coalesce-vs-simplify build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (trace.step_count == 0 || trace.steps[0].event_kind != VALUE_SSA_ALLOC_MOVE_EVENT_REMOVE ||
        trace.steps[0].value_id != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-coalesce-vs-simplify expected first step to be simplify remove of ssa.1, got count=%zu first=%s ssa.%zu\n",
            trace.step_count,
            trace.step_count > 0 ? value_ssa_allocator_move_transition_event_kind_name(trace.steps[0].event_kind) : "<none>",
            trace.step_count > 0 ? trace.steps[0].value_id : 0);
        ok = 0;
    }
    if (!actual_text || strstr(actual_text, "0: event=remove phase=simplify ssa.1 root=ssa.1") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-coalesce-vs-simplify expected simplify-first dump, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_move_transition_trace_free(&trace);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_move_transition_trace_prefers_stronger_freeze_coalesce_pair(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocatorMoveTransitionTrace trace;
    const ValueSsaAllocatorMoveTransitionStep *coalesce_step = NULL;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocator_move_transition_trace_init(&trace);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-freeze-coalesce-priority setup failed: %s\n", error.message);
        return 0;
    }

    prep.value_count = 4;
    prep.use_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(4, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(4, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(4, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(4, sizeof(unsigned char));
    worklist.value_count = 4;
    worklist.item_count = 4;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(4, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 4;
    interference.interferes = (unsigned char *)calloc(16, sizeof(unsigned char));
    affinity.value_count = 4;
    affinity.weights = (size_t *)calloc(16, sizeof(size_t));
    coalesce.value_count = 4;
    coalesce.item_count = 2;
    coalesce.items = (ValueSsaAllocatorCoalesceItem *)calloc(2, sizeof(ValueSsaAllocatorCoalesceItem));
    coalesce.value_roots = (size_t *)calloc(4, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(4, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights || !coalesce.items ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-freeze-coalesce-priority manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.move_related[0] = 1;
    prep.move_related[1] = 1;
    prep.move_related[2] = 1;
    prep.move_related[3] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 6;
    prep.affinity_sums[2] = 1;
    prep.affinity_sums[3] = 6;
    prep.interference_degrees[0] = 0;
    prep.interference_degrees[1] = 2;
    prep.interference_degrees[2] = 0;
    prep.interference_degrees[3] = 2;
    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[0].priority = 20;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[1].priority = 40;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[2].priority = 18;
    worklist.items[3].value_id = 3;
    worklist.items[3].work_class = VALUE_SSA_ALLOC_WORK_MOVE_HINT;
    worklist.items[3].priority = 38;
    affinity.weights[0 * 4 + 2] = 1;
    affinity.weights[2 * 4 + 0] = 1;
    affinity.weights[1 * 4 + 3] = 6;
    affinity.weights[3 * 4 + 1] = 6;
    coalesce.items[0].lhs_value_id = 0;
    coalesce.items[0].rhs_value_id = 2;
    coalesce.items[0].weight = 1;
    coalesce.items[0].can_coalesce = 1;
    coalesce.items[1].lhs_value_id = 1;
    coalesce.items[1].rhs_value_id = 3;
    coalesce.items[1].weight = 6;
    coalesce.items[1].can_coalesce = 1;
    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 1;
    coalesce.value_roots[2] = 2;
    coalesce.value_roots[3] = 3;
    coalesce.class_sizes[0] = 1;
    coalesce.class_sizes[1] = 1;
    coalesce.class_sizes[2] = 1;
    coalesce.class_sizes[3] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error) ||
        !value_ssa_compute_allocator_move_transition_trace(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &trace, &error) ||
        !value_ssa_dump_allocator_move_transition_trace(&program.functions[0], &trace, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-freeze-coalesce-priority build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_allocator_move_transition_trace_find_value_step(
            &trace, 1, VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE, NULL, &coalesce_step) ||
        !coalesce_step) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: move-transition-freeze-coalesce-priority expected coalesce step for ssa.1\n");
        ok = 0;
        goto cleanup;
    }
    if (trace.step_count == 0 || trace.steps[0].event_kind != VALUE_SSA_ALLOC_MOVE_EVENT_COALESCE ||
        trace.steps[0].value_id != 1 || coalesce_step->phase != VALUE_SSA_ALLOCATOR_PHASE_FREEZE) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-freeze-coalesce-priority expected first coalesce to be freeze-phase pair rooted at ssa.1, got count=%zu first=%s ssa.%zu phase=%s\n",
            trace.step_count,
            trace.step_count > 0 ? value_ssa_allocator_move_transition_event_kind_name(trace.steps[0].event_kind) : "<none>",
            trace.step_count > 0 ? trace.steps[0].value_id : 0,
            coalesce_step ? value_ssa_allocator_phase_name(coalesce_step->phase) : "<none>");
        ok = 0;
    }
    if (!actual_text || strstr(actual_text, "0: event=coalesce phase=freeze ssa.1 root=ssa.1") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: move-transition-freeze-coalesce-priority expected freeze-phase coalesce first, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_move_transition_trace_free(&trace);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_loop(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorPlan plan;
    size_t invariant_value;
    size_t init_value;
    size_t phi_value;
    size_t step_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_loop_plan_program(
            &program, &invariant_value, &init_value, &phi_value, &step_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-loop setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_plan(&program.functions[0], NULL, NULL, NULL, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-loop build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-LOOP", &plan, invariant_value, VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-LOOP", &plan, phi_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-LOOP", &plan, init_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY);
    ok &= expect_plan_removal_kind(
        "VALUE-SSA-ALLOC-PLAN-LOOP", &plan, invariant_value, VALUE_SSA_ALLOCATOR_REMOVE_SPILL);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_confirms_blocked_spill_remove(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorPlan plan;
    ValueSsaAllocationResult result;
    size_t invariant_value;
    size_t init_value;
    size_t phi_value;
    size_t step_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_plan_init(&plan);
    value_ssa_allocation_result_init(&result);
    if (!build_alloc_loop_plan_program(
            &program, &invariant_value, &init_value, &phi_value, &step_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: confirmed-spill setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_plan(&program.functions[0], NULL, NULL, NULL, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: confirmed-spill plan failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (!value_ssa_allocate_function(&program.functions[0], 1, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: confirmed-spill allocate failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_removal_kind(
        "VALUE-SSA-ALLOC-CONFIRMED-SPILL", &plan, invariant_value, VALUE_SSA_ALLOCATOR_REMOVE_SPILL);
    ok &= expect_has_color("VALUE-SSA-ALLOC-CONFIRMED-SPILL", &result, invariant_value, 0);
    ok &= expect_query_spill("VALUE-SSA-ALLOC-CONFIRMED-SPILL", &result, invariant_value, 1);
    ok &= expect_query_spill_intent("VALUE-SSA-ALLOC-CONFIRMED-SPILL", &result, invariant_value, 1, 1);

cleanup:
    value_ssa_allocation_result_free(&result);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_drops_move_related_when_affinity_is_removed(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaAllocatorPlan plan;
    size_t source_value;
    size_t copy_value;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_allocator_plan_init(&plan);

    if (!build_alloc_affinity_downgrade_program(&program, &source_value, &copy_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-affinity-drop setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, NULL, NULL, NULL, 2, &plan, &error) ||
        !value_ssa_dump_allocator_plan(&program.functions[0], &prep, &plan, 2, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-affinity-drop build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (strstr(actual_text, "degree=1->0") == NULL ||
        strstr(actual_text, "remove=simplify-remove") == NULL ||
        strstr(actual_text, "move_related=yes->no") == NULL ||
        !expect_plan_phase("VALUE-SSA-ALLOC-PLAN-AFFINITY-DROP", &plan, source_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY) ||
        !expect_plan_phase("VALUE-SSA-ALLOC-PLAN-AFFINITY-DROP", &plan, copy_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY) ||
        !expect_plan_removal_kind(
            "VALUE-SSA-ALLOC-PLAN-AFFINITY-DROP", &plan, copy_value, VALUE_SSA_ALLOCATOR_REMOVE_SIMPLIFY)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-affinity-drop expected move-hint family to fully thaw into simplify, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
            ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_preserves_move_related_when_affinity_remains(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaAllocatorPlan plan;
    size_t root_value;
    size_t mid_value;
    size_t leaf_value;
    char *actual_text = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_allocator_plan_init(&plan);

    if (!build_alloc_move_related_drop_program(&program, &root_value, &mid_value, &leaf_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-move-keep setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[0], NULL, NULL, NULL, NULL, &prep, &error) ||
        !value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error) ||
        !value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, NULL, NULL, NULL, 2, &plan, &error) ||
        !value_ssa_dump_allocator_plan(&program.functions[0], &prep, &plan, 2, &actual_text, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-move-keep build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (strstr(actual_text, "move_related=yes->no") == NULL ||
        !expect_plan_phase("VALUE-SSA-ALLOC-PLAN-MOVE-KEEP", &plan, root_value, VALUE_SSA_ALLOCATOR_PHASE_SIMPLIFY) ||
        !expect_plan_phase("VALUE-SSA-ALLOC-PLAN-MOVE-KEEP", &plan, mid_value, VALUE_SSA_ALLOCATOR_PHASE_SPILL) ||
        !expect_plan_removal_kind(
            "VALUE-SSA-ALLOC-PLAN-MOVE-KEEP", &plan, mid_value, VALUE_SSA_ALLOCATOR_REMOVE_SPILL)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-move-keep expected frozen move hints to eventually clear into simplify, got:\n%s\n",
            actual_text ? actual_text : "<no dump>");
        ok = 0;
    }

cleanup:
    free(actual_text);
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_cost_components(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorPlan plan;
    size_t low_cost_value;
    size_t high_cost_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_spill_cost_order_program(&program, &low_cost_value, &high_cost_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-cost-components setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_plan(&program.functions[0], NULL, NULL, NULL, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-cost-components build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-COST-COMPONENTS",
        &plan,
        low_cost_value,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-COST-COMPONENTS",
        &plan,
        high_cost_value,
        2,
        1,
        0);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocatorPlan plan;
    size_t low_cost_value;
    size_t high_cost_value;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_spill_cost_order_program(&program, &low_cost_value, &high_cost_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-cost-order setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocator_plan(&program.functions[0], NULL, NULL, NULL, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-cost-order build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-COST-ORDER",
        &plan,
        low_cost_value,
        VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_removal_kind("VALUE-SSA-ALLOC-PLAN-SPILL-COST-ORDER",
        &plan,
        low_cost_value,
        VALUE_SSA_ALLOCATOR_REMOVE_SPILL);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-COST-ORDER",
        &plan,
        low_cost_value,
        high_cost_value);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_loop_weight(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-loop-weight setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.interference_degrees ||
        !prep.affinity_sums || !prep.move_related || !worklist.items || !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-loop-weight manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.loop_depth_sums[0] = 0;
    prep.loop_depth_sums[1] = 3;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-loop-weight build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-LOOP-WEIGHT",
        &plan,
        0,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-LOOP-WEIGHT",
        &plan,
        1,
        1,
        4,
        0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-LOOP-WEIGHT", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocation_prep_tracks_call_density(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    size_t call_live_value = (size_t)-1;
    size_t post_call_value = (size_t)-1;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    if (!build_alloc_call_density_prep_program(&program, &call_live_value, &post_call_value, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-prep-call-density setup failed: %s\n", error.message);
        return 0;
    }

    if (!value_ssa_compute_allocation_prep(&program.functions[1], NULL, NULL, NULL, NULL, &prep, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-prep-call-density build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }
    if (!prep.call_density_sums) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-prep-call-density expected call-density summary\n");
        ok = 0;
        goto cleanup;
    }
    if (!prep.call_crossing_counts) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-prep-call-density expected call-crossing summary\n");
        ok = 0;
        goto cleanup;
    }
    if (prep.call_density_sums[call_live_value] != 1 || prep.call_density_sums[post_call_value] != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-prep-call-density expected call sums (ssa.%zu=1, ssa.%zu=0), got (%zu,%zu)\n",
            call_live_value,
            post_call_value,
            prep.call_density_sums[call_live_value],
            prep.call_density_sums[post_call_value]);
        ok = 0;
    }
    if (prep.call_crossing_counts[call_live_value] != 1 || prep.call_crossing_counts[post_call_value] != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-prep-call-density expected call crossings (ssa.%zu=1, ssa.%zu=0), got (%zu,%zu)\n",
            call_live_value,
            post_call_value,
            prep.call_crossing_counts[call_live_value],
            prep.call_crossing_counts[post_call_value]);
        ok = 0;
    }

cleanup:
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_call_density(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-call-density setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.call_density_sums ||
        !prep.interference_degrees || !prep.affinity_sums || !prep.move_related || !worklist.items ||
        !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-call-density manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.call_density_sums[0] = 0;
    prep.call_density_sums[1] = 3;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-call-density build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-DENSITY",
        &plan,
        0,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-DENSITY",
        &plan,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-DENSITY",
        &plan,
        1,
        1,
        4,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-DENSITY",
        &plan,
        1,
        0,
        0,
        1,
        0,
        3,
        0,
        0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-DENSITY", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_single_block_on_cost_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-single-block setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.single_block_live_ranges = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.call_density_sums ||
        !prep.single_block_live_ranges || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-single-block manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.single_block_live_ranges[0] = 1;
    prep.single_block_live_ranges[1] = 0;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-single-block build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-SINGLE-BLOCK",
        &plan,
        0,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-SINGLE-BLOCK",
        &plan,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-SINGLE-BLOCK",
        &plan,
        1,
        1,
        2,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-SINGLE-BLOCK",
        &plan,
        1,
        0,
        0,
        1,
        0,
        0,
        0,
        1);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-SINGLE-BLOCK", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_call_crossing(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-call-crossing setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_crossing_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.single_block_live_ranges = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.call_density_sums ||
        !prep.call_crossing_counts || !prep.single_block_live_ranges || !prep.interference_degrees ||
        !prep.affinity_sums || !prep.move_related || !worklist.items || !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-call-crossing manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.call_crossing_counts[0] = 0;
    prep.call_crossing_counts[1] = 2;
    prep.single_block_live_ranges[0] = 1;
    prep.single_block_live_ranges[1] = 1;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-call-crossing build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-CROSSING",
        &plan,
        0,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-CROSSING",
        &plan,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-CROSSING",
        &plan,
        1,
        1,
        3,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-CROSSING",
        &plan,
        1,
        0,
        0,
        1,
        0,
        0,
        2,
        0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-CALL-CROSSING", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_use_loop_depth(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-use-loop setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.use_loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_crossing_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.single_block_live_ranges = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.use_loop_depth_sums || !prep.live_block_counts || !prep.loop_depth_sums ||
        !prep.call_density_sums || !prep.call_crossing_counts || !prep.single_block_live_ranges ||
        !prep.interference_degrees || !prep.affinity_sums || !prep.move_related || !worklist.items ||
        !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-use-loop manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.use_loop_depth_sums[0] = 0;
    prep.use_loop_depth_sums[1] = 2;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.single_block_live_ranges[0] = 1;
    prep.single_block_live_ranges[1] = 1;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-use-loop build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-USE-LOOP",
        &plan,
        0,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-USE-LOOP",
        &plan,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-USE-LOOP",
        &plan,
        1,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-USE-LOOP",
        &plan,
        1,
        2,
        0,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-USE-LOOP", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_use_call_density(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-use-call setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.use_loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.use_call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_crossing_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.single_block_live_ranges = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.use_loop_depth_sums || !prep.use_call_density_sums || !prep.live_block_counts ||
        !prep.loop_depth_sums || !prep.call_density_sums || !prep.call_crossing_counts ||
        !prep.single_block_live_ranges || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-use-call manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.use_call_density_sums[0] = 0;
    prep.use_call_density_sums[1] = 2;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.single_block_live_ranges[0] = 1;
    prep.single_block_live_ranges[1] = 1;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-use-call build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-USE-CALL",
        &plan,
        0,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-USE-CALL",
        &plan,
        0,
        0,
        0,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_spill_cost_components("VALUE-SSA-ALLOC-PLAN-SPILL-USE-CALL",
        &plan,
        1,
        1,
        1,
        0);
    ok &= expect_plan_spill_cost_detail("VALUE-SSA-ALLOC-PLAN-SPILL-USE-CALL",
        &plan,
        1,
        0,
        2,
        1,
        0,
        0,
        0,
        0);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-USE-CALL", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocation_worklist_priority_tracks_new_spill_signals(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: worklist-priority-new-signals setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.call_density_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.single_block_live_ranges = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.call_density_sums ||
        !prep.single_block_live_ranges || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: worklist-priority-new-signals manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.call_density_sums[0] = 0;
    prep.call_density_sums[1] = 2;
    prep.call_crossing_counts = (size_t *)calloc(2, sizeof(size_t));
    if (!prep.call_crossing_counts) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: worklist-priority-new-signals call-crossing allocation failed\n");
        ok = 0;
        goto cleanup;
    }
    prep.call_crossing_counts[0] = 0;
    prep.call_crossing_counts[1] = 1;
    prep.single_block_live_ranges[0] = 1;
    prep.single_block_live_ranges[1] = 0;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 1;
    prep.move_related[0] = 1;
    prep.move_related[1] = 1;

    if (!value_ssa_compute_allocation_worklist(&program.functions[0], &prep, &worklist, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: worklist-priority-new-signals build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_worklist_priority("VALUE-SSA-ALLOC-WORKLIST-NEW-SIGNALS", &worklist, 0, 15);
    ok &= expect_worklist_priority("VALUE-SSA-ALLOC-WORKLIST-NEW-SIGNALS", &worklist, 1, 19);
    ok &= expect_worklist_index_less_than("VALUE-SSA-ALLOC-WORKLIST-NEW-SIGNALS", &worklist, 1, 0);

cleanup:
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_rematerializable_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-remat setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.rematerializable = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.interference_degrees ||
        !prep.affinity_sums || !prep.move_related || !prep.rematerializable || !worklist.items ||
        !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-remat manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;
    prep.rematerializable[0] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-remat build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-REMAT", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-REMAT", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_split_child_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-split-child setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.rematerializable = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.split_child_values = (unsigned char *)calloc(2, sizeof(unsigned char));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.interference_degrees ||
        !prep.affinity_sums || !prep.move_related || !prep.rematerializable || !prep.split_child_values ||
        !worklist.items || !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-split-child manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;
    prep.split_child_values[0] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-split-child build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-SPLIT-CHILD", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-SPLIT-CHILD", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_deeper_split_child_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_manual_spill_cost_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-split-depth setup failed: %s\n",
            error.message);
        return 0;
    }

    prep.value_count = 2;
    prep.use_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(2, sizeof(size_t));
    prep.loop_depth_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(2, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(2, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.rematerializable = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.split_child_values = (unsigned char *)calloc(2, sizeof(unsigned char));
    prep.split_child_depths = (size_t *)calloc(2, sizeof(size_t));
    worklist.value_count = 2;
    worklist.item_count = 2;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(2, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 2;
    interference.interferes = (unsigned char *)calloc(4, sizeof(unsigned char));
    if (!prep.use_counts || !prep.live_block_counts || !prep.loop_depth_sums || !prep.interference_degrees ||
        !prep.affinity_sums || !prep.move_related || !prep.rematerializable || !prep.split_child_values ||
        !prep.split_child_depths || !worklist.items || !interference.interferes) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-split-depth manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.interference_degrees[0] = 1;
    prep.interference_degrees[1] = 1;
    prep.split_child_values[0] = 1;
    prep.split_child_values[1] = 1;
    prep.split_child_depths[0] = 2;
    prep.split_child_depths[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;

    interference.interferes[0 * 2 + 1] = 1;
    interference.interferes[1 * 2 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(&program.functions[0], &prep, &worklist, &interference, NULL, NULL, 1, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-split-depth build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-SPLIT-DEPTH", &plan, 0, VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-SPLIT-DEPTH", &plan, 0, 1);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_internal_over_move_family_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-move-tie setup failed: %s\n", error.message);
        return 0;
    }
    program.functions[0].next_value_id = 3;

    prep.value_count = 3;
    prep.use_counts = (size_t *)calloc(3, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(3, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(3, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(3, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(3, sizeof(unsigned char));
    worklist.value_count = 3;
    worklist.item_count = 3;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(3, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 3;
    interference.interferes = (unsigned char *)calloc(9, sizeof(unsigned char));
    affinity.value_count = 3;
    affinity.weights = (size_t *)calloc(9, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes || !affinity.weights) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-move-tie manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.use_counts[2] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.live_block_counts[2] = 1;
    prep.interference_degrees[0] = 2;
    prep.interference_degrees[1] = 2;
    prep.interference_degrees[2] = 2;
    prep.affinity_sums[0] = 1;
    prep.affinity_sums[1] = 1;
    prep.affinity_sums[2] = 0;
    prep.move_related[0] = 1;
    prep.move_related[1] = 1;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[2].priority = 10;

    interference.interferes[0 * 3 + 1] = 1;
    interference.interferes[0 * 3 + 2] = 1;
    interference.interferes[1 * 3 + 0] = 1;
    interference.interferes[1 * 3 + 2] = 1;
    interference.interferes[2 * 3 + 0] = 1;
    interference.interferes[2 * 3 + 1] = 1;

    affinity.weights[0 * 3 + 1] = 1;
    affinity.weights[1 * 3 + 0] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, NULL, 2, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-move-tie build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    if (plan.item_count == 0) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-move-tie expected non-empty plan\n");
        ok = 0;
        goto cleanup;
    }
    if (plan.items[0].value_id != 2 || plan.items[0].phase != VALUE_SSA_ALLOCATOR_PHASE_SPILL ||
        plan.items[0].removal_kind != VALUE_SSA_ALLOCATOR_REMOVE_SPILL ||
        plan.items[0].move_work_class != VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-move-tie expected first remove to be internal spill ssa.2, got ssa.%zu phase=%s remove=%s move_work=%s\n",
            plan.items[0].value_id,
            value_ssa_allocator_phase_name(plan.items[0].phase),
            value_ssa_allocator_removal_kind_name(plan.items[0].removal_kind),
            value_ssa_allocator_move_work_class_name(plan.items[0].move_work_class));
        ok = 0;
    }
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-MOVE-TIE", &plan, 2, 0);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_smaller_coalesce_class_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-family-tie setup failed: %s\n", error.message);
        return 0;
    }
    program.functions[0].next_value_id = 3;

    prep.value_count = 3;
    prep.use_counts = (size_t *)calloc(3, sizeof(size_t));
    prep.live_block_counts = (size_t *)calloc(3, sizeof(size_t));
    prep.interference_degrees = (size_t *)calloc(3, sizeof(size_t));
    prep.affinity_sums = (size_t *)calloc(3, sizeof(size_t));
    prep.move_related = (unsigned char *)calloc(3, sizeof(unsigned char));
    worklist.value_count = 3;
    worklist.item_count = 3;
    worklist.items = (ValueSsaAllocationWorkItem *)calloc(3, sizeof(ValueSsaAllocationWorkItem));
    interference.value_count = 3;
    interference.interferes = (unsigned char *)calloc(9, sizeof(unsigned char));
    coalesce.value_count = 3;
    coalesce.item_count = 0;
    coalesce.items = NULL;
    coalesce.value_roots = (size_t *)calloc(3, sizeof(size_t));
    coalesce.class_sizes = (size_t *)calloc(3, sizeof(size_t));
    if (!prep.use_counts || !prep.live_block_counts || !prep.interference_degrees || !prep.affinity_sums ||
        !prep.move_related || !worklist.items || !interference.interferes ||
        !coalesce.value_roots || !coalesce.class_sizes) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-family-tie manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    prep.use_counts[0] = 1;
    prep.use_counts[1] = 1;
    prep.use_counts[2] = 1;
    prep.live_block_counts[0] = 1;
    prep.live_block_counts[1] = 1;
    prep.live_block_counts[2] = 1;
    prep.interference_degrees[0] = 2;
    prep.interference_degrees[1] = 2;
    prep.interference_degrees[2] = 2;

    worklist.items[0].value_id = 0;
    worklist.items[0].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[0].priority = 10;
    worklist.items[1].value_id = 1;
    worklist.items[1].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[1].priority = 10;
    worklist.items[2].value_id = 2;
    worklist.items[2].work_class = VALUE_SSA_ALLOC_WORK_CONSTRAINED;
    worklist.items[2].priority = 10;

    interference.interferes[0 * 3 + 1] = 1;
    interference.interferes[0 * 3 + 2] = 1;
    interference.interferes[1 * 3 + 0] = 1;
    interference.interferes[1 * 3 + 2] = 1;
    interference.interferes[2 * 3 + 0] = 1;
    interference.interferes[2 * 3 + 1] = 1;

    coalesce.value_roots[0] = 0;
    coalesce.value_roots[1] = 0;
    coalesce.value_roots[2] = 2;
    coalesce.class_sizes[0] = 2;
    coalesce.class_sizes[2] = 1;

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, NULL, &coalesce, 1, &plan, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocator-plan-spill-family-tie build failed: %s\n", error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-TIE", &plan, 2, VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_removal_kind("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-TIE",
        &plan,
        2,
        VALUE_SSA_ALLOCATOR_REMOVE_SPILL);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-TIE", &plan, 2, 0);

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_lower_family_affinity_pressure_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    const ValueSsaAllocatorPlanItem *light_item = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-affinity setup failed: %s\n",
            error.message);
        return 0;
    }
    program.functions[0].next_value_id = 6;

    if (!setup_manual_spill_family_pressure_artifacts(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 0)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-affinity manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-affinity build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-AFFINITY",
        &plan,
        0,
        VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_move_work_class("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-AFFINITY",
        &plan,
        0,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-AFFINITY", &plan, 0, 2);
    if (!value_ssa_allocator_plan_find_value(&plan, 0, NULL, &light_item) || !light_item) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-affinity plan lookup failed\n");
        ok = 0;
        goto cleanup;
    }
    if (light_item->family_external_neighbor_count != 1 || light_item->family_external_affinity_weight_sum != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-affinity expected first spill-family pressure (ssa.0 neighbors=1 affinity=1), got (ssa.0 neighbors=%zu affinity=%zu)\n",
            light_item->family_external_neighbor_count,
            light_item->family_external_affinity_weight_sum);
        ok = 0;
    }

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocator_plan_spill_phase_prefers_fewer_family_neighbors_on_tie(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationPrep prep;
    ValueSsaAllocationWorklist worklist;
    ValueSsaInterferenceGraph interference;
    ValueSsaCopyAffinityGraph affinity;
    ValueSsaAllocatorCoalesceAnalysis coalesce;
    ValueSsaAllocatorPlan plan;
    const ValueSsaAllocatorPlanItem *narrow_item = NULL;
    int ok = 1;

    value_ssa_program_init(&program);
    value_ssa_allocation_prep_init(&prep);
    value_ssa_allocation_worklist_init(&worklist);
    value_ssa_interference_graph_init(&interference);
    value_ssa_copy_affinity_graph_init(&affinity);
    value_ssa_allocator_coalesce_analysis_init(&coalesce);
    value_ssa_allocator_plan_init(&plan);
    if (!build_alloc_coalesce_budget_blocked_program(&program, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-neighbors setup failed: %s\n",
            error.message);
        return 0;
    }
    program.functions[0].next_value_id = 6;

    if (!setup_manual_spill_family_pressure_artifacts(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 1)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-neighbors manual facts allocation failed\n");
        ok = 0;
        goto cleanup;
    }

    if (!value_ssa_compute_allocator_plan(
            &program.functions[0], &prep, &worklist, &interference, &affinity, &coalesce, 2, &plan, &error)) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-neighbors build failed: %s\n",
            error.message);
        ok = 0;
        goto cleanup;
    }

    ok &= expect_plan_phase("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-NEIGHBORS",
        &plan,
        0,
        VALUE_SSA_ALLOCATOR_PHASE_SPILL);
    ok &= expect_plan_move_work_class("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-NEIGHBORS",
        &plan,
        0,
        VALUE_SSA_ALLOC_MOVE_WORK_RELEASED);
    ok &= expect_plan_index_less_than("VALUE-SSA-ALLOC-PLAN-SPILL-FAMILY-NEIGHBORS", &plan, 0, 2);
    if (!value_ssa_allocator_plan_find_value(&plan, 0, NULL, &narrow_item) || !narrow_item) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-neighbors plan lookup failed\n");
        ok = 0;
        goto cleanup;
    }
    if (narrow_item->family_external_neighbor_count != 1 ||
        narrow_item->family_external_affinity_weight_sum != 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocator-plan-spill-family-neighbors expected first spill-family pressure (ssa.0 neighbors=1 affinity=2), got (ssa.0 neighbors=%zu affinity=%zu)\n",
            narrow_item->family_external_neighbor_count,
            narrow_item->family_external_affinity_weight_sum);
        ok = 0;
    }

cleanup:
    value_ssa_allocator_plan_free(&plan);
    value_ssa_allocator_coalesce_analysis_free(&coalesce);
    value_ssa_copy_affinity_graph_free(&affinity);
    value_ssa_interference_graph_free(&interference);
    value_ssa_allocation_worklist_free(&worklist);
    value_ssa_allocation_prep_free(&prep);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_function_queries(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocationResult result;
    int ok;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: query setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (!value_ssa_allocate_function(&program.functions[0], 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: query allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = 1;
    ok &= expect_query_spill("VALUE-SSA-ALLOC-QUERY", &result, 3, 0);
    ok &= expect_query_color("VALUE-SSA-ALLOC-QUERY", &result, 4, 1, result.colors[4]);
    ok &= expect_query_priority("VALUE-SSA-ALLOC-QUERY", &result, 4, 3);
    ok &= expect_query_spill_slot("VALUE-SSA-ALLOC-QUERY", &result, 4, 0, 0);

    value_ssa_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_program_smoke(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    int ok;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_program(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = 1;
    if (result.function_count != 1) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: expected function_count 1, got %zu\n", result.function_count);
        ok = 0;
    } else {
        ok &= expect_has_color("VALUE-SSA-ALLOC-PROGRAM", &result.function_results[0], 4, 1);
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_program_matches_direct_function_allocation(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult program_result;
    ValueSsaAllocationResult direct_result;
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-match setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&program_result);
    value_ssa_allocation_result_init(&direct_result);
    if (!value_ssa_allocate_program(&program, 2, &program_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-match program allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }
    if (!value_ssa_allocate_function(&program.functions[0], 2, &direct_result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-match direct allocate failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&program_result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (program_result.function_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: program-match expected one function result, got %zu\n",
            program_result.function_count);
        ok = 0;
    } else {
        ok &= expect_alloc_results_equal_by_dump(
            "VALUE-SSA-ALLOC-PROGRAM-MATCH",
            &program.functions[0],
            &program_result.function_results[0],
            &direct_result);
    }

    value_ssa_allocation_result_free(&direct_result);
    value_ssa_program_allocation_result_free(&program_result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_program_dump(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    int ok;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program dump setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_program(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program dump allocate failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_program_alloc_dump("VALUE-SSA-ALLOC-PROGRAM-DUMP",
        &program,
        &result,
        "alloc func main colors=2 values=5\n"
        "ssa.0 color=0 priority=3\n"
        "ssa.1 color=0 priority=3\n"
        "ssa.2 color=0 priority=3\n"
        "ssa.3 color=0 priority=11\n"
        "ssa.4 color=0 priority=11\n");

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_program_allocation_layout_dump(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    const ValueSsaFunctionAllocationLayout *function_layout = NULL;
    const size_t *functions_with_colors = NULL;
    const size_t *functions_with_spills = NULL;
    size_t functions_with_colors_count = 0;
    size_t functions_with_spills_count = 0;
    const size_t expected_functions_with_colors[] = {0};
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    if (!value_ssa_allocate_program(&program, 2, &result, &error) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout build failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_layout_summary("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-DUMP",
        &layout,
        1,
        5,
        5,
        0,
        0,
        0,
        0,
        2,
        0);
    if (!value_ssa_program_allocation_layout_get_function_layout(&layout, 0, &function_layout) || !function_layout) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout expected main function layout\n");
        ok = 0;
    } else if (!expect_function_layout_summary("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-DUMP-FUNCTION",
                   function_layout,
                   NULL,
                   5,
                   2,
                   0,
                   5,
                   0,
                   0,
                   0,
                   0,
                   1,
                   1,
                   0)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout expected main layout with five colored values\n");
        ok = 0;
    }
    if (!value_ssa_program_allocation_layout_get_functions_with_colors(
            &layout, &functions_with_colors_count, &functions_with_colors) ||
        !value_ssa_program_allocation_layout_get_functions_with_spills(
            &layout, &functions_with_spills_count, &functions_with_spills)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout grouped queries failed\n");
        ok = 0;
    } else {
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-DUMP",
            functions_with_colors,
            functions_with_colors_count,
            expected_functions_with_colors,
            sizeof(expected_functions_with_colors) / sizeof(expected_functions_with_colors[0]),
            "functions-with-colors");
        ok &= expect_program_function_index_list(
            "VALUE-SSA-ALLOC-PROGRAM-LAYOUT-DUMP", functions_with_spills, functions_with_spills_count, NULL, 0, "functions-with-spills");
    }

    ok &= expect_program_layout_dump("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-DUMP",
        &program,
        &layout,
        "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=2 max_spill_slots=0\n"
        "functions-with-colors: 0\n"
        "functions-with-spills:\n"
        "functions-with-confirmed-spills:\n"
        "functions-with-optimistic-colors:\n"
        "functions-with-recovered-colors:\n"
        "\n"
        "alloc-layout func <anon> colors=2 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_program_allocation_layout_attach_program_context(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-attach setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    if (!value_ssa_allocate_program(&program, 2, &result, &error) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-attach build failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_program_layout_dump("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-ATTACH-BEFORE",
        &program,
        &layout,
        "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=2 max_spill_slots=0\n"
        "functions-with-colors: 0\n"
        "functions-with-spills:\n"
        "functions-with-confirmed-spills:\n"
        "functions-with-optimistic-colors:\n"
        "functions-with-recovered-colors:\n"
        "\n"
        "alloc-layout func <anon> colors=2 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    if (!value_ssa_program_allocation_layout_attach_program_context(&layout, &program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-attach attach failed: %s\n", error.message);
        ok = 0;
    } else {
        const char *function_name = NULL;
        size_t function_index = 0;

        if (!value_ssa_program_allocation_layout_get_function_name(&layout, 0, &function_name) || !function_name ||
            strcmp(function_name, "main") != 0) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-attach expected function name main\n");
            ok = 0;
        }
        if (!value_ssa_program_allocation_layout_find_function_index_by_name(&layout, "main", &function_index) ||
            function_index != 0) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-attach expected to find main at index 0\n");
            ok = 0;
        }
        ok &= expect_program_layout_dump("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-ATTACH-AFTER",
            &program,
            &layout,
            "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=2 max_spill_slots=0\n"
            "functions-with-colors: 0\n"
            "functions-with-spills:\n"
            "functions-with-confirmed-spills:\n"
            "functions-with-optimistic-colors:\n"
            "functions-with-recovered-colors:\n"
            "\n"
            "alloc-layout func main colors=2 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
            "used-colors: 0\n"
            "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
            "spilled-values:\n"
            "optimistic-colored-values:\n"
            "confirmed-spilled-values:\n"
            "recovered-colored-values:\n"
            "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
            "ssa.0 color=0\n"
            "ssa.1 color=0\n"
            "ssa.2 color=0\n"
            "ssa.3 color=0\n"
            "ssa.4 color=0\n");
    }

    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_program_layout_matches_result_projection(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout direct_layout;
    ValueSsaProgramAllocationLayout projected_layout;
    const ValueSsaFunctionAllocationLayout *direct_function_layout = NULL;
    const ValueSsaFunctionAllocationLayout *projected_function_layout = NULL;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-entry setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&direct_layout);
    value_ssa_program_allocation_layout_init(&projected_layout);
    if (!value_ssa_allocate_program_layout(&program, 2, &direct_layout, &error) ||
        !value_ssa_allocate_program(&program, 2, &result, &error) ||
        !value_ssa_build_program_allocation_layout(&result, &projected_layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-entry build failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&projected_layout);
        value_ssa_program_allocation_layout_free(&direct_layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_program_layouts_equivalent_ignoring_names(
        "VALUE-SSA-ALLOC-PROGRAM-LAYOUT-ENTRY", &direct_layout, &projected_layout);
    if (!value_ssa_program_allocation_layout_get_function_layout(&direct_layout, 0, &direct_function_layout) ||
        !value_ssa_program_allocation_layout_get_function_layout(
            &projected_layout, 0, &projected_function_layout)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: VALUE-SSA-ALLOC-PROGRAM-LAYOUT-ENTRY-FUNCTION lookup failed\n");
        ok = 0;
    } else {
        ok &= expect_function_layouts_equivalent_ignoring_names(
            "VALUE-SSA-ALLOC-PROGRAM-LAYOUT-ENTRY-FUNCTION", direct_function_layout, projected_function_layout);
    }

    value_ssa_program_allocation_layout_free(&projected_layout);
    value_ssa_program_allocation_layout_free(&direct_layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_program_allocation_layout_spill_summary(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    const ValueSsaFunctionAllocationLayout *function_layout = NULL;
    const ValueSsaAllocatedValuePlacement *spill_placement = NULL;
    const size_t *functions_with_colors = NULL;
    const size_t *functions_with_spills = NULL;
    size_t functions_with_colors_count = 0;
    size_t functions_with_spills_count = 0;
    const size_t expected_functions_with_colors[] = {0};
    const size_t expected_functions_with_spills[] = {0};
    int ok = 1;

    if (!build_alloc_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-spill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    if (!value_ssa_allocate_program(&program, 2, &result, &error) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-spill build failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_program_allocation_layout_get_function_layout(&layout, 0, &function_layout) || !function_layout ||
        function_layout->spilled_count != 1 || function_layout->spill_slot_count != 1) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-spill expected one spilled value in slot 0\n");
        ok = 0;
    }
    if (!value_ssa_program_allocation_layout_get_function_placement(&layout, 0, 0, &spill_placement) ||
        !spill_placement || spill_placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL ||
        spill_placement->spill_slot != 0 || spill_placement->spill_confirmed != 1) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-spill expected spilled placement for ssa.0\n");
        ok = 0;
    }
    ok &= expect_layout_summary("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-SPILL",
        &layout,
        1,
        5,
        4,
        1,
        1,
        1,
        0,
        2,
        1);
    if (!value_ssa_program_allocation_layout_get_functions_with_colors(
            &layout, &functions_with_colors_count, &functions_with_colors) ||
        !value_ssa_program_allocation_layout_get_functions_with_spills(
            &layout, &functions_with_spills_count, &functions_with_spills)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-spill grouped queries failed\n");
        ok = 0;
    } else {
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-SPILL",
            functions_with_colors,
            functions_with_colors_count,
            expected_functions_with_colors,
            sizeof(expected_functions_with_colors) / sizeof(expected_functions_with_colors[0]),
            "functions-with-colors");
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-SPILL",
            functions_with_spills,
            functions_with_spills_count,
            expected_functions_with_spills,
            sizeof(expected_functions_with_spills) / sizeof(expected_functions_with_spills[0]),
            "functions-with-spills");
    }

    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_program_allocation_layout_multi_function_summary(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    const size_t *functions_with_colors = NULL;
    const size_t *functions_with_spills = NULL;
    const size_t *functions_with_confirmed_spills = NULL;
    const size_t *functions_with_optimistic_colors = NULL;
    size_t functions_with_colors_count = 0;
    size_t functions_with_spills_count = 0;
    size_t functions_with_confirmed_spills_count = 0;
    size_t functions_with_optimistic_colors_count = 0;
    const size_t expected_color_functions[] = {0, 1};
    const size_t expected_spill_functions[] = {1};
    const size_t expected_confirmed_spill_functions[] = {1};
    const size_t expected_optimistic_color_functions[] = {1};
    int ok = 1;

    if (!build_alloc_two_function_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-multi setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    if (!value_ssa_allocate_program(&program, 2, &result, &error) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-multi build failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_layout_summary("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-MULTI",
        &layout,
        2,
        10,
        9,
        1,
        1,
        1,
        0,
        2,
        1);

    if (!value_ssa_program_allocation_layout_get_functions_with_colors(
            &layout, &functions_with_colors_count, &functions_with_colors) ||
        !value_ssa_program_allocation_layout_get_functions_with_spills(
            &layout, &functions_with_spills_count, &functions_with_spills) ||
        !value_ssa_program_allocation_layout_get_functions_with_confirmed_spills(
            &layout, &functions_with_confirmed_spills_count, &functions_with_confirmed_spills) ||
        !value_ssa_program_allocation_layout_get_functions_with_optimistic_colors(
            &layout, &functions_with_optimistic_colors_count, &functions_with_optimistic_colors)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-multi grouped queries failed\n");
        ok = 0;
    } else {
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-MULTI",
            functions_with_colors,
            functions_with_colors_count,
            expected_color_functions,
            sizeof(expected_color_functions) / sizeof(expected_color_functions[0]),
            "functions-with-colors");
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-MULTI",
            functions_with_spills,
            functions_with_spills_count,
            expected_spill_functions,
            sizeof(expected_spill_functions) / sizeof(expected_spill_functions[0]),
            "functions-with-spills");
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-MULTI",
            functions_with_confirmed_spills,
            functions_with_confirmed_spills_count,
            expected_confirmed_spill_functions,
            sizeof(expected_confirmed_spill_functions) / sizeof(expected_confirmed_spill_functions[0]),
            "functions-with-confirmed-spills");
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-MULTI",
            functions_with_optimistic_colors,
            functions_with_optimistic_colors_count,
            expected_optimistic_color_functions,
            sizeof(expected_optimistic_color_functions) / sizeof(expected_optimistic_color_functions[0]),
            "functions-with-optimistic-colors");
    }

    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_program_allocation_layout_multi_function_outcome_filters(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaProgramAllocationLayout layout;
    const size_t *functions_with_confirmed_spills = NULL;
    const size_t *functions_with_optimistic_colors = NULL;
    const size_t *functions_with_recovered_colors = NULL;
    size_t confirmed_count = 0;
    size_t optimistic_count = 0;
    size_t recovered_count = 0;
    const size_t expected_single_function[] = {1};
    int ok = 1;

    if (!build_alloc_two_function_program(&program, NULL)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-outcome-filters setup failed\n");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_program_allocation_layout_init(&layout);
    if (!prepare_manual_two_function_program_result(&result, &program) ||
        !value_ssa_build_program_allocation_layout(&result, &layout, NULL)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-outcome-filters build failed\n");
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_program_allocation_layout_get_functions_with_confirmed_spills(
            &layout, &confirmed_count, &functions_with_confirmed_spills) ||
        !value_ssa_program_allocation_layout_get_functions_with_optimistic_colors(
            &layout, &optimistic_count, &functions_with_optimistic_colors) ||
        !value_ssa_program_allocation_layout_get_functions_with_recovered_colors(
            &layout, &recovered_count, &functions_with_recovered_colors)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: program-layout-outcome-filters grouped queries failed\n");
        ok = 0;
    } else {
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-OUTCOME-FILTERS",
            functions_with_confirmed_spills,
            confirmed_count,
            expected_single_function,
            1,
            "functions-with-confirmed-spills");
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-OUTCOME-FILTERS",
            functions_with_optimistic_colors,
            optimistic_count,
            expected_single_function,
            1,
            "functions-with-optimistic-colors");
        ok &= expect_program_function_index_list("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-OUTCOME-FILTERS",
            functions_with_recovered_colors,
            recovered_count,
            expected_single_function,
            1,
            "functions-with-recovered-colors");
    }

    ok &= expect_layout_summary("VALUE-SSA-ALLOC-PROGRAM-LAYOUT-OUTCOME-FILTERS",
        &layout,
        2,
        10,
        9,
        1,
        2,
        1,
        1,
        2,
        1);

    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_layout_report_smoke(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    int ok;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 8, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_allocate_rewrite_layout_report("VALUE-SSA-ALLOC-REWRITE-LAYOUT-REPORT",
        &program,
        &result,
        &stats,
        "allocate+rewrite-report allocation_rounds=1 rewrite_rounds=0\n"
        "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=8 max_spill_slots=0\n"
        "functions-with-colors: 0\n"
        "functions-with-spills:\n"
        "functions-with-confirmed-spills:\n"
        "functions-with-optimistic-colors:\n"
        "functions-with-recovered-colors:\n"
        "\n"
        "alloc-layout func main colors=8 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_build_allocate_rewrite_layout_report(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    ValueSsaAllocateRewriteLayoutReport report;
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: build-layout-report setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    value_ssa_allocate_rewrite_layout_report_init(&report);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 8, &result, &stats, &error) ||
        !value_ssa_build_allocate_rewrite_layout_report(&result, &stats, &report, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: build-layout-report build failed: %s\n", error.message);
        value_ssa_allocate_rewrite_layout_report_free(&report);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_allocate_rewrite_layout_report_artifact("VALUE-SSA-ALLOC-BUILD-LAYOUT-REPORT",
        &report,
        "allocate+rewrite-report allocation_rounds=1 rewrite_rounds=0\n"
        "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=8 max_spill_slots=0\n"
        "functions-with-colors: 0\n"
        "functions-with-spills:\n"
        "functions-with-confirmed-spills:\n"
        "functions-with-optimistic-colors:\n"
        "functions-with-recovered-colors:\n"
        "\n"
        "alloc-layout func <anon> colors=8 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");
    ok &= expect_report_summary("VALUE-SSA-ALLOC-BUILD-LAYOUT-REPORT",
        &report,
        1,
        0,
        1,
        5,
        5,
        0,
        0,
        0,
        0,
        8,
        0);

    value_ssa_allocate_rewrite_layout_report_free(&report);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_layout_entry_matches_report(void) {
    ValueSsaProgram program_a;
    ValueSsaProgram program_b;
    ValueSsaError error;
    ValueSsaProgramAllocationLayout layout_a;
    ValueSsaProgramAllocationLayout layout_b;
    ValueSsaAllocateRewriteStats stats_a;
    ValueSsaAllocateRewriteStats stats_b;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program_a, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-entry setup A failed: %s\n", error.message);
        return 0;
    }
    if (!build_alloc_cross_block_spill_program(&program_b, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-entry setup B failed: %s\n", error.message);
        value_ssa_program_free(&program_a);
        return 0;
    }

    value_ssa_program_allocation_layout_init(&layout_a);
    value_ssa_program_allocation_layout_init(&layout_b);
    value_ssa_allocate_rewrite_stats_init(&stats_a);
    value_ssa_allocate_rewrite_stats_init(&stats_b);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_layout_with_stats(
            &program_a, 2, &layout_a, &stats_a, &error) ||
        !value_ssa_allocate_and_rewrite_program_single_block_spills_layout_with_stats(
            &program_b, 2, &layout_b, &stats_b, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-entry run failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&layout_b);
        value_ssa_program_allocation_layout_free(&layout_a);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (stats_a.allocation_rounds < 1 || stats_a.rewrite_rounds < 1 || stats_b.allocation_rounds < 1 ||
        stats_b.rewrite_rounds < 1) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-entry expected rewrite-bearing stats\n");
        ok = 0;
    }
    ok &= expect_program_layouts_equivalent_ignoring_names(
        "VALUE-SSA-ALLOC-REWRITE-LAYOUT-ENTRY", &layout_a, &layout_b);

    value_ssa_program_allocation_layout_free(&layout_b);
    value_ssa_program_allocation_layout_free(&layout_a);
    value_ssa_program_free(&program_b);
    value_ssa_program_free(&program_a);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_layout_report_entry(void) {
    ValueSsaProgram program_a;
    ValueSsaProgram program_b;
    ValueSsaProgram program_c;
    ValueSsaError error;
    ValueSsaAllocateRewriteLayoutReport report;
    ValueSsaAllocateRewriteLayoutReport report_b;
    ValueSsaProgramAllocationLayout layout;
    ValueSsaAllocateRewriteStats stats;
    size_t layout_function_count = 0;
    size_t layout_total_value_count = 0;
    size_t layout_total_colored_count = 0;
    size_t layout_total_spilled_count = 0;
    size_t layout_total_optimistic_colored_count = 0;
    size_t layout_total_confirmed_spilled_count = 0;
    size_t layout_total_recovered_colored_count = 0;
    size_t layout_max_color_budget = 0;
    size_t layout_max_spill_slot_count = 0;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program_a, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-entry setup A failed: %s\n", error.message);
        return 0;
    }
    if (!build_alloc_cross_block_spill_program(&program_b, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-entry setup B failed: %s\n", error.message);
        value_ssa_program_free(&program_a);
        return 0;
    }
    if (!build_alloc_cross_block_spill_program(&program_c, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-entry setup C failed: %s\n", error.message);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    value_ssa_allocate_rewrite_layout_report_init(&report);
    value_ssa_allocate_rewrite_layout_report_init(&report_b);
    value_ssa_program_allocation_layout_init(&layout);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(
            &program_a, 2, &report, &error) ||
        !value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(
            &program_b, 2, &report_b, &error) ||
        !value_ssa_allocate_and_rewrite_program_single_block_spills_layout_with_stats(
            &program_c, 2, &layout, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-entry run failed: %s\n", error.message);
        value_ssa_program_allocation_layout_free(&layout);
        value_ssa_allocate_rewrite_layout_report_free(&report_b);
        value_ssa_allocate_rewrite_layout_report_free(&report);
        value_ssa_program_free(&program_c);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (!value_ssa_program_allocation_layout_get_summary(&layout,
            &layout_function_count,
            &layout_total_value_count,
            &layout_total_colored_count,
            &layout_total_spilled_count,
            &layout_total_optimistic_colored_count,
            &layout_total_confirmed_spilled_count,
            &layout_total_recovered_colored_count,
            &layout_max_color_budget,
            &layout_max_spill_slot_count)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-entry missing comparison layout summary\n");
        ok = 0;
    } else {
        ok &= expect_report_summary("VALUE-SSA-ALLOC-REWRITE-LAYOUT-REPORT-ENTRY-SUMMARY",
            &report,
            stats.allocation_rounds,
            stats.rewrite_rounds,
            layout_function_count,
            layout_total_value_count,
            layout_total_colored_count,
            layout_total_spilled_count,
            layout_total_optimistic_colored_count,
            layout_total_confirmed_spilled_count,
            layout_total_recovered_colored_count,
            layout_max_color_budget,
            layout_max_spill_slot_count);
    }
    {
        const ValueSsaProgramAllocationLayout *report_layout = NULL;

        if (!value_ssa_allocate_rewrite_layout_report_get_program_layout(&report, &report_layout) || !report_layout) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-entry missing report layout view\n");
            ok = 0;
        } else {
            ok &= expect_program_layouts_equivalent_ignoring_names(
                "VALUE-SSA-ALLOC-REWRITE-LAYOUT-REPORT-ENTRY", report_layout, &layout);
        }
    }
    ok &= expect_allocate_rewrite_layout_reports_equivalent_ignoring_names(
        "VALUE-SSA-ALLOC-REWRITE-LAYOUT-REPORT-ENTRY-REPORTS", &report, &report_b);

    value_ssa_program_allocation_layout_free(&layout);
    value_ssa_allocate_rewrite_layout_report_free(&report_b);
    value_ssa_allocate_rewrite_layout_report_free(&report);
    value_ssa_program_free(&program_c);
    value_ssa_program_free(&program_b);
    value_ssa_program_free(&program_a);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_layout_report_artifact_dump(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaAllocateRewriteLayoutReport report;
    int ok;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-artifact setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_allocate_rewrite_layout_report_init(&report);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(&program, 8, &report, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-layout-report-artifact run failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_allocate_rewrite_layout_report_artifact("VALUE-SSA-ALLOC-REWRITE-LAYOUT-REPORT-ARTIFACT",
        &report,
        "allocate+rewrite-report allocation_rounds=1 rewrite_rounds=0\n"
        "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=8 max_spill_slots=0\n"
        "functions-with-colors: 0\n"
        "functions-with-spills:\n"
        "functions-with-confirmed-spills:\n"
        "functions-with-optimistic-colors:\n"
        "functions-with-recovered-colors:\n"
        "\n"
        "alloc-layout func main colors=8 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    value_ssa_allocate_rewrite_layout_report_free(&report);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_rewrite_layout_report_attach_program_context(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteLayoutReport report;
    ValueSsaAllocateRewriteStats stats;
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-attach setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_layout_report_init(&report);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 8, &result, &stats, &error) ||
        !value_ssa_allocate_rewrite_layout_report_set_stats(&report, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-attach build failed: %s\n", error.message);
        value_ssa_allocate_rewrite_layout_report_free(&report);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }
    {
        ValueSsaProgramAllocationLayout layout;

        value_ssa_program_allocation_layout_init(&layout);
        if (!value_ssa_build_program_allocation_layout(&result, &layout, &error) ||
            !value_ssa_allocate_rewrite_layout_report_set_program_layout(&report, &layout, &error)) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-attach build failed: %s\n", error.message);
            value_ssa_program_allocation_layout_free(&layout);
            value_ssa_allocate_rewrite_layout_report_free(&report);
            value_ssa_program_allocation_result_free(&result);
            value_ssa_program_free(&program);
            return 0;
        }
        value_ssa_program_allocation_layout_free(&layout);
    }

    ok &= expect_allocate_rewrite_layout_report_artifact("VALUE-SSA-ALLOC-REWRITE-LAYOUT-ATTACH-BEFORE",
        &report,
        "allocate+rewrite-report allocation_rounds=1 rewrite_rounds=0\n"
        "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=8 max_spill_slots=0\n"
        "functions-with-colors: 0\n"
        "functions-with-spills:\n"
        "functions-with-confirmed-spills:\n"
        "functions-with-optimistic-colors:\n"
        "functions-with-recovered-colors:\n"
        "\n"
        "alloc-layout func <anon> colors=8 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
        "used-colors: 0\n"
        "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "spilled-values:\n"
        "optimistic-colored-values:\n"
        "confirmed-spilled-values:\n"
        "recovered-colored-values:\n"
        "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
        "ssa.0 color=0\n"
        "ssa.1 color=0\n"
        "ssa.2 color=0\n"
        "ssa.3 color=0\n"
        "ssa.4 color=0\n");

    if (!value_ssa_allocate_rewrite_layout_report_attach_program_context(&report, &program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-attach attach failed: %s\n", error.message);
        ok = 0;
    } else {
        const ValueSsaProgramAllocationLayout *attached_layout = NULL;
        const char *function_name = NULL;

        if (!value_ssa_allocate_rewrite_layout_report_get_program_layout(&report, &attached_layout) || !attached_layout) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-attach failed to expose program layout\n");
            ok = 0;
        } else if (!value_ssa_allocate_rewrite_layout_report_get_function_name(&report, 0, &function_name) ||
            !function_name || strcmp(function_name, "main") != 0) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-attach expected attached layout name main\n");
            ok = 0;
        }
        ok &= expect_allocate_rewrite_layout_report_artifact("VALUE-SSA-ALLOC-REWRITE-LAYOUT-ATTACH-AFTER",
            &report,
            "allocate+rewrite-report allocation_rounds=1 rewrite_rounds=0\n"
            "alloc-layout program functions=1 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 functions_with_colors=1 functions_with_spills=0 functions_with_confirmed_spills=0 functions_with_optimistic_colors=0 functions_with_recovered_colors=0 max_colors=8 max_spill_slots=0\n"
            "functions-with-colors: 0\n"
            "functions-with-spills:\n"
            "functions-with-confirmed-spills:\n"
            "functions-with-optimistic-colors:\n"
            "functions-with-recovered-colors:\n"
            "\n"
            "alloc-layout func main colors=8 values=5 colored=5 spilled=0 optimistic_colored=0 confirmed_spilled=0 recovered_colored=0 spill_slots=0\n"
            "used-colors: 0\n"
            "colored-values: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
            "spilled-values:\n"
            "optimistic-colored-values:\n"
            "confirmed-spilled-values:\n"
            "recovered-colored-values:\n"
            "color-group 0: ssa.0 ssa.1 ssa.2 ssa.3 ssa.4\n"
            "ssa.0 color=0\n"
            "ssa.1 color=0\n"
            "ssa.2 color=0\n"
            "ssa.3 color=0\n"
            "ssa.4 color=0\n");
    }

    value_ssa_allocate_rewrite_layout_report_free(&report);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_rewrite_layout_report_query_surface(void) {
    ValueSsaProgram program;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteLayoutReport report;
    ValueSsaAllocateRewriteStats stats;
    const ValueSsaProgramAllocationLayout *layout = NULL;
    const ValueSsaFunctionAllocationLayout *function_layout = NULL;
    const ValueSsaFunctionAllocationLayout *function_layout_by_name = NULL;
    const ValueSsaAllocatedValuePlacement *placement = NULL;
    const size_t *functions_with_colors = NULL;
    const size_t *functions_with_spills = NULL;
    const size_t *functions_with_confirmed_spills = NULL;
    const size_t *functions_with_optimistic_colors = NULL;
    const size_t *functions_with_recovered_colors = NULL;
    const size_t expected_color_functions[] = {0, 1};
    const size_t expected_single_function[] = {1};
    const char *function_name = NULL;
    size_t function_index = 0;
    size_t function_count = 0;
    size_t color_count = 0;
    size_t spill_count = 0;
    size_t confirmed_count = 0;
    size_t optimistic_count = 0;
    size_t recovered_count = 0;
    int ok = 1;

    if (!build_alloc_two_function_program(&program, NULL)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query setup failed\n");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_layout_report_init(&report);
    value_ssa_allocate_rewrite_stats_init(&stats);
    stats.allocation_rounds = 3;
    stats.rewrite_rounds = 2;
    if (!prepare_manual_two_function_program_result(&result, &program) ||
        !value_ssa_build_allocate_rewrite_layout_report(&result, &stats, &report, NULL)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query build failed\n");
        value_ssa_allocate_rewrite_layout_report_free(&report);
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    ok &= expect_report_summary("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-BEFORE-ATTACH",
        &report,
        3,
        2,
        2,
        10,
        9,
        1,
        2,
        1,
        1,
        2,
        1);
    if (!value_ssa_allocate_rewrite_layout_report_get_program_layout(&report, &layout) || !layout) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query missing program layout view\n");
        ok = 0;
    }
    if (!value_ssa_allocate_rewrite_layout_report_get_function_layout(&report, 1, &function_layout) || !function_layout ||
        function_layout->spilled_count != 1 || function_layout->confirmed_spilled_count != 1 ||
        function_layout->optimistic_colored_count != 2 || function_layout->recovered_colored_count != 1) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected function 1 outcome layout\n");
        ok = 0;
    }
    ok &= expect_function_layout_summary("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-FN1-BEFORE-ATTACH",
        function_layout,
        NULL,
        5,
        2,
        1,
        4,
        1,
        2,
        1,
        1,
        2,
        2,
        1);
    if (!value_ssa_allocate_rewrite_layout_report_get_function_name(&report, 1, &function_name) ||
        function_name != NULL) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected unnamed function before attach\n");
        ok = 0;
    }
    if (value_ssa_allocate_rewrite_layout_report_find_function_index_by_name(&report, "spill", &function_index)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query unexpectedly found name before attach\n");
        ok = 0;
    }
    if (!value_ssa_allocate_rewrite_layout_report_get_functions_with_colors(
            &report, &color_count, &functions_with_colors) ||
        !value_ssa_allocate_rewrite_layout_report_get_functions_with_spills(
            &report, &spill_count, &functions_with_spills) ||
        !value_ssa_allocate_rewrite_layout_report_get_functions_with_confirmed_spills(
            &report, &confirmed_count, &functions_with_confirmed_spills) ||
        !value_ssa_allocate_rewrite_layout_report_get_functions_with_optimistic_colors(
            &report, &optimistic_count, &functions_with_optimistic_colors) ||
        !value_ssa_allocate_rewrite_layout_report_get_functions_with_recovered_colors(
            &report, &recovered_count, &functions_with_recovered_colors)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query grouped queries failed before attach\n");
        ok = 0;
    } else {
        ok &= expect_report_function_index_list("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-COLORS",
            functions_with_colors,
            color_count,
            expected_color_functions,
            sizeof(expected_color_functions) / sizeof(expected_color_functions[0]),
            "report-functions-with-colors");
        ok &= expect_report_function_index_list("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-SPILLS",
            functions_with_spills,
            spill_count,
            expected_single_function,
            1,
            "report-functions-with-spills");
        ok &= expect_report_function_index_list("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-CONFIRMED",
            functions_with_confirmed_spills,
            confirmed_count,
            expected_single_function,
            1,
            "report-functions-with-confirmed-spills");
        ok &= expect_report_function_index_list("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-OPTIMISTIC",
            functions_with_optimistic_colors,
            optimistic_count,
            expected_single_function,
            1,
            "report-functions-with-optimistic-colors");
        ok &= expect_report_function_index_list("VALUE-SSA-ALLOC-REWRITE-LAYOUT-QUERY-RECOVERED",
            functions_with_recovered_colors,
            recovered_count,
            expected_single_function,
            1,
            "report-functions-with-recovered-colors");
    }

    if (!value_ssa_allocate_rewrite_layout_report_attach_program_context(&report, &program, NULL)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query attach failed\n");
        ok = 0;
    } else {
        if (!value_ssa_allocate_rewrite_layout_report_get_function_name(&report, 1, &function_name) ||
            !function_name || strcmp(function_name, "spill") != 0) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected spill after attach\n");
            ok = 0;
        }
        if (!value_ssa_allocate_rewrite_layout_report_get_function_layout_by_name(
                &report, "spill", &function_index, &function_layout_by_name) ||
            function_index != 1 || function_layout_by_name != function_layout) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected spill layout by name at index 1\n");
            ok = 0;
        }
        if (!value_ssa_allocate_rewrite_layout_report_find_function_index_by_name(
                &report, "spill", &function_index) ||
            function_index != 1) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected spill at index 1\n");
            ok = 0;
        }
        if (!value_ssa_allocate_rewrite_layout_report_get_function_placement(&report, 1, 0, &placement) ||
            !placement || placement->kind != VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL ||
            placement->spill_slot != 0 || placement->spill_confirmed != 1) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected spill placement for spill:ssa.0\n");
            ok = 0;
        }
        if (!value_ssa_allocate_rewrite_layout_report_get_function_summary(&report,
                1,
                &function_name,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL,
                NULL) ||
            !function_name || strcmp(function_name, "spill") != 0) {
            fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected named function summary after attach\n");
            ok = 0;
        }
    }

    if (layout && layout->function_count != 2) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected attached layout function count 2\n");
        ok = 0;
    }
    if (!value_ssa_allocate_rewrite_layout_report_get_summary(&report,
            NULL,
            NULL,
            &function_count,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL) ||
        function_count != 2) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: layout-report-query expected function-count-only summary = 2\n");
        ok = 0;
    }

    value_ssa_allocate_rewrite_layout_report_free(&report);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_returns_final_allocation(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_verify_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite verify failed: %s\n", error.message);
        ok = 0;
    } else if (result.function_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite expected function_count 1, got %zu\n",
            result.function_count);
        ok = 0;
    } else if (result.function_results[0].value_count != program.functions[0].next_value_id) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite result/program value count mismatch (%zu vs %zu)\n",
            result.function_results[0].value_count,
            program.functions[0].next_value_id);
        ok = 0;
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_matches_stats_wrapper(void) {
    ValueSsaProgram program_a;
    ValueSsaProgram program_b;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result_a;
    ValueSsaProgramAllocationResult result_b;
    ValueSsaAllocateRewriteStats stats;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program_a, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-wrapper-match setup A failed: %s\n", error.message);
        return 0;
    }
    if (!build_alloc_cross_block_spill_program(&program_b, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-wrapper-match setup B failed: %s\n", error.message);
        value_ssa_program_free(&program_a);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result_a);
    value_ssa_program_allocation_result_init(&result_b);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills(&program_a, 2, &result_a, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-wrapper-match wrapper allocate failed: %s\n", error.message);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
            &program_b, 2, &result_b, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-wrapper-match stats allocate failed: %s\n", error.message);
        value_ssa_program_allocation_result_free(&result_a);
        value_ssa_program_free(&program_b);
        value_ssa_program_free(&program_a);
        return 0;
    }

    if (result_a.function_count != 1 || result_b.function_count != 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-wrapper-match expected function_count 1/1, got %zu/%zu\n",
            result_a.function_count,
            result_b.function_count);
        ok = 0;
    } else {
        ok &= expect_alloc_results_equal_by_dump(
            "VALUE-SSA-ALLOC-REWRITE-WRAPPER-MATCH",
            &program_a.functions[0],
            &result_a.function_results[0],
            &result_b.function_results[0]);
    }

    value_ssa_program_allocation_result_free(&result_b);
    value_ssa_program_allocation_result_free(&result_a);
    value_ssa_program_free(&program_b);
    value_ssa_program_free(&program_a);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_reuses_spill_slots(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_noninterfering_spill_reuse_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-slot-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 0, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-slot-reuse failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-slot-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (stats.allocation_rounds < 2 || stats.rewrite_rounds < 1 || result.function_count != 1 ||
        strstr(actual_text, "ret 22") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-slot-reuse expected rewrite-driven convergence, stats=(%zu,%zu), got:\n%s\n",
            stats.allocation_rounds,
            stats.rewrite_rounds,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_does_not_respill_reload_family(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    char *actual_text = NULL;
    size_t spill_local_count = 0;
    const char *cursor;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-respill setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills(&program, 2, &result, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-respill failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-respill dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    for (cursor = actual_text; (cursor = strstr(cursor, "spill.")) != NULL; ++cursor) {
        ++spill_local_count;
    }

    if (spill_local_count > 2) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-respill expected bounded spill family, got:\n%s\n",
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_handles_phi_defined_spills(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_phi_defined_multi_use_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-phi-def setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 1, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-phi-def failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-phi-def dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (stats.rewrite_rounds < 1 || result.function_count != 1 ||
        result.function_results[0].value_count != program.functions[0].next_value_id ||
        strstr(actual_text, "store_local spill.") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-phi-def expected high-level rewrite and aligned result, "
            "stats=(%zu,%zu), got:\n%s\n",
            stats.allocation_rounds,
            stats.rewrite_rounds,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_reuses_branch_split_blocks(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    char *actual_text = NULL;
    int ok = 1;

    if (!build_alloc_branch_predecessor_two_distinct_spills_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-split-reuse setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 1, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-split-reuse failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (!value_ssa_dump_program(&program, &actual_text)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-split-reuse dump failed\n");
        value_ssa_program_allocation_result_free(&result);
        value_ssa_program_free(&program);
        return 0;
    }

    if (stats.rewrite_rounds < 1 || result.function_count != 1 ||
        result.function_results[0].value_count != program.functions[0].next_value_id ||
        strstr(actual_text, "ret 85") == NULL) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-split-reuse expected converged rewritten endpoint, "
            "stats=(%zu,%zu), got:\n%s\n",
            stats.allocation_rounds,
            stats.rewrite_rounds,
            actual_text);
        ok = 0;
    }

    free(actual_text);
    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_reports_stats(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    int ok = 1;

    if (!build_alloc_cross_block_spill_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-stats setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 2, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-stats failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (stats.allocation_rounds < 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-stats expected >=1 allocation round, got %zu\n",
            stats.allocation_rounds);
        ok = 0;
    }
    if (stats.rewrite_rounds < 1) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-stats expected >=1 rewrite round, got %zu\n",
            stats.rewrite_rounds);
        ok = 0;
    }
    if (stats.allocation_rounds < stats.rewrite_rounds) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-stats expected allocations >= rewrites, got %zu < %zu\n",
            stats.allocation_rounds,
            stats.rewrite_rounds);
        ok = 0;
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_smoke_stats_without_rewrite(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    int ok = 1;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-noop-stats setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 8, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-noop-stats failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    if (stats.allocation_rounds != 1 || stats.rewrite_rounds != 0) {
        fprintf(stderr,
            "[value-ssa-alloc] FAIL: allocate+rewrite-noop-stats expected (1,0), got (%zu,%zu)\n",
            stats.allocation_rounds,
            stats.rewrite_rounds);
        ok = 0;
    }

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

static int test_value_ssa_allocate_and_rewrite_program_stats_dump(void) {
    ValueSsaProgram program;
    ValueSsaError error;
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    int ok;

    if (!build_alloc_smoke_program(&program, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-stats-dump setup failed: %s\n", error.message);
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (!value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(&program, 8, &result, &stats, &error)) {
        fprintf(stderr, "[value-ssa-alloc] FAIL: allocate+rewrite-stats-dump failed: %s\n", error.message);
        value_ssa_program_free(&program);
        return 0;
    }

    ok = expect_allocate_rewrite_stats_dump("VALUE-SSA-ALLOC-STATS-DUMP",
        &stats,
        "allocate+rewrite allocation_rounds=1 rewrite_rounds=0\n");

    value_ssa_program_allocation_result_free(&result);
    value_ssa_program_free(&program);
    return ok;
}

int main(void) {
    int ok = 1;

    ok &= test_value_ssa_allocator_coalesce_analysis_safe_pair();
    ok &= test_value_ssa_allocator_coalesce_analysis_merges_chain_family();
    ok &= test_value_ssa_allocator_coalesce_analysis_blocks_interfering_pair();
    ok &= test_value_ssa_allocator_coalesce_analysis_blocks_budget_heavy_merge();
    ok &= test_value_ssa_allocator_coalesce_analysis_blocks_george_unsafe_budget_heavy_merge();
    ok &= test_value_ssa_allocator_plan_uses_coalesced_effective_pressure();
    ok &= test_value_ssa_allocator_plan_propagates_family_external_move_and_freeze();
    ok &= test_value_ssa_allocator_plan_prefers_released_simplify_family();
    ok &= test_value_ssa_allocator_plan_prefers_mutual_best_ready_family_on_tie();
    ok &= test_value_ssa_allocator_plan_prefers_higher_priority_released_family();
    ok &= test_value_ssa_allocator_plan_picks_best_member_within_released_family();
    ok &= test_value_ssa_allocator_plan_picks_best_member_within_interleaved_released_family();
    ok &= test_value_ssa_allocator_move_family_analysis_internal_family();
    ok &= test_value_ssa_allocator_move_family_analysis_frozen_family();
    ok &= test_value_ssa_allocator_move_worklist_internal_family();
    ok &= test_value_ssa_allocator_move_worklist_manual_coalesce_ready_family();
    ok &= test_value_ssa_allocator_move_worklist_prefers_more_ready_neighbors_on_tie();
    ok &= test_value_ssa_allocator_move_worklist_prefers_heavier_ready_affinity_on_tie();
    ok &= test_value_ssa_allocator_move_worklist_prefers_mutual_best_ready_partner_on_tie();
    ok &= test_value_ssa_allocator_coalesce_opportunity_agenda_manual_pairs();
    ok &= test_value_ssa_allocator_move_worklist_frozen_and_released_families();
    ok &= test_value_ssa_allocator_move_transition_trace_family_progression();
    ok &= test_value_ssa_allocator_move_transition_trace_explicit_coalesce_action();
    ok &= test_value_ssa_allocator_move_transition_trace_defers_weaker_coalesce_to_stronger_simplify();
    ok &= test_value_ssa_allocator_move_transition_trace_prefers_stronger_freeze_coalesce_pair();
    ok &= test_value_ssa_allocator_plan_smoke();
    ok &= test_value_ssa_allocator_plan_loop();
    ok &= test_value_ssa_allocate_function_confirms_blocked_spill_remove();
    ok &= test_value_ssa_allocator_plan_drops_move_related_when_affinity_is_removed();
    ok &= test_value_ssa_allocator_plan_preserves_move_related_when_affinity_remains();
    ok &= test_value_ssa_allocator_plan_spill_cost_components();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_loop_weight();
    ok &= test_value_ssa_allocation_prep_tracks_call_density();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_call_density();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_single_block_on_cost_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_call_crossing();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_use_loop_depth();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_spill_cost_with_use_call_density();
    ok &= test_value_ssa_allocation_worklist_priority_tracks_new_spill_signals();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_rematerializable_on_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_split_child_on_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_deeper_split_child_on_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_lower_family_affinity_pressure_on_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_fewer_family_neighbors_on_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_smaller_coalesce_class_on_tie();
    ok &= test_value_ssa_allocator_plan_spill_phase_prefers_internal_over_move_family_on_tie();
    ok &= test_value_ssa_allocate_function_smoke();
    ok &= test_value_ssa_allocate_function_matches_from_plan_pipeline();
    ok &= test_value_ssa_allocate_function_evicts_only_unique_blocker();
    ok &= test_value_ssa_allocate_function_prefers_highest_affinity_color();
    ok &= test_value_ssa_allocate_function_prefers_coalescible_partner_color();
    ok &= test_value_ssa_allocate_function_prefers_coalesced_class_color();
    ok &= test_value_ssa_allocate_function_from_plan_preserves_preferred_coalesce_color_on_targeted_eviction();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_family_supported_targeted_recolor();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_weighted_family_targeted_recolor();
    ok &= test_value_ssa_allocate_function_from_plan_uses_runtime_merged_family_alias();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_runtime_family_dominant_color();
    ok &= test_value_ssa_allocate_function_from_plan_uses_plan_family_color_without_analysis();
    ok &= test_value_ssa_allocate_function_from_plan_marks_evicted_blocker_spill_confirmed();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_evicting_spill_intended_blocker_on_priority_tie();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_evicting_rematerializable_blocker_on_priority_tie();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_evicting_single_block_blocker_on_cost_tie();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_evicting_split_child_blocker_on_cost_tie();
    ok &= test_value_ssa_allocate_function_from_plan_retries_spilled_value_after_late_eviction();
    ok &= test_value_ssa_allocate_function_from_plan_retries_with_late_retry_eviction();
    ok &= test_value_ssa_allocate_function_from_plan_recolors_retry_blocker_instead_of_spilling();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_spill_intended_candidate();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_split_child_candidate();
    ok &= test_value_ssa_retry_family_agenda_split_summary_tracks_representative_root();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_select_recolorable_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_preferred_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_larger_coalesce_class();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_family_pressure_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_family_members_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_runtime_family_activity_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_picks_retry_family_representative();
    ok &= test_value_ssa_allocate_function_from_plan_picks_retry_family_representative_by_blocker_aftermath();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_more_ready_family_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_ready_affinity_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_mutual_best_ready_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_batches_retry_provisional_family_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_batches_retry_confirmed_family_recovery();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_heavier_family_pressure_with_eviction();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_retry_lighter_blocker_family_with_eviction();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_smaller_family_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_lighter_family_pressure_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_fewer_family_neighbors_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_lighter_family_members_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_lighter_runtime_family_activity_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_main_recolor_target_supported_blocker();
    ok &= test_value_ssa_allocate_function_from_plan_prefers_weighted_main_recolor_target_supported_blocker();
    ok &= test_value_ssa_allocate_function_spill();
    ok &= test_value_ssa_build_function_allocation_layout_smoke();
    ok &= test_value_ssa_allocate_function_layout_matches_result_projection();
    ok &= test_value_ssa_allocate_function_select_stats();
    ok &= test_value_ssa_allocate_function_select_trace();
    ok &= test_value_ssa_allocate_function_spill_dump_has_slot();
    ok &= test_value_ssa_build_function_allocation_layout_spill_mix();
    ok &= test_value_ssa_allocate_function_reuses_spill_slot_for_noninterfering_values();
    ok &= test_value_ssa_allocate_function_reuses_parent_spill_slot_for_split_child();
    ok &= test_value_ssa_rewrite_program_single_block_spills();
    ok &= test_value_ssa_rewrite_program_reuses_spill_local_for_shared_slot();
    ok &= test_value_ssa_rewrite_program_single_block_spill_return();
    ok &= test_value_ssa_rewrite_program_reuses_same_block_reload();
    ok &= test_value_ssa_rewrite_program_cross_block_spills();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits_term_use();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits_multiple_blocks();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits_def_block();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits_reports_change_flag();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits_reports_no_change_flag();
    ok &= test_value_ssa_rewrite_program_edge_phi_spill_splits_branch_edge();
    ok &= test_value_ssa_rewrite_program_edge_phi_spill_splits_loop_backedge();
    ok &= test_value_ssa_rewrite_program_edge_phi_spill_splits_branch_tail_and_phi_together();
    ok &= test_value_ssa_rewrite_program_edge_phi_spill_splits_reuse_one_branch_split_block_across_values();
    ok &= test_value_ssa_rewrite_program_edge_phi_spill_splits_reuse_one_loop_backedge_block_across_values();
    ok &= test_value_ssa_rewrite_program_edge_phi_spill_splits_reuse_one_branch_split_block_with_two_tail_values();
    ok &= test_value_ssa_rewrite_program_block_local_spill_splits_phi_defined_header_uses();
    ok &= test_value_ssa_rewrite_program_composes_local_and_edge_splits_for_same_value();
    ok &= test_value_ssa_rewrite_program_processes_two_edge_split_families_for_same_value();
    ok &= test_value_ssa_rewrite_program_composes_jump_edge_phi_and_local_tail_splits();
    ok &= test_value_ssa_rewrite_program_jump_edge_single_phi_pair_can_follow_local_split();
    ok &= test_value_ssa_rewrite_program_split_child_spill_reuses_parent_spill_family();
    ok &= test_value_ssa_rewrite_program_split_chain_spill_reuses_root_family();
    ok &= test_value_ssa_rewrite_program_reuses_unique_predecessor_reload();
    ok &= test_value_ssa_rewrite_program_reuses_unique_predecessor_chain_reload();
    ok &= test_value_ssa_rewrite_program_reuses_dominating_reload_after_reconvergence();
    ok &= test_value_ssa_rewrite_program_does_not_reuse_dominating_reload_past_clobber();
    ok &= test_value_ssa_rewrite_program_reuses_reload_through_loop_header();
    ok &= test_value_ssa_rewrite_program_does_not_reuse_reload_past_loop_clobber();
    ok &= test_value_ssa_rewrite_program_reuses_reload_through_nested_reconvergence();
    ok &= test_value_ssa_rewrite_program_does_not_reuse_reload_past_nested_clobber();
    ok &= test_value_ssa_rewrite_program_splits_loop_backedge_for_phi_input();
    ok &= test_value_ssa_rewrite_program_reuses_loop_backedge_split_for_two_phi_inputs();
    ok &= test_value_ssa_rewrite_program_spills_and_canonicalizes();
    ok &= test_value_ssa_rewrite_program_phi_input_spills();
    ok &= test_value_ssa_rewrite_program_phi_defined_spills();
    ok &= test_value_ssa_rewrite_program_phi_defined_multi_use_spills();
    ok &= test_value_ssa_rewrite_program_branch_predecessor_phi_input_spills();
    ok &= test_value_ssa_rewrite_program_reuses_branch_split_for_two_phi_inputs();
    ok &= test_value_ssa_rewrite_program_reuses_branch_split_for_distinct_spill_slots();
    ok &= test_value_ssa_allocate_function_evicts_lower_priority_neighbor();
    ok &= test_value_ssa_allocate_function_queries();
    ok &= test_value_ssa_allocate_program_smoke();
    ok &= test_value_ssa_allocate_program_matches_direct_function_allocation();
    ok &= test_value_ssa_allocate_program_dump();
    ok &= test_value_ssa_build_program_allocation_layout_dump();
    ok &= test_value_ssa_program_allocation_layout_attach_program_context();
    ok &= test_value_ssa_allocate_program_layout_matches_result_projection();
    ok &= test_value_ssa_build_program_allocation_layout_spill_summary();
    ok &= test_value_ssa_build_program_allocation_layout_multi_function_summary();
    ok &= test_value_ssa_build_program_allocation_layout_multi_function_outcome_filters();
    ok &= test_value_ssa_build_allocate_rewrite_layout_report();
    ok &= test_value_ssa_allocate_and_rewrite_layout_report_smoke();
    ok &= test_value_ssa_allocate_and_rewrite_program_layout_entry_matches_report();
    ok &= test_value_ssa_allocate_and_rewrite_program_layout_report_entry();
    ok &= test_value_ssa_allocate_and_rewrite_layout_report_artifact_dump();
    ok &= test_value_ssa_allocate_rewrite_layout_report_attach_program_context();
    ok &= test_value_ssa_allocate_rewrite_layout_report_query_surface();
    ok &= test_value_ssa_allocate_and_rewrite_program_returns_final_allocation();
    ok &= test_value_ssa_allocate_and_rewrite_program_matches_stats_wrapper();
    ok &= test_value_ssa_allocate_and_rewrite_program_reuses_spill_slots();
    ok &= test_value_ssa_allocate_and_rewrite_program_does_not_respill_reload_family();
    ok &= test_value_ssa_allocate_and_rewrite_program_handles_phi_defined_spills();
    ok &= test_value_ssa_allocate_and_rewrite_program_reuses_branch_split_blocks();
    ok &= test_value_ssa_allocate_and_rewrite_program_reports_stats();
    ok &= test_value_ssa_allocate_and_rewrite_program_smoke_stats_without_rewrite();
    ok &= test_value_ssa_allocate_and_rewrite_program_stats_dump();
    if (!ok) {
        return 1;
    }

    printf("[value-ssa-alloc] PASS\n");
    return 0;
}
