#if defined(__INTELLISENSE__) && !defined(MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_RUNTIME_SPLIT_AGGREGATOR)
#define MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT
#include "machine_runtime.c"

#else

int machine_runtime_dump_file(const MachineRuntimeFile *runtime_file,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeStringBuilder builder;
    size_t segment_index;
    size_t entry_segment_index = 0u;
    size_t stack_segment_index = 0u;
    const MachineRuntimeSegment *entry_segment = NULL;
    const MachineRuntimeSegment *stack_segment = NULL;
    MachineRuntimeHeaderSummary header_summary;

    memset(&builder, 0, sizeof(builder));
    memset(&header_summary, 0, sizeof(header_summary));
    if (!runtime_file || !out_text || !machine_runtime_verify_file(runtime_file, error) ||
        !machine_runtime_file_get_header_summary(runtime_file, &header_summary) ||
        !machine_runtime_file_get_entry_segment(runtime_file, &entry_segment_index, &entry_segment) ||
        !machine_runtime_file_get_stack_segment(runtime_file, &stack_segment_index, &stack_segment)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-148: invalid runtime dump contract");
        return 0;
    }

    if (!machine_runtime_append_format(
            &builder,
            "machine_runtime profile=%s elf_origin=%s elf_semantics=%s base=0x%zx entry=0x%zx sp=0x%zx segments=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(header_summary.target_profile),
            machine_elf_target_profile_name(
                runtime_file->load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                runtime_file->load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            header_summary.base_virtual_address,
            header_summary.entry_virtual_address,
            header_summary.initial_stack_pointer,
            header_summary.segment_count,
            header_summary.mapped_byte_count) ||
        !machine_runtime_append_format(
            &builder,
            "entry_segment: rseg.%zu %s perms=%c%c%c\n",
            entry_segment_index,
            entry_segment->name ? entry_segment->name : "",
            entry_segment->readable ? 'r' : '-',
            entry_segment->writable ? 'w' : '-',
            entry_segment->executable ? 'x' : '-') ||
        !machine_runtime_append_format(
            &builder,
            "stack_segment: rseg.%zu %s perms=%c%c%c\n",
            stack_segment_index,
            stack_segment->name ? stack_segment->name : "",
            stack_segment->readable ? 'r' : '-',
            stack_segment->writable ? 'w' : '-',
            stack_segment->executable ? 'x' : '-') ||
        !machine_runtime_append_format(&builder, "segments:\n")) {
        free(builder.data);
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-149: out of memory building runtime dump");
        return 0;
    }

    for (segment_index = 0u; segment_index < runtime_file->segment_count; ++segment_index) {
        const MachineRuntimeSegment *segment = &runtime_file->segments[segment_index];

        if (segment->kind == MACHINE_RUNTIME_SEGMENT_KIND_LOAD) {
            if (!machine_runtime_append_format(
                    &builder,
                    "  rseg.%zu %s kind=load lseg=%zu vaddr=0x%zx bytes=%zu perms=%c%c%c\n",
                    segment_index,
                    segment->name ? segment->name : "",
                    segment->load_segment_index,
                    segment->virtual_address,
                    segment->byte_count,
                    segment->readable ? 'r' : '-',
                    segment->writable ? 'w' : '-',
                    segment->executable ? 'x' : '-')) {
                free(builder.data);
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-150: out of memory appending runtime segment");
                return 0;
            }
        } else if (!machine_runtime_append_format(
                       &builder,
                       "  rseg.%zu %s kind=stack vaddr=0x%zx bytes=%zu perms=%c%c%c\n",
                       segment_index,
                       segment->name ? segment->name : "",
                       segment->virtual_address,
                       segment->byte_count,
                       segment->readable ? 'r' : '-',
                       segment->writable ? 'w' : '-',
                       segment->executable ? 'x' : '-')) {
            free(builder.data);
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-151: out of memory appending runtime segment");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_runtime_build_dump_from_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    return machine_runtime_dump_file(source, out_text, error);
}

int machine_runtime_build_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_load_file(source, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_load_report(report, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_exec_file(source, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_exec_report(report, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_image_file(source, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_image_report(report, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_file(source, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_file_with_profile(source, profile, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_report(report, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_report_with_profile(report, profile, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_bytes(bytes, byte_count, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_ir_report(report, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_ir_report_with_profile(report, profile, &runtime_file, error) &&
        machine_runtime_dump_file(&runtime_file, out_text, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}


#endif
