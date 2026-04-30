#if defined(__INTELLISENSE__) && !defined(MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IR_SPLIT_AGGREGATOR)
#define MACHINE_IR_SPLIT_INTELLISENSE_CONTEXT
#include "machine_ir.c"
#else

int machine_ir_register_bank_get_summary(const MachineIrRegisterBank *bank,
    size_t *out_register_count,
    size_t *out_allocatable_count,
    size_t *out_caller_clobbered_count,
    size_t *out_callee_preserved_count) {
    size_t register_index;
    size_t allocatable_count = 0;
    size_t caller_clobbered_count = 0;
    size_t callee_preserved_count = 0;

    if (!bank) {
        return 0;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const MachineIrRegisterDesc *desc = &bank->registers[register_index];

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

int machine_ir_register_bank_get_register(const MachineIrRegisterBank *bank,
    size_t register_index,
    const MachineIrRegisterDesc **out_desc) {
    if (!bank || !out_desc || register_index >= bank->register_count || !bank->registers) {
        return 0;
    }

    *out_desc = &bank->registers[register_index];
    return 1;
}

int machine_ir_register_bank_find_register_by_name(const MachineIrRegisterBank *bank,
    const char *register_name,
    size_t *out_register_index,
    const MachineIrRegisterDesc **out_desc) {
    size_t register_index;

    if (!bank || !register_name) {
        return 0;
    }

    for (register_index = 0; register_index < bank->register_count; ++register_index) {
        const MachineIrRegisterDesc *desc = &bank->registers[register_index];

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

int machine_ir_basic_block_get_summary(const MachineIrBasicBlock *block,
    size_t *out_block_id,
    size_t *out_phi_count,
    size_t *out_instruction_count,
    int *out_has_terminator,
    MachineIrTerminatorKind *out_terminator_kind) {
    if (!block) {
        return 0;
    }

    if (out_block_id) {
        *out_block_id = block->id;
    }
    if (out_phi_count) {
        *out_phi_count = block->phi_count;
    }
    if (out_instruction_count) {
        *out_instruction_count = block->instruction_count;
    }
    if (out_has_terminator) {
        *out_has_terminator = block->has_terminator;
    }
    if (out_terminator_kind) {
        *out_terminator_kind = block->terminator.kind;
    }
    return 1;
}

int machine_ir_function_get_summary(const MachineIrFunction *function,
    const char **out_function_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count) {
    if (!function) {
        return 0;
    }

    if (out_function_name) {
        *out_function_name = function->name;
    }
    if (out_has_body) {
        *out_has_body = function->has_body;
    }
    if (out_parameter_count) {
        *out_parameter_count = function->parameter_count;
    }
    if (out_local_count) {
        *out_local_count = function->local_count;
    }
    if (out_block_count) {
        *out_block_count = function->block_count;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count = function->spill_slot_count;
    }
    return 1;
}

int machine_ir_function_get_block(const MachineIrFunction *function,
    size_t block_id,
    const MachineIrBasicBlock **out_block) {
    if (!function || !out_block || block_id >= function->block_count || !function->blocks) {
        return 0;
    }

    *out_block = &function->blocks[block_id];
    return 1;
}

int machine_ir_program_get_summary(const MachineIrProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count) {
    if (!program) {
        return 0;
    }

    if (out_register_count) {
        *out_register_count = program->register_bank.register_count;
    }
    if (out_global_count) {
        *out_global_count = program->global_count;
    }
    if (out_function_count) {
        *out_function_count = program->function_count;
    }
    return 1;
}

int machine_ir_program_get_function(const MachineIrProgram *program,
    size_t function_index,
    const MachineIrFunction **out_function) {
    if (!program || !out_function || function_index >= program->function_count || !program->functions) {
        return 0;
    }

    *out_function = &program->functions[function_index];
    return 1;
}

int machine_ir_program_get_function_by_name(const MachineIrProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineIrFunction **out_function) {
    size_t function_index;

    if (!program || !function_name) {
        return 0;
    }

    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineIrFunction *function = &program->functions[function_index];

        if (function->name && strcmp(function->name, function_name) == 0) {
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_function) {
                *out_function = function;
            }
            return 1;
        }
    }

    return 0;
}

int machine_ir_allocate_rewrite_report_get_summary(const MachineIrAllocateRewriteReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count) {
    if (!report) {
        return 0;
    }

    if (out_allocation_rounds) {
        *out_allocation_rounds = report->allocation_rounds;
    }
    if (out_rewrite_rounds) {
        *out_rewrite_rounds = report->rewrite_rounds;
    }
    return machine_ir_program_get_summary(
        &report->program, out_register_count, out_global_count, out_function_count);
}

int machine_ir_allocate_rewrite_report_get_program(const MachineIrAllocateRewriteReport *report,
    const MachineIrProgram **out_program) {
    if (!report || !out_program) {
        return 0;
    }

    *out_program = &report->program;
    return 1;
}

int machine_ir_allocate_rewrite_report_get_function(const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    const MachineIrFunction **out_function) {
    if (!report) {
        return 0;
    }

    return machine_ir_program_get_function(&report->program, function_index, out_function);
}

int machine_ir_allocate_rewrite_report_get_function_by_name(const MachineIrAllocateRewriteReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineIrFunction **out_function) {
    if (!report) {
        return 0;
    }

    return machine_ir_program_get_function_by_name(
        &report->program, function_name, out_function_index, out_function);
}

int machine_ir_allocate_rewrite_report_get_function_shape_summary(const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    size_t *out_phi_count,
    size_t *out_call_count,
    size_t *out_spill_slot_count,
    size_t *out_load_local_count,
    size_t *out_store_local_count,
    size_t *out_load_global_count,
    size_t *out_store_global_count,
    size_t *out_memory_op_count) {
    if (!report || function_index >= report->program.function_count) {
        return 0;
    }

    if (out_phi_count) {
        *out_phi_count = report->function_phi_counts ? report->function_phi_counts[function_index] : 0;
    }
    if (out_call_count) {
        *out_call_count = report->function_call_counts ? report->function_call_counts[function_index] : 0;
    }
    if (out_spill_slot_count) {
        *out_spill_slot_count =
            report->function_spill_slot_counts ? report->function_spill_slot_counts[function_index] : 0;
    }
    if (out_load_local_count) {
        *out_load_local_count =
            report->function_load_local_counts ? report->function_load_local_counts[function_index] : 0;
    }
    if (out_store_local_count) {
        *out_store_local_count =
            report->function_store_local_counts ? report->function_store_local_counts[function_index] : 0;
    }
    if (out_load_global_count) {
        *out_load_global_count =
            report->function_load_global_counts ? report->function_load_global_counts[function_index] : 0;
    }
    if (out_store_global_count) {
        *out_store_global_count =
            report->function_store_global_counts ? report->function_store_global_counts[function_index] : 0;
    }
    if (out_memory_op_count) {
        *out_memory_op_count =
            report->function_memory_op_counts ? report->function_memory_op_counts[function_index] : 0;
    }
    return 1;
}

int machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(
    const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    const MachineIrFunctionShapeSummary **out_summary) {
    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_shape_summaries) {
        return 0;
    }

    *out_summary = &report->function_shape_summaries[function_index];
    return 1;
}

int machine_ir_allocate_rewrite_report_get_block_shape_summary(const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    size_t block_index,
    const MachineIrBlockShapeSummary **out_summary) {
    size_t offset;
    size_t next_offset;

    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_block_summary_offsets || !report->block_shape_summaries) {
        return 0;
    }

    offset = report->function_block_summary_offsets[function_index];
    next_offset = function_index + 1 < report->program.function_count
        ? report->function_block_summary_offsets[function_index + 1]
        : report->total_block_summary_count;
    if (block_index >= next_offset - offset) {
        return 0;
    }

    *out_summary = &report->block_shape_summaries[offset + block_index];
    return 1;
}

int machine_ir_allocate_rewrite_report_get_functions_with_phi(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_phi_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_phi;
    }
    return 1;
}

int machine_ir_allocate_rewrite_report_get_functions_with_call(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_call_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_call;
    }
    return 1;
}

int machine_ir_allocate_rewrite_report_get_functions_with_spills(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_spills_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_spills;
    }
    return 1;
}

int machine_ir_allocate_rewrite_report_get_functions_with_memory_ops(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_with_memory_ops_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_memory_ops;
    }
    return 1;
}

int machine_ir_allocate_rewrite_report_get_functions_register_only(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (out_count) {
        *out_count = 0;
    }
    if (out_function_indices) {
        *out_function_indices = NULL;
    }
    if (!report) {
        return 0;
    }

    if (out_count) {
        *out_count = report->functions_register_only_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_register_only;
    }
    return 1;
}

#endif
