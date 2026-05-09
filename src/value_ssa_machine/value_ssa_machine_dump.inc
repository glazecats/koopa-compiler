#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

static int value_ssa_machine_function_allocation_view_dump_contract_valid(
    const ValueSsaFunctionMachineAllocationView *view) {
    size_t group_index;

    if (!view) {
        return 0;
    }
    if (view->value_count > 0 && !view->placements) {
        return 0;
    }
    if (view->caller_clobbered_value_count > 0 && !view->caller_clobbered_value_ids) {
        return 0;
    }
    if (view->callee_preserved_value_count > 0 && !view->callee_preserved_value_ids) {
        return 0;
    }
    if (view->used_machine_register_count > 0 && !view->used_machine_register_ids) {
        return 0;
    }
    if (view->used_machine_register_count != view->machine_register_group_count) {
        return 0;
    }
    if (view->machine_register_group_count > 0 && !view->machine_register_groups) {
        return 0;
    }
    for (group_index = 0; group_index < view->machine_register_group_count; ++group_index) {
        const ValueSsaMachineRegisterValueGroup *group = &view->machine_register_groups[group_index];

        if (group->value_count > 0 && !group->value_ids) {
            return 0;
        }
    }
    return 1;
}

static int value_ssa_machine_register_bank_dump_contract_valid(const ValueSsaMachineRegisterBank *bank) {
    size_t lhs_index;
    size_t rhs_index;

    if (!bank) {
        return 0;
    }
    if (bank->register_count > 0 && !bank->registers) {
        return 0;
    }
    for (lhs_index = 0; lhs_index < bank->register_count; ++lhs_index) {
        if (bank->registers[lhs_index].caller_clobbered && bank->registers[lhs_index].callee_preserved) {
            return 0;
        }
    }
    for (lhs_index = 0; lhs_index < bank->register_count; ++lhs_index) {
        for (rhs_index = lhs_index + 1; rhs_index < bank->register_count; ++rhs_index) {
            if (bank->registers[lhs_index].register_id == bank->registers[rhs_index].register_id) {
                return 0;
            }
        }
    }
    return 1;
}

static int value_ssa_machine_program_allocation_view_dump_contract_valid(
    const ValueSsaProgramMachineAllocationView *view) {
    if (!view) {
        return 0;
    }
    if (view->function_count > 0 && !view->function_views) {
        return 0;
    }
    if (view->functions_with_machine_colors_count > 0 && !view->function_indices_with_machine_colors) {
        return 0;
    }
    if (view->functions_with_spills_count > 0 && !view->function_indices_with_spills) {
        return 0;
    }
    if (view->functions_with_caller_clobbered_values_count > 0 &&
        !view->function_indices_with_caller_clobbered_values) {
        return 0;
    }
    if (view->functions_with_callee_preserved_values_count > 0 &&
        !view->function_indices_with_callee_preserved_values) {
        return 0;
    }
    return 1;
}

int value_ssa_dump_machine_register_bank(const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t register_index;

    if (!bank || !out_text) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-011: invalid machine register bank dump contract");
        return 0;
    }
    if (!value_ssa_machine_register_bank_dump_contract_valid(bank)) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-067: malformed machine register bank artifact");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(&builder, "machine-register-bank registers=%zu\n", bank->register_count)) {
        free(builder.data);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-012: out of memory building machine register bank dump");
        return 0;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const ValueSsaMachineRegisterDesc *desc = &bank->registers[register_index];
        const char *class_name = desc->register_class == VALUE_SSA_MACHINE_REGISTER_CLASS_GENERAL ? "general" : "unknown";

        if (!value_ssa_machine_append_format(&builder,
                "r%zu name=%s class=%s alloc=%u caller_clobbered=%u callee_preserved=%u\n",
                desc->register_id,
                desc->name ? desc->name : "<anon>",
                class_name,
                (unsigned)desc->allocatable,
                (unsigned)desc->caller_clobbered,
                (unsigned)desc->callee_preserved)) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-013: out of memory appending machine register dump");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_function_machine_allocation_view(const ValueSsaFunctionMachineAllocationView *view,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t value_id;
    size_t register_index;

    if (!view || !out_text) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-014: invalid function machine allocation dump contract");
        return 0;
    }
    if (!value_ssa_machine_function_allocation_view_dump_contract_valid(view)) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-065: malformed function machine allocation dump artifact");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(&builder,
            "machine-allocation func %s regs=%zu values=%zu colored=%zu spilled=%zu spill_slots=%zu caller_clobbered=%zu callee_preserved=%zu used_regs=%zu reg_groups=%zu\n",
            view->function_name ? view->function_name : "<anon>",
            view->machine_register_count,
            view->value_count,
            view->machine_colored_count,
            view->spilled_count,
            view->spill_slot_count,
            view->caller_clobbered_value_count,
            view->callee_preserved_value_count,
            view->used_machine_register_count,
            view->machine_register_group_count)) {
        free(builder.data);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-015: out of memory building function machine allocation dump");
        return 0;
    }

    if (view->used_machine_register_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "used-registers:")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-030: out of memory appending used-register header");
            return 0;
        }
        for (register_index = 0; register_index < view->used_machine_register_count; ++register_index) {
            const ValueSsaMachineRegisterValueGroup *group = &view->machine_register_groups[register_index];

            if (!value_ssa_machine_append_format(
                    &builder, " %s", group->machine_register_name ? group->machine_register_name : "<anon>")) {
                free(builder.data);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-031: out of memory appending used-register list");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-032: out of memory finishing used-register list");
            return 0;
        }
    }

    if (view->caller_clobbered_value_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "caller-clobbered-values:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-033: out of memory appending caller-clobbered header");
            return 0;
        }
        for (value_id = 0; value_id < view->caller_clobbered_value_count; ++value_id) {
            if (!value_ssa_machine_append_format(&builder, " ssa.%zu", view->caller_clobbered_value_ids[value_id])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-034: out of memory appending caller-clobbered values");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-035: out of memory finishing caller-clobbered values");
            return 0;
        }
    }

    if (view->callee_preserved_value_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "callee-preserved-values:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-036: out of memory appending callee-preserved header");
            return 0;
        }
        for (value_id = 0; value_id < view->callee_preserved_value_count; ++value_id) {
            if (!value_ssa_machine_append_format(&builder, " ssa.%zu", view->callee_preserved_value_ids[value_id])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-037: out of memory appending callee-preserved values");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-038: out of memory finishing callee-preserved values");
            return 0;
        }
    }

    for (value_id = 0; value_id < view->value_count; ++value_id) {
        const ValueSsaMachineAllocatedValuePlacement *placement = &view->placements[value_id];

        if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_UNALLOCATED) {
            continue;
        }
        if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR) {
            if (!value_ssa_machine_append_format(&builder,
                    "ssa.%zu reg=%s%s%s\n",
                    placement->value_id,
                    placement->machine_register_name ? placement->machine_register_name : "<none>",
                    placement->spill_intended ? " spill-intended" : "",
                    placement->spill_recovered ? " spill-recovered" : "")) {
                free(builder.data);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-016: out of memory appending machine-colored value");
                return 0;
            }
        } else if (placement->kind == VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL) {
            if (!value_ssa_machine_append_format(&builder,
                    "ssa.%zu spill=%zu%s\n",
                    placement->value_id,
                    placement->spill_slot,
                    placement->spill_confirmed ? " spill-confirmed" : "")) {
                free(builder.data);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-017: out of memory appending machine-spilled value");
                return 0;
            }
        }
    }

    for (register_index = 0; register_index < view->machine_register_group_count; ++register_index) {
        const ValueSsaMachineRegisterValueGroup *group = &view->machine_register_groups[register_index];
        size_t group_value_index;

        if (!value_ssa_machine_append_format(
                &builder, "register-group %s:", group->machine_register_name ? group->machine_register_name : "<anon>")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-039: out of memory appending register-group header");
            return 0;
        }
        for (group_value_index = 0; group_value_index < group->value_count; ++group_value_index) {
            if (!value_ssa_machine_append_format(&builder, " ssa.%zu", group->value_ids[group_value_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-040: out of memory appending register-group values");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-041: out of memory finishing register-group");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int value_ssa_dump_program_machine_allocation_view(const ValueSsaProgramMachineAllocationView *view,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t function_index;
    char *function_text = NULL;

    if (!view || !out_text) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-018: invalid program machine allocation dump contract");
        return 0;
    }
    if (!value_ssa_machine_program_allocation_view_dump_contract_valid(view)) {
        value_ssa_machine_set_error(
            error, 0, 0, "VALUE-SSA-MACHINE-066: malformed program machine allocation dump artifact");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(&builder,
            "machine-allocation program functions=%zu values=%zu colored=%zu spilled=%zu caller_clobbered=%zu callee_preserved=%zu colored_funcs=%zu spilled_funcs=%zu caller_clobbered_funcs=%zu callee_preserved_funcs=%zu\n",
            view->function_count,
            view->total_value_count,
            view->total_machine_colored_count,
            view->total_spilled_count,
            view->total_caller_clobbered_value_count,
            view->total_callee_preserved_value_count,
            view->functions_with_machine_colors_count,
            view->functions_with_spills_count,
            view->functions_with_caller_clobbered_values_count,
            view->functions_with_callee_preserved_values_count)) {
        free(builder.data);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-019: out of memory building program machine allocation header");
        return 0;
    }

    if (view->functions_with_machine_colors_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-machine-colors:")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-042: out of memory appending machine-color filter header");
            return 0;
        }
        for (function_index = 0; function_index < view->functions_with_machine_colors_count; ++function_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", view->function_indices_with_machine_colors[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-043: out of memory appending machine-color filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-044: out of memory finishing machine-color filter");
            return 0;
        }
    }

    if (view->functions_with_spills_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-spills:")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-045: out of memory appending spill filter header");
            return 0;
        }
        for (function_index = 0; function_index < view->functions_with_spills_count; ++function_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", view->function_indices_with_spills[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-046: out of memory appending spill filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-047: out of memory finishing spill filter");
            return 0;
        }
    }

    if (view->functions_with_caller_clobbered_values_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-caller-clobbered-values:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-048: out of memory appending caller-clobbered filter header");
            return 0;
        }
        for (function_index = 0; function_index < view->functions_with_caller_clobbered_values_count; ++function_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", view->function_indices_with_caller_clobbered_values[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-049: out of memory appending caller-clobbered filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-050: out of memory finishing caller-clobbered filter");
            return 0;
        }
    }

    if (view->functions_with_callee_preserved_values_count > 0) {
        if (!value_ssa_machine_append_format(&builder, "functions-with-callee-preserved-values:")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-051: out of memory appending callee-preserved filter header");
            return 0;
        }
        for (function_index = 0; function_index < view->functions_with_callee_preserved_values_count; ++function_index) {
            if (!value_ssa_machine_append_format(
                    &builder, " %zu", view->function_indices_with_callee_preserved_values[function_index])) {
                free(builder.data);
                value_ssa_machine_set_error(
                    error, 0, 0, "VALUE-SSA-MACHINE-052: out of memory appending callee-preserved filter");
                return 0;
            }
        }
        if (!value_ssa_machine_append_format(&builder, "\n")) {
            free(builder.data);
            value_ssa_machine_set_error(
                error, 0, 0, "VALUE-SSA-MACHINE-053: out of memory finishing callee-preserved filter");
            return 0;
        }
    }

    for (function_index = 0; function_index < view->function_count; ++function_index) {
        free(function_text);
        function_text = NULL;
        if (!value_ssa_dump_function_machine_allocation_view(&view->function_views[function_index], &function_text, error)) {
            free(builder.data);
            return 0;
        }
        if (!value_ssa_machine_append_format(&builder, "%s%s", function_index == 0 ? "" : "\n", function_text ? function_text : "")) {
            free(function_text);
            free(builder.data);
            value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-020: out of memory appending program machine allocation body");
            return 0;
        }
    }

    free(function_text);
    *out_text = builder.data;
    return 1;
}

#endif
