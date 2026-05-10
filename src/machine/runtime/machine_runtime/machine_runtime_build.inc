#if defined(__INTELLISENSE__) && !defined(MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_RUNTIME_SPLIT_AGGREGATOR)
#define MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT
#include "machine_runtime.c"

#else

int machine_runtime_build_from_machine_load_file(const MachineLoadFile *load_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadMemorySummary load_memory_summary;
    MachineRuntimeTargetPolicySummary target_policy_summary;
    size_t segment_index;
    size_t stack_base_virtual_address;
    size_t stack_base_before_alignment;

    memset(&load_memory_summary, 0, sizeof(load_memory_summary));
    memset(&target_policy_summary, 0, sizeof(target_policy_summary));
    if (!load_file || !out_runtime_file) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-122: invalid runtime build contract");
        return 0;
    }
    if (!machine_load_verify_file(load_file, NULL)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-123: invalid load input");
        return 0;
    }
    if (!machine_load_file_get_memory_summary(load_file, &load_memory_summary) ||
        !machine_runtime_get_target_policy_summary(load_file->exec_file.image_file.target_profile, &target_policy_summary)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-124: failed deriving runtime policy");
        return 0;
    }

    machine_runtime_file_free(out_runtime_file);
    if (!machine_load_clone_file(load_file, &out_runtime_file->load_file, NULL)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-125: failed cloning load input");
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }

    out_runtime_file->entry_virtual_address = load_file->entry_virtual_address;
    out_runtime_file->segment_count = load_file->segment_count + 1u;
    out_runtime_file->segment_capacity = out_runtime_file->segment_count;
    out_runtime_file->segments = (MachineRuntimeSegment *)calloc(
        out_runtime_file->segment_count, sizeof(MachineRuntimeSegment));
    if (!out_runtime_file->segments) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-126: out of memory building runtime segments");
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }

    for (segment_index = 0u; segment_index < load_file->segment_count; ++segment_index) {
        const MachineLoadSegment *load_segment = &load_file->segments[segment_index];
        MachineRuntimeSegment *runtime_segment = &out_runtime_file->segments[segment_index];

        runtime_segment->name = machine_runtime_strdup(load_segment->name ? load_segment->name : "");
        runtime_segment->kind = MACHINE_RUNTIME_SEGMENT_KIND_LOAD;
        runtime_segment->load_segment_index = segment_index;
        runtime_segment->virtual_address = load_segment->virtual_address;
        runtime_segment->byte_count = load_segment->memory_byte_count;
        runtime_segment->readable = load_segment->readable;
        runtime_segment->writable = load_segment->writable;
        runtime_segment->executable = load_segment->executable;
        runtime_segment->bytes = load_segment->memory_byte_count > 0u
            ? (unsigned char *)malloc(load_segment->memory_byte_count)
            : NULL;
        if (!runtime_segment->name || (load_segment->memory_byte_count > 0u && !runtime_segment->bytes)) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-127: out of memory materializing runtime load segment");
            machine_runtime_file_free(out_runtime_file);
            return 0;
        }
        if (load_segment->memory_byte_count > 0u) {
            memcpy(runtime_segment->bytes, load_segment->bytes, load_segment->memory_byte_count);
        }
        if (out_runtime_file->total_mapped_byte_count > ((size_t)-1) - runtime_segment->byte_count) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-127: out of memory materializing runtime load segment");
            machine_runtime_file_free(out_runtime_file);
            return 0;
        }
        out_runtime_file->total_mapped_byte_count += runtime_segment->byte_count;
    }

    if (!machine_runtime_try_add_size(
            load_memory_summary.end_virtual_address,
            target_policy_summary.stack_gap_byte_count,
            &stack_base_virtual_address)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-124: failed deriving runtime policy");
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }
    stack_base_before_alignment = stack_base_virtual_address;
    stack_base_virtual_address = machine_runtime_align_up(
        stack_base_virtual_address,
        target_policy_summary.stack_alignment);
    if (stack_base_virtual_address < stack_base_before_alignment) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-124: failed deriving runtime policy");
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }
    out_runtime_file->stack_segment_index = load_file->segment_count;
    out_runtime_file->has_stack_segment = 1;
    if (!machine_runtime_try_add_size(
            stack_base_virtual_address,
            target_policy_summary.stack_byte_count,
            &out_runtime_file->initial_stack_pointer)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-128: out of memory materializing runtime stack segment");
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }
    {
        MachineRuntimeSegment *stack_segment = &out_runtime_file->segments[out_runtime_file->stack_segment_index];

        stack_segment->name = machine_runtime_strdup(".stack");
        stack_segment->kind = MACHINE_RUNTIME_SEGMENT_KIND_STACK;
        stack_segment->load_segment_index = (size_t)-1;
        stack_segment->virtual_address = stack_base_virtual_address;
        stack_segment->byte_count = target_policy_summary.stack_byte_count;
        stack_segment->readable = 1u;
        stack_segment->writable = 1u;
        stack_segment->executable = 0u;
        stack_segment->bytes = target_policy_summary.stack_byte_count > 0u
            ? (unsigned char *)calloc(target_policy_summary.stack_byte_count, 1u)
            : NULL;
        if (!stack_segment->name || (target_policy_summary.stack_byte_count > 0u && !stack_segment->bytes)) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-128: out of memory materializing runtime stack segment");
            machine_runtime_file_free(out_runtime_file);
            return 0;
        }
    }
    if (out_runtime_file->total_mapped_byte_count > ((size_t)-1) - target_policy_summary.stack_byte_count) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-128: out of memory materializing runtime stack segment");
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }
    out_runtime_file->total_mapped_byte_count += target_policy_summary.stack_byte_count;

    if (!machine_runtime_verify_file(out_runtime_file, error)) {
        machine_runtime_file_free(out_runtime_file);
        return 0;
    }
    return 1;
}

int machine_runtime_build_from_machine_load_report(const MachineLoadReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    const MachineLoadFile *load_file = NULL;

    if (!report) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-129: invalid runtime build-from-load-report contract");
        return 0;
    }
    if (!machine_load_report_get_file(report, &load_file) || !load_file) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-130: failed recovering load file from report");
        return 0;
    }
    return machine_runtime_build_from_machine_load_file(load_file, out_runtime_file, error);
}

int machine_runtime_build_from_machine_exec_file(const MachineExecFile *exec_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_exec_file(exec_file, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-131: failed building runtime from exec file");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_exec_report(const MachineExecReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_exec_report(report, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-132: failed building runtime from exec report");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_image_file(const MachineImageFile *image_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_image_file(image_file, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-133: failed building runtime from image file");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_image_report(const MachineImageReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_image_report(report, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-134: failed building runtime from image report");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_file(elf_file, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-135: failed building runtime from elf file");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_file_with_profile(elf_file, profile, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-136: failed building profiled runtime from elf file");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_elf_report(const MachineElfReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_report(report, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-137: failed building runtime from elf report");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_report_with_profile(report, profile, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-138: failed building profiled runtime from elf report");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_bytes(bytes, byte_count, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-139: failed building runtime from elf bytes");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-140: failed building profiled runtime from elf bytes");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_ir_report(report, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-141: failed building runtime from machine-ir report");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_ir_report_with_profile(report, profile, &load_file, NULL) &&
        machine_runtime_build_from_machine_load_file(&load_file, out_runtime_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-142: failed building profiled runtime from machine-ir report");
    }
    machine_load_file_free(&load_file);
    return ok;
}

int machine_runtime_build_report_from_file(const MachineRuntimeFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    size_t segment_index;
    size_t executable_count = 0u;
    size_t non_executable_count = 0u;

    if (!source || !out_report) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-143: invalid runtime report build contract");
        return 0;
    }
    if (!machine_runtime_verify_file(source, error)) {
        return 0;
    }

    machine_runtime_report_free(out_report);
    if (!machine_runtime_clone_file(source, &out_report->file, error)) {
        machine_runtime_report_free(out_report);
        return 0;
    }

    out_report->segment_summary_count = source->segment_count;
    out_report->segment_summaries = source->segment_count > 0u
        ? (MachineRuntimeSegmentSummary *)calloc(source->segment_count, sizeof(MachineRuntimeSegmentSummary))
        : NULL;
    out_report->executable_segment_indices = source->segment_count > 0u
        ? (size_t *)calloc(source->segment_count, sizeof(size_t))
        : NULL;
    out_report->non_executable_segment_indices = source->segment_count > 0u
        ? (size_t *)calloc(source->segment_count, sizeof(size_t))
        : NULL;
    if ((source->segment_count > 0u && !out_report->segment_summaries) ||
        (source->segment_count > 0u && !out_report->executable_segment_indices) ||
        (source->segment_count > 0u && !out_report->non_executable_segment_indices)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-144: out of memory building runtime report");
        machine_runtime_report_free(out_report);
        return 0;
    }

    out_report->stack_segment_index = source->stack_segment_index;
    out_report->has_stack_segment = source->has_stack_segment;
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        const MachineRuntimeSegment *segment = &out_report->file.segments[segment_index];

        if (!machine_runtime_segment_get_summary(segment, &out_report->segment_summaries[segment_index])) {
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-145: failed summarizing runtime segment");
            machine_runtime_report_free(out_report);
            return 0;
        }
        if (segment->executable) {
            out_report->executable_segment_indices[executable_count++] = segment_index;
        } else {
            out_report->non_executable_segment_indices[non_executable_count++] = segment_index;
        }
    }
    out_report->executable_segment_count = executable_count;
    out_report->non_executable_segment_count = non_executable_count;
    if (!machine_runtime_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_runtime_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary) ||
        !machine_runtime_file_get_memory_summary(&out_report->file, &out_report->memory_summary) ||
        !machine_runtime_file_get_stack_summary(&out_report->file, &out_report->stack_summary) ||
        !machine_runtime_file_get_gap_summary(&out_report->file, &out_report->gap_summary) ||
        !machine_runtime_file_get_launch_summary(&out_report->file, &out_report->launch_summary) ||
        !machine_runtime_file_get_initial_stack_summary(&out_report->file, &out_report->initial_stack_summary)) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-146: failed summarizing runtime report");
        machine_runtime_report_free(out_report);
        return 0;
    }
    return 1;
}

#define MACHINE_RUNTIME_DEFINE_REPORT_WRAPPER(NAME, BUILD_CALL, ERRNUM, ERRMSG) \
int NAME(const MachineLoadFile *source, MachineRuntimeReport *out_report, MachineRuntimeError *error); /* suppress */ \

int machine_runtime_build_report_from_machine_load_file(const MachineLoadFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_load_file(source, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_load_report(const MachineLoadReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_load_report(report, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_exec_file(const MachineExecFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_exec_file(source, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_exec_report(const MachineExecReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_exec_report(report, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_image_file(const MachineImageFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_image_file(source, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_image_report(const MachineImageReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_image_report(report, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_file(source, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_file_with_profile(source, profile, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_report(report, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_report_with_profile(report, profile, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_bytes(bytes, byte_count, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_ir_report(report, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error) {
    MachineRuntimeFile runtime_file;
    int ok;

    machine_runtime_file_init(&runtime_file);
    ok = machine_runtime_build_from_machine_ir_report_with_profile(report, profile, &runtime_file, error) &&
        machine_runtime_build_report_from_file(&runtime_file, out_report, error);
    machine_runtime_file_free(&runtime_file);
    return ok;
}

int machine_runtime_report_refresh(MachineRuntimeReport *report,
    MachineRuntimeError *error) {
    MachineRuntimeReport refreshed_report;
    int ok;

    if (!report) {
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-147: invalid runtime report refresh contract");
        return 0;
    }
    machine_runtime_report_init(&refreshed_report);
    ok = machine_runtime_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_runtime_report_free(report);
        *report = refreshed_report;
    } else {
        machine_runtime_report_free(&refreshed_report);
    }
    return ok;
}


#endif
