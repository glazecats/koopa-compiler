#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

int value_ssa_dump_function_allocation_layout(const ValueSsaFunction *function,
    const ValueSsaFunctionAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaFunctionAllocationLayout local_layout;
    int ok;

    if (!function || !layout || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-145: invalid function allocation layout dump contract");
        return 0;
    }

    value_ssa_function_allocation_layout_init(&local_layout);
    local_layout = *layout;
    local_layout.function_name = function->name ? value_ssa_alloc_strdup(function->name) : NULL;
    if (function->name && !local_layout.function_name) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-203: out of memory copying function dump name");
        return 0;
    }
    ok = value_ssa_dump_function_allocation_layout_artifact(&local_layout, out_text, error);
    free(local_layout.function_name);
    return ok;
}

int value_ssa_dump_function_allocation_layout_artifact(const ValueSsaFunctionAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
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
    const size_t *colored_values = NULL;
    const size_t *spilled_values = NULL;
    const size_t *optimistic_colored_values = NULL;
    const size_t *confirmed_spilled_values = NULL;
    const size_t *recovered_colored_values = NULL;
    const size_t *used_colors = NULL;
    size_t value_id;
    size_t color_index;
    size_t list_index;
    size_t group_index;

    if (!layout || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-204: invalid function allocation layout artifact dump contract");
        return 0;
    }
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
            &spill_slot_group_count) ||
        !value_ssa_function_allocation_layout_get_colored_values(layout, &colored_count, &colored_values) ||
        !value_ssa_function_allocation_layout_get_spilled_values(layout, &spilled_count, &spilled_values) ||
        !value_ssa_function_allocation_layout_get_optimistic_colored_values(
            layout, &optimistic_colored_count, &optimistic_colored_values) ||
        !value_ssa_function_allocation_layout_get_confirmed_spilled_values(
            layout, &confirmed_spilled_count, &confirmed_spilled_values) ||
        !value_ssa_function_allocation_layout_get_recovered_colored_values(
            layout, &recovered_colored_count, &recovered_colored_values) ||
        !value_ssa_function_allocation_layout_get_used_colors(layout, &used_color_count, &used_colors)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-214: invalid function layout summary query contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "alloc-layout func %s colors=%zu values=%zu colored=%zu spilled=%zu optimistic_colored=%zu confirmed_spilled=%zu recovered_colored=%zu spill_slots=%zu\n",
            function_name ? function_name : "<anon>",
            color_budget,
            value_count,
            colored_count,
            spilled_count,
            optimistic_colored_count,
            confirmed_spilled_count,
            recovered_colored_count,
            spill_slot_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-146: out of memory building function allocation layout dump");
        return 0;
    }
    if (!value_ssa_alloc_append_format(&builder, "used-colors:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-156: out of memory appending used-color header");
        return 0;
    }
    for (color_index = 0; color_index < used_color_count; ++color_index) {
        if (!value_ssa_alloc_append_format(&builder, " %zu", used_colors[color_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-157: out of memory appending used color");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\ncolored-values:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-158: out of memory appending colored-values header");
        return 0;
    }
    for (list_index = 0; list_index < colored_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", colored_values[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-159: out of memory appending colored value id");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nspilled-values:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-160: out of memory appending spilled-values header");
        return 0;
    }
    for (list_index = 0; list_index < spilled_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", spilled_values[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-161: out of memory appending spilled value id");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\n")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-162: out of memory finishing layout summary");
        return 0;
    }
    if (!value_ssa_alloc_append_format(&builder, "optimistic-colored-values:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-178: out of memory appending optimistic-colored header");
        return 0;
    }
    for (list_index = 0; list_index < optimistic_colored_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", optimistic_colored_values[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-179: out of memory appending optimistic-colored member");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nconfirmed-spilled-values:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-180: out of memory appending confirmed-spilled header");
        return 0;
    }
    for (list_index = 0; list_index < confirmed_spilled_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", confirmed_spilled_values[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-181: out of memory appending confirmed-spilled member");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nrecovered-colored-values:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-182: out of memory appending recovered-colored header");
        return 0;
    }
    for (list_index = 0; list_index < recovered_colored_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", recovered_colored_values[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-183: out of memory appending recovered-colored member");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\n")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-184: out of memory finishing flavor summary");
        return 0;
    }
    for (group_index = 0; group_index < color_group_count; ++group_index) {
        const ValueSsaAllocationColorGroup *group = NULL;

        if (!value_ssa_function_allocation_layout_get_color_group_at(layout, group_index, &group) || !group) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-215: invalid color-group query contract");
            return 0;
        }

        if (!value_ssa_alloc_append_format(&builder, "color-group %zu:", group->color)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-166: out of memory appending color-group header");
            return 0;
        }
        for (list_index = 0; list_index < group->value_count; ++list_index) {
            if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", group->value_ids[list_index])) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-167: out of memory appending color-group member");
                return 0;
            }
        }
        if (!value_ssa_alloc_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-168: out of memory finishing color-group line");
            return 0;
        }
    }
    for (group_index = 0; group_index < spill_slot_group_count; ++group_index) {
        const ValueSsaAllocationSpillSlotGroup *group = NULL;

        if (!value_ssa_function_allocation_layout_get_spill_slot_group_at(layout, group_index, &group) || !group) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-216: invalid spill-slot-group query contract");
            return 0;
        }

        if (!value_ssa_alloc_append_format(&builder, "spill-slot-group %zu:", group->spill_slot)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-169: out of memory appending spill-slot-group header");
            return 0;
        }
        for (list_index = 0; list_index < group->value_count; ++list_index) {
            if (!value_ssa_alloc_append_format(&builder, " ssa.%zu", group->value_ids[list_index])) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-170: out of memory appending spill-slot-group member");
                return 0;
            }
        }
        if (!value_ssa_alloc_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-171: out of memory finishing spill-slot-group line");
            return 0;
        }
    }

    for (value_id = 0; value_id < value_count; ++value_id) {
        const ValueSsaAllocatedValuePlacement *placement = NULL;

        if (!value_ssa_function_allocation_layout_get_placement(layout, value_id, &placement) || !placement ||
            placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_UNALLOCATED) {
            continue;
        }
        if (!value_ssa_alloc_append_format(&builder,
                "ssa.%zu %s",
                placement->value_id,
                value_ssa_allocated_value_placement_kind_name(placement->kind))) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-147: out of memory appending placement kind");
            return 0;
        }
        if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
            if (!value_ssa_alloc_append_format(&builder, "=%zu", placement->color)) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-148: out of memory appending placement color");
                return 0;
            }
        } else if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
            if (!value_ssa_alloc_append_format(&builder, "=%zu", placement->spill_slot)) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-149: out of memory appending placement spill slot");
                return 0;
            }
        }
        if (placement->spill_intended) {
            if (!value_ssa_alloc_append_format(&builder, " spill-intended")) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-150: out of memory appending placement intended flag");
                return 0;
            }
        }
        if (placement->spill_confirmed) {
            if (!value_ssa_alloc_append_format(&builder, " spill-confirmed")) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-151: out of memory appending placement confirmed flag");
                return 0;
            }
        }
        if (placement->spill_recovered) {
            if (!value_ssa_alloc_append_format(&builder, " spill-recovered")) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-152: out of memory appending placement recovered flag");
                return 0;
            }
        }
        if (!value_ssa_alloc_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-153: out of memory finishing placement line");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_allocation_layout(const ValueSsaProgram *program,
    const ValueSsaProgramAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error) {
    if (!program || !layout || !out_text || program->function_count != layout->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-154: invalid program allocation layout dump contract");
        return 0;
    }
    (void)program;
    return value_ssa_dump_program_allocation_layout_artifact(layout, out_text, error);
}

int value_ssa_dump_program_allocation_layout_artifact(const ValueSsaProgramAllocationLayout *layout,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t function_count = 0;
    size_t total_value_count = 0;
    size_t total_colored_count = 0;
    size_t total_spilled_count = 0;
    size_t total_optimistic_colored_count = 0;
    size_t total_confirmed_spilled_count = 0;
    size_t total_recovered_colored_count = 0;
    size_t max_color_budget = 0;
    size_t max_spill_slot_count = 0;
    size_t functions_with_colors_count = 0;
    size_t functions_with_spills_count = 0;
    size_t functions_with_confirmed_spills_count = 0;
    size_t functions_with_optimistic_colors_count = 0;
    size_t functions_with_recovered_colors_count = 0;
    const size_t *functions_with_colors = NULL;
    const size_t *functions_with_spills = NULL;
    const size_t *functions_with_confirmed_spills = NULL;
    const size_t *functions_with_optimistic_colors = NULL;
    const size_t *functions_with_recovered_colors = NULL;
    size_t function_index;
    size_t list_index;
    char *function_text = NULL;

    if (!layout || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-206: invalid program allocation layout artifact dump contract");
        return 0;
    }
    if (!value_ssa_program_allocation_layout_get_summary(layout,
            &function_count,
            &total_value_count,
            &total_colored_count,
            &total_spilled_count,
            &total_optimistic_colored_count,
            &total_confirmed_spilled_count,
            &total_recovered_colored_count,
            &max_color_budget,
            &max_spill_slot_count) ||
        !value_ssa_program_allocation_layout_get_functions_with_colors(
            layout, &functions_with_colors_count, &functions_with_colors) ||
        !value_ssa_program_allocation_layout_get_functions_with_spills(
            layout, &functions_with_spills_count, &functions_with_spills) ||
        !value_ssa_program_allocation_layout_get_functions_with_confirmed_spills(
            layout, &functions_with_confirmed_spills_count, &functions_with_confirmed_spills) ||
        !value_ssa_program_allocation_layout_get_functions_with_optimistic_colors(
            layout, &functions_with_optimistic_colors_count, &functions_with_optimistic_colors) ||
        !value_ssa_program_allocation_layout_get_functions_with_recovered_colors(
            layout, &functions_with_recovered_colors_count, &functions_with_recovered_colors)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-209: invalid program layout summary query contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "alloc-layout program functions=%zu values=%zu colored=%zu spilled=%zu optimistic_colored=%zu confirmed_spilled=%zu recovered_colored=%zu functions_with_colors=%zu functions_with_spills=%zu functions_with_confirmed_spills=%zu functions_with_optimistic_colors=%zu functions_with_recovered_colors=%zu max_colors=%zu max_spill_slots=%zu\n",
            function_count,
            total_value_count,
            total_colored_count,
            total_spilled_count,
            total_optimistic_colored_count,
            total_confirmed_spilled_count,
            total_recovered_colored_count,
            functions_with_colors_count,
            functions_with_spills_count,
            functions_with_confirmed_spills_count,
            functions_with_optimistic_colors_count,
            functions_with_recovered_colors_count,
            max_color_budget,
            max_spill_slot_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-172: out of memory building program allocation layout header");
        return 0;
    }
    if (!value_ssa_alloc_append_format(&builder, "functions-with-colors:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-173: out of memory appending program color-function header");
        return 0;
    }
    for (list_index = 0; list_index < functions_with_colors_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " %zu", functions_with_colors[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-174: out of memory appending color function index");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nfunctions-with-spills:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-175: out of memory appending program spill-function header");
        return 0;
    }
    for (list_index = 0; list_index < functions_with_spills_count; ++list_index) {
        if (!value_ssa_alloc_append_format(&builder, " %zu", functions_with_spills[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-176: out of memory appending spill function index");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nfunctions-with-confirmed-spills:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-185: out of memory appending confirmed-spill function header");
        return 0;
    }
    for (list_index = 0; list_index < functions_with_confirmed_spills_count; ++list_index) {
        if (!value_ssa_alloc_append_format(
                &builder, " %zu", functions_with_confirmed_spills[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-186: out of memory appending confirmed-spill function index");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nfunctions-with-optimistic-colors:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-190: out of memory appending optimistic-color function header");
        return 0;
    }
    for (list_index = 0; list_index < functions_with_optimistic_colors_count; ++list_index) {
        if (!value_ssa_alloc_append_format(
                &builder, " %zu", functions_with_optimistic_colors[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-191: out of memory appending optimistic-color function index");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\nfunctions-with-recovered-colors:")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-192: out of memory appending recovered-color function header");
        return 0;
    }
    for (list_index = 0; list_index < functions_with_recovered_colors_count; ++list_index) {
        if (!value_ssa_alloc_append_format(
                &builder, " %zu", functions_with_recovered_colors[list_index])) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-193: out of memory appending recovered-color function index");
            return 0;
        }
    }
    if (!value_ssa_alloc_append_format(&builder, "\n\n")) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-177: out of memory finishing program allocation layout summary");
        return 0;
    }
    for (function_index = 0; function_index < function_count; ++function_index) {
        const ValueSsaFunctionAllocationLayout *function_layout = NULL;

        free(function_text);
        function_text = NULL;
        if (!value_ssa_program_allocation_layout_get_function_layout(layout, function_index, &function_layout) ||
            !function_layout ||
            !value_ssa_dump_function_allocation_layout_artifact(function_layout, &function_text, error)) {
            free(builder.data);
            return 0;
        }
        if (!value_ssa_alloc_append_format(
                &builder, "%s%s", function_index == 0 ? "" : "\n", function_text ? function_text : "")) {
            free(function_text);
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-155: out of memory building program allocation layout dump");
            return 0;
        }
    }

    free(function_text);
    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_allocate_rewrite_layout_report(const ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocateRewriteStats *stats,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocateRewriteLayoutReport report;
    int ok = 0;

    if (!program || !result || !stats || !out_text || result->function_count != program->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-187: invalid allocate+rewrite layout report contract");
        return 0;
    }

    value_ssa_allocate_rewrite_layout_report_init(&report);
    if (value_ssa_build_allocate_rewrite_layout_report(result, stats, &report, error)) {
        if (!value_ssa_allocate_rewrite_layout_report_attach_program_context(&report, program, error)) {
            value_ssa_allocate_rewrite_layout_report_free(&report);
            return 0;
        }
        ok = value_ssa_dump_allocate_rewrite_layout_report_artifact(&report, out_text, error);
    }
    value_ssa_allocate_rewrite_layout_report_free(&report);
    return ok;
}

int value_ssa_dump_allocate_rewrite_layout_report_artifact(const ValueSsaAllocateRewriteLayoutReport *report,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t allocation_rounds = 0;
    size_t rewrite_rounds = 0;
    const ValueSsaProgramAllocationLayout *layout = NULL;
    char *layout_text = NULL;

    if (!report || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-198: invalid allocate+rewrite layout report artifact contract");
        return 0;
    }
    if (!value_ssa_allocate_rewrite_layout_report_get_stats(report, &allocation_rounds, &rewrite_rounds) ||
        !value_ssa_allocate_rewrite_layout_report_get_program_layout(report, &layout) || !layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-217: invalid layout report query contract");
        return 0;
    }
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "allocate+rewrite-report allocation_rounds=%zu rewrite_rounds=%zu\n",
            allocation_rounds,
            rewrite_rounds)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-188: out of memory building allocate+rewrite report header");
        return 0;
    }
    if (!value_ssa_dump_program_allocation_layout_artifact(layout, &layout_text, error)) {
        free(builder.data);
        return 0;
    }
    if (!value_ssa_alloc_append_format(&builder, "%s", layout_text ? layout_text : "")) {
        free(layout_text);
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-189: out of memory appending allocate+rewrite layout report");
        return 0;
    }

    free(layout_text);
    *out_text = builder.data;
    return 1;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_layout_with_stats(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaAllocateRewriteStats *out_stats,
    ValueSsaError *error) {
    ValueSsaProgramAllocationResult result;
    int ok = 0;

    if (!program || !out_layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-196: invalid allocate+rewrite layout contract");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
            program, color_budget, &result, out_stats, error)) {
        ok = value_ssa_build_program_allocation_layout(&result, out_layout, error);
    }
    value_ssa_program_allocation_result_free(&result);
    return ok;
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_layout(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaError *error) {
    ValueSsaAllocateRewriteStats stats;
    value_ssa_allocate_rewrite_stats_init(&stats);
    return value_ssa_allocate_and_rewrite_program_single_block_spills_layout_with_stats(
        program, color_budget, out_layout, &stats, error);
}

int value_ssa_allocate_and_rewrite_program_single_block_spills_layout_report(ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaAllocateRewriteLayoutReport *out_report,
    ValueSsaError *error) {
    ValueSsaProgramAllocationResult result;
    ValueSsaAllocateRewriteStats stats;
    int ok = 0;

    if (!program || !out_report) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-197: invalid allocate+rewrite layout report entry contract");
        return 0;
    }

    value_ssa_allocate_rewrite_layout_report_free(out_report);
    value_ssa_program_allocation_result_init(&result);
    value_ssa_allocate_rewrite_stats_init(&stats);
    if (value_ssa_allocate_and_rewrite_program_single_block_spills_with_stats(
            program, color_budget, &result, &stats, error) &&
        value_ssa_build_allocate_rewrite_layout_report(&result, &stats, out_report, error) &&
        value_ssa_allocate_rewrite_layout_report_attach_program_context(out_report, program, error)) {
        ok = 1;
    }

    value_ssa_program_allocation_result_free(&result);
    return ok;
}

#endif
