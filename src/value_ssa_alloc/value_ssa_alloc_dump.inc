#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

int value_ssa_dump_allocation_result(const ValueSsaFunction *function,
    const ValueSsaAllocationResult *result,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaFunctionAllocationLayout layout;
    int ok = 0;

    if (!function || !result || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-009: invalid allocation dump contract");
        return 0;
    }

    value_ssa_function_allocation_layout_init(&layout);
    if (!value_ssa_build_function_allocation_layout(result, &layout, error)) {
        return 0;
    }
    if (!value_ssa_function_allocation_layout_attach_name(&layout, function->name, error)) {
        value_ssa_function_allocation_layout_free(&layout);
        return 0;
    }
    ok = value_ssa_dump_allocation_result_artifact(&layout, result->spill_priorities, out_text, error);

    value_ssa_function_allocation_layout_free(&layout);
    return ok;
}

int value_ssa_dump_allocation_result_artifact(const ValueSsaFunctionAllocationLayout *layout,
    const size_t *spill_priorities,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    const char *function_name = NULL;
    size_t value_count = 0;
    size_t color_budget = 0;
    size_t value_id;

    if (!layout || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-211: invalid allocation artifact dump contract");
        return 0;
    }
    if (!value_ssa_function_allocation_layout_get_summary(layout,
            &function_name,
            &value_count,
            &color_budget,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL)) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-212: invalid allocation artifact summary query");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "alloc func %s colors=%zu values=%zu\n",
            function_name ? function_name : "<anon>",
            color_budget,
            value_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-010: out of memory building dump");
        return 0;
    }

    for (value_id = 0; value_id < value_count; ++value_id) {
        const ValueSsaAllocatedValuePlacement *placement = NULL;

        if (!value_ssa_function_allocation_layout_get_placement(layout, value_id, &placement) || !placement ||
            placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_UNALLOCATED) {
            continue;
        }
        if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
            if (!value_ssa_alloc_append_format(
                    &builder,
                    "ssa.%zu spill slot=%zu priority=%zu%s\n",
                    placement->value_id,
                    placement->spill_slot,
                    spill_priorities ? spill_priorities[value_id] : 0,
                    placement->spill_confirmed ? " spill-confirmed" : "")) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-011: out of memory building dump");
                return 0;
            }
            continue;
        }
        if (!value_ssa_alloc_append_format(
                &builder,
                "ssa.%zu color=%zu priority=%zu%s%s\n",
                placement->value_id,
                placement->color,
                spill_priorities ? spill_priorities[value_id] : 0,
                placement->spill_intended ? " spill-intended" : "",
                placement->spill_recovered ? " spill-recovered" : "")) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-012: out of memory building dump");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_allocation_result(const ValueSsaProgram *program,
    const ValueSsaProgramAllocationResult *result,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaProgramAllocationLayout layout;
    ValueSsaAllocStringBuilder builder;
    size_t function_index;
    char *function_text = NULL;

    if (!program || !result || !out_text || result->function_count != program->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-016: invalid program allocation dump contract");
        return 0;
    }

    value_ssa_program_allocation_layout_init(&layout);
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_build_program_allocation_layout(result, &layout, error)) {
        return 0;
    }
    for (function_index = 0; function_index < layout.function_count; ++function_index) {
        const ValueSsaFunctionAllocationLayout *function_layout = NULL;
        const ValueSsaFunction *function = &program->functions[function_index];
        const ValueSsaAllocationResult *function_result = &result->function_results[function_index];
        ValueSsaFunctionAllocationLayout named_layout;

        if (!value_ssa_program_allocation_layout_get_function_layout(&layout, function_index, &function_layout) ||
            !function_layout) {
            value_ssa_program_allocation_layout_free(&layout);
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-210: invalid program layout function lookup");
            return 0;
        }

        free(function_text);
        function_text = NULL;
        value_ssa_function_allocation_layout_init(&named_layout);
        named_layout = *function_layout;
        named_layout.function_name = function->name ? value_ssa_alloc_strdup(function->name) : NULL;
        if (function->name && !named_layout.function_name) {
            value_ssa_program_allocation_layout_free(&layout);
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-213: out of memory copying program dump function name");
            return 0;
        }
        if (!value_ssa_dump_allocation_result_artifact(&named_layout, function_result->spill_priorities, &function_text, error)) {
            free(named_layout.function_name);
            value_ssa_program_allocation_layout_free(&layout);
            free(builder.data);
            return 0;
        }
        free(named_layout.function_name);
        if (!value_ssa_alloc_append_format(
                &builder, "%s%s", function_index == 0 ? "" : "\n", function_text ? function_text : "")) {
            free(function_text);
            value_ssa_program_allocation_layout_free(&layout);
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-017: out of memory building program dump");
            return 0;
        }
    }

    free(function_text);
    value_ssa_program_allocation_layout_free(&layout);
    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_allocate_rewrite_stats(const ValueSsaAllocateRewriteStats *stats,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;

    if (!stats || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-049: invalid allocate+rewrite stats dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "allocate+rewrite allocation_rounds=%zu rewrite_rounds=%zu\n",
            stats->allocation_rounds,
            stats->rewrite_rounds)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-050: out of memory building stats dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_allocation_select_stats(const ValueSsaAllocationSelectStats *stats,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;

    if (!stats || !out_text) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-076: invalid select stats dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_alloc_append_format(&builder,
            "select-stats colored=%zu spilled=%zu spill_intended=%zu spill_confirmed=%zu spill_recovered=%zu retry_eviction_recovered=%zu optimistic_direct=%zu optimistic_recovered=%zu optimistic_colored=%zu\n",
            stats->colored_count,
            stats->spilled_count,
            stats->spill_intended_count,
            stats->spill_confirmed_count,
            stats->spill_recovered_count,
            stats->retry_eviction_recovered_count,
            stats->optimistic_direct_count,
            stats->optimistic_recovered_count,
            stats->optimistic_colored_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-077: out of memory building select stats dump");
        return 0;
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_allocation_select_trace(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocStringBuilder builder;
    size_t select_index;

    if (!plan || !result || !out_text || plan->value_count != result->value_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-078: invalid select trace dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;

    if (!value_ssa_alloc_append_format(&builder,
            "select-trace colors=%zu values=%zu\n",
            result->color_budget,
            result->value_count)) {
        free(builder.data);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-079: out of memory building select trace");
        return 0;
    }

    for (select_index = 0; select_index < plan->item_count; ++select_index) {
        const ValueSsaAllocatorPlanItem *item = &plan->items[plan->item_count - 1 - select_index];
        size_t value_id = item->value_id;
        const char *outcome = "unassigned";

        if (result->spill_flags[value_id]) {
            outcome = result->spill_confirmed_flags && result->spill_confirmed_flags[value_id]
                ? "spill-confirmed"
                : "spill";
        } else if (result->has_color[value_id]) {
            if (result->spill_recovered_flags && result->spill_recovered_flags[value_id]) {
                outcome = result->spill_intended_flags && result->spill_intended_flags[value_id]
                    ? "optimistic-recovered"
                    : "recovered-colored";
            } else {
                outcome = result->spill_intended_flags && result->spill_intended_flags[value_id]
                    ? "optimistic-colored"
                    : "colored";
            }
        }

        if (!value_ssa_alloc_append_format(&builder,
                "  %zu: ssa.%zu remove=%s spill_cost=%zu(uses=%zu,blocks=%zu,affinity=%zu) outcome=%s",
                select_index,
                value_id,
                value_ssa_allocator_removal_kind_name(item->removal_kind),
                item->spill_cost,
                item->spill_use_cost,
                item->spill_live_range_cost,
                item->spill_affinity_cost,
                outcome)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-080: out of memory appending select trace");
            return 0;
        }
        if (result->used_preferred_color_flags && result->used_preferred_color_flags[value_id]) {
            if (!value_ssa_alloc_append_format(&builder,
                    " preferred=%s",
                    value_ssa_allocation_preferred_color_source_name(
                        result->preferred_color_sources ? result->preferred_color_sources[value_id]
                                                        : VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE))) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-099: out of memory appending preferred-color source");
                return 0;
            }
            if (result->preferred_color_partner_value_ids &&
                result->preferred_color_partner_value_ids[value_id] != (size_t)-1) {
                if (!value_ssa_alloc_append_format(
                        &builder, "(ssa.%zu)", result->preferred_color_partner_value_ids[value_id])) {
                    free(builder.data);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-100: out of memory appending preferred-color partner");
                    return 0;
                }
            }
        }
        if (result->has_color[value_id]) {
            if (!value_ssa_alloc_append_format(&builder, " color=%zu", result->colors[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-081: out of memory appending select color");
                return 0;
            }
        }
        if (result->spill_recovered_flags && result->spill_recovered_flags[value_id] &&
            result->spill_recovery_orders && result->spill_recovery_orders[value_id] != (size_t)-1) {
            if (!value_ssa_alloc_append_format(
                    &builder, " retry-step=%zu", result->spill_recovery_orders[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-130: out of memory appending retry step");
                return 0;
            }
        }
        if (result->spill_recovered_flags && result->spill_recovered_flags[value_id] &&
            result->spill_recovery_phase_ids && result->spill_recovery_phase_ids[value_id] != (size_t)-1 &&
            result->spill_recovery_phase_entries &&
            result->spill_recovery_phase_entries[value_id].root_value_id != (size_t)-1) {
            if (!value_ssa_alloc_append_format(&builder,
                    " retry-phase=%zu@ssa.%zu",
                    result->spill_recovery_phase_ids[value_id],
                    result->spill_recovery_phase_entries[value_id].root_value_id)) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-131: out of memory appending retry phase");
                return 0;
            }
        }
        if (result->spill_recovered_flags && result->spill_recovered_flags[value_id] &&
            result->spill_recovery_phase_member_orders &&
            result->spill_recovery_phase_member_orders[value_id] != (size_t)-1) {
            if (!value_ssa_alloc_append_format(
                    &builder, " retry-phase-step=%zu", result->spill_recovery_phase_member_orders[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-132: out of memory appending retry phase step");
                return 0;
            }
        }
        if (result->spill_recovered_flags && result->spill_recovered_flags[value_id] &&
            result->spill_recovery_phase_member_counts &&
            result->spill_recovery_phase_member_counts[value_id] != (size_t)-1) {
            if (!value_ssa_alloc_append_format(
                    &builder, " retry-phase-size=%zu", result->spill_recovery_phase_member_counts[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-133: out of memory appending retry phase size");
                return 0;
            }
        }
        if (result->spill_recovered_flags && result->spill_recovered_flags[value_id] &&
            result->spill_recovery_phase_entries &&
            result->spill_recovery_phase_entries[value_id].representative_value_id != (size_t)-1 &&
            result->spill_recovery_phase_entries[value_id].recoverable_member_count != (size_t)-1 &&
            result->spill_recovery_phase_entries[value_id].recoverable_intended_count != (size_t)-1) {
            if (!value_ssa_alloc_append_format(&builder,
                    " retry-entry=ssa.%zu[%zu/%zu]",
                    result->spill_recovery_phase_entries[value_id].representative_value_id,
                    result->spill_recovery_phase_entries[value_id].recoverable_member_count,
                    result->spill_recovery_phase_entries[value_id].recoverable_intended_count)) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-134: out of memory appending retry entry");
                return 0;
            }
        }
        if (result->used_generic_eviction_flags && result->used_generic_eviction_flags[value_id] &&
            result->generic_eviction_blocker_value_ids &&
            result->generic_eviction_blocker_value_ids[value_id] != (size_t)-1) {
            if (!value_ssa_alloc_append_format(
                    &builder, " main-evict=ssa.%zu", result->generic_eviction_blocker_value_ids[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-128: out of memory appending main-eviction detail");
                return 0;
            }
            if (result->generic_eviction_blocker_recolored_flags &&
                result->generic_eviction_blocker_recolored_flags[value_id] &&
                result->generic_eviction_blocker_new_colors &&
                result->generic_eviction_blocker_new_colors[value_id] != (size_t)-1) {
                if (!value_ssa_alloc_append_format(
                        &builder, "->color%zu", result->generic_eviction_blocker_new_colors[value_id])) {
                    free(builder.data);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-129: out of memory appending main-recolor detail");
                    return 0;
                }
            }
        }
        if (result->used_targeted_eviction_flags && result->used_targeted_eviction_flags[value_id] &&
            result->targeted_eviction_blocker_value_ids &&
            result->targeted_eviction_blocker_value_ids[value_id] != (size_t)-1) {
            if (!value_ssa_alloc_append_format(
                    &builder, " targeted-evict=ssa.%zu", result->targeted_eviction_blocker_value_ids[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-101: out of memory appending targeted-eviction detail");
                return 0;
            }
            if (result->targeted_eviction_blocker_recolored_flags &&
                result->targeted_eviction_blocker_recolored_flags[value_id] &&
                result->targeted_eviction_blocker_new_colors &&
                result->targeted_eviction_blocker_new_colors[value_id] != (size_t)-1) {
                if (!value_ssa_alloc_append_format(
                        &builder, "->color%zu", result->targeted_eviction_blocker_new_colors[value_id])) {
                    free(builder.data);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-126: out of memory appending targeted-recolor detail");
                    return 0;
                }
            }
        }
        if (result->used_retry_eviction_flags && result->used_retry_eviction_flags[value_id] &&
            result->retry_eviction_blocker_value_ids &&
            result->retry_eviction_blocker_value_ids[value_id] != (size_t)-1) {
            if (!value_ssa_alloc_append_format(
                    &builder, " retry-evict=ssa.%zu", result->retry_eviction_blocker_value_ids[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-107: out of memory appending retry-eviction detail");
                return 0;
            }
            if (result->retry_eviction_blocker_recolored_flags &&
                result->retry_eviction_blocker_recolored_flags[value_id] &&
                result->retry_eviction_blocker_new_colors &&
                result->retry_eviction_blocker_new_colors[value_id] != (size_t)-1) {
                if (!value_ssa_alloc_append_format(
                        &builder, "->color%zu", result->retry_eviction_blocker_new_colors[value_id])) {
                    free(builder.data);
                    value_ssa_alloc_set_error(
                        error, 0, 0, "VALUE-SSA-ALLOC-127: out of memory appending retry-recolor detail");
                    return 0;
                }
            }
        }
        if (result->spill_flags[value_id]) {
            if (!value_ssa_alloc_append_format(&builder, " slot=%zu", result->spill_slots[value_id])) {
                free(builder.data);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-082: out of memory appending select spill slot");
                return 0;
            }
        }
        if (!value_ssa_alloc_append_format(&builder,
                " detail=(use_loops=%zu,use_calls=%zu,live=%zu,loops=%zu,calls=%zu,call_cross=%zu,xblock=%zu)\n",
                item->spill_use_loop_depth_cost,
                item->spill_use_call_density_cost,
                item->spill_live_block_cost,
                item->spill_loop_depth_cost,
                item->spill_call_density_cost,
                item->spill_call_crossing_cost,
                item->spill_cross_block_cost)) {
            free(builder.data);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-083: out of memory finishing select trace line");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

#endif
