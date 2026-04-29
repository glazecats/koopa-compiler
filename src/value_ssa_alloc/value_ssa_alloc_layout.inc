#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

int value_ssa_function_allocation_layout_attach_name(ValueSsaFunctionAllocationLayout *layout,
    const char *function_name,
    ValueSsaError *error) {
    char *copied_name = NULL;

    if (!layout) {
        return 0;
    }
    if (!function_name) {
        return 1;
    }

    copied_name = value_ssa_alloc_strdup(function_name);
    if (!copied_name) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-199: out of memory copying function layout name");
        return 0;
    }

    free(layout->function_name);
    layout->function_name = copied_name;
    return 1;
}

int value_ssa_program_allocation_layout_attach_program_context(ValueSsaProgramAllocationLayout *layout,
    const ValueSsaProgram *program,
    ValueSsaError *error) {
    size_t function_index;

    if (!layout || !program || layout->function_count != program->function_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-205: invalid program-context attachment contract");
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        char *copied_name = NULL;

        if (!value_ssa_function_allocation_layout_attach_name(
                &layout->function_layouts[function_index], program->functions[function_index].name, error)) {
            return 0;
        }
        if (!program->functions[function_index].name) {
            continue;
        }
        copied_name = value_ssa_alloc_strdup(program->functions[function_index].name);
        if (!copied_name) {
            value_ssa_alloc_set_error(
                error, 0, 0, "VALUE-SSA-ALLOC-202: out of memory copying program layout function name table");
            return 0;
        }
        free(layout->function_names[function_index]);
        layout->function_names[function_index] = copied_name;
    }

    return 1;
}

int value_ssa_allocate_function_layout(const ValueSsaFunction *function,
    size_t color_budget,
    ValueSsaFunctionAllocationLayout *out_layout,
    ValueSsaError *error) {
    ValueSsaAllocationResult result;
    int ok = 0;

    if (!function || !out_layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-194: invalid function allocation layout entry contract");
        return 0;
    }

    value_ssa_allocation_result_init(&result);
    if (value_ssa_allocate_function(function, color_budget, &result, error)) {
        ok = value_ssa_build_function_allocation_layout(&result, out_layout, error);
        if (ok && !value_ssa_function_allocation_layout_attach_name(out_layout, function->name, error)) {
            value_ssa_function_allocation_layout_free(out_layout);
            ok = 0;
        }
    }
    value_ssa_allocation_result_free(&result);
    return ok;
}

int value_ssa_build_function_allocation_layout(const ValueSsaAllocationResult *result,
    ValueSsaFunctionAllocationLayout *out_layout,
    ValueSsaError *error) {
    size_t value_id;
    size_t group_index;
    unsigned char *used_color_flags = NULL;
    size_t *color_value_counts = NULL;
    size_t *spill_slot_value_counts = NULL;

    if (!result || !out_layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-141: invalid function allocation layout contract");
        return 0;
    }

    value_ssa_function_allocation_layout_free(out_layout);
    out_layout->value_count = result->value_count;
    out_layout->color_budget = result->color_budget;
    out_layout->spill_slot_count = result->spill_slot_count;
    if (result->value_count == 0) {
        return 1;
    }

    out_layout->placements =
        (ValueSsaAllocatedValuePlacement *)calloc(result->value_count, sizeof(ValueSsaAllocatedValuePlacement));
    out_layout->colored_value_ids = (size_t *)malloc(result->value_count * sizeof(size_t));
    out_layout->spilled_value_ids = (size_t *)malloc(result->value_count * sizeof(size_t));
    out_layout->optimistic_colored_value_ids = (size_t *)malloc(result->value_count * sizeof(size_t));
    out_layout->confirmed_spilled_value_ids = (size_t *)malloc(result->value_count * sizeof(size_t));
    out_layout->recovered_colored_value_ids = (size_t *)malloc(result->value_count * sizeof(size_t));
    out_layout->used_colors = result->color_budget > 0 ? (size_t *)malloc(result->color_budget * sizeof(size_t)) : NULL;
    out_layout->color_groups =
        result->color_budget > 0 ? (ValueSsaAllocationColorGroup *)calloc(result->color_budget, sizeof(ValueSsaAllocationColorGroup))
                                 : NULL;
    out_layout->spill_slot_groups =
        result->spill_slot_count > 0
        ? (ValueSsaAllocationSpillSlotGroup *)calloc(result->spill_slot_count, sizeof(ValueSsaAllocationSpillSlotGroup))
        : NULL;
    used_color_flags =
        result->color_budget > 0 ? (unsigned char *)calloc(result->color_budget, sizeof(unsigned char)) : NULL;
    color_value_counts =
        result->color_budget > 0 ? (size_t *)calloc(result->color_budget, sizeof(size_t)) : NULL;
    spill_slot_value_counts =
        result->spill_slot_count > 0 ? (size_t *)calloc(result->spill_slot_count, sizeof(size_t)) : NULL;
    if (!out_layout->placements || !out_layout->colored_value_ids || !out_layout->spilled_value_ids ||
        !out_layout->optimistic_colored_value_ids || !out_layout->confirmed_spilled_value_ids ||
        !out_layout->recovered_colored_value_ids ||
        (result->color_budget > 0 &&
            (!out_layout->used_colors || !out_layout->color_groups || !used_color_flags || !color_value_counts)) ||
        (result->spill_slot_count > 0 && (!out_layout->spill_slot_groups || !spill_slot_value_counts))) {
        free(used_color_flags);
        free(color_value_counts);
        free(spill_slot_value_counts);
        value_ssa_function_allocation_layout_free(out_layout);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-142: out of memory building function allocation layout");
        return 0;
    }

    for (value_id = 0; value_id < result->value_count; ++value_id) {
        ValueSsaAllocatedValuePlacement *placement = &out_layout->placements[value_id];

        placement->value_id = value_id;
        placement->kind = VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_UNALLOCATED;
        placement->color = (size_t)-1;
        placement->spill_slot = (size_t)-1;
        placement->spill_intended =
            result->spill_intended_flags ? result->spill_intended_flags[value_id] : 0;
        placement->spill_confirmed =
            result->spill_confirmed_flags ? result->spill_confirmed_flags[value_id] : 0;
        placement->spill_recovered =
            result->spill_recovered_flags ? result->spill_recovered_flags[value_id] : 0;

        if (result->spill_flags && result->spill_flags[value_id]) {
            placement->kind = VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL;
            placement->spill_slot =
                result->spill_slots ? result->spill_slots[value_id] : (size_t)-1;
            out_layout->spilled_value_ids[out_layout->spilled_count++] = value_id;
            if (placement->spill_confirmed) {
                out_layout->confirmed_spilled_value_ids[out_layout->confirmed_spilled_count++] = value_id;
            }
            if (placement->spill_slot < result->spill_slot_count && spill_slot_value_counts) {
                ++spill_slot_value_counts[placement->spill_slot];
            }
            continue;
        }
        if (result->has_color && result->has_color[value_id]) {
            placement->kind = VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR;
            placement->color = result->colors ? result->colors[value_id] : (size_t)-1;
            out_layout->colored_value_ids[out_layout->colored_count++] = value_id;
            if (placement->spill_intended) {
                out_layout->optimistic_colored_value_ids[out_layout->optimistic_colored_count++] = value_id;
            }
            if (placement->spill_recovered) {
                out_layout->recovered_colored_value_ids[out_layout->recovered_colored_count++] = value_id;
            }
            if (placement->color < result->color_budget && used_color_flags && !used_color_flags[placement->color]) {
                used_color_flags[placement->color] = 1;
                out_layout->used_colors[out_layout->used_color_count++] = placement->color;
            }
            if (placement->color < result->color_budget && color_value_counts) {
                ++color_value_counts[placement->color];
            }
        }
    }

    for (group_index = 0; group_index < result->color_budget; ++group_index) {
        if (color_value_counts && color_value_counts[group_index] > 0) {
            ValueSsaAllocationColorGroup *group = &out_layout->color_groups[out_layout->color_group_count];
            group->color = group_index;
            group->value_count = color_value_counts[group_index];
            group->value_ids = (size_t *)malloc(group->value_count * sizeof(size_t));
            if (!group->value_ids) {
                free(used_color_flags);
                free(color_value_counts);
                free(spill_slot_value_counts);
                value_ssa_function_allocation_layout_free(out_layout);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-163: out of memory building color group");
                return 0;
            }
            ++out_layout->color_group_count;
        }
    }
    for (group_index = 0; group_index < result->spill_slot_count; ++group_index) {
        if (spill_slot_value_counts && spill_slot_value_counts[group_index] > 0) {
            ValueSsaAllocationSpillSlotGroup *group =
                &out_layout->spill_slot_groups[out_layout->spill_slot_group_count];
            group->spill_slot = group_index;
            group->value_count = spill_slot_value_counts[group_index];
            group->value_ids = (size_t *)malloc(group->value_count * sizeof(size_t));
            if (!group->value_ids) {
                free(used_color_flags);
                free(color_value_counts);
                free(spill_slot_value_counts);
                value_ssa_function_allocation_layout_free(out_layout);
                value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-164: out of memory building spill-slot group");
                return 0;
            }
            ++out_layout->spill_slot_group_count;
        }
    }

    if (out_layout->color_group_count > 0 || out_layout->spill_slot_group_count > 0) {
        size_t *color_write_offsets =
            out_layout->color_group_count > 0 ? (size_t *)calloc(out_layout->color_group_count, sizeof(size_t)) : NULL;
        size_t *spill_write_offsets =
            out_layout->spill_slot_group_count > 0 ? (size_t *)calloc(out_layout->spill_slot_group_count, sizeof(size_t)) : NULL;

        if ((out_layout->color_group_count > 0 && !color_write_offsets) ||
            (out_layout->spill_slot_group_count > 0 && !spill_write_offsets)) {
            free(used_color_flags);
            free(color_value_counts);
            free(spill_slot_value_counts);
            free(color_write_offsets);
            free(spill_write_offsets);
            value_ssa_function_allocation_layout_free(out_layout);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-165: out of memory finalizing allocation groups");
            return 0;
        }

        for (value_id = 0; value_id < result->value_count; ++value_id) {
            const ValueSsaAllocatedValuePlacement *placement = &out_layout->placements[value_id];
            size_t search_group_index;

            if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
                for (search_group_index = 0; search_group_index < out_layout->color_group_count; ++search_group_index) {
                    if (out_layout->color_groups[search_group_index].color == placement->color) {
                        out_layout->color_groups[search_group_index]
                            .value_ids[color_write_offsets[search_group_index]++] = value_id;
                        break;
                    }
                }
            } else if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
                for (search_group_index = 0; search_group_index < out_layout->spill_slot_group_count;
                     ++search_group_index) {
                    if (out_layout->spill_slot_groups[search_group_index].spill_slot == placement->spill_slot) {
                        out_layout->spill_slot_groups[search_group_index]
                            .value_ids[spill_write_offsets[search_group_index]++] = value_id;
                        break;
                    }
                }
            }
        }

        free(color_write_offsets);
        free(spill_write_offsets);
    }

    free(used_color_flags);
    free(color_value_counts);
    free(spill_slot_value_counts);
    return 1;
}

int value_ssa_build_program_allocation_layout(const ValueSsaProgramAllocationResult *result,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaError *error) {
    size_t function_index;

    if (!result || !out_layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-143: invalid program allocation layout contract");
        return 0;
    }

    value_ssa_program_allocation_layout_free(out_layout);
    out_layout->function_count = result->function_count;
    if (result->function_count == 0) {
        return 1;
    }

    out_layout->function_layouts =
        (ValueSsaFunctionAllocationLayout *)calloc(result->function_count, sizeof(ValueSsaFunctionAllocationLayout));
    out_layout->function_names = (char **)calloc(result->function_count, sizeof(char *));
    out_layout->function_indices_with_colors = (size_t *)malloc(result->function_count * sizeof(size_t));
    out_layout->function_indices_with_spills = (size_t *)malloc(result->function_count * sizeof(size_t));
    out_layout->function_indices_with_confirmed_spills = (size_t *)malloc(result->function_count * sizeof(size_t));
    out_layout->function_indices_with_optimistic_colors = (size_t *)malloc(result->function_count * sizeof(size_t));
    out_layout->function_indices_with_recovered_colors = (size_t *)malloc(result->function_count * sizeof(size_t));
    if (!out_layout->function_layouts || !out_layout->function_names || !out_layout->function_indices_with_colors ||
        !out_layout->function_indices_with_spills || !out_layout->function_indices_with_confirmed_spills ||
        !out_layout->function_indices_with_optimistic_colors ||
        !out_layout->function_indices_with_recovered_colors) {
        value_ssa_program_allocation_layout_free(out_layout);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-144: out of memory building program allocation layout");
        return 0;
    }

    for (function_index = 0; function_index < result->function_count; ++function_index) {
        value_ssa_function_allocation_layout_init(&out_layout->function_layouts[function_index]);
        if (!value_ssa_build_function_allocation_layout(
                &result->function_results[function_index], &out_layout->function_layouts[function_index], error)) {
            value_ssa_program_allocation_layout_free(out_layout);
            return 0;
        }
        if (out_layout->function_layouts[function_index].function_name) {
            out_layout->function_names[function_index] =
                value_ssa_alloc_strdup(out_layout->function_layouts[function_index].function_name);
            if (!out_layout->function_names[function_index]) {
                value_ssa_program_allocation_layout_free(out_layout);
                value_ssa_alloc_set_error(
                    error, 0, 0, "VALUE-SSA-ALLOC-200: out of memory copying program layout function name");
                return 0;
            }
        }
        out_layout->total_value_count += out_layout->function_layouts[function_index].value_count;
        out_layout->total_colored_count += out_layout->function_layouts[function_index].colored_count;
        out_layout->total_spilled_count += out_layout->function_layouts[function_index].spilled_count;
        out_layout->total_optimistic_colored_count +=
            out_layout->function_layouts[function_index].optimistic_colored_count;
        out_layout->total_confirmed_spilled_count +=
            out_layout->function_layouts[function_index].confirmed_spilled_count;
        out_layout->total_recovered_colored_count +=
            out_layout->function_layouts[function_index].recovered_colored_count;
        if (out_layout->function_layouts[function_index].colored_count > 0) {
            out_layout->function_indices_with_colors[out_layout->functions_with_colors_count++] = function_index;
        }
        if (out_layout->function_layouts[function_index].spilled_count > 0) {
            out_layout->function_indices_with_spills[out_layout->functions_with_spills_count++] = function_index;
        }
        if (out_layout->function_layouts[function_index].confirmed_spilled_count > 0) {
            out_layout->function_indices_with_confirmed_spills[out_layout->functions_with_confirmed_spills_count++] =
                function_index;
        }
        if (out_layout->function_layouts[function_index].optimistic_colored_count > 0) {
            out_layout->function_indices_with_optimistic_colors
                [out_layout->functions_with_optimistic_colors_count++] = function_index;
        }
        if (out_layout->function_layouts[function_index].recovered_colored_count > 0) {
            out_layout->function_indices_with_recovered_colors
                [out_layout->functions_with_recovered_colors_count++] = function_index;
        }
        if (out_layout->function_layouts[function_index].color_budget > out_layout->max_color_budget) {
            out_layout->max_color_budget = out_layout->function_layouts[function_index].color_budget;
        }
        if (out_layout->function_layouts[function_index].spill_slot_count > out_layout->max_spill_slot_count) {
            out_layout->max_spill_slot_count = out_layout->function_layouts[function_index].spill_slot_count;
        }
    }

    return 1;
}

int value_ssa_allocate_program_layout(const ValueSsaProgram *program,
    size_t color_budget,
    ValueSsaProgramAllocationLayout *out_layout,
    ValueSsaError *error) {
    ValueSsaProgramAllocationResult result;
    int ok = 0;

    if (!program || !out_layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-195: invalid program allocation layout entry contract");
        return 0;
    }

    value_ssa_program_allocation_result_init(&result);
    if (value_ssa_allocate_program(program, color_budget, &result, error)) {
        ok = value_ssa_build_program_allocation_layout(&result, out_layout, error);
        if (ok && !value_ssa_program_allocation_layout_attach_program_context(out_layout, program, error)) {
            value_ssa_program_allocation_layout_free(out_layout);
            ok = 0;
        }
    }
    value_ssa_program_allocation_result_free(&result);
    return ok;
}

int value_ssa_function_allocation_layout_get_placement(const ValueSsaFunctionAllocationLayout *layout,
    size_t value_id,
    const ValueSsaAllocatedValuePlacement **out_placement) {
    if (out_placement) {
        *out_placement = NULL;
    }
    if (!layout || !out_placement || value_id >= layout->value_count || !layout->placements) {
        return 0;
    }

    *out_placement = &layout->placements[value_id];
    return 1;
}

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
    size_t *out_spill_slot_group_count) {
    if (!layout) {
        return 0;
    }
    if (out_function_name) {
        *out_function_name = layout->function_name;
    }
    if (out_value_count) {
        *out_value_count = layout->value_count;
    }
    if (out_color_budget) {
        *out_color_budget = layout->color_budget;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count = layout->spill_slot_count;
    }
    if (out_colored_count) {
        *out_colored_count = layout->colored_count;
    }
    if (out_spilled_count) {
        *out_spilled_count = layout->spilled_count;
    }
    if (out_optimistic_colored_count) {
        *out_optimistic_colored_count = layout->optimistic_colored_count;
    }
    if (out_confirmed_spilled_count) {
        *out_confirmed_spilled_count = layout->confirmed_spilled_count;
    }
    if (out_recovered_colored_count) {
        *out_recovered_colored_count = layout->recovered_colored_count;
    }
    if (out_used_color_count) {
        *out_used_color_count = layout->used_color_count;
    }
    if (out_color_group_count) {
        *out_color_group_count = layout->color_group_count;
    }
    if (out_spill_slot_group_count) {
        *out_spill_slot_group_count = layout->spill_slot_group_count;
    }

    return 1;
}

int value_ssa_function_allocation_layout_get_colored_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!layout || !out_count || !out_value_ids) {
        return 0;
    }

    *out_count = layout->colored_count;
    *out_value_ids = layout->colored_value_ids;
    return 1;
}

int value_ssa_function_allocation_layout_get_spilled_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!layout || !out_count || !out_value_ids) {
        return 0;
    }

    *out_count = layout->spilled_count;
    *out_value_ids = layout->spilled_value_ids;
    return 1;
}

int value_ssa_function_allocation_layout_get_optimistic_colored_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!layout || !out_count || !out_value_ids) {
        return 0;
    }

    *out_count = layout->optimistic_colored_count;
    *out_value_ids = layout->optimistic_colored_value_ids;
    return 1;
}

int value_ssa_function_allocation_layout_get_confirmed_spilled_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!layout || !out_count || !out_value_ids) {
        return 0;
    }

    *out_count = layout->confirmed_spilled_count;
    *out_value_ids = layout->confirmed_spilled_value_ids;
    return 1;
}

int value_ssa_function_allocation_layout_get_recovered_colored_values(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!layout || !out_count || !out_value_ids) {
        return 0;
    }

    *out_count = layout->recovered_colored_count;
    *out_value_ids = layout->recovered_colored_value_ids;
    return 1;
}

int value_ssa_function_allocation_layout_get_used_colors(const ValueSsaFunctionAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_colors) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_colors) {
        *out_colors = NULL;
    }
    if (!layout || !out_count || !out_colors) {
        return 0;
    }

    *out_count = layout->used_color_count;
    *out_colors = layout->used_colors;
    return 1;
}

int value_ssa_function_allocation_layout_get_color_group(const ValueSsaFunctionAllocationLayout *layout,
    size_t color,
    const ValueSsaAllocationColorGroup **out_group) {
    size_t group_index;

    if (out_group) {
        *out_group = NULL;
    }
    if (!layout || !out_group) {
        return 0;
    }

    for (group_index = 0; group_index < layout->color_group_count; ++group_index) {
        if (layout->color_groups[group_index].color == color) {
            *out_group = &layout->color_groups[group_index];
            return 1;
        }
    }

    return 0;
}

int value_ssa_function_allocation_layout_get_color_group_at(const ValueSsaFunctionAllocationLayout *layout,
    size_t group_index,
    const ValueSsaAllocationColorGroup **out_group) {
    if (out_group) {
        *out_group = NULL;
    }
    if (!layout || !out_group || group_index >= layout->color_group_count || !layout->color_groups) {
        return 0;
    }

    *out_group = &layout->color_groups[group_index];
    return 1;
}

int value_ssa_function_allocation_layout_get_spill_slot_group(const ValueSsaFunctionAllocationLayout *layout,
    size_t spill_slot,
    const ValueSsaAllocationSpillSlotGroup **out_group) {
    size_t group_index;

    if (out_group) {
        *out_group = NULL;
    }
    if (!layout || !out_group) {
        return 0;
    }

    for (group_index = 0; group_index < layout->spill_slot_group_count; ++group_index) {
        if (layout->spill_slot_groups[group_index].spill_slot == spill_slot) {
            *out_group = &layout->spill_slot_groups[group_index];
            return 1;
        }
    }

    return 0;
}

int value_ssa_function_allocation_layout_get_spill_slot_group_at(const ValueSsaFunctionAllocationLayout *layout,
    size_t group_index,
    const ValueSsaAllocationSpillSlotGroup **out_group) {
    if (out_group) {
        *out_group = NULL;
    }
    if (!layout || !out_group || group_index >= layout->spill_slot_group_count || !layout->spill_slot_groups) {
        return 0;
    }

    *out_group = &layout->spill_slot_groups[group_index];
    return 1;
}

int value_ssa_program_allocation_layout_get_function_layout(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout) {
    if (out_function_layout) {
        *out_function_layout = NULL;
    }
    if (!layout || !out_function_layout || function_index >= layout->function_count || !layout->function_layouts) {
        return 0;
    }

    *out_function_layout = &layout->function_layouts[function_index];
    return 1;
}

int value_ssa_program_allocation_layout_get_function_layout_by_name(const ValueSsaProgramAllocationLayout *layout,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout) {
    size_t function_index = 0;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function_layout) {
        *out_function_layout = NULL;
    }
    if (!layout || !function_name || !out_function_index || !out_function_layout) {
        return 0;
    }
    if (!value_ssa_program_allocation_layout_find_function_index_by_name(layout, function_name, &function_index)) {
        return 0;
    }

    *out_function_index = function_index;
    *out_function_layout = &layout->function_layouts[function_index];
    return 1;
}

int value_ssa_program_allocation_layout_get_function_name(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    const char **out_function_name) {
    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (!layout || !out_function_name || function_index >= layout->function_count) {
        return 0;
    }

    if (layout->function_names) {
        *out_function_name = layout->function_names[function_index];
    }
    return 1;
}

int value_ssa_program_allocation_layout_find_function_index_by_name(const ValueSsaProgramAllocationLayout *layout,
    const char *function_name,
    size_t *out_function_index) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!layout || !function_name || !out_function_index) {
        return 0;
    }

    for (function_index = 0; function_index < layout->function_count; ++function_index) {
        const char *candidate_name = NULL;

        if (layout->function_names) {
            candidate_name = layout->function_names[function_index];
        }
        if (candidate_name && strcmp(candidate_name, function_name) == 0) {
            *out_function_index = function_index;
            return 1;
        }
    }

    return 0;
}

int value_ssa_program_allocation_layout_get_function_placement(const ValueSsaProgramAllocationLayout *layout,
    size_t function_index,
    size_t value_id,
    const ValueSsaAllocatedValuePlacement **out_placement) {
    const ValueSsaFunctionAllocationLayout *function_layout = NULL;

    if (out_placement) {
        *out_placement = NULL;
    }
    if (!layout || !out_placement) {
        return 0;
    }
    if (!value_ssa_program_allocation_layout_get_function_layout(layout, function_index, &function_layout)) {
        return 0;
    }

    return value_ssa_function_allocation_layout_get_placement(function_layout, value_id, out_placement);
}

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
    size_t *out_spill_slot_group_count) {
    const ValueSsaFunctionAllocationLayout *function_layout = NULL;

    if (!layout) {
        return 0;
    }
    if (!value_ssa_program_allocation_layout_get_function_layout(layout, function_index, &function_layout)) {
        return 0;
    }

    return value_ssa_function_allocation_layout_get_summary(function_layout,
        out_function_name,
        out_value_count,
        out_color_budget,
        out_spill_slot_count,
        out_colored_count,
        out_spilled_count,
        out_optimistic_colored_count,
        out_confirmed_spilled_count,
        out_recovered_colored_count,
        out_used_color_count,
        out_color_group_count,
        out_spill_slot_group_count);
}

int value_ssa_program_allocation_layout_get_functions_with_colors(const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!layout || !out_count || !out_function_indices) {
        return 0;
    }

    *out_count = layout->functions_with_colors_count;
    *out_function_indices = layout->function_indices_with_colors;
    return 1;
}

int value_ssa_program_allocation_layout_get_functions_with_spills(const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!layout || !out_count || !out_function_indices) {
        return 0;
    }

    *out_count = layout->functions_with_spills_count;
    *out_function_indices = layout->function_indices_with_spills;
    return 1;
}

int value_ssa_program_allocation_layout_get_functions_with_confirmed_spills(
    const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!layout || !out_count || !out_function_indices) {
        return 0;
    }

    *out_count = layout->functions_with_confirmed_spills_count;
    *out_function_indices = layout->function_indices_with_confirmed_spills;
    return 1;
}

int value_ssa_program_allocation_layout_get_functions_with_optimistic_colors(
    const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!layout || !out_count || !out_function_indices) {
        return 0;
    }

    *out_count = layout->functions_with_optimistic_colors_count;
    *out_function_indices = layout->function_indices_with_optimistic_colors;
    return 1;
}

int value_ssa_program_allocation_layout_get_functions_with_recovered_colors(
    const ValueSsaProgramAllocationLayout *layout,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!layout || !out_count || !out_function_indices) {
        return 0;
    }

    *out_count = layout->functions_with_recovered_colors_count;
    *out_function_indices = layout->function_indices_with_recovered_colors;
    return 1;
}

int value_ssa_program_allocation_layout_get_summary(const ValueSsaProgramAllocationLayout *layout,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_colored_count,
    size_t *out_total_spilled_count,
    size_t *out_total_optimistic_colored_count,
    size_t *out_total_confirmed_spilled_count,
    size_t *out_total_recovered_colored_count,
    size_t *out_max_color_budget,
    size_t *out_max_spill_slot_count) {
    if (!layout) {
        return 0;
    }
    if (out_function_count) {
        *out_function_count = layout->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = layout->total_value_count;
    }
    if (out_total_colored_count) {
        *out_total_colored_count = layout->total_colored_count;
    }
    if (out_total_spilled_count) {
        *out_total_spilled_count = layout->total_spilled_count;
    }
    if (out_total_optimistic_colored_count) {
        *out_total_optimistic_colored_count = layout->total_optimistic_colored_count;
    }
    if (out_total_confirmed_spilled_count) {
        *out_total_confirmed_spilled_count = layout->total_confirmed_spilled_count;
    }
    if (out_total_recovered_colored_count) {
        *out_total_recovered_colored_count = layout->total_recovered_colored_count;
    }
    if (out_max_color_budget) {
        *out_max_color_budget = layout->max_color_budget;
    }
    if (out_max_spill_slot_count) {
        *out_max_spill_slot_count = layout->max_spill_slot_count;
    }

    return 1;
}

#endif
