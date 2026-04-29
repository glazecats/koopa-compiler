#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_MACHINE_SPLIT_AGGREGATOR)
#define VALUE_SSA_MACHINE_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_machine.c"
#else

int value_ssa_dump_machine_register_bank(const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    ValueSsaError *error) {
    ValueSsaMachineStringBuilder builder;
    size_t register_index;

    if (!bank || !out_text) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-011: invalid machine register bank dump contract");
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

    if (!view || !out_text) {
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-014: invalid function machine allocation dump contract");
        return 0;
    }

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(&builder,
            "machine-allocation func %s regs=%zu values=%zu colored=%zu spilled=%zu spill_slots=%zu\n",
            view->function_name ? view->function_name : "<anon>",
            view->machine_register_count,
            view->value_count,
            view->machine_colored_count,
            view->spilled_count,
            view->spill_slot_count)) {
        free(builder.data);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-015: out of memory building function machine allocation dump");
        return 0;
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

    builder.data = NULL;
    builder.length = 0;
    builder.capacity = 0;
    *out_text = NULL;
    if (!value_ssa_machine_append_format(&builder,
            "machine-allocation program functions=%zu values=%zu colored=%zu spilled=%zu\n",
            view->function_count,
            view->total_value_count,
            view->total_machine_colored_count,
            view->total_spilled_count)) {
        free(builder.data);
        value_ssa_machine_set_error(error, 0, 0, "VALUE-SSA-MACHINE-019: out of memory building program machine allocation header");
        return 0;
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
