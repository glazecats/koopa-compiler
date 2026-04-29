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
    view->placements = NULL;
}

void value_ssa_function_machine_allocation_view_free(ValueSsaFunctionMachineAllocationView *view) {
    if (!view) {
        return;
    }

    free(view->function_name);
    free(view->placements);
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
    value_ssa_program_machine_allocation_view_init(view);
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

int value_ssa_build_function_machine_allocation_view(const ValueSsaFunctionAllocationLayout *layout,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaFunctionMachineAllocationView *out_view,
    ValueSsaError *error) {
    size_t value_id;

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
    if (!out_view->placements) {
        value_ssa_function_machine_allocation_view_free(out_view);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-006: out of memory building function machine view");
        return 0;
    }

    for (value_id = 0; value_id < layout->value_count; ++value_id) {
        const ValueSsaAllocatedValuePlacement *placement = NULL;
        ValueSsaMachineAllocatedValuePlacement *machine_placement = &out_view->placements[value_id];

        if (!value_ssa_function_allocation_layout_get_placement(layout, value_id, &placement) || !placement) {
            value_ssa_function_machine_allocation_view_free(out_view);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-007: invalid function layout placement query");
            return 0;
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
                return 0;
            }
            machine_placement->machine_register_id = bank->registers[placement->color].register_id;
            machine_placement->machine_register_name = bank->registers[placement->color].name;
            ++out_view->machine_colored_count;
        } else if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
            ++out_view->spilled_count;
        }
    }

    return 1;
}

int value_ssa_build_program_machine_allocation_view(const ValueSsaProgramAllocationLayout *layout,
    const ValueSsaMachineRegisterBank *bank,
    ValueSsaProgramMachineAllocationView *out_view,
    ValueSsaError *error) {
    size_t function_index;

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
    if (!out_view->function_views) {
        value_ssa_program_machine_allocation_view_free(out_view);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-010: out of memory building program machine view");
        return 0;
    }

    for (function_index = 0; function_index < layout->function_count; ++function_index) {
        const ValueSsaFunctionAllocationLayout *function_layout = NULL;

        value_ssa_function_machine_allocation_view_init(&out_view->function_views[function_index]);
        if (!value_ssa_program_allocation_layout_get_function_layout(layout, function_index, &function_layout) ||
            !function_layout ||
            !value_ssa_build_function_machine_allocation_view(function_layout, bank, &out_view->function_views[function_index], error)) {
            value_ssa_program_machine_allocation_view_free(out_view);
            return 0;
        }
        out_view->total_value_count += out_view->function_views[function_index].value_count;
        out_view->total_machine_colored_count += out_view->function_views[function_index].machine_colored_count;
        out_view->total_spilled_count += out_view->function_views[function_index].spilled_count;
    }

    return 1;
}

#endif
