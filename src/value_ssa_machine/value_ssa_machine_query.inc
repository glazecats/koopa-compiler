#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

static int value_ssa_machine_register_bank_query_contract_valid(const ValueSsaMachineRegisterBank *bank) {
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

static int value_ssa_function_machine_allocation_view_query_contract_valid(
    const ValueSsaFunctionMachineAllocationView *view);

static int value_ssa_function_machine_allocation_view_query_contract_valid(
    const ValueSsaFunctionMachineAllocationView *view) {
    size_t group_index;

    if (!view) {
        return 0;
    }
    if (view->value_count > 0 && !view->placements) {
        return 0;
    }
    if (view->machine_colored_count > 0 && !view->machine_colored_value_ids) {
        return 0;
    }
    if (view->spilled_count > 0 && !view->spilled_value_ids) {
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
    if (view->machine_register_group_count > 0 && !view->machine_register_groups) {
        return 0;
    }
    if (view->used_machine_register_count != view->machine_register_group_count) {
        return 0;
    }
    for (group_index = 0; group_index < view->machine_register_group_count; ++group_index) {
        if (view->machine_register_groups[group_index].value_count > 0 &&
            !view->machine_register_groups[group_index].value_ids) {
            return 0;
        }
    }
    return 1;
}

static int value_ssa_program_machine_allocation_view_query_contract_valid(
    const ValueSsaProgramMachineAllocationView *view) {
    size_t function_index;

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
    for (function_index = 0; function_index < view->function_count; ++function_index) {
        if (!value_ssa_function_machine_allocation_view_query_contract_valid(&view->function_views[function_index])) {
            return 0;
        }
    }
    return 1;
}

int value_ssa_machine_register_bank_get_summary(const ValueSsaMachineRegisterBank *bank,
    size_t *out_register_count,
    size_t *out_allocatable_count,
    size_t *out_caller_clobbered_count,
    size_t *out_callee_preserved_count) {
    size_t register_index;
    size_t allocatable_count = 0;
    size_t caller_clobbered_count = 0;
    size_t callee_preserved_count = 0;

    if (out_register_count) {
        *out_register_count = 0;
    }
    if (out_allocatable_count) {
        *out_allocatable_count = 0;
    }
    if (out_caller_clobbered_count) {
        *out_caller_clobbered_count = 0;
    }
    if (out_callee_preserved_count) {
        *out_callee_preserved_count = 0;
    }
    if (!value_ssa_machine_register_bank_query_contract_valid(bank)) {
        return 0;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const ValueSsaMachineRegisterDesc *desc = &bank->registers[register_index];

        if (desc->allocatable) {
            ++allocatable_count;
        }
        if (desc->caller_clobbered) {
            ++caller_clobbered_count;
        }
        if (desc->callee_preserved) {
            ++callee_preserved_count;
        }
    }

    if (out_register_count) {
        *out_register_count = bank->register_count;
    }
    if (out_allocatable_count) {
        *out_allocatable_count = allocatable_count;
    }
    if (out_caller_clobbered_count) {
        *out_caller_clobbered_count = caller_clobbered_count;
    }
    if (out_callee_preserved_count) {
        *out_callee_preserved_count = callee_preserved_count;
    }
    return 1;
}

int value_ssa_machine_register_bank_get_register(const ValueSsaMachineRegisterBank *bank,
    size_t register_index,
    const ValueSsaMachineRegisterDesc **out_desc) {
    if (out_desc) {
        *out_desc = NULL;
    }
    if (!value_ssa_machine_register_bank_query_contract_valid(bank) || !out_desc || register_index >= bank->register_count) {
        return 0;
    }

    *out_desc = &bank->registers[register_index];
    return 1;
}

int value_ssa_machine_register_bank_find_register_by_name(const ValueSsaMachineRegisterBank *bank,
    const char *register_name,
    size_t *out_register_index,
    const ValueSsaMachineRegisterDesc **out_desc) {
    size_t register_index;

    if (out_register_index) {
        *out_register_index = 0;
    }
    if (out_desc) {
        *out_desc = NULL;
    }
    if (!value_ssa_machine_register_bank_query_contract_valid(bank) || !register_name) {
        return 0;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const ValueSsaMachineRegisterDesc *desc = &bank->registers[register_index];

        if (desc->name && strcmp(desc->name, register_name) == 0) {
            if (out_register_index) {
                *out_register_index = register_index;
            }
            if (out_desc) {
                *out_desc = desc;
            }
            return 1;
        }
    }

    return 0;
}

int value_ssa_function_machine_allocation_view_get_summary(const ValueSsaFunctionMachineAllocationView *view,
    const char **out_function_name,
    size_t *out_value_count,
    size_t *out_machine_register_count,
    size_t *out_spill_slot_count,
    size_t *out_machine_colored_count,
    size_t *out_spilled_count,
    size_t *out_caller_clobbered_value_count,
    size_t *out_callee_preserved_value_count,
    size_t *out_used_machine_register_count,
    size_t *out_machine_register_group_count) {
    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (out_value_count) {
        *out_value_count = 0;
    }
    if (out_machine_register_count) {
        *out_machine_register_count = 0;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count = 0;
    }
    if (out_machine_colored_count) {
        *out_machine_colored_count = 0;
    }
    if (out_spilled_count) {
        *out_spilled_count = 0;
    }
    if (out_caller_clobbered_value_count) {
        *out_caller_clobbered_value_count = 0;
    }
    if (out_callee_preserved_value_count) {
        *out_callee_preserved_value_count = 0;
    }
    if (out_used_machine_register_count) {
        *out_used_machine_register_count = 0;
    }
    if (out_machine_register_group_count) {
        *out_machine_register_group_count = 0;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_function_name) {
        *out_function_name = view->function_name;
    }
    if (out_value_count) {
        *out_value_count = view->value_count;
    }
    if (out_machine_register_count) {
        *out_machine_register_count = view->machine_register_count;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count = view->spill_slot_count;
    }
    if (out_machine_colored_count) {
        *out_machine_colored_count = view->machine_colored_count;
    }
    if (out_spilled_count) {
        *out_spilled_count = view->spilled_count;
    }
    if (out_caller_clobbered_value_count) {
        *out_caller_clobbered_value_count = view->caller_clobbered_value_count;
    }
    if (out_callee_preserved_value_count) {
        *out_callee_preserved_value_count = view->callee_preserved_value_count;
    }
    if (out_used_machine_register_count) {
        *out_used_machine_register_count = view->used_machine_register_count;
    }
    if (out_machine_register_group_count) {
        *out_machine_register_group_count = view->machine_register_group_count;
    }
    return 1;
}

int value_ssa_function_machine_allocation_view_get_placement(const ValueSsaFunctionMachineAllocationView *view,
    size_t value_id,
    const ValueSsaMachineAllocatedValuePlacement **out_placement) {
    if (out_placement) {
        *out_placement = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view) || !out_placement ||
        value_id >= view->value_count || !view->placements) {
        return 0;
    }

    *out_placement = &view->placements[value_id];
    return 1;
}

int value_ssa_function_machine_allocation_view_get_machine_colored_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->machine_colored_count;
    }
    if (out_value_ids) {
        *out_value_ids = view->machine_colored_value_ids;
    }
    return 1;
}

int value_ssa_function_machine_allocation_view_get_spilled_values(const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->spilled_count;
    }
    if (out_value_ids) {
        *out_value_ids = view->spilled_value_ids;
    }
    return 1;
}

int value_ssa_function_machine_allocation_view_get_caller_clobbered_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->caller_clobbered_value_count;
    }
    if (out_value_ids) {
        *out_value_ids = view->caller_clobbered_value_ids;
    }
    return 1;
}

int value_ssa_function_machine_allocation_view_get_callee_preserved_values(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_value_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_value_ids) {
        *out_value_ids = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->callee_preserved_value_count;
    }
    if (out_value_ids) {
        *out_value_ids = view->callee_preserved_value_ids;
    }
    return 1;
}

int value_ssa_function_machine_allocation_view_get_used_machine_registers(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_machine_register_ids) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_machine_register_ids) {
        *out_machine_register_ids = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->used_machine_register_count;
    }
    if (out_machine_register_ids) {
        *out_machine_register_ids = view->used_machine_register_ids;
    }
    return 1;
}

int value_ssa_function_machine_allocation_view_get_machine_register_group(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t machine_register_id,
    const ValueSsaMachineRegisterValueGroup **out_group) {
    size_t group_index;

    if (out_group) {
        *out_group = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view) || !out_group ||
        !view->machine_register_groups) {
        return 0;
    }

    for (group_index = 0; group_index < view->machine_register_group_count; ++group_index) {
        if (view->machine_register_groups[group_index].machine_register_id == machine_register_id) {
            *out_group = &view->machine_register_groups[group_index];
            return 1;
        }
    }
    return 0;
}

int value_ssa_function_machine_allocation_view_get_machine_register_group_at(
    const ValueSsaFunctionMachineAllocationView *view,
    size_t group_index,
    const ValueSsaMachineRegisterValueGroup **out_group) {
    if (out_group) {
        *out_group = NULL;
    }
    if (!value_ssa_function_machine_allocation_view_query_contract_valid(view) || !out_group ||
        group_index >= view->machine_register_group_count || !view->machine_register_groups) {
        return 0;
    }

    *out_group = &view->machine_register_groups[group_index];
    return 1;
}

int value_ssa_program_machine_allocation_view_get_summary(const ValueSsaProgramMachineAllocationView *view,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_machine_colored_count,
    size_t *out_total_spilled_count,
    size_t *out_total_caller_clobbered_value_count,
    size_t *out_total_callee_preserved_value_count) {
    if (out_function_count) {
        *out_function_count = 0;
    }
    if (out_total_value_count) {
        *out_total_value_count = 0;
    }
    if (out_total_machine_colored_count) {
        *out_total_machine_colored_count = 0;
    }
    if (out_total_spilled_count) {
        *out_total_spilled_count = 0;
    }
    if (out_total_caller_clobbered_value_count) {
        *out_total_caller_clobbered_value_count = 0;
    }
    if (out_total_callee_preserved_value_count) {
        *out_total_callee_preserved_value_count = 0;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_function_count) {
        *out_function_count = view->function_count;
    }
    if (out_total_value_count) {
        *out_total_value_count = view->total_value_count;
    }
    if (out_total_machine_colored_count) {
        *out_total_machine_colored_count = view->total_machine_colored_count;
    }
    if (out_total_spilled_count) {
        *out_total_spilled_count = view->total_spilled_count;
    }
    if (out_total_caller_clobbered_value_count) {
        *out_total_caller_clobbered_value_count = view->total_caller_clobbered_value_count;
    }
    if (out_total_callee_preserved_value_count) {
        *out_total_callee_preserved_value_count = view->total_callee_preserved_value_count;
    }
    return 1;
}

int value_ssa_program_machine_allocation_view_get_function_view(const ValueSsaProgramMachineAllocationView *view,
    size_t function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view) {
    if (out_function_view) {
        *out_function_view = NULL;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view) || !out_function_view ||
        function_index >= view->function_count || !view->function_views) {
        return 0;
    }

    *out_function_view = &view->function_views[function_index];
    return 1;
}

int value_ssa_program_machine_allocation_view_get_function_view_by_name(
    const ValueSsaProgramMachineAllocationView *view,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function_view) {
        *out_function_view = NULL;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view) || !function_name ||
        !view->function_views) {
        return 0;
    }

    for (function_index = 0; function_index < view->function_count; ++function_index) {
        const ValueSsaFunctionMachineAllocationView *function_view = &view->function_views[function_index];

        if (function_view->function_name && strcmp(function_view->function_name, function_name) == 0) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_function_view) {
                *out_function_view = function_view;
            }
            return 1;
        }
    }

    return 0;
}

int value_ssa_allocate_rewrite_machine_report_get_stats(
    const ValueSsaAllocateRewriteMachineReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds) {
    if (out_allocation_rounds) {
        *out_allocation_rounds = 0;
    }
    if (out_rewrite_rounds) {
        *out_rewrite_rounds = 0;
    }
    if (!report) {
        return 0;
    }

    if (out_allocation_rounds) {
        *out_allocation_rounds = report->stats.allocation_rounds;
    }
    if (out_rewrite_rounds) {
        *out_rewrite_rounds = report->stats.rewrite_rounds;
    }
    return 1;
}

int value_ssa_allocate_rewrite_machine_report_get_program_view(
    const ValueSsaAllocateRewriteMachineReport *report,
    const ValueSsaProgramMachineAllocationView **out_view) {
    if (out_view) {
        *out_view = NULL;
    }
    if (!report || !out_view || !value_ssa_program_machine_allocation_view_query_contract_valid(&report->view)) {
        return 0;
    }

    *out_view = &report->view;
    return 1;
}

int value_ssa_allocate_rewrite_machine_report_get_function_view(
    const ValueSsaAllocateRewriteMachineReport *report,
    size_t function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view) {
    if (out_function_view) {
        *out_function_view = NULL;
    }
    if (!report || !value_ssa_program_machine_allocation_view_query_contract_valid(&report->view)) {
        return 0;
    }

    return value_ssa_program_machine_allocation_view_get_function_view(
        &report->view, function_index, out_function_view);
}

int value_ssa_allocate_rewrite_machine_report_get_function_view_by_name(
    const ValueSsaAllocateRewriteMachineReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionMachineAllocationView **out_function_view) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function_view) {
        *out_function_view = NULL;
    }
    if (!report || !value_ssa_program_machine_allocation_view_query_contract_valid(&report->view)) {
        return 0;
    }

    return value_ssa_program_machine_allocation_view_get_function_view_by_name(
        &report->view, function_name, out_function_index, out_function_view);
}

int value_ssa_program_machine_allocation_view_get_functions_with_machine_colors(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->functions_with_machine_colors_count;
    }
    if (out_function_indices && view->functions_with_machine_colors_count > 0) {
        *out_function_indices = view->function_indices_with_machine_colors;
    }
    return 1;
}

int value_ssa_program_machine_allocation_view_get_functions_with_spills(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->functions_with_spills_count;
    }
    if (out_function_indices && view->functions_with_spills_count > 0) {
        *out_function_indices = view->function_indices_with_spills;
    }
    return 1;
}

int value_ssa_program_machine_allocation_view_get_functions_with_caller_clobbered_values(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->functions_with_caller_clobbered_values_count;
    }
    if (out_function_indices && view->functions_with_caller_clobbered_values_count > 0) {
        *out_function_indices = view->function_indices_with_caller_clobbered_values;
    }
    return 1;
}

int value_ssa_program_machine_allocation_view_get_functions_with_callee_preserved_values(
    const ValueSsaProgramMachineAllocationView *view,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!value_ssa_program_machine_allocation_view_query_contract_valid(view)) {
        return 0;
    }

    if (out_count) {
        *out_count = view->functions_with_callee_preserved_values_count;
    }
    if (out_function_indices && view->functions_with_callee_preserved_values_count > 0) {
        *out_function_indices = view->function_indices_with_callee_preserved_values;
    }
    return 1;
}

int value_ssa_allocate_rewrite_machine_report_get_summary(
    const ValueSsaAllocateRewriteMachineReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_machine_colored_count,
    size_t *out_total_spilled_count) {
    if (out_allocation_rounds) {
        *out_allocation_rounds = 0;
    }
    if (out_rewrite_rounds) {
        *out_rewrite_rounds = 0;
    }
    if (out_function_count) {
        *out_function_count = 0;
    }
    if (out_total_value_count) {
        *out_total_value_count = 0;
    }
    if (out_total_machine_colored_count) {
        *out_total_machine_colored_count = 0;
    }
    if (out_total_spilled_count) {
        *out_total_spilled_count = 0;
    }
    if (!report || !value_ssa_program_machine_allocation_view_query_contract_valid(&report->view)) {
        return 0;
    }
    if (!value_ssa_allocate_rewrite_machine_report_get_stats(
            report, out_allocation_rounds, out_rewrite_rounds)) {
        return 0;
    }

    return value_ssa_program_machine_allocation_view_get_summary(
        &report->view,
        out_function_count,
        out_total_value_count,
        out_total_machine_colored_count,
        out_total_spilled_count,
        NULL,
        NULL);
}

#endif
