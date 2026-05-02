#if defined(__INTELLISENSE__) && !defined(MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EXEC_SPLIT_AGGREGATOR)
#define MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT
#include "machine_exec.c"

#else

int machine_exec_verify_file(const MachineExecFile *exec_file,
    MachineExecError *error) {
    size_t segment_index;
    MachineImageError image_error;
    size_t unresolved_count = 0u;

    memset(&image_error, 0, sizeof(image_error));
    if (!exec_file) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-116: invalid exec verify contract");
        return 0;
    }
    if (!machine_image_verify_file(&exec_file->image_file, &image_error)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-117: invalid embedded image: %s", image_error.message);
        return 0;
    }
    if (!exec_file->image_file.has_entry || exec_file->entry_virtual_address != exec_file->image_file.entry_virtual_address) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-118: exec entry is missing or mismatched");
        return 0;
    }
    if (exec_file->segment_count == 0u || exec_file->segment_count != exec_file->image_file.segment_count || !exec_file->segments) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-119: exec segments are missing");
        return 0;
    }
    if (!machine_image_file_get_unresolved_relocation_count(&exec_file->image_file, &unresolved_count) || unresolved_count != 0u) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-120: exec image still has unresolved relocations");
        return 0;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        const MachineExecSegment *segment = &exec_file->segments[segment_index];
        const MachineImageSegment *image_segment = &exec_file->image_file.segments[segment_index];

        if (!segment->name || segment->image_segment_index != segment_index ||
            segment->virtual_address != image_segment->virtual_address ||
            segment->byte_count != image_segment->byte_count ||
            !segment->readable) {
            machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-121: invalid exec segment");
            return 0;
        }
    }
    if (!machine_exec_file_get_entry_segment(exec_file, NULL, NULL)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-129: exec entry is not covered by an executable segment");
        return 0;
    }
    return 1;
}

int machine_exec_dump_file(const MachineExecFile *exec_file,
    char **out_text,
    MachineExecError *error) {
    MachineExecStringBuilder builder;
    size_t segment_index;
    size_t entry_segment_index = 0u;
    const MachineExecSegment *entry_segment = NULL;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!exec_file || !out_text) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-122: invalid exec dump contract");
        return 0;
    }
    if (!machine_exec_verify_file(exec_file, error)) {
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_exec_file_get_entry_segment(exec_file, &entry_segment_index, &entry_segment) ||
        !entry_segment) {
        goto cleanup;
    }
    if (!machine_exec_append_format(
            &builder,
            "machine_exec profile=%s base=0x%zx entry=0x%zx segments=%zu bytes=%zu\n",
            machine_elf_target_profile_name(exec_file->image_file.target_profile),
            exec_file->image_file.base_virtual_address,
            exec_file->entry_virtual_address,
            exec_file->segment_count,
            exec_file->image_file.byte_count)) {
        goto cleanup;
    }
    if (!machine_exec_append_format(
            &builder,
            "entry_segment: xseg.%zu %s perms=%c%c%c\n",
            entry_segment_index,
            entry_segment->name ? entry_segment->name : "<null>",
            entry_segment->readable ? 'r' : '-',
            entry_segment->writable ? 'w' : '-',
            entry_segment->executable ? 'x' : '-')) {
        goto cleanup;
    }
    if (!machine_exec_append_format(&builder, "segments:\n")) {
        goto cleanup;
    }
    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        const MachineExecSegment *segment = &exec_file->segments[segment_index];

        if (!machine_exec_append_format(
                &builder,
                "  xseg.%zu %s img-seg=%zu vaddr=0x%zx bytes=%zu perms=%c%c%c\n",
                segment_index,
                segment->name ? segment->name : "<null>",
                segment->image_segment_index,
                segment->virtual_address,
                segment->byte_count,
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

int machine_exec_build_dump_from_file(const MachineExecFile *source,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_clone_file(source, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_image_file(source, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_image_report(report, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_file(source, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_file_with_profile(source, profile, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_report(report, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_report_with_profile(report, profile, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_bytes(bytes, byte_count, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_ir_report(report, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_ir_report_with_profile(report, profile, &exec_file, error) &&
        machine_exec_dump_file(&exec_file, out_text, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

#endif
