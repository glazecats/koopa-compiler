#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

int machine_layout_program_get_summary(const MachineLayoutProgram *program,
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

int machine_layout_program_get_function(const MachineLayoutProgram *program,
    size_t function_index,
    const MachineLayoutFunction **out_function) {
    if (!program || !out_function || function_index >= program->function_count || !program->functions) {
        return 0;
    }
    *out_function = &program->functions[function_index];
    return 1;
}

int machine_layout_program_get_function_by_name(const MachineLayoutProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineLayoutFunction **out_function) {
    size_t function_index;

    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function) {
        *out_function = NULL;
    }
    if (!program || !function_name || (program->function_count > 0 && !program->functions)) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineLayoutFunction *function = &program->functions[function_index];

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

int machine_layout_function_get_summary(const MachineLayoutFunction *function,
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

int machine_layout_function_get_block(const MachineLayoutFunction *function,
    size_t layout_index,
    const MachineLayoutBlock **out_block) {
    if (!function || !out_block || layout_index >= function->block_count || !function->blocks) {
        return 0;
    }
    *out_block = &function->blocks[layout_index];
    return 1;
}

int machine_layout_block_get_summary(const MachineLayoutBlock *block,
    MachineLayoutBlockSummary *out_summary) {
    if (!block || !out_summary) {
        return 0;
    }
    out_summary->layout_index = block->layout_index;
    out_summary->original_block_id = block->original_block_id;
    out_summary->op_count = block->op_count;
    out_summary->has_terminator = block->has_terminator;
    out_summary->terminator_kind = block->terminator.kind;
    return 1;
}

#endif
