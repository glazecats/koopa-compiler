#if defined(__INTELLISENSE__) && !defined(MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EMIT_SPLIT_AGGREGATOR)
#define MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_emit.c"
#else

int machine_emit_program_get_summary(const MachineEmitProgram *program,
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

int machine_emit_program_get_function(const MachineEmitProgram *program,
    size_t function_index,
    const MachineEmitFunction **out_function) {
    if (!program || !out_function || function_index >= program->function_count || !program->functions) {
        return 0;
    }
    *out_function = &program->functions[function_index];
    return 1;
}

int machine_emit_program_get_function_by_name(const MachineEmitProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineEmitFunction **out_function) {
    size_t function_index;

    if (!program || !function_name || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEmitFunction *function = &program->functions[function_index];

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

int machine_emit_program_find_block_by_label(const MachineEmitProgram *program,
    const char *label_name,
    size_t *out_function_index,
    const MachineEmitFunction **out_function,
    const MachineEmitBlock **out_block) {
    size_t function_index;

    if (!program || !label_name || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineEmitFunction *function = &program->functions[function_index];
        size_t block_index;

        if (!function->blocks) {
            continue;
        }
        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineEmitBlock *block = &function->blocks[block_index];

            if (block->label_name && strcmp(block->label_name, label_name) == 0) {
                if (out_function_index) {
                    *out_function_index = function_index;
                }
                if (out_function) {
                    *out_function = function;
                }
                if (out_block) {
                    *out_block = block;
                }
                return 1;
            }
        }
    }
    return 0;
}

int machine_emit_lower_report_get_summary(const MachineEmitLowerReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count) {
    if (!report) {
        return 0;
    }
    if (!machine_emit_program_get_summary(
            &report->program, out_register_count, out_global_count, out_function_count)) {
        return 0;
    }
    if (out_total_block_summary_count) {
        *out_total_block_summary_count = report->total_block_summary_count;
    }
    return 1;
}

int machine_emit_lower_report_get_program(const MachineEmitLowerReport *report,
    const MachineEmitProgram **out_program) {
    if (!report || !out_program) {
        return 0;
    }
    *out_program = &report->program;
    return 1;
}

int machine_emit_lower_report_get_function(const MachineEmitLowerReport *report,
    size_t function_index,
    const MachineEmitFunction **out_function) {
    if (!report) {
        return 0;
    }
    return machine_emit_program_get_function(&report->program, function_index, out_function);
}

int machine_emit_lower_report_get_function_by_name(const MachineEmitLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineEmitFunction **out_function) {
    if (!report) {
        return 0;
    }
    return machine_emit_program_get_function_by_name(
        &report->program, function_name, out_function_index, out_function);
}

int machine_emit_lower_report_find_block_shape_by_label(const MachineEmitLowerReport *report,
    const char *label_name,
    size_t *out_function_index,
    const MachineEmitBlockShapeSummary **out_summary) {
    size_t function_index;

    if (!report || !label_name || !out_summary || !report->function_block_summary_offsets ||
        !report->block_shape_summaries) {
        return 0;
    }
    for (function_index = 0; function_index < report->program.function_count; ++function_index) {
        size_t block_offset = report->function_block_summary_offsets[function_index];
        size_t next_block_offset = function_index + 1 < report->program.function_count
            ? report->function_block_summary_offsets[function_index + 1]
            : report->total_block_summary_count;
        size_t block_summary_index;

        for (block_summary_index = block_offset; block_summary_index < next_block_offset; ++block_summary_index) {
            const MachineEmitBlockShapeSummary *summary = &report->block_shape_summaries[block_summary_index];

            if (summary->label_name && strcmp(summary->label_name, label_name) == 0) {
                if (out_function_index) {
                    *out_function_index = function_index;
                }
                *out_summary = summary;
                return 1;
            }
        }
    }
    return 0;
}

int machine_emit_lower_report_get_function_shape_summary(const MachineEmitLowerReport *report,
    size_t function_index,
    const MachineEmitFunctionShapeSummary **out_summary) {
    if (!report || !out_summary || function_index >= report->program.function_count ||
        !report->function_shape_summaries) {
        return 0;
    }
    *out_summary = &report->function_shape_summaries[function_index];
    return 1;
}

int machine_emit_lower_report_get_block_shape_summary(const MachineEmitLowerReport *report,
    size_t function_index,
    size_t block_index,
    const MachineEmitBlockShapeSummary **out_summary) {
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

int machine_emit_lower_report_get_functions_with_calls(const MachineEmitLowerReport *report,
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
        *out_count = report->functions_with_calls_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_calls;
    }
    return 1;
}

int machine_emit_lower_report_get_functions_with_fallthrough(const MachineEmitLowerReport *report,
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
        *out_count = report->functions_with_fallthrough_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_fallthrough;
    }
    return 1;
}

int machine_emit_lower_report_get_functions_with_branches(const MachineEmitLowerReport *report,
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
        *out_count = report->functions_with_branches_count;
    }
    if (out_function_indices) {
        *out_function_indices = report->function_indices_with_branches;
    }
    return 1;
}

int machine_emit_function_get_summary(const MachineEmitFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count) {
    if (!function) {
        return 0;
    }
    if (out_name) {
        *out_name = function->name;
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

int machine_emit_function_get_block(const MachineEmitFunction *function,
    size_t emit_index,
    const MachineEmitBlock **out_block) {
    if (!function || !out_block || emit_index >= function->block_count || !function->blocks) {
        return 0;
    }
    *out_block = &function->blocks[emit_index];
    return 1;
}

int machine_emit_function_find_block_by_label(const MachineEmitFunction *function,
    const char *label_name,
    size_t *out_emit_index,
    const MachineEmitBlock **out_block) {
    size_t block_index;

    if (!function || !label_name || !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineEmitBlock *block = &function->blocks[block_index];

        if (block->label_name && strcmp(block->label_name, label_name) == 0) {
            if (out_emit_index) {
                *out_emit_index = block_index;
            }
            if (out_block) {
                *out_block = block;
            }
            return 1;
        }
    }
    return 0;
}

int machine_emit_block_get_summary(const MachineEmitBlock *block,
    MachineEmitBlockSummary *out_summary) {
    if (!block || !out_summary) {
        return 0;
    }
    out_summary->emit_index = block->emit_index;
    out_summary->original_layout_index = block->original_layout_index;
    out_summary->original_block_id = block->original_block_id;
    out_summary->label_name = block->label_name;
    out_summary->op_count = block->op_count;
    out_summary->has_terminator = block->has_terminator;
    out_summary->terminator_kind = block->terminator.kind;
    return 1;
}

#endif
