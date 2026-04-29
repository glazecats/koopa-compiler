#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_SPLIT_AGGREGATOR)
#define VALUE_SSA_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa.c"
#else

const char *value_ssa_allocation_work_class_name(ValueSsaAllocationWorkClass work_class) {
    switch (work_class) {
    case VALUE_SSA_ALLOC_WORK_MOVE_HINT:
        return "move-hint";
    case VALUE_SSA_ALLOC_WORK_CONSTRAINED:
        return "constrained";
    case VALUE_SSA_ALLOC_WORK_SINGLE_BLOCK:
        return "single-block";
    case VALUE_SSA_ALLOC_WORK_GLOBAL:
        return "global";
    case VALUE_SSA_ALLOC_WORK_ISOLATED:
        return "isolated";
    default:
        return "<unknown>";
    }
}

int value_ssa_allocation_prep_get_live_blocks(const ValueSsaAllocationPrep *allocation_prep,
    size_t value_id,
    const size_t **out_blocks,
    size_t *out_count) {
    if (!allocation_prep || !out_blocks || !out_count || value_id >= allocation_prep->value_count) {
        return 0;
    }

    *out_blocks = allocation_prep->live_blocks
        ? &allocation_prep->live_blocks[allocation_prep->live_block_offsets[value_id]]
        : NULL;
    *out_count = allocation_prep->live_block_counts[value_id];
    return 1;
}

int value_ssa_allocation_prep_find_affinity_candidate(const ValueSsaAllocationPrep *allocation_prep,
    size_t lhs,
    size_t rhs,
    size_t *out_index,
    const ValueSsaCopyAffinityCandidate **out_candidate) {
    size_t candidate_index;

    if (!allocation_prep || lhs >= allocation_prep->value_count || rhs >= allocation_prep->value_count) {
        return 0;
    }

    for (candidate_index = 0; candidate_index < allocation_prep->candidate_count; ++candidate_index) {
        const ValueSsaCopyAffinityCandidate *candidate = &allocation_prep->candidates[candidate_index];

        if (!((candidate->lhs == lhs && candidate->rhs == rhs) ||
                (candidate->lhs == rhs && candidate->rhs == lhs))) {
            continue;
        }

        if (out_index) {
            *out_index = candidate_index;
        }
        if (out_candidate) {
            *out_candidate = candidate;
        }
        return 1;
    }

    return 0;
}

size_t value_ssa_allocation_prep_get_affinity_weight(const ValueSsaAllocationPrep *allocation_prep,
    size_t lhs,
    size_t rhs) {
    const ValueSsaCopyAffinityCandidate *candidate = NULL;

    return value_ssa_allocation_prep_find_affinity_candidate(
        allocation_prep, lhs, rhs, NULL, &candidate) ? candidate->weight : 0;
}

int value_ssa_allocation_prep_get_top_affinity_neighbors(const ValueSsaAllocationPrep *allocation_prep,
    size_t value_id,
    size_t max_count,
    size_t *out_neighbor_value_ids,
    size_t *out_weights,
    size_t *out_count) {
    size_t write_count = 0;
    size_t candidate_index;

    if (!allocation_prep || !out_count || value_id >= allocation_prep->value_count) {
        return 0;
    }
    if (max_count > 0 && (!out_neighbor_value_ids || !out_weights)) {
        return 0;
    }

    for (candidate_index = 0; candidate_index < allocation_prep->candidate_count && write_count < max_count;
         ++candidate_index) {
        const ValueSsaCopyAffinityCandidate *candidate = &allocation_prep->candidates[candidate_index];
        size_t neighbor_value;

        if (candidate->lhs == value_id) {
            neighbor_value = candidate->rhs;
        } else if (candidate->rhs == value_id) {
            neighbor_value = candidate->lhs;
        } else {
            continue;
        }

        out_neighbor_value_ids[write_count] = neighbor_value;
        out_weights[write_count] = candidate->weight;
        ++write_count;
    }

    *out_count = write_count;
    return 1;
}

int value_ssa_allocation_worklist_find_value(const ValueSsaAllocationWorklist *worklist,
    size_t value_id,
    size_t *out_index,
    const ValueSsaAllocationWorkItem **out_item) {
    size_t item_index;

    if (!worklist || value_id >= worklist->value_count) {
        return 0;
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        if (worklist->items[item_index].value_id != value_id) {
            continue;
        }

        if (out_index) {
            *out_index = item_index;
        }
        if (out_item) {
            *out_item = &worklist->items[item_index];
        }
        return 1;
    }

    return 0;
}

int value_ssa_allocation_worklist_get_class_range(const ValueSsaAllocationWorklist *worklist,
    ValueSsaAllocationWorkClass work_class,
    size_t *out_begin,
    size_t *out_end) {
    size_t begin = 0;
    size_t end = 0;
    size_t item_index;
    int found = 0;

    if (!worklist || !out_begin || !out_end) {
        return 0;
    }

    for (item_index = 0; item_index < worklist->item_count; ++item_index) {
        if (worklist->items[item_index].work_class != work_class) {
            if (found) {
                break;
            }
            continue;
        }

        if (!found) {
            begin = item_index;
            found = 1;
        }
        end = item_index + 1;
    }

    if (!found) {
        begin = 0;
        end = 0;
    }

    *out_begin = begin;
    *out_end = end;
    return 1;
}

static int value_ssa_append_allocation_live_blocks(ValueSsaStringBuilder *builder,
    const ValueSsaAllocationPrep *prep,
    size_t value_id) {
    size_t offset;
    size_t count;
    size_t index;

    if (!builder || !prep || value_id >= prep->value_count) {
        return 0;
    }

    if (!value_ssa_append_string(builder, "[")) {
        return 0;
    }

    offset = prep->live_block_offsets[value_id];
    count = prep->live_block_counts[value_id];
    for (index = 0; index < count; ++index) {
        if (index > 0 && !value_ssa_append_string(builder, ", ")) {
            return 0;
        }
        if (!value_ssa_append_format(builder, "bb.%zu", prep->live_blocks[offset + index])) {
            return 0;
        }
    }

    return value_ssa_append_string(builder, "]");
}

int value_ssa_dump_allocation_prep(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocationPrep owned_prep;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    ValueSsaStringBuilder builder;
    int ok = 0;

    if (!function || !out_text) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-227: invalid allocation-prep dump contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-228: cannot dump allocation prep for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    *out_text = NULL;
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    value_ssa_allocation_prep_init(&owned_prep);

    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    } else if (active_prep->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-229: allocation-prep dump value_count=%zu does not match function value_count=%zu",
            active_prep->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!value_ssa_append_format(&builder,
            "alloc-prep %s:\n",
            function->name ? function->name : "<anon>")) {
        goto cleanup;
    }

    {
        size_t value_id;

        for (value_id = 0; value_id < active_prep->value_count; ++value_id) {
            if (!value_ssa_append_format(&builder,
                    "  ssa.%zu: def=",
                    value_id)) {
                goto cleanup;
            }
            if (active_prep->def_block_ids[value_id] == (size_t)-1) {
                if (!value_ssa_append_string(&builder, "<none>")) {
                    goto cleanup;
                }
            } else if (!value_ssa_append_format(&builder, "bb.%zu", active_prep->def_block_ids[value_id])) {
                goto cleanup;
            }

            if (!value_ssa_append_format(&builder,
                    " uses=%zu live_blocks=",
                    active_prep->use_counts[value_id]) ||
                !value_ssa_append_allocation_live_blocks(&builder, active_prep, value_id) ||
                !value_ssa_append_format(&builder,
                    " single_block=%s use_loops=%zu use_calls=%zu loops=%zu calls=%zu call_cross=%zu degree=%zu affinity=%zu move_related=%s\n",
                    active_prep->single_block_live_ranges[value_id] ? "yes" : "no",
                    active_prep->use_loop_depth_sums ? active_prep->use_loop_depth_sums[value_id] : 0,
                    active_prep->use_call_density_sums ? active_prep->use_call_density_sums[value_id] : 0,
                    active_prep->loop_depth_sums ? active_prep->loop_depth_sums[value_id] : 0,
                    active_prep->call_density_sums ? active_prep->call_density_sums[value_id] : 0,
                    active_prep->call_crossing_counts ? active_prep->call_crossing_counts[value_id] : 0,
                    active_prep->interference_degrees[value_id],
                    active_prep->affinity_sums[value_id],
                    active_prep->move_related[value_id] ? "yes" : "no")) {
                goto cleanup;
            }
        }
    }

    if (!value_ssa_append_string(&builder, "  candidates:\n")) {
        goto cleanup;
    }
    {
        size_t candidate_index;

        for (candidate_index = 0; candidate_index < active_prep->candidate_count; ++candidate_index) {
            const ValueSsaCopyAffinityCandidate *candidate = &active_prep->candidates[candidate_index];

            if (!value_ssa_append_format(&builder,
                    "    ssa.%zu <-> ssa.%zu weight=%zu\n",
                    candidate->lhs,
                    candidate->rhs,
                    candidate->weight)) {
                goto cleanup;
            }
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    value_ssa_allocation_prep_free(&owned_prep);
    return ok;
}

int value_ssa_dump_allocation_worklist(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    const ValueSsaAllocationWorklist *worklist,
    char **out_text,
    ValueSsaError *error) {
    return value_ssa_dump_allocation_worklist_for_class(
        function, allocation_prep, worklist, (ValueSsaAllocationWorkClass)-1, out_text, error);
}

int value_ssa_dump_allocation_worklist_for_class(const ValueSsaFunction *function,
    const ValueSsaAllocationPrep *allocation_prep,
    const ValueSsaAllocationWorklist *worklist,
    ValueSsaAllocationWorkClass work_class,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaAllocationPrep owned_prep;
    ValueSsaAllocationWorklist owned_worklist;
    const ValueSsaAllocationPrep *active_prep = allocation_prep;
    const ValueSsaAllocationWorklist *active_worklist = worklist;
    ValueSsaStringBuilder builder;
    int ok = 0;

    if (!function || !out_text) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-230: invalid allocation-worklist dump contract");
        return 0;
    }
    if (!function->has_body) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-231: cannot dump allocation worklist for declaration '%s'",
            function->name ? function->name : "<anon>");
        return 0;
    }

    *out_text = NULL;
    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    value_ssa_allocation_prep_init(&owned_prep);
    value_ssa_allocation_worklist_init(&owned_worklist);

    if (!active_prep) {
        if (!value_ssa_compute_allocation_prep(function, NULL, NULL, NULL, NULL, &owned_prep, error)) {
            goto cleanup;
        }
        active_prep = &owned_prep;
    } else if (active_prep->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-232: allocation-worklist dump prep value_count=%zu does not match function value_count=%zu",
            active_prep->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!active_worklist) {
        if (!value_ssa_compute_allocation_worklist(function, active_prep, &owned_worklist, error)) {
            goto cleanup;
        }
        active_worklist = &owned_worklist;
    } else if (active_worklist->value_count != function->next_value_id) {
        value_ssa_set_error(error,
            0,
            0,
            "VALUE-SSA-233: allocation-worklist dump value_count=%zu does not match function value_count=%zu",
            active_worklist->value_count,
            function->next_value_id);
        goto cleanup;
    }

    if (!value_ssa_append_format(&builder,
            "alloc-worklist %s:\n",
            function->name ? function->name : "<anon>")) {
        goto cleanup;
    }

    {
        size_t item_index;

        for (item_index = 0; item_index < active_worklist->item_count; ++item_index) {
            const ValueSsaAllocationWorkItem *item = &active_worklist->items[item_index];

            if (work_class != (ValueSsaAllocationWorkClass)-1 && item->work_class != work_class) {
                continue;
            }

            if (!value_ssa_append_format(&builder,
                    "  %zu: ssa.%zu class=%s priority=%zu blocks=%zu use_loops=%zu use_calls=%zu loops=%zu calls=%zu call_cross=%zu single_block=%s degree=%zu affinity=%zu\n",
                    item_index,
                    item->value_id,
                    value_ssa_allocation_work_class_name(item->work_class),
                    item->priority,
                    active_prep->live_block_counts[item->value_id],
                    active_prep->use_loop_depth_sums ? active_prep->use_loop_depth_sums[item->value_id] : 0,
                    active_prep->use_call_density_sums ? active_prep->use_call_density_sums[item->value_id] : 0,
                    active_prep->loop_depth_sums ? active_prep->loop_depth_sums[item->value_id] : 0,
                    active_prep->call_density_sums ? active_prep->call_density_sums[item->value_id] : 0,
                    active_prep->call_crossing_counts ? active_prep->call_crossing_counts[item->value_id] : 0,
                    active_prep->single_block_live_ranges[item->value_id] ? "yes" : "no",
                    active_prep->interference_degrees[item->value_id],
                    active_prep->affinity_sums[item->value_id])) {
                goto cleanup;
            }
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    value_ssa_allocation_worklist_free(&owned_worklist);
    value_ssa_allocation_prep_free(&owned_prep);
    return ok;
}

#endif
