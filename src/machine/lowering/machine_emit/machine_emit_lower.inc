#if defined(__INTELLISENSE__) && !defined(MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EMIT_SPLIT_AGGREGATOR)
#define MACHINE_EMIT_SPLIT_INTELLISENSE_CONTEXT
#include "machine_emit.c"
#else

int machine_emit_lower_program_from_machine_layout(const MachineLayoutProgram *source,
    MachineEmitProgram *out_program,
    MachineEmitError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-300: invalid machine-layout lower contract");
        return 0;
    }

    machine_emit_program_free(out_program);
    machine_emit_program_init(out_program);

    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineEmitRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineEmitRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-301: out of memory copying registers");
            machine_emit_program_free(out_program);
            return 0;
        }
        out_program->register_bank.register_count = source->register_bank.register_count;
        for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
            out_program->register_bank.registers[register_index] = source->register_bank.registers[register_index];
            out_program->register_bank.registers[register_index].name =
                machine_emit_strdup(source->register_bank.registers[register_index].name);
            if (source->register_bank.registers[register_index].name &&
                !out_program->register_bank.registers[register_index].name) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-302: out of memory copying register name");
                machine_emit_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->global_count > 0) {
        out_program->globals = (MachineEmitGlobal *)calloc(source->global_count, sizeof(MachineEmitGlobal));
        if (!out_program->globals) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-303: out of memory copying globals");
            machine_emit_program_free(out_program);
            return 0;
        }
        out_program->global_count = source->global_count;
        out_program->global_capacity = source->global_count;
        for (global_index = 0; global_index < source->global_count; ++global_index) {
            out_program->globals[global_index] = source->globals[global_index];
            out_program->globals[global_index].name = machine_emit_strdup(source->globals[global_index].name);
            if (source->globals[global_index].name && !out_program->globals[global_index].name) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-304: out of memory copying global name");
                machine_emit_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->function_count > 0) {
        out_program->functions = (MachineEmitFunction *)calloc(source->function_count, sizeof(MachineEmitFunction));
        if (!out_program->functions) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-305: out of memory copying functions");
            machine_emit_program_free(out_program);
            return 0;
        }
        out_program->function_count = source->function_count;
        out_program->function_capacity = source->function_count;
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineLayoutFunction *src_function = &source->functions[function_index];
        MachineEmitFunction *dest_function = &out_program->functions[function_index];
        size_t block_index;

        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->name = machine_emit_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-306: out of memory copying function name");
            machine_emit_program_free(out_program);
            return 0;
        }

        if (src_function->block_count > 0) {
            dest_function->blocks = (MachineEmitBlock *)calloc(src_function->block_count, sizeof(MachineEmitBlock));
            if (!dest_function->blocks) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-307: out of memory copying blocks");
                machine_emit_program_free(out_program);
                return 0;
            }
            dest_function->block_count = src_function->block_count;
            dest_function->block_capacity = src_function->block_count;
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineLayoutBlock *src_block = &src_function->blocks[block_index];
            MachineEmitBlock *dest_block = &dest_function->blocks[block_index];
            size_t op_index;

            dest_block->emit_index = block_index;
            dest_block->original_layout_index = src_block->layout_index;
            dest_block->original_block_id = src_block->original_block_id;
            dest_block->label_name = machine_emit_make_label(function_index, block_index);
            if (!dest_block->label_name) {
                machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-308: out of memory building emitted label");
                machine_emit_program_free(out_program);
                return 0;
            }
            dest_block->has_terminator = src_block->has_terminator;
            dest_block->terminator = src_block->terminator;

            if (src_block->op_count > 0) {
                dest_block->ops = (MachineEmitOp *)calloc(src_block->op_count, sizeof(MachineEmitOp));
                if (!dest_block->ops) {
                    machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-309: out of memory copying block ops");
                    machine_emit_program_free(out_program);
                    return 0;
                }
                dest_block->op_count = src_block->op_count;
                dest_block->op_capacity = src_block->op_count;
            }

            for (op_index = 0; op_index < src_block->op_count; ++op_index) {
                if (!machine_emit_op_clone(&dest_block->ops[op_index], &src_block->ops[op_index])) {
                    machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-310: out of memory cloning selected op");
                    machine_emit_program_free(out_program);
                    return 0;
                }
            }
        }
    }

    return machine_emit_verify_program(out_program, error);
}

int machine_emit_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineEmitProgram *out_program,
    MachineEmitError *error) {
    MachineLayoutProgram layout_program;
    MachineLayoutError layout_error;
    int ok;

    if (!source || !out_program) {
        machine_emit_set_error(error, 0, 0, "MACHINE-EMIT-311: invalid machine-ir bridge contract");
        return 0;
    }

    memset(&layout_error, 0, sizeof(layout_error));
    machine_layout_program_init(&layout_program);
    ok = machine_layout_lower_program_from_machine_ir(source, &layout_program, &layout_error);
    if (!ok) {
        machine_emit_set_error(error, layout_error.line, layout_error.column, "%s", layout_error.message);
        machine_layout_program_free(&layout_program);
        return 0;
    }
    ok = machine_emit_lower_program_from_machine_layout(&layout_program, out_program, error);
    machine_layout_program_free(&layout_program);
    return ok;
}

int machine_emit_lower_dump_from_machine_layout(const MachineLayoutProgram *source,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitProgram program;
    int ok;

    machine_emit_program_init(&program);
    ok = machine_emit_lower_program_from_machine_layout(source, &program, error) &&
        machine_emit_dump_program(&program, out_text, error);
    machine_emit_program_free(&program);
    return ok;
}

int machine_emit_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineEmitError *error) {
    MachineEmitProgram program;
    int ok;

    machine_emit_program_init(&program);
    ok = machine_emit_lower_program_from_machine_ir(source, &program, error) &&
        machine_emit_dump_program(&program, out_text, error);
    machine_emit_program_free(&program);
    return ok;
}

int machine_emit_function_compute_summary(const MachineEmitFunction *function,
    MachineEmitFunctionSummary *out_summary) {
    size_t block_index;

    if (!function || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->block_count = function->block_count;

    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineEmitBlock *block = &function->blocks[block_index];

        out_summary->op_count += block->op_count;
        switch (block->terminator.kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
                ++out_summary->return_count;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
                ++out_summary->return_imm_count;
                break;
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                ++out_summary->return_spill_count;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                ++out_summary->fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_JUMP:
                ++out_summary->jump_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
                ++out_summary->branch_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                ++out_summary->branch_fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
                ++out_summary->compare_branch_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                ++out_summary->compare_branch_imm_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
                ++out_summary->compare_branch_fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                ++out_summary->compare_branch_imm_fallthrough_count;
                break;
            default:
                break;
        }
    }
    return 1;
}

#endif
