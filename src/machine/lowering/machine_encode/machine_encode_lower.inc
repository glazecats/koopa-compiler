#if defined(__INTELLISENSE__) && !defined(MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ENCODE_SPLIT_AGGREGATOR)
#define MACHINE_ENCODE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_encode.c"
#else

int machine_encode_lower_program_from_machine_emit(const MachineEmitProgram *source,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-300: invalid lower contract");
        return 0;
    }

    machine_encode_program_free(out_program);

    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineEmitRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineEmitRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-301: out of memory copying registers");
            machine_encode_program_free(out_program);
            return 0;
        }
        out_program->register_bank.register_count = source->register_bank.register_count;
        for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
            out_program->register_bank.registers[register_index] = source->register_bank.registers[register_index];
            out_program->register_bank.registers[register_index].name =
                machine_encode_strdup(source->register_bank.registers[register_index].name);
            if (source->register_bank.registers[register_index].name &&
                !out_program->register_bank.registers[register_index].name) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-302: out of memory copying register name");
                machine_encode_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->global_count > 0) {
        out_program->globals = (MachineEmitGlobal *)calloc(source->global_count, sizeof(MachineEmitGlobal));
        if (!out_program->globals) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-303: out of memory copying globals");
            machine_encode_program_free(out_program);
            return 0;
        }
        out_program->global_count = source->global_count;
        out_program->global_capacity = source->global_count;
        for (global_index = 0; global_index < source->global_count; ++global_index) {
            out_program->globals[global_index] = source->globals[global_index];
            out_program->globals[global_index].name = machine_encode_strdup(source->globals[global_index].name);
            if (source->globals[global_index].name && !out_program->globals[global_index].name) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-304: out of memory copying global name");
                machine_encode_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->function_count > 0) {
        out_program->functions = (MachineEncodeFunction *)calloc(source->function_count, sizeof(MachineEncodeFunction));
        if (!out_program->functions) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-305: out of memory copying functions");
            machine_encode_program_free(out_program);
            return 0;
        }
        out_program->function_count = source->function_count;
        out_program->function_capacity = source->function_count;
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineEmitFunction *src_function = &source->functions[function_index];
        MachineEncodeFunction *dest_function = &out_program->functions[function_index];
        size_t block_index;
        size_t current_offset = 0;

        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->name = machine_encode_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-306: out of memory copying function name");
            machine_encode_program_free(out_program);
            return 0;
        }

        if (src_function->block_count > 0) {
            dest_function->blocks = (MachineEncodeBlock *)calloc(src_function->block_count, sizeof(MachineEncodeBlock));
            if (!dest_function->blocks) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-307: out of memory copying blocks");
                machine_encode_program_free(out_program);
                return 0;
            }
            dest_function->block_count = src_function->block_count;
            dest_function->block_capacity = src_function->block_count;
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineEmitBlock *src_block = &src_function->blocks[block_index];
            MachineEncodeBlock *dest_block = &dest_function->blocks[block_index];

            dest_block->emit_index = src_block->emit_index;
            dest_block->original_layout_index = src_block->original_layout_index;
            dest_block->original_block_id = src_block->original_block_id;
            dest_block->label_name = machine_encode_strdup(src_block->label_name);
            if (src_block->label_name && !dest_block->label_name) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-308: out of memory copying block label");
                machine_encode_program_free(out_program);
                return 0;
            }
            dest_block->start_offset = current_offset;
            dest_block->terminator_offset = current_offset + src_block->op_count;
            dest_block->end_offset = dest_block->terminator_offset + 1;
            dest_block->span.start_offset = dest_block->start_offset;
            dest_block->span.terminator_offset = dest_block->terminator_offset;
            dest_block->span.end_offset = dest_block->end_offset;
            if (!machine_encode_clone_block_ops(dest_block, src_block)) {
                machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-309: out of memory cloning emitted block");
                machine_encode_program_free(out_program);
                return 0;
            }
            current_offset = dest_block->end_offset;
        }
    }

    return machine_encode_verify_program(out_program, error);
}

int machine_encode_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error) {
    MachineEmitProgram emit_program;
    MachineEmitError emit_error;
    int ok;

    if (!source || !out_program) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-310: invalid machine-ir bridge contract");
        return 0;
    }

    memset(&emit_error, 0, sizeof(emit_error));
    machine_emit_program_init(&emit_program);
    ok = machine_emit_lower_program_from_machine_ir(source, &emit_program, &emit_error);
    if (!ok) {
        machine_encode_set_error(error, emit_error.line, emit_error.column, "%s", emit_error.message);
        machine_emit_program_free(&emit_program);
        return 0;
    }
    ok = machine_encode_lower_program_from_machine_emit(&emit_program, out_program, error);
    machine_emit_program_free(&emit_program);
    return ok;
}

int machine_encode_lower_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error) {
    MachineEmitLowerReport emit_report;
    MachineEmitError emit_error;
    int ok;

    if (!report || !out_program) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-312: invalid machine-ir report bridge contract");
        return 0;
    }

    memset(&emit_error, 0, sizeof(emit_error));
    machine_emit_lower_report_init(&emit_report);
    ok = machine_emit_build_report_from_machine_ir_report(report, &emit_report, &emit_error);
    if (!ok) {
        machine_encode_set_error(error, emit_error.line, emit_error.column, "%s", emit_error.message);
        machine_emit_lower_report_free(&emit_report);
        return 0;
    }
    ok = machine_encode_lower_program_from_machine_emit_report(&emit_report, out_program, error);
    machine_emit_lower_report_free(&emit_report);
    return ok;
}

int machine_encode_lower_program_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error) {
    if (!report || !out_program) {
        machine_encode_set_error(error, 0, 0, "MACHINE-ENCODE-311: invalid emit-report bridge contract");
        return 0;
    }
    return machine_encode_lower_program_from_machine_emit(&report->program, out_program, error);
}

int machine_encode_lower_dump_from_machine_emit(const MachineEmitProgram *source,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeProgram program;
    int ok;

    machine_encode_program_init(&program);
    ok = machine_encode_lower_program_from_machine_emit(source, &program, error) &&
        machine_encode_dump_program(&program, out_text, error);
    machine_encode_program_free(&program);
    return ok;
}

int machine_encode_lower_dump_from_machine_emit_report(const MachineEmitLowerReport *report,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeProgram program;
    int ok;

    machine_encode_program_init(&program);
    ok = machine_encode_lower_program_from_machine_emit_report(report, &program, error) &&
        machine_encode_dump_program(&program, out_text, error);
    machine_encode_program_free(&program);
    return ok;
}

int machine_encode_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeProgram program;
    int ok;

    machine_encode_program_init(&program);
    ok = machine_encode_lower_program_from_machine_ir(source, &program, error) &&
        machine_encode_dump_program(&program, out_text, error);
    machine_encode_program_free(&program);
    return ok;
}

int machine_encode_lower_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEncodeError *error) {
    MachineEncodeProgram program;
    int ok;

    machine_encode_program_init(&program);
    ok = machine_encode_lower_program_from_machine_ir_report(report, &program, error) &&
        machine_encode_dump_program(&program, out_text, error);
    machine_encode_program_free(&program);
    return ok;
}

#endif
