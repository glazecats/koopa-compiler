#if defined(__INTELLISENSE__) && !defined(MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EXEC_SPLIT_AGGREGATOR)
#define MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT
#include "machine_exec.c"

#else

int machine_exec_build_from_machine_image_file(const MachineImageFile *image_file,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    size_t unresolved_count = 0u;
    size_t segment_index;
    MachineImageError image_error;

    memset(&image_error, 0, sizeof(image_error));
    if (!image_file || !out_exec_file) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-100: invalid exec build contract");
        return 0;
    }
    if (!machine_image_verify_file(image_file, &image_error)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-101: invalid image input: %s", image_error.message);
        return 0;
    }
    if (!image_file->has_entry) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-102: executable image requires entry symbol");
        return 0;
    }
    if (!machine_image_file_get_unresolved_relocation_count(image_file, &unresolved_count)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-103: failed reading unresolved relocation count");
        return 0;
    }
    if (unresolved_count != 0u) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-104: executable image still has unresolved relocations");
        return 0;
    }

    machine_exec_file_free(out_exec_file);
    if (!machine_image_clone_file(image_file, &out_exec_file->image_file, &image_error)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-105: failed cloning image file: %s", image_error.message);
        machine_exec_file_free(out_exec_file);
        return 0;
    }
    out_exec_file->entry_virtual_address = image_file->entry_virtual_address;
    out_exec_file->segment_count = image_file->segment_count;
    out_exec_file->segment_capacity = image_file->segment_count;
    out_exec_file->segments = image_file->segment_count > 0u
        ? (MachineExecSegment *)calloc(image_file->segment_count, sizeof(MachineExecSegment))
        : NULL;
    if (image_file->segment_count > 0u && !out_exec_file->segments) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-106: out of memory building exec segments");
        machine_exec_file_free(out_exec_file);
        return 0;
    }

    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        const MachineImageSegment *image_segment = &image_file->segments[segment_index];
        MachineExecSegment *exec_segment = &out_exec_file->segments[segment_index];

        exec_segment->name = machine_exec_strdup(image_segment->name ? image_segment->name : "");
        exec_segment->image_segment_index = segment_index;
        exec_segment->virtual_address = image_segment->virtual_address;
        exec_segment->byte_count = image_segment->byte_count;
        exec_segment->readable = 1u;
        exec_segment->writable = (unsigned char)machine_exec_is_writable_segment_name(image_segment->name);
        exec_segment->executable = (unsigned char)machine_exec_is_executable_segment_name(image_segment->name);
        if (!exec_segment->name) {
            machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-107: out of memory naming exec segment");
            machine_exec_file_free(out_exec_file);
            return 0;
        }
    }

    if (!machine_exec_verify_file(out_exec_file, error)) {
        machine_exec_file_free(out_exec_file);
        return 0;
    }
    return 1;
}

int machine_exec_build_from_machine_image_report(const MachineImageReport *report,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    const MachineImageFile *image_file = NULL;

    if (!report) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-108: invalid exec build-from-image-report contract");
        return 0;
    }
    if (!machine_image_report_get_file(report, &image_file) || !image_file) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-109: failed recovering image file from image report");
        return 0;
    }
    return machine_exec_build_from_machine_image_file(image_file, out_exec_file, error);
}

int machine_exec_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_file(elf_file, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-130: failed building exec image from elf file: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_file_with_profile(elf_file, profile, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-131: failed building profiled exec image from elf file: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_elf_report(const MachineElfReport *report,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_report(report, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-132: failed building exec image from elf report: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_report_with_profile(report, profile, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-133: failed building profiled exec image from elf report: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_bytes(bytes, byte_count, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-134: failed building exec image from elf bytes: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-135: failed building profiled exec image from elf bytes: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_ir_report(report, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-110: failed building image bridge: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error) {
    MachineImageFile image_file;
    MachineImageError image_error;
    int ok;

    memset(&image_error, 0, sizeof(image_error));
    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_ir_report_with_profile(report, profile, &image_file, &image_error);
    if (!ok) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-136: failed building profiled exec image bridge: %s", image_error.message);
        machine_image_file_free(&image_file);
        return 0;
    }
    ok = machine_exec_build_from_machine_image_file(&image_file, out_exec_file, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_exec_build_report_from_file(const MachineExecFile *source,
    MachineExecReport *out_report,
    MachineExecError *error) {
    size_t segment_index;
    size_t executable_count = 0u;
    size_t non_executable_count = 0u;

    if (!source || !out_report) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-111: invalid exec report build contract");
        return 0;
    }
    if (!machine_exec_verify_file(source, error)) {
        return 0;
    }

    machine_exec_report_free(out_report);
    if (!machine_image_clone_file(&source->image_file, &out_report->file.image_file, NULL)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-112: failed cloning embedded image file");
        machine_exec_report_free(out_report);
        return 0;
    }
    out_report->file.entry_virtual_address = source->entry_virtual_address;
    out_report->file.segment_count = source->segment_count;
    out_report->file.segment_capacity = source->segment_count;
    out_report->file.segments = source->segment_count > 0u
        ? (MachineExecSegment *)calloc(source->segment_count, sizeof(MachineExecSegment))
        : NULL;
    out_report->segment_summary_count = source->segment_count;
    out_report->segment_summaries = source->segment_count > 0u
        ? (MachineExecSegmentSummary *)calloc(source->segment_count, sizeof(MachineExecSegmentSummary))
        : NULL;
    out_report->executable_segment_indices = source->segment_count > 0u
        ? (size_t *)calloc(source->segment_count, sizeof(size_t))
        : NULL;
    out_report->non_executable_segment_indices = source->segment_count > 0u
        ? (size_t *)calloc(source->segment_count, sizeof(size_t))
        : NULL;
    if ((source->segment_count > 0u && !out_report->file.segments) ||
        (source->segment_count > 0u && !out_report->segment_summaries) ||
        (source->segment_count > 0u && !out_report->executable_segment_indices) ||
        (source->segment_count > 0u && !out_report->non_executable_segment_indices)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-113: out of memory building exec report");
        machine_exec_report_free(out_report);
        return 0;
    }
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        const MachineExecSegment *src = &source->segments[segment_index];
        MachineExecSegment *dst = &out_report->file.segments[segment_index];

        dst->name = machine_exec_strdup(src->name ? src->name : "");
        dst->image_segment_index = src->image_segment_index;
        dst->virtual_address = src->virtual_address;
        dst->byte_count = src->byte_count;
        dst->readable = src->readable;
        dst->writable = src->writable;
        dst->executable = src->executable;
        if (!dst->name || !machine_exec_segment_get_summary(dst, &out_report->segment_summaries[segment_index])) {
            machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-114: failed summarizing exec segment");
            machine_exec_report_free(out_report);
            return 0;
        }
        if (dst->executable) {
            out_report->executable_segment_indices[executable_count++] = segment_index;
        } else {
            out_report->non_executable_segment_indices[non_executable_count++] = segment_index;
        }
    }
    out_report->executable_segment_count = executable_count;
    out_report->non_executable_segment_count = non_executable_count;
    if (!machine_exec_file_get_header_summary(&out_report->file, &out_report->header_summary)) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-115: failed summarizing exec header");
        machine_exec_report_free(out_report);
        return 0;
    }
    return 1;
}

int machine_exec_build_report_from_machine_image_file(const MachineImageFile *source,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_image_file(source, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_image_report(const MachineImageReport *report,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_image_report(report, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_file(source, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_file_with_profile(source, profile, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_report(report, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_report_with_profile(report, profile, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_bytes(bytes, byte_count, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_ir_report(report, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error) {
    MachineExecFile exec_file;
    int ok;

    machine_exec_file_init(&exec_file);
    ok = machine_exec_build_from_machine_ir_report_with_profile(report, profile, &exec_file, error) &&
        machine_exec_build_report_from_file(&exec_file, out_report, error);
    machine_exec_file_free(&exec_file);
    return ok;
}

int machine_exec_report_refresh(MachineExecReport *report,
    MachineExecError *error) {
    MachineExecReport refreshed_report;
    int ok;

    if (!report) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-128: invalid exec report refresh contract");
        return 0;
    }

    machine_exec_report_init(&refreshed_report);
    ok = machine_exec_build_report_from_file(&report->file, &refreshed_report, error);
    if (ok) {
        machine_exec_report_free(report);
        *report = refreshed_report;
    } else {
        machine_exec_report_free(&refreshed_report);
    }
    return ok;
}

#endif
