#if defined(__INTELLISENSE__) && !defined(MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LAYOUT_SPLIT_AGGREGATOR)
#define MACHINE_LAYOUT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_layout.c"
#else

void machine_layout_program_init(MachineLayoutProgram *program) {
    if (!program) {
        return;
    }
    program->register_bank.register_count = 0;
    program->register_bank.registers = NULL;
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

void machine_layout_program_free(MachineLayoutProgram *program) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!program) {
        return;
    }
    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        machine_layout_register_desc_free(&program->register_bank.registers[register_index]);
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        machine_layout_global_free(&program->globals[global_index]);
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        machine_layout_function_free(&program->functions[function_index]);
    }
    free(program->register_bank.registers);
    free(program->globals);
    free(program->functions);
    machine_layout_program_init(program);
}

void machine_layout_report_init(MachineLayoutReport *report) {
    if (!report) {
        return;
    }
    machine_layout_program_init(&report->program);
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->function_summaries = NULL;
    report->function_summary_count = 0;
    report->function_block_summary_offsets = NULL;
    report->block_summaries = NULL;
    report->total_block_summary_count = 0;
    report->functions_with_fallthrough_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_fallthrough = NULL;
    report->function_indices_with_branches = NULL;
}

void machine_layout_report_free(MachineLayoutReport *report) {
    if (!report) {
        return;
    }
    machine_layout_program_free(&report->program);
    free(report->function_summaries);
    free(report->function_block_summary_offsets);
    free(report->block_summaries);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    machine_layout_report_init(report);
}

int machine_layout_clone_program(const MachineLayoutProgram *source,
    MachineLayoutProgram *out_program,
    MachineLayoutError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-050: invalid clone contract");
        return 0;
    }

    machine_layout_program_free(out_program);

    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineLayoutRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineLayoutRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-051: out of memory cloning registers");
            machine_layout_program_free(out_program);
            return 0;
        }
        out_program->register_bank.register_count = source->register_bank.register_count;
        for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
            out_program->register_bank.registers[register_index] = source->register_bank.registers[register_index];
            out_program->register_bank.registers[register_index].name =
                machine_layout_strdup(source->register_bank.registers[register_index].name);
            if (source->register_bank.registers[register_index].name &&
                !out_program->register_bank.registers[register_index].name) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-052: out of memory cloning register name");
                machine_layout_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->global_count > 0) {
        out_program->globals = (MachineLayoutGlobal *)calloc(source->global_count, sizeof(MachineLayoutGlobal));
        if (!out_program->globals) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-053: out of memory cloning globals");
            machine_layout_program_free(out_program);
            return 0;
        }
        out_program->global_count = source->global_count;
        out_program->global_capacity = source->global_count;
        for (global_index = 0; global_index < source->global_count; ++global_index) {
            out_program->globals[global_index] = source->globals[global_index];
            out_program->globals[global_index].name = machine_layout_strdup(source->globals[global_index].name);
            if (source->globals[global_index].name && !out_program->globals[global_index].name) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-054: out of memory cloning global name");
                machine_layout_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->function_count > 0) {
        out_program->functions = (MachineLayoutFunction *)calloc(source->function_count, sizeof(MachineLayoutFunction));
        if (!out_program->functions) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-055: out of memory cloning functions");
            machine_layout_program_free(out_program);
            return 0;
        }
        out_program->function_count = source->function_count;
        out_program->function_capacity = source->function_count;
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineLayoutFunction *src_function = &source->functions[function_index];
        MachineLayoutFunction *dest_function = &out_program->functions[function_index];
        size_t block_index;

        dest_function->name = machine_layout_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-056: out of memory cloning function name");
            machine_layout_program_free(out_program);
            return 0;
        }
        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->block_count = src_function->block_count;
        dest_function->block_capacity = src_function->block_count;

        if (src_function->block_count > 0) {
            dest_function->blocks = (MachineLayoutBlock *)calloc(src_function->block_count, sizeof(MachineLayoutBlock));
            if (!dest_function->blocks) {
                machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-057: out of memory cloning blocks");
                machine_layout_program_free(out_program);
                return 0;
            }
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineLayoutBlock *src_block = &src_function->blocks[block_index];
            MachineLayoutBlock *dest_block = &dest_function->blocks[block_index];
            size_t op_index;

            dest_block->layout_index = src_block->layout_index;
            dest_block->original_block_id = src_block->original_block_id;
            dest_block->has_terminator = src_block->has_terminator;
            dest_block->terminator = src_block->terminator;

            if (src_block->op_count > 0) {
                dest_block->ops = (MachineLayoutOp *)calloc(src_block->op_count, sizeof(MachineLayoutOp));
                if (!dest_block->ops) {
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-058: out of memory cloning block ops");
                    machine_layout_program_free(out_program);
                    return 0;
                }
                dest_block->op_count = src_block->op_count;
                dest_block->op_capacity = src_block->op_count;
            }

            for (op_index = 0; op_index < src_block->op_count; ++op_index) {
                if (!machine_layout_op_clone(&dest_block->ops[op_index], &src_block->ops[op_index])) {
                    machine_layout_set_error(error, 0, 0, "MACHINE-LAYOUT-059: out of memory cloning layout op");
                    machine_layout_program_free(out_program);
                    return 0;
                }
            }
        }
    }

    return machine_layout_verify_program(out_program, error);
}

#endif
