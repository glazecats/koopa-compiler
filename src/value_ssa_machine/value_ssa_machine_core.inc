#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

void value_ssa_machine_register_bank_init(ValueSsaMachineRegisterBank *bank) {
    if (!bank) {
        return;
    }

    bank->register_count = 0;
    bank->registers = NULL;
}

void value_ssa_machine_register_bank_free(ValueSsaMachineRegisterBank *bank) {
    if (!bank) {
        return;
    }

    free(bank->registers);
    value_ssa_machine_register_bank_init(bank);
}

void value_ssa_function_machine_allocation_view_init(ValueSsaFunctionMachineAllocationView *view) {
    if (!view) {
        return;
    }

    view->function_name = NULL;
    view->value_count = 0;
    view->machine_register_count = 0;
    view->spill_slot_count = 0;
    view->machine_colored_count = 0;
    view->spilled_count = 0;
    view->caller_clobbered_value_count = 0;
    view->callee_preserved_value_count = 0;
    view->used_machine_register_count = 0;
    view->machine_register_group_count = 0;
    view->placements = NULL;
    view->machine_colored_value_ids = NULL;
    view->spilled_value_ids = NULL;
    view->caller_clobbered_value_ids = NULL;
    view->callee_preserved_value_ids = NULL;
    view->used_machine_register_ids = NULL;
    view->machine_register_groups = NULL;
}

void value_ssa_function_machine_allocation_view_free(ValueSsaFunctionMachineAllocationView *view) {
    size_t group_index;

    if (!view) {
        return;
    }

    free(view->function_name);
    free(view->placements);
    free(view->machine_colored_value_ids);
    free(view->spilled_value_ids);
    free(view->caller_clobbered_value_ids);
    free(view->callee_preserved_value_ids);
    free(view->used_machine_register_ids);
    if (view->machine_register_groups) {
        for (group_index = 0; group_index < view->machine_register_group_count; ++group_index) {
            free(view->machine_register_groups[group_index].value_ids);
        }
    }
    free(view->machine_register_groups);
    value_ssa_function_machine_allocation_view_init(view);
}

void value_ssa_program_machine_allocation_view_init(ValueSsaProgramMachineAllocationView *view) {
    if (!view) {
        return;
    }

    view->function_count = 0;
    view->function_views = NULL;
    view->total_value_count = 0;
    view->total_machine_colored_count = 0;
    view->total_spilled_count = 0;
    view->total_caller_clobbered_value_count = 0;
    view->total_callee_preserved_value_count = 0;
    view->functions_with_machine_colors_count = 0;
    view->functions_with_spills_count = 0;
    view->functions_with_caller_clobbered_values_count = 0;
    view->functions_with_callee_preserved_values_count = 0;
    view->function_indices_with_machine_colors = NULL;
    view->function_indices_with_spills = NULL;
    view->function_indices_with_caller_clobbered_values = NULL;
    view->function_indices_with_callee_preserved_values = NULL;
}

void value_ssa_program_machine_allocation_view_free(ValueSsaProgramMachineAllocationView *view) {
    size_t function_index;

    if (!view) {
        return;
    }

    for (function_index = 0; function_index < view->function_count; ++function_index) {
        value_ssa_function_machine_allocation_view_free(&view->function_views[function_index]);
    }
    free(view->function_views);
    free(view->function_indices_with_machine_colors);
    free(view->function_indices_with_spills);
    free(view->function_indices_with_caller_clobbered_values);
    free(view->function_indices_with_callee_preserved_values);
    value_ssa_program_machine_allocation_view_init(view);
}

void value_ssa_allocate_rewrite_machine_report_init(ValueSsaAllocateRewriteMachineReport *report) {
    if (!report) {
        return;
    }

    value_ssa_allocate_rewrite_stats_init(&report->stats);
    value_ssa_program_machine_allocation_view_init(&report->view);
}

void value_ssa_allocate_rewrite_machine_report_free(ValueSsaAllocateRewriteMachineReport *report) {
    if (!report) {
        return;
    }

    value_ssa_program_machine_allocation_view_free(&report->view);
    value_ssa_allocate_rewrite_machine_report_init(report);
}

int value_ssa_build_flat_machine_register_bank(size_t register_count,
    ValueSsaMachineRegisterBank *out_bank,
    ValueSsaError *error) {
    size_t register_index;
    static char names[256][16];

    if (!out_bank) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-001: invalid machine register bank build contract");
        return 0;
    }

    value_ssa_machine_register_bank_free(out_bank);
    out_bank->register_count = register_count;
    if (register_count == 0) {
        return 1;
    }

    out_bank->registers = (ValueSsaMachineRegisterDesc *)calloc(register_count, sizeof(ValueSsaMachineRegisterDesc));
    if (!out_bank->registers) {
        value_ssa_machine_register_bank_free(out_bank);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-002: out of memory building machine register bank");
        return 0;
    }

    for (register_index = 0; register_index < register_count; ++register_index) {
        if (register_index >= 256) {
            value_ssa_machine_register_bank_free(out_bank);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-003: flat machine register bank currently capped at 256 registers");
            return 0;
        }
        snprintf(names[register_index], sizeof(names[register_index]), "r%zu", register_index);
        out_bank->registers[register_index].register_id = register_index;
        out_bank->registers[register_index].name = names[register_index];
        out_bank->registers[register_index].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
        out_bank->registers[register_index].allocatable = 1u;
        out_bank->registers[register_index].caller_clobbered = 1u;
        out_bank->registers[register_index].callee_preserved = 0u;
    }

    return 1;
}

int value_ssa_build_split_machine_register_bank(size_t caller_clobbered_count,
    size_t callee_preserved_count,
    ValueSsaMachineRegisterBank *out_bank,
    ValueSsaError *error) {
    size_t register_count = caller_clobbered_count + callee_preserved_count;
    size_t register_index;
    static char names[256][16];

    if (!out_bank) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-179: invalid split machine register bank build contract");
        return 0;
    }

    value_ssa_machine_register_bank_free(out_bank);
    out_bank->register_count = register_count;
    if (register_count == 0) {
        return 1;
    }

    out_bank->registers = (ValueSsaMachineRegisterDesc *)calloc(register_count, sizeof(ValueSsaMachineRegisterDesc));
    if (!out_bank->registers) {
        value_ssa_machine_register_bank_free(out_bank);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-180: out of memory building split machine register bank");
        return 0;
    }

    for (register_index = 0; register_index < register_count; ++register_index) {
        if (register_index >= 256) {
            value_ssa_machine_register_bank_free(out_bank);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-181: split machine register bank currently capped at 256 registers");
            return 0;
        }
        snprintf(names[register_index], sizeof(names[register_index]), "r%zu", register_index);
        out_bank->registers[register_index].register_id = register_index;
        out_bank->registers[register_index].name = names[register_index];
        out_bank->registers[register_index].register_class = VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL;
        out_bank->registers[register_index].allocatable = 1u;
        out_bank->registers[register_index].caller_clobbered = register_index < caller_clobbered_count ? 1u : 0u;
        out_bank->registers[register_index].callee_preserved = register_index >= caller_clobbered_count ? 1u : 0u;
    }

    return 1;
}

int value_ssa_build_function_machine_allocation_view(const ValueSsaFunctionAllocationLayout *layout,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error) {
    size_t value_id;
    unsigned char *used_register_flags = NULL;
    size_t *register_value_counts = NULL;
    int ok = 0;

    if (!layout || !bank || !out_view) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-004: invalid function machine allocation view contract");
        return 0;
    }

    value_ssa_function_machine_allocation_view_free(out_view);
    out_view->value_count = layout->value_count;
    out_view->machine_register_count = bank->register_count;
    out_view->spill_slot_count = layout->spill_slot_count;
    if (layout->function_name) {
        out_view->function_name = value_ssa_machine_strdup(layout->function_name);
        if (!out_view->function_name) {
            value_ssa_function_machine_allocation_view_free(out_view);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-005: out of memory copying machine view function name");
            return 0;
        }
    }
    if (layout->value_count == 0) {
        return 1;
    }

    out_view->placements =
        (ValueSsaMachineAllocatedValuePlacement *)calloc(layout->value_count, sizeof(ValueSsaMachineAllocatedValuePlacement));
    out_view->machine_colored_value_ids = (size_t *)malloc(layout->value_count * sizeof(size_t));
    out_view->spilled_value_ids = (size_t *)malloc(layout->value_count * sizeof(size_t));
    out_view->caller_clobbered_value_ids = (size_t *)malloc(layout->value_count * sizeof(size_t));
    out_view->callee_preserved_value_ids = (size_t *)malloc(layout->value_count * sizeof(size_t));
    out_view->used_machine_register_ids =
        bank->register_count > 0 ? (size_t *)malloc(bank->register_count * sizeof(size_t)) : NULL;
    out_view->machine_register_groups =
        bank->register_count > 0 ? (ValueSsaMachineRegisterValueGroup *)calloc(
                                       bank->register_count, sizeof(ValueSsaMachineRegisterValueGroup))
                                 : NULL;
    used_register_flags =
        bank->register_count > 0 ? (unsigned char *)calloc(bank->register_count, sizeof(unsigned char)) : NULL;
    register_value_counts =
        bank->register_count > 0 ? (size_t *)calloc(bank->register_count, sizeof(size_t)) : NULL;
    if (!out_view->placements || !out_view->machine_colored_value_ids || !out_view->spilled_value_ids ||
        !out_view->caller_clobbered_value_ids || !out_view->callee_preserved_value_ids ||
        (bank->register_count > 0 &&
            (!out_view->used_machine_register_ids || !out_view->machine_register_groups || !used_register_flags ||
                !register_value_counts))) {
        value_ssa_function_machine_allocation_view_free(out_view);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-006: out of memory building function machine view");
        goto cleanup;
    }

    for (value_id = 0; value_id < layout->value_count; ++value_id) {
        const ValueSsaAllocatedValuePlacement *placement = NULL;
        ValueSsaMachineAllocatedValuePlacement *machine_placement = &out_view->placements[value_id];

        if (!value_ssa_function_allocation_layout_get_placement(layout, value_id, &placement) || !placement) {
            value_ssa_function_machine_allocation_view_free(out_view);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-007: invalid function layout placement query");
            goto cleanup;
        }

        machine_placement->value_id = value_id;
        machine_placement->kind = placement->kind;
        machine_placement->machine_register_id = (size_t)-1;
        machine_placement->machine_register_name = NULL;
        machine_placement->spill_slot = placement->spill_slot;
        machine_placement->spill_intended = placement->spill_intended;
        machine_placement->spill_confirmed = placement->spill_confirmed;
        machine_placement->spill_recovered = placement->spill_recovered;

        if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
            if (placement->color >= bank->register_count || !bank->registers[placement->color].allocatable) {
                value_ssa_function_machine_allocation_view_free(out_view);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-008: abstract color cannot map into machine register bank");
                goto cleanup;
            }
            machine_placement->machine_register_id = bank->registers[placement->color].register_id;
            machine_placement->machine_register_name = bank->registers[placement->color].name;
            out_view->machine_colored_value_ids[out_view->machine_colored_count] = value_id;
            ++out_view->machine_colored_count;
            if (bank->registers[placement->color].caller_clobbered) {
                out_view->caller_clobbered_value_ids[out_view->caller_clobbered_value_count++] = value_id;
            }
            if (bank->registers[placement->color].callee_preserved) {
                out_view->callee_preserved_value_ids[out_view->callee_preserved_value_count++] = value_id;
            }
            if (!used_register_flags[placement->color]) {
                used_register_flags[placement->color] = 1u;
                out_view->used_machine_register_ids[out_view->used_machine_register_count++] = placement->color;
            }
            ++register_value_counts[placement->color];
        } else if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
            out_view->spilled_value_ids[out_view->spilled_count] = value_id;
            ++out_view->spilled_count;
        }
    }

    if (out_view->used_machine_register_count > 0) {
        size_t register_group_index;
        size_t *register_write_offsets = (size_t *)calloc(
            out_view->used_machine_register_count, sizeof(size_t));

        if (!register_write_offsets) {
            value_ssa_function_machine_allocation_view_free(out_view);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-028: out of memory building register groups");
            goto cleanup;
        }

        out_view->machine_register_group_count = out_view->used_machine_register_count;
        for (register_group_index = 0; register_group_index < out_view->used_machine_register_count; ++register_group_index) {
            size_t register_id = out_view->used_machine_register_ids[register_group_index];
            ValueSsaMachineRegisterValueGroup *group = &out_view->machine_register_groups[register_group_index];

            group->machine_register_id = register_id;
            group->machine_register_name = bank->registers[register_id].name;
            group->value_count = register_value_counts[register_id];
            group->value_ids = group->value_count > 0 ? (size_t *)malloc(group->value_count * sizeof(size_t)) : NULL;
            if (group->value_count > 0 && !group->value_ids) {
                free(register_write_offsets);
                value_ssa_function_machine_allocation_view_free(out_view);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-029: out of memory building register-group members");
                goto cleanup;
            }
        }

        for (value_id = 0; value_id < out_view->machine_colored_count; ++value_id) {
            size_t colored_value_id = out_view->machine_colored_value_ids[value_id];
            const ValueSsaMachineAllocatedValuePlacement *placement = &out_view->placements[colored_value_id];
            size_t register_group_index;

            for (register_group_index = 0; register_group_index < out_view->used_machine_register_count; ++register_group_index) {
                ValueSsaMachineRegisterValueGroup *group = &out_view->machine_register_groups[register_group_index];

                if (group->machine_register_id == placement->machine_register_id) {
                    group->value_ids[register_write_offsets[register_group_index]++] = colored_value_id;
                    break;
                }
            }
        }

        free(register_write_offsets);
    }

    ok = 1;

cleanup:
    free(register_value_counts);
    free(used_register_flags);
    return ok;
}

int value_ssa_build_program_machine_allocation_view(const ValueSsaProgramAllocationLayout *layout,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error) {
    size_t function_index;
    int ok = 0;

    if (!layout || !bank || !out_view) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-009: invalid program machine allocation view contract");
        return 0;
    }

    value_ssa_program_machine_allocation_view_free(out_view);
    out_view->function_count = layout->function_count;
    if (layout->function_count == 0) {
        return 1;
    }

    out_view->function_views =
        (ValueSsaFunctionMachineAllocationView *)calloc(layout->function_count, sizeof(ValueSsaFunctionMachineAllocationView));
    out_view->function_indices_with_machine_colors = (size_t *)malloc(layout->function_count * sizeof(size_t));
    out_view->function_indices_with_spills = (size_t *)malloc(layout->function_count * sizeof(size_t));
    out_view->function_indices_with_caller_clobbered_values = (size_t *)malloc(layout->function_count * sizeof(size_t));
    out_view->function_indices_with_callee_preserved_values = (size_t *)malloc(layout->function_count * sizeof(size_t));
    if (!out_view->function_views || !out_view->function_indices_with_machine_colors ||
        !out_view->function_indices_with_spills || !out_view->function_indices_with_caller_clobbered_values ||
        !out_view->function_indices_with_callee_preserved_values) {
        value_ssa_program_machine_allocation_view_free(out_view);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-010: out of memory building program machine view");
        goto cleanup;
    }

    for (function_index = 0; function_index < layout->function_count; ++function_index) {
        const ValueSsaFunctionAllocationLayout *function_layout = NULL;

        value_ssa_function_machine_allocation_view_init(&out_view->function_views[function_index]);
        if (!value_ssa_program_allocation_layout_get_function_layout(layout, function_index, &function_layout) ||
            !function_layout ||
            !value_ssa_build_function_machine_allocation_view(function_layout, bank, &out_view->function_views[function_index], error)) {
            value_ssa_program_machine_allocation_view_free(out_view);
            goto cleanup;
        }
        out_view->total_value_count += out_view->function_views[function_index].value_count;
        out_view->total_machine_colored_count += out_view->function_views[function_index].machine_colored_count;
        out_view->total_spilled_count += out_view->function_views[function_index].spilled_count;
        out_view->total_caller_clobbered_value_count +=
            out_view->function_views[function_index].caller_clobbered_value_count;
        out_view->total_callee_preserved_value_count +=
            out_view->function_views[function_index].callee_preserved_value_count;
        if (out_view->function_views[function_index].machine_colored_count > 0) {
            out_view->function_indices_with_machine_colors[out_view->functions_with_machine_colors_count++] = function_index;
        }
        if (out_view->function_views[function_index].spilled_count > 0) {
            out_view->function_indices_with_spills[out_view->functions_with_spills_count++] = function_index;
        }
        if (out_view->function_views[function_index].caller_clobbered_value_count > 0) {
            out_view->function_indices_with_caller_clobbered_values
                [out_view->functions_with_caller_clobbered_values_count++] = function_index;
        }
        if (out_view->function_views[function_index].callee_preserved_value_count > 0) {
            out_view->function_indices_with_callee_preserved_values
                [out_view->functions_with_callee_preserved_values_count++] = function_index;
        }
    }

    ok = 1;

cleanup:
    return ok;
}

#endif
