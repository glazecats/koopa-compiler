#if defined(__INTELLISENSE__) && !defined(MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_BYTES_SPLIT_AGGREGATOR)
#define MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT
#include "machine_bytes.c"
#else

int machine_bytes_lower_program_from_machine_encode_with_profile(const MachineEncodeProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program || !machine_bytes_target_profile_is_valid(profile)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-300: invalid lower contract");
        return 0;
    }

    machine_bytes_program_free(out_program);
    out_program->target_profile = profile;

    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineEmitRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineEmitRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-301: out of memory copying registers");
            machine_bytes_program_free(out_program);
            return 0;
        }
        out_program->register_bank.register_count = source->register_bank.register_count;
        for (register_index = 0; register_index < source->register_bank.register_count; ++register_index) {
            out_program->register_bank.registers[register_index] = source->register_bank.registers[register_index];
            out_program->register_bank.registers[register_index].name =
                machine_bytes_strdup(source->register_bank.registers[register_index].name);
            if (source->register_bank.registers[register_index].name &&
                !out_program->register_bank.registers[register_index].name) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-302: out of memory copying register name");
                machine_bytes_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->global_count > 0) {
        out_program->globals = (MachineEmitGlobal *)calloc(source->global_count, sizeof(MachineEmitGlobal));
        if (!out_program->globals) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-303: out of memory copying globals");
            machine_bytes_program_free(out_program);
            return 0;
        }
        out_program->global_count = source->global_count;
        out_program->global_capacity = source->global_count;
        for (global_index = 0; global_index < source->global_count; ++global_index) {
            out_program->globals[global_index] = source->globals[global_index];
            out_program->globals[global_index].name = machine_bytes_strdup(source->globals[global_index].name);
            if (source->globals[global_index].name && !out_program->globals[global_index].name) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-304: out of memory copying global name");
                machine_bytes_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->function_count > 0) {
        out_program->functions = (MachineBytesFunction *)calloc(source->function_count, sizeof(MachineBytesFunction));
        if (!out_program->functions) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-305: out of memory copying functions");
            machine_bytes_program_free(out_program);
            return 0;
        }
        out_program->function_count = source->function_count;
        out_program->function_capacity = source->function_count;
    }

        for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineEncodeFunction *src_function = &source->functions[function_index];
        MachineBytesFunction *dest_function = &out_program->functions[function_index];
        size_t block_index;
        size_t running_offset = 0;

        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->name = machine_bytes_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-306: out of memory copying function name");
            machine_bytes_program_free(out_program);
            return 0;
        }
        if (src_function->block_count > 0) {
            dest_function->blocks = (MachineBytesBlock *)calloc(src_function->block_count, sizeof(MachineBytesBlock));
            if (!dest_function->blocks) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-307: out of memory copying blocks");
                machine_bytes_program_free(out_program);
                return 0;
            }
            dest_function->block_count = src_function->block_count;
            dest_function->block_capacity = src_function->block_count;
        }

        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            const MachineEncodeBlock *src_block = &src_function->blocks[block_index];
            MachineBytesBlock *dest_block = &dest_function->blocks[block_index];
            size_t op_index;
            size_t byte_count = 0;

            if (!machine_bytes_clone_block_payload(dest_block, src_block)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-308: out of memory cloning encoded block");
                machine_bytes_program_free(out_program);
                return 0;
            }
            for (op_index = 0; op_index < src_block->block.op_count; ++op_index) {
                byte_count += machine_bytes_op_encoded_size_for_profile(profile, &src_block->block.ops[op_index]);
            }
            byte_count += machine_bytes_terminator_encoded_size_for_profile(profile, &src_block->block.terminator);
            dest_block->start_byte_offset = running_offset;
            dest_block->terminator_byte_offset =
                running_offset + byte_count -
                machine_bytes_terminator_encoded_size_for_profile(profile, &src_block->block.terminator);
            dest_block->end_byte_offset = running_offset + byte_count;
            dest_block->byte_count = byte_count;
            dest_block->bytes = (unsigned char *)malloc(dest_block->byte_count);
            if (!dest_block->bytes) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-309: out of memory allocating block bytes");
                machine_bytes_program_free(out_program);
                return 0;
            }
            if (!machine_bytes_write_block_bytes_for_profile(profile, dest_block)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-310: failed to materialize block bytes");
                machine_bytes_program_free(out_program);
                return 0;
            }
            running_offset = dest_block->end_byte_offset;
        }
    }

    return machine_bytes_verify_program(out_program, error);
}

#endif

int machine_bytes_lower_program_from_machine_encode(const MachineEncodeProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    return machine_bytes_lower_program_from_machine_encode_with_profile(
        source,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_program,
        error);
}

int machine_bytes_lower_program_from_machine_encode_report_with_profile(const MachineEncodeReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    if (!report || !out_program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-310: invalid encode-report bridge contract");
        return 0;
    }
    return machine_bytes_lower_program_from_machine_encode_with_profile(&report->program, profile, out_program, error);
}

int machine_bytes_lower_program_from_machine_encode_report(const MachineEncodeReport *report,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    return machine_bytes_lower_program_from_machine_encode_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_program,
        error);
}

int machine_bytes_lower_program_from_machine_emit_with_profile(const MachineEmitProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    MachineEncodeProgram encode_program;
    MachineEncodeError encode_error;
    int ok;

    if (!source || !out_program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-313: invalid machine-emit bridge contract");
        return 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    machine_encode_program_init(&encode_program);
    ok = machine_encode_lower_program_from_machine_emit(source, &encode_program, &encode_error);
    if (!ok) {
        machine_bytes_set_error(error, encode_error.line, encode_error.column, "%s", encode_error.message);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    ok = machine_bytes_lower_program_from_machine_encode_with_profile(&encode_program, profile, out_program, error);
    machine_encode_program_free(&encode_program);
    return ok;
}

int machine_bytes_lower_program_from_machine_emit(const MachineEmitProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    return machine_bytes_lower_program_from_machine_emit_with_profile(
        source,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_program,
        error);
}

int machine_bytes_lower_program_from_machine_emit_report_with_profile(const MachineEmitLowerReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    MachineEncodeReport encode_report;
    MachineEncodeError encode_error;
    int ok;

    if (!report || !out_program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-314: invalid machine-emit report bridge contract");
        return 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    machine_encode_report_init(&encode_report);
    ok = machine_encode_build_report_from_machine_emit_report(report, &encode_report, &encode_error);
    if (!ok) {
        machine_bytes_set_error(error, encode_error.line, encode_error.column, "%s", encode_error.message);
        machine_encode_report_free(&encode_report);
        return 0;
    }
    ok = machine_bytes_lower_program_from_machine_encode_report_with_profile(&encode_report, profile, out_program, error);
    machine_encode_report_free(&encode_report);
    return ok;
}

int machine_bytes_lower_program_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    return machine_bytes_lower_program_from_machine_emit_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_program,
        error);
}

int machine_bytes_lower_program_from_machine_ir_with_profile(const MachineIrProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    MachineEncodeProgram encode_program;
    MachineEncodeError encode_error;
    int ok;

    if (!source || !out_program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-311: invalid machine-ir bridge contract");
        return 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    machine_encode_program_init(&encode_program);
    ok = machine_encode_lower_program_from_machine_ir(source, &encode_program, &encode_error);
    if (!ok) {
        machine_bytes_set_error(error, encode_error.line, encode_error.column, "%s", encode_error.message);
        machine_encode_program_free(&encode_program);
        return 0;
    }
    ok = machine_bytes_lower_program_from_machine_encode_with_profile(&encode_program, profile, out_program, error);
    machine_encode_program_free(&encode_program);
    return ok;
}

int machine_bytes_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    return machine_bytes_lower_program_from_machine_ir_with_profile(
        source,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_program,
        error);
}

int machine_bytes_lower_program_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    MachineEncodeReport encode_report;
    MachineEncodeError encode_error;
    int ok;

    if (!report || !out_program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-312: invalid machine-ir report bridge contract");
        return 0;
    }

    memset(&encode_error, 0, sizeof(encode_error));
    machine_encode_report_init(&encode_report);
    ok = machine_encode_build_report_from_machine_ir_report(report, &encode_report, &encode_error);
    if (!ok) {
        machine_bytes_set_error(error, encode_error.line, encode_error.column, "%s", encode_error.message);
        machine_encode_report_free(&encode_report);
        return 0;
    }
    ok = machine_bytes_lower_program_from_machine_encode_report_with_profile(&encode_report, profile, out_program, error);
    machine_encode_report_free(&encode_report);
    return ok;
}

int machine_bytes_lower_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    return machine_bytes_lower_program_from_machine_ir_report_with_profile(
        report,
        MACHINE_BYTES_TARGET_PROFILE_GENERIC,
        out_program,
        error);
}
