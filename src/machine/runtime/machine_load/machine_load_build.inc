#if defined(__INTELLISENSE__) && !defined(MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LOAD_SPLIT_AGGREGATOR)
#define MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT
#include "machine_load.c"

#else

int machine_load_build_from_machine_exec_file(const MachineExecFile *exec_file,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    size_t segment_alignment;
    size_t segment_index;

    if (!exec_file || !out_load_file) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-104: invalid load build contract");
        return 0;
    }
    if (!machine_exec_verify_file(exec_file, NULL)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-105: invalid exec input");
        return 0;
    }
    segment_alignment = machine_load_segment_alignment(exec_file->image_file.target_profile);

    machine_load_file_free(out_load_file);
    if (!machine_exec_clone_file(exec_file, &out_load_file->exec_file, NULL)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-106: failed cloning exec input");
        machine_load_file_free(out_load_file);
        return 0;
    }
    out_load_file->entry_virtual_address = exec_file->entry_virtual_address;
    out_load_file->segment_count = exec_file->segment_count;
    out_load_file->segment_capacity = exec_file->segment_count;
    out_load_file->segments = exec_file->segment_count > 0u
        ? (MachineLoadSegment *)calloc(exec_file->segment_count, sizeof(MachineLoadSegment))
        : NULL;
    if (exec_file->segment_count > 0u && !out_load_file->segments) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-107: out of memory building load segments");
        machine_load_file_free(out_load_file);
        return 0;
    }

    for (segment_index = 0u; segment_index < exec_file->segment_count; ++segment_index) {
        const MachineExecSegment *exec_segment = &exec_file->segments[segment_index];
        const MachineImageSegment *image_segment =
            &exec_file->image_file.segments[exec_segment->image_segment_index];
        MachineLoadSegment *load_segment = &out_load_file->segments[segment_index];
        size_t aligned_memory_byte_count = machine_load_align_up(exec_segment->byte_count, segment_alignment);

        load_segment->name = machine_load_strdup(exec_segment->name ? exec_segment->name : "");
        load_segment->exec_segment_index = segment_index;
        load_segment->virtual_address = exec_segment->virtual_address;
        load_segment->file_byte_count = exec_segment->byte_count;
        load_segment->memory_byte_count = aligned_memory_byte_count;
        load_segment->readable = exec_segment->readable;
        load_segment->writable = exec_segment->writable;
        load_segment->executable = exec_segment->executable;
        load_segment->bytes = load_segment->memory_byte_count > 0u
            ? (unsigned char *)calloc(load_segment->memory_byte_count, 1u)
            : NULL;
        if (!load_segment->name ||
            aligned_memory_byte_count < exec_segment->byte_count ||
            (load_segment->memory_byte_count > 0u && !load_segment->bytes)) {
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-108: out of memory materializing load segment");
            machine_load_file_free(out_load_file);
            return 0;
        }
        if (exec_segment->byte_count > 0u) {
            memcpy(load_segment->bytes,
                exec_file->image_file.bytes + image_segment->image_offset,
                exec_segment->byte_count);
        }
        if (out_load_file->total_memory_byte_count > ((size_t)-1) - load_segment->memory_byte_count) {
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-108: out of memory materializing load segment");
            machine_load_file_free(out_load_file);
            return 0;
        }
        out_load_file->total_memory_byte_count += load_segment->memory_byte_count;
    }

    if (!machine_load_verify_file(out_load_file, error)) {
        machine_load_file_free(out_load_file);
        return 0;
    }
    return 1;
}

int machine_load_build_from_machine_exec_report(const MachineExecReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    const MachineExecFile *exec_file = NULL;

    if (!report) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-109: invalid load build-from-exec-report contract");
        return 0;
    }
    if (!machine_exec_report_get_file(report, &exec_file) || !exec_file) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-110: failed recovering exec file from exec report");
        return 0;
    }
    return machine_load_build_from_machine_exec_file(exec_file, out_load_file, error);
}

int machine_load_build_from_machine_image_file(const MachineImageFile *image_file,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_image_file(image_file, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-111: failed building load from image file");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_image_report(const MachineImageReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_image_report(report, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-112: failed building load from image report");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_file(elf_file, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-113: failed building load from elf file");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_file_with_profile(elf_file, profile, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-114: failed building profiled load from elf file");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_elf_report(const MachineElfReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_report(report, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-115: failed building load from elf report");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_report_with_profile(report, profile, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-116: failed building profiled load from elf report");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_bytes(bytes, byte_count, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-117: failed building load from elf bytes");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-118: failed building profiled load from elf bytes");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_ir_report(report, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-119: failed building load from machine-ir report");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_ir_report_with_profile(report, profile, &exec_file, NULL) &&
        machine_load_build_from_machine_exec_file(&exec_file, out_load_file, error);
    if (!ok && error && error->message[0] == '\0') {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-120: failed building profiled load from machine-ir report");
    }
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_load_build_report_from_file(const MachineLoadFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    size_t segment_index;
    size_t executable_count = 0u;
    size_t non_executable_count = 0u;

    if (!source || !out_report) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-121: invalid load report build contract");
        return 0;
    }
    if (!machine_load_verify_file(source, error)) {
        return 0;
    }

    machine_load_report_free(out_report);
    if (!machine_load_clone_file(source, &out_report->file, error)) {
        machine_load_report_free(out_report);
        return 0;
    }
    out_report->segment_summary_count = source->segment_count;
    out_report->segment_summaries = source->segment_count > 0u
        ? (MachineLoadSegmentSummary *)calloc(source->segment_count, sizeof(MachineLoadSegmentSummary))
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
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-122: out of memory building load report");
        machine_load_report_free(out_report);
        return 0;
    }
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        const MachineLoadSegment *segment = &out_report->file.segments[segment_index];

        if (!machine_load_segment_get_summary(segment, &out_report->segment_summaries[segment_index])) {
            machine_load_set_error(error, 0, 0, "MACHINE-LOAD-123: failed summarizing load segment");
            machine_load_report_free(out_report);
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
    if (!machine_load_file_get_header_summary(&out_report->file, &out_report->header_summary)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-124: failed summarizing load header");
        machine_load_report_free(out_report);
        return 0;
    }
    if (!machine_load_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-124: failed summarizing load target policy");
        machine_load_report_free(out_report);
        return 0;
    }
    if (!machine_load_file_get_memory_summary(&out_report->file, &out_report->memory_summary)) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-124: failed summarizing load memory span");
        machine_load_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_load_build_report_from_machine_exec_file(const MachineExecFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_exec_file(source, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_exec_report(const MachineExecReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_exec_report(report, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_image_file(const MachineImageFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_image_file(source, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_image_report(const MachineImageReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_image_report(report, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_file(source, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_file_with_profile(source, profile, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_report(report, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_report_with_profile(report, profile, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_bytes(bytes, byte_count, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_ir_report(report, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error) {
    MachineLoadFile load_file;
    int ok;

    machine_load_file_init(&load_file);
    ok = machine_load_build_from_machine_ir_report_with_profile(report, profile, &load_file, error) &&
        machine_load_build_report_from_file(&load_file, out_report, error);
    machine_load_file_free(&load_file);
    return ok;
}

int machine_load_report_refresh(MachineLoadReport *report,
    MachineLoadError *error) {
    MachineLoadReport refreshed_report;
    int ok;

    if (!report) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-125: invalid load report refresh contract");
        return 0;
    }
    machine_load_report_init(&refreshed_report);
    ok = machine_load_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_load_report_free(report);
        *report = refreshed_report;
    } else {
        machine_load_report_free(&refreshed_report);
    }
    return ok;
}


#endif
