#if defined(__INTELLISENSE__) && !defined(MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LOAD_SPLIT_AGGREGATOR)
#define MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT
#include "machine_load.c"

#else

int machine_load_verify_file(const MachineLoadFile *load_file,
    MachineLoadError *error) {
    size_t segment_index;
    size_t total_memory_byte_count = 0u;

    if (!load_file) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-126: invalid load verify contract");
        return 0;
    }
    if (!machine_exec_verify_file(&load_file->exec_file, NULL)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-127: invalid embedded exec file");
        return 0;
    }
    if (load_file->segment_count == 0u ||
        load_file->segment_count != load_file->exec_file.segment_count ||
        !load_file->segments) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-128: load segments are missing");
        return 0;
    }
    if (load_file->entry_virtual_address != load_file->exec_file.entry_virtual_address) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-129: load entry mismatches exec entry");
        return 0;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        const MachineLoadSegment *segment = &load_file->segments[segment_index];
        const MachineExecSegment *exec_segment = &load_file->exec_file.segments[segment_index];
        const MachineImageSegment *image_segment =
            &load_file->exec_file.image_file.segments[exec_segment->image_segment_index];
        size_t zero_fill_index;
        size_t segment_end = 0u;

        if (!segment->name ||
            segment->exec_segment_index != segment_index ||
            segment->virtual_address != exec_segment->virtual_address ||
            segment->file_byte_count != exec_segment->byte_count ||
            segment->memory_byte_count < segment->file_byte_count ||
            segment->readable != exec_segment->readable ||
            segment->writable != exec_segment->writable ||
            segment->executable != exec_segment->executable ||
            !machine_load_try_add_size(segment->virtual_address, segment->memory_byte_count, &segment_end) ||
            (segment->memory_byte_count > 0u && !segment->bytes) ||
            (segment->file_byte_count > 0u &&
                memcmp(segment->bytes,
                    load_file->exec_file.image_file.bytes + image_segment->image_offset,
                    segment->file_byte_count) != 0)) {
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-130: invalid load segment");
            return 0;
        }
        for (zero_fill_index = segment->file_byte_count;
             zero_fill_index < segment->memory_byte_count;
             ++zero_fill_index) {
            if (segment->bytes[zero_fill_index] != 0u) {
                machine_load_set_error(error, 0, 0, "MACHINE-LOAD-130: invalid zero-fill tail");
                return 0;
            }
        }
        if (total_memory_byte_count > ((size_t)-1) - segment->memory_byte_count) {
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-131: invalid load memory-byte accounting");
            return 0;
        }
        total_memory_byte_count += segment->memory_byte_count;
    }
    if (total_memory_byte_count != load_file->total_memory_byte_count) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-131: invalid load memory-byte accounting");
        return 0;
    }
    if (!machine_load_file_get_entry_segment(load_file, NULL, NULL)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-132: load entry is not covered by an executable load segment");
        return 0;
    }
    return 1;
}

int machine_load_dump_file(const MachineLoadFile *load_file,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadStringBuilder builder;
    size_t segment_index;
    size_t entry_segment_index = 0u;
    const MachineLoadSegment *entry_segment = NULL;
    size_t segment_alignment;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!load_file || !out_text) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-133: invalid load dump contract");
        return 0;
    }
    if (!machine_load_verify_file(load_file, error)) {
        return 0;
    }
    segment_alignment = machine_load_segment_alignment(load_file->exec_file.image_file.target_profile);

    memset(&builder, 0, sizeof(builder));
    if (!machine_load_file_get_entry_segment(load_file, &entry_segment_index, &entry_segment) ||
        !entry_segment) {
        goto cleanup;
    }
    if (!machine_load_append_format(
            &builder,
            "machine_load profile=%s elf_origin=%s elf_semantics=%s base=0x%zx entry=0x%zx segments=%zu file_bytes=%zu memory_bytes=%zu\n",
            machine_elf_target_profile_name(load_file->exec_file.image_file.target_profile),
            machine_elf_target_profile_name(
                load_file->exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                load_file->exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            load_file->exec_file.image_file.base_virtual_address,
            load_file->entry_virtual_address,
            load_file->segment_count,
            load_file->exec_file.image_file.byte_count,
            load_file->total_memory_byte_count) ||
        !machine_load_append_format(
            &builder,
            "entry_segment: lseg.%zu %s perms=%c%c%c\n",
            entry_segment_index,
            entry_segment->name ? entry_segment->name : "<null>",
            entry_segment->readable ? 'r' : '-',
            entry_segment->writable ? 'w' : '-',
            entry_segment->executable ? 'x' : '-') ||
        !machine_load_append_format(&builder, "segments:\n")) {
        goto cleanup;
    }
    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        const MachineLoadSegment *segment = &load_file->segments[segment_index];
        size_t zero_fill_byte_count = 0u;

        if (segment->memory_byte_count < segment->file_byte_count) {
            free(builder.data);
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-139: invalid load dump zero-fill accounting");
            return 0;
        }
        zero_fill_byte_count = segment->memory_byte_count - segment->file_byte_count;

        if (!machine_load_append_format(
                &builder,
                "  lseg.%zu %s xseg=%zu vaddr=0x%zx file-bytes=%zu mem-bytes=%zu zero-fill=%zu align=%zu perms=%c%c%c\n",
                segment_index,
                segment->name ? segment->name : "<null>",
                segment->exec_segment_index,
                segment->virtual_address,
                segment->file_byte_count,
                segment->memory_byte_count,
                zero_fill_byte_count,
                segment_alignment,
                segment->readable ? 'r' : '-',
                segment->writable ? 'w' : '-',
                segment->executable ? 'x' : '-')) {
            goto cleanup;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    return ok;
}

int machine_load_build_dump_from_file(const MachineLoadFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_clone_file(source, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_exec_file(source, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_exec_report(report, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_image_file(source, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_image_report(report, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_file(source, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_file_with_profile(source, profile, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_report(report, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_report_with_profile(report, profile, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_bytes(bytes, byte_count, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_ir_report(report, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_ir_report_with_profile(report, profile, &load_file, error) &&
        machine_load_dump_file(&load_file, out_text, error);
    machine_load_file_free(&load_file);
    return ok;
}


#endif
