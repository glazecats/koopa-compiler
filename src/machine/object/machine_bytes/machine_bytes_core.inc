#if defined(__INTELLISENSE__) && !defined(MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_BYTES_SPLIT_AGGREGATOR)
#define MACHINE_BYTES_SPLIT_INTELLISENSE_CONTEXT
#include "machine_bytes.c"
#else

void machine_bytes_program_init(MachineBytesProgram *program) {
    if (!program) {
        return;
    }
    program->target_profile = MACHINE_BYTES_TARGET_PROFILE_GENERIC;
    program->register_bank.register_count = 0;
    program->register_bank.registers = NULL;
    program->globals = NULL;
    program->global_count = 0;
    program->global_capacity = 0;
    program->functions = NULL;
    program->function_count = 0;
    program->function_capacity = 0;
}

#endif

void machine_bytes_report_init(MachineBytesReport *report) {
    if (!report) {
        return;
    }
    machine_bytes_program_init(&report->program);
    report->function_summaries = NULL;
    report->function_byte_offsets = NULL;
    report->function_block_summary_offsets = NULL;
    report->function_reference_offsets = NULL;
    report->function_fixup_offsets = NULL;
    report->block_summaries = NULL;
    report->reference_summaries = NULL;
    report->fixup_summaries = NULL;
    report->symbol_summaries = NULL;
    report->section_summaries = NULL;
    report->total_block_summary_count = 0;
    report->total_program_byte_count = 0;
    report->total_reference_summary_count = 0;
    report->total_fixup_summary_count = 0;
    report->total_symbol_summary_count = 0;
    report->total_section_summary_count = 0;
    report->functions_with_calls_count = 0;
    report->functions_with_fallthrough_count = 0;
    report->functions_with_branches_count = 0;
    report->function_indices_with_calls = NULL;
    report->function_indices_with_fallthrough = NULL;
    report->function_indices_with_branches = NULL;
}

void machine_bytes_program_free(MachineBytesProgram *program) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!program) {
        return;
    }
    for (register_index = 0; register_index < program->register_bank.register_count; ++register_index) {
        machine_bytes_register_desc_free(&program->register_bank.registers[register_index]);
    }
    for (global_index = 0; global_index < program->global_count; ++global_index) {
        free(program->globals[global_index].name);
        program->globals[global_index].name = NULL;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        machine_bytes_function_free(&program->functions[function_index]);
    }
    free(program->register_bank.registers);
    free(program->globals);
    free(program->functions);
    machine_bytes_program_init(program);
}

void machine_bytes_report_free(MachineBytesReport *report) {
    if (!report) {
        return;
    }
    machine_bytes_program_free(&report->program);
    free(report->function_summaries);
    free(report->function_byte_offsets);
    free(report->function_block_summary_offsets);
    free(report->function_reference_offsets);
    free(report->function_fixup_offsets);
    free(report->block_summaries);
    free(report->reference_summaries);
    free(report->fixup_summaries);
    free(report->symbol_summaries);
    free(report->section_summaries);
    free(report->function_indices_with_calls);
    free(report->function_indices_with_fallthrough);
    free(report->function_indices_with_branches);
    machine_bytes_report_init(report);
}

int machine_bytes_clone_program(const MachineBytesProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error) {
    size_t register_index;
    size_t global_index;
    size_t function_index;

    if (!source || !out_program) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-050: invalid clone contract");
        return 0;
    }

    machine_bytes_program_free(out_program);
    out_program->target_profile = source->target_profile;

    if (source->register_bank.register_count > 0) {
        out_program->register_bank.registers =
            (MachineEmitRegisterDesc *)calloc(source->register_bank.register_count, sizeof(MachineEmitRegisterDesc));
        if (!out_program->register_bank.registers) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-051: out of memory cloning registers");
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
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-052: out of memory cloning register name");
                machine_bytes_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->global_count > 0) {
        out_program->globals = (MachineEmitGlobal *)calloc(source->global_count, sizeof(MachineEmitGlobal));
        if (!out_program->globals) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-053: out of memory cloning globals");
            machine_bytes_program_free(out_program);
            return 0;
        }
        out_program->global_count = source->global_count;
        out_program->global_capacity = source->global_count;
        for (global_index = 0; global_index < source->global_count; ++global_index) {
            out_program->globals[global_index] = source->globals[global_index];
            out_program->globals[global_index].name = machine_bytes_strdup(source->globals[global_index].name);
            if (source->globals[global_index].name && !out_program->globals[global_index].name) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-054: out of memory cloning global name");
                machine_bytes_program_free(out_program);
                return 0;
            }
        }
    }

    if (source->function_count > 0) {
        out_program->functions = (MachineBytesFunction *)calloc(source->function_count, sizeof(MachineBytesFunction));
        if (!out_program->functions) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-055: out of memory cloning functions");
            machine_bytes_program_free(out_program);
            return 0;
        }
        out_program->function_count = source->function_count;
        out_program->function_capacity = source->function_count;
    }

    for (function_index = 0; function_index < source->function_count; ++function_index) {
        const MachineBytesFunction *src_function = &source->functions[function_index];
        MachineBytesFunction *dest_function = &out_program->functions[function_index];
        size_t block_index;

        dest_function->has_body = src_function->has_body;
        dest_function->parameter_count = src_function->parameter_count;
        dest_function->local_count = src_function->local_count;
        dest_function->spill_slot_count = src_function->spill_slot_count;
        dest_function->name = machine_bytes_strdup(src_function->name);
        if (src_function->name && !dest_function->name) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-056: out of memory cloning function name");
            machine_bytes_program_free(out_program);
            return 0;
        }
        if (src_function->block_count > 0) {
            dest_function->blocks = (MachineBytesBlock *)calloc(src_function->block_count, sizeof(MachineBytesBlock));
            if (!dest_function->blocks) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-057: out of memory cloning blocks");
                machine_bytes_program_free(out_program);
                return 0;
            }
            dest_function->block_count = src_function->block_count;
            dest_function->block_capacity = src_function->block_count;
        }
        for (block_index = 0; block_index < src_function->block_count; ++block_index) {
            MachineBytesBlock *dest_block = &dest_function->blocks[block_index];
            const MachineBytesBlock *src_block = &src_function->blocks[block_index];
            MachineEncodeBlock temp_block;

            memset(&temp_block, 0, sizeof(temp_block));
            temp_block.emit_index = src_block->emit_index;
            temp_block.original_layout_index = src_block->original_layout_index;
            temp_block.original_block_id = src_block->original_block_id;
            temp_block.label_name = src_block->label_name;
            temp_block.start_offset = src_block->start_byte_offset;
            temp_block.terminator_offset = src_block->terminator_byte_offset;
            temp_block.end_offset = src_block->end_byte_offset;
            temp_block.block = src_block->block;

            if (!machine_bytes_clone_block_payload(dest_block, &temp_block)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-058: out of memory cloning block payload");
                machine_bytes_program_free(out_program);
                return 0;
            }
            dest_block->start_byte_offset = src_block->start_byte_offset;
            dest_block->terminator_byte_offset = src_block->terminator_byte_offset;
            dest_block->end_byte_offset = src_block->end_byte_offset;
            if (!machine_bytes_clone_block_bytes(dest_block, src_block)) {
                machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-059: out of memory cloning block bytes");
                machine_bytes_program_free(out_program);
                return 0;
            }
        }
    }

    return 1;
}

int machine_bytes_program_get_summary(const MachineBytesProgram *program,
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

int machine_bytes_program_get_function(const MachineBytesProgram *program,
    size_t function_index,
    const MachineBytesFunction **out_function) {
    if (!program || !out_function || function_index >= program->function_count || !program->functions) {
        return 0;
    }
    *out_function = &program->functions[function_index];
    return 1;
}

int machine_bytes_program_get_function_by_name(const MachineBytesProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineBytesFunction **out_function) {
    size_t function_index;

    if (!program || !function_name || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = &program->functions[function_index];

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

int machine_bytes_program_find_block_by_label(const MachineBytesProgram *program,
    const char *label_name,
    size_t *out_function_index,
    const MachineBytesFunction **out_function,
    const MachineBytesBlock **out_block) {
    size_t function_index;

    if (!program || !label_name || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = &program->functions[function_index];
        size_t block_index;

        for (block_index = 0; block_index < function->block_count; ++block_index) {
            const MachineBytesBlock *block = &function->blocks[block_index];

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

int machine_bytes_program_find_block_by_function_name_and_offset(const MachineBytesProgram *program,
    const char *function_name,
    size_t byte_offset,
    size_t *out_function_index,
    size_t *out_emit_index,
    const MachineBytesFunction **out_function,
    const MachineBytesBlock **out_block) {
    const MachineBytesFunction *function;
    size_t function_index = 0;

    if (!program || !function_name) {
        return 0;
    }
    if (!machine_bytes_program_get_function_by_name(program, function_name, &function_index, &function)) {
        return 0;
    }
    if (!machine_bytes_function_find_block_covering_offset(function, byte_offset, out_emit_index, out_block)) {
        return 0;
    }
    if (out_function_index) {
        *out_function_index = function_index;
    }
    if (out_function) {
        *out_function = function;
    }
    return 1;
}

int machine_bytes_program_find_block_by_program_byte_offset(const MachineBytesProgram *program,
    size_t program_byte_offset,
    size_t *out_function_index,
    size_t *out_emit_index,
    const MachineBytesFunction **out_function,
    const MachineBytesBlock **out_block) {
    size_t function_index;
    size_t running_offset = 0;

    if (!program || !program->functions) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = &program->functions[function_index];
        size_t function_byte_count = machine_bytes_function_total_byte_count(function);

        if (program_byte_offset >= running_offset && program_byte_offset < running_offset + function_byte_count) {
            if (!machine_bytes_function_find_block_covering_offset(
                    function, program_byte_offset - running_offset, out_emit_index, out_block)) {
                return 0;
            }
            if (out_function_index) {
                *out_function_index = function_index;
            }
            if (out_function) {
                *out_function = function;
            }
            return 1;
        }
        running_offset += function_byte_count;
    }
    return 0;
}

int machine_bytes_program_get_total_byte_count(const MachineBytesProgram *program,
    size_t *out_total_byte_count) {
    size_t total_byte_count = 0;
    size_t function_index;

    if (!program || !out_total_byte_count) {
        return 0;
    }
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        total_byte_count += machine_bytes_function_total_byte_count(&program->functions[function_index]);
    }
    *out_total_byte_count = total_byte_count;
    return 1;
}

int machine_bytes_program_get_function_byte_span(const MachineBytesProgram *program,
    size_t function_index,
    size_t *out_start_program_byte_offset,
    size_t *out_end_program_byte_offset) {
    size_t start = 0;
    size_t index;

    if (!program || function_index >= program->function_count || !program->functions) {
        return 0;
    }
    for (index = 0; index < function_index; ++index) {
        start += machine_bytes_function_total_byte_count(&program->functions[index]);
    }
    if (out_start_program_byte_offset) {
        *out_start_program_byte_offset = start;
    }
    if (out_end_program_byte_offset) {
        *out_end_program_byte_offset = start + machine_bytes_function_total_byte_count(&program->functions[function_index]);
    }
    return 1;
}

int machine_bytes_program_copy_bytes(const MachineBytesProgram *program,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    unsigned char *bytes = NULL;
    size_t total_byte_count = 0;
    size_t function_index;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!program || !out_bytes) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-122: invalid program byte-copy contract");
        return 0;
    }
    if (!machine_bytes_program_get_total_byte_count(program, &total_byte_count)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-123: invalid program byte-count query");
        return 0;
    }
    if (total_byte_count == 0) {
        return 1;
    }

    bytes = (unsigned char *)malloc(total_byte_count);
    if (!bytes) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-124: out of memory copying program bytes");
        return 0;
    }

    total_byte_count = 0;
    for (function_index = 0; function_index < program->function_count; ++function_index) {
        const MachineBytesFunction *function = &program->functions[function_index];
        size_t function_byte_count = machine_bytes_function_total_byte_count(function);

        if (!machine_bytes_copy_function_bytes(function, bytes + total_byte_count, function_byte_count)) {
            machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-125: failed to flatten function byte image");
            free(bytes);
            return 0;
        }
        total_byte_count += function_byte_count;
    }

    *out_bytes = bytes;
    if (out_byte_count) {
        *out_byte_count = total_byte_count;
    }
    return 1;
}

int machine_bytes_program_copy_function_bytes(const MachineBytesProgram *program,
    size_t function_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    const MachineBytesFunction *function;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!program || !out_bytes ||
        !machine_bytes_program_get_function(program, function_index, &function)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-126: invalid program function byte-copy contract");
        return 0;
    }
    return machine_bytes_function_copy_bytes(function, out_bytes, out_byte_count, error);
}

int machine_bytes_program_copy_block_bytes(const MachineBytesProgram *program,
    size_t function_index,
    size_t emit_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    const MachineBytesFunction *function;
    const MachineBytesBlock *block;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!program || !out_bytes ||
        !machine_bytes_program_get_function(program, function_index, &function) ||
        !machine_bytes_function_get_block(function, emit_index, &block)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-127: invalid program block byte-copy contract");
        return 0;
    }
    return machine_bytes_block_copy_bytes(block, out_bytes, out_byte_count, error);
}

int machine_bytes_function_get_summary(const MachineBytesFunction *function,
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

int machine_bytes_function_get_block(const MachineBytesFunction *function,
    size_t emit_index,
    const MachineBytesBlock **out_block) {
    if (!function || !out_block || emit_index >= function->block_count || !function->blocks) {
        return 0;
    }
    *out_block = &function->blocks[emit_index];
    return 1;
}

int machine_bytes_function_find_block_by_label(const MachineBytesFunction *function,
    const char *label_name,
    size_t *out_emit_index,
    const MachineBytesBlock **out_block) {
    size_t block_index;

    if (!function || !label_name || !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        if (function->blocks[block_index].label_name &&
            strcmp(function->blocks[block_index].label_name, label_name) == 0) {
            if (out_emit_index) {
                *out_emit_index = block_index;
            }
            if (out_block) {
                *out_block = &function->blocks[block_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_bytes_function_find_block_covering_offset(const MachineBytesFunction *function,
    size_t byte_offset,
    size_t *out_emit_index,
    const MachineBytesBlock **out_block) {
    size_t block_index;

    if (!function || !function->blocks) {
        return 0;
    }
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineBytesBlock *block = &function->blocks[block_index];

        if (byte_offset >= block->start_byte_offset && byte_offset < block->end_byte_offset) {
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

int machine_bytes_function_copy_bytes(const MachineBytesFunction *function,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    unsigned char *bytes = NULL;
    size_t byte_count;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!function || !out_bytes) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-125: invalid function byte-copy contract");
        return 0;
    }

    byte_count = machine_bytes_function_total_byte_count(function);
    if (byte_count == 0) {
        return 1;
    }

    bytes = (unsigned char *)malloc(byte_count);
    if (!bytes) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-126: out of memory copying function bytes");
        return 0;
    }
    if (!machine_bytes_copy_function_bytes(function, bytes, byte_count)) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-127: failed to flatten function byte image");
        free(bytes);
        return 0;
    }

    *out_bytes = bytes;
    if (out_byte_count) {
        *out_byte_count = byte_count;
    }
    return 1;
}

int machine_bytes_block_get_summary(const MachineBytesBlock *block,
    MachineBytesBlockSummary *out_summary) {
    if (!block || !out_summary) {
        return 0;
    }
    out_summary->emit_index = block->emit_index;
    out_summary->original_layout_index = block->original_layout_index;
    out_summary->original_block_id = block->original_block_id;
    out_summary->label_name = block->label_name;
    out_summary->start_byte_offset = block->start_byte_offset;
    out_summary->terminator_byte_offset = block->terminator_byte_offset;
    out_summary->end_byte_offset = block->end_byte_offset;
    out_summary->byte_count = block->byte_count;
    return 1;
}

int machine_bytes_block_get_bytes(const MachineBytesBlock *block,
    const unsigned char **out_bytes,
    size_t *out_byte_count) {
    if (!block || !out_bytes) {
        return 0;
    }
    *out_bytes = block->bytes;
    if (out_byte_count) {
        *out_byte_count = block->byte_count;
    }
    return 1;
}

int machine_bytes_block_copy_bytes(const MachineBytesBlock *block,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error) {
    unsigned char *bytes = NULL;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!block || !out_bytes) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-128: invalid block byte-copy contract");
        return 0;
    }
    if (block->byte_count == 0) {
        return 1;
    }

    bytes = (unsigned char *)malloc(block->byte_count);
    if (!bytes) {
        machine_bytes_set_error(error, 0, 0, "MACHINE-BYTES-129: out of memory copying block bytes");
        return 0;
    }
    memcpy(bytes, block->bytes, block->byte_count);
    *out_bytes = bytes;
    if (out_byte_count) {
        *out_byte_count = block->byte_count;
    }
    return 1;
}

int machine_bytes_function_get_block_terminator_summary(const MachineBytesFunction *function,
    size_t emit_index,
    MachineBytesTerminatorSummary *out_summary) {
    const MachineBytesBlock *block;

    if (!function || !out_summary || !machine_bytes_function_get_block(function, emit_index, &block)) {
        return 0;
    }

    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->kind = block->block.terminator.kind;
    switch (block->block.terminator.kind) {
        case MACHINE_LAYOUT_TERM_FALLTHROUGH: {
            const MachineBytesBlock *target =
                machine_bytes_target_block(function, block->block.terminator.as.fallthrough_target);
            if (!target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = target->label_name;
            out_summary->primary_target_byte_offset = target->start_byte_offset;
            break;
        }
        case MACHINE_LAYOUT_TERM_JUMP: {
            const MachineBytesBlock *target =
                machine_bytes_target_block(function, block->block.terminator.as.jump_target);
            if (!target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = target->label_name;
            out_summary->primary_target_byte_offset = target->start_byte_offset;
            break;
        }
        case MACHINE_LAYOUT_TERM_BRANCH: {
            const MachineBytesBlock *then_target =
                machine_bytes_target_block(function, block->block.terminator.as.branch.then_target);
            const MachineBytesBlock *else_target =
                machine_bytes_target_block(function, block->block.terminator.as.branch.else_target);
            if (!then_target || !else_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = then_target->label_name;
            out_summary->primary_target_byte_offset = then_target->start_byte_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = else_target->label_name;
            out_summary->secondary_target_byte_offset = else_target->start_byte_offset;
            break;
        }
        case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH: {
            const MachineBytesBlock *taken_target =
                machine_bytes_target_block(function, block->block.terminator.as.branch_fallthrough.taken_target);
            const MachineBytesBlock *fallthrough_target =
                machine_bytes_target_block(function, block->block.terminator.as.branch_fallthrough.fallthrough_target);
            if (!taken_target || !fallthrough_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = taken_target->label_name;
            out_summary->primary_target_byte_offset = taken_target->start_byte_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = fallthrough_target->label_name;
            out_summary->secondary_target_byte_offset = fallthrough_target->start_byte_offset;
            out_summary->branch_on_true = block->block.terminator.as.branch_fallthrough.branch_on_true;
            break;
        }
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM: {
            const MachineBytesBlock *then_target =
                machine_bytes_target_block(function, block->block.terminator.as.compare_branch.then_target);
            const MachineBytesBlock *else_target =
                machine_bytes_target_block(function, block->block.terminator.as.compare_branch.else_target);
            if (!then_target || !else_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = then_target->label_name;
            out_summary->primary_target_byte_offset = then_target->start_byte_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = else_target->label_name;
            out_summary->secondary_target_byte_offset = else_target->start_byte_offset;
            out_summary->compare_op = block->block.terminator.as.compare_branch.op;
            break;
        }
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
        case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH: {
            const MachineBytesBlock *taken_target = machine_bytes_target_block(
                function, block->block.terminator.as.compare_branch_fallthrough.taken_target);
            const MachineBytesBlock *fallthrough_target = machine_bytes_target_block(
                function, block->block.terminator.as.compare_branch_fallthrough.fallthrough_target);
            if (!taken_target || !fallthrough_target) {
                return 0;
            }
            out_summary->has_primary_target = 1;
            out_summary->primary_target_label_name = taken_target->label_name;
            out_summary->primary_target_byte_offset = taken_target->start_byte_offset;
            out_summary->has_secondary_target = 1;
            out_summary->secondary_target_label_name = fallthrough_target->label_name;
            out_summary->secondary_target_byte_offset = fallthrough_target->start_byte_offset;
            out_summary->branch_on_true = block->block.terminator.as.compare_branch_fallthrough.branch_on_true;
            out_summary->compare_op = block->block.terminator.as.compare_branch_fallthrough.op;
            break;
        }
        default:
            break;
    }
    return 1;
}

int machine_bytes_function_compute_summary(const MachineBytesFunction *function,
    MachineBytesFunctionSummary *out_summary) {
    size_t block_index;

    if (!function || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->block_count = function->block_count;
    for (block_index = 0; block_index < function->block_count; ++block_index) {
        const MachineBytesBlock *block = &function->blocks[block_index];
        size_t op_index;
        size_t block_call_count = 0;

        out_summary->byte_count += block->byte_count;
        out_summary->op_byte_count += block->terminator_byte_offset - block->start_byte_offset;
        for (op_index = 0; op_index < block->block.op_count; ++op_index) {
            if (machine_bytes_op_has_call_payload(block->block.ops[op_index].kind)) {
                ++block_call_count;
            }
        }
        out_summary->terminator_byte_count += block->end_byte_offset - block->terminator_byte_offset;
        out_summary->call_count += block_call_count;
        if (block_call_count > 0) {
            ++out_summary->blocks_with_calls_count;
        }
        switch (block->block.terminator.kind) {
            case MACHINE_LAYOUT_TERM_RETURN:
            case MACHINE_LAYOUT_TERM_RETURN_IMM:
            case MACHINE_LAYOUT_TERM_RETURN_SPILL:
                ++out_summary->return_count;
                break;
            case MACHINE_LAYOUT_TERM_JUMP:
                ++out_summary->jump_count;
                break;
            case MACHINE_LAYOUT_TERM_FALLTHROUGH:
                ++out_summary->jump_count;
                ++out_summary->fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH:
                ++out_summary->branch_count;
                break;
            case MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH:
                ++out_summary->branch_count;
                ++out_summary->fallthrough_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM:
                ++out_summary->compare_branch_count;
                break;
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH:
            case MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH:
                ++out_summary->compare_branch_count;
                ++out_summary->fallthrough_count;
                break;
            default:
                break;
        }
    }
    return 1;
}
