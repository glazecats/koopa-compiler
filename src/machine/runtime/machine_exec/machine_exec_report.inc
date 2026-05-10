#if defined(__INTELLISENSE__) && !defined(MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_EXEC_SPLIT_AGGREGATOR)
#define MACHINE_EXEC_SPLIT_INTELLISENSE_CONTEXT
#include "machine_exec.c"

#else

static int machine_exec_report_has_segment_tables(const MachineExecReport *report) {
    return report &&
        report->segment_summary_count == report->file.segment_count &&
        (report->segment_summary_count == 0u || report->segment_summaries) &&
        (report->file.segment_count == 0u || report->file.segments) &&
        (report->executable_segment_count == 0u || report->executable_segment_indices) &&
        (report->non_executable_segment_count == 0u || report->non_executable_segment_indices);
}

int machine_exec_report_get_summary(const MachineExecReport *report,
    size_t *out_segment_count,
    size_t *out_byte_count,
    size_t *out_executable_segment_count) {
    if (!machine_exec_report_has_segment_tables(report)) {
        return 0;
    }
    if (out_segment_count) {
        *out_segment_count = report->segment_summary_count;
    }
    if (out_byte_count) {
        *out_byte_count = report->file.image_file.byte_count;
    }
    if (out_executable_segment_count) {
        *out_executable_segment_count = report->executable_segment_count;
    }
    return 1;
}

int machine_exec_report_get_overview_artifact(const MachineExecReport *report,
    MachineExecReportOverviewArtifact *out_artifact) {
    size_t entry_segment_index = 0u;
    const MachineExecSegmentSummary *entry_segment_summary = NULL;

    if (!report || !out_artifact) {
        return 0;
    }
    if (!machine_exec_report_get_entry_segment_summary_artifact(
            report, &entry_segment_index, &entry_segment_summary) ||
        !entry_segment_summary) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->entry_segment_index = entry_segment_index;
    out_artifact->entry_segment_summary = entry_segment_summary;
    out_artifact->executable_segment_indices = report->executable_segment_indices;
    out_artifact->executable_segment_count = report->executable_segment_count;
    out_artifact->non_executable_segment_indices = report->non_executable_segment_indices;
    out_artifact->non_executable_segment_count = report->non_executable_segment_count;
    return 1;
}

int machine_exec_report_get_file(const MachineExecReport *report,
    const MachineExecFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_exec_report_get_header_summary_artifact(const MachineExecReport *report,
    const MachineExecHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_exec_report_get_entry_segment_summary_artifact(const MachineExecReport *report,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary) {
    size_t segment_index = 0u;
    const MachineExecSegment *segment = NULL;

    if (!machine_exec_report_has_segment_tables(report)) {
        return 0;
    }
    if (!machine_exec_file_get_entry_segment(&report->file, &segment_index, &segment) || !segment) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    if (out_summary) {
        return machine_exec_report_get_segment_summary(report, segment_index, out_summary);
    }
    return 1;
}

int machine_exec_report_get_segment_summary(const MachineExecReport *report,
    size_t segment_index,
    const MachineExecSegmentSummary **out_summary) {
    if (!machine_exec_report_has_segment_tables(report) ||
        !out_summary ||
        segment_index >= report->segment_summary_count) {
        return 0;
    }
    *out_summary = &report->segment_summaries[segment_index];
    return 1;
}

int machine_exec_report_get_segment_artifact(const MachineExecReport *report,
    size_t segment_index,
    MachineExecReportSegmentArtifact *out_artifact) {
    const MachineExecSegmentSummary *summary = NULL;
    size_t entry_segment_index = 0u;

    if (!machine_exec_report_has_segment_tables(report) ||
        !out_artifact ||
        !machine_exec_report_get_segment_summary(report, segment_index, &summary) ||
        !summary) {
        return 0;
    }
    if (!machine_exec_report_get_entry_segment_summary_artifact(report, &entry_segment_index, NULL)) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->segment_index = segment_index;
    out_artifact->segment_summary = summary;
    out_artifact->is_entry_segment = (int)(segment_index == entry_segment_index);
    out_artifact->is_executable_segment = (int)summary->executable;
    return 1;
}

int machine_exec_report_find_segment_summary_by_name(const MachineExecReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_exec_report_has_segment_tables(report) || !segment_name) {
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        if (report->segment_summaries[segment_index].name &&
            strcmp(report->segment_summaries[segment_index].name, segment_name) == 0) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_summary) {
                *out_summary = &report->segment_summaries[segment_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_exec_report_find_segment_artifact_by_name(const MachineExecReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!report || !segment_name || !out_artifact ||
        !machine_exec_report_find_segment_summary_by_name(report, segment_name, &segment_index, NULL) ||
        !machine_exec_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_exec_report_find_segment_summary_covering_virtual_address(const MachineExecReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_exec_report_has_segment_tables(report)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        const MachineExecSegmentSummary *summary = &report->segment_summaries[segment_index];
        size_t delta;

        if (summary->byte_count == 0u || virtual_address < summary->virtual_address) {
            continue;
        }
        delta = virtual_address - summary->virtual_address;
        if (delta >= summary->byte_count) {
            continue;
        }
        if (out_segment_index) {
            *out_segment_index = segment_index;
        }
        if (out_summary) {
            *out_summary = summary;
        }
        return 1;
    }
    return 0;
}

int machine_exec_report_find_segment_artifact_covering_virtual_address(const MachineExecReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!report || !out_artifact ||
        !machine_exec_report_find_segment_summary_covering_virtual_address(
            report, virtual_address, &segment_index, NULL) ||
        !machine_exec_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_exec_report_get_segment_filter_count(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t *out_count) {
    if (!machine_exec_report_has_segment_tables(report)) {
        return 0;
    }
    return machine_exec_report_get_segment_filter_storage(report, filter_kind, NULL, out_count);
}

int machine_exec_report_get_segment_summary_by_filter_index(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary) {
    const size_t *indices = NULL;
    size_t count = 0u;
    size_t segment_index = 0u;

    if (!machine_exec_report_has_segment_tables(report)) {
        return 0;
    }
    if (!machine_exec_report_get_segment_filter_storage(report, filter_kind, &indices, &count) ||
        filtered_index >= count) {
        return 0;
    }
    if (filter_kind == MACHINE_EXEC_SEGMENT_FILTER_ENTRY) {
        if (!machine_exec_report_get_entry_segment_summary_artifact(report, &segment_index, out_summary)) {
            return 0;
        }
    } else {
        if (!indices) {
            return 0;
        }
        segment_index = indices[filtered_index];
        if (out_summary && !machine_exec_report_get_segment_summary(report, segment_index, out_summary)) {
            return 0;
        }
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_exec_report_get_segment_artifact_by_filter_index(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!report || !out_artifact ||
        !machine_exec_report_get_segment_summary_by_filter_index(
            report, filter_kind, filtered_index, &segment_index, NULL) ||
        !machine_exec_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_exec_report_overview_artifact_get_segment_summary(
    const MachineExecReportOverviewArtifact *artifact,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_exec_report_get_segment_summary_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_segment_index, out_summary);
}

int machine_exec_report_overview_artifact_get_segment_artifact(
    const MachineExecReportOverviewArtifact *artifact,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_exec_report_get_segment_artifact_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_segment_index, out_artifact);
}

int machine_exec_report_get_executable_segment_count(const MachineExecReport *report,
    size_t *out_count) {
    if (!machine_exec_report_has_segment_tables(report) || !out_count) {
        return 0;
    }
    *out_count = report->executable_segment_count;
    return 1;
}

int machine_exec_report_get_executable_segment_summary_by_index(const MachineExecReport *report,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_exec_report_has_segment_tables(report) ||
        !report->executable_segment_indices ||
        executable_index >= report->executable_segment_count) {
        return 0;
    }
    segment_index = report->executable_segment_indices[executable_index];
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    if (out_summary) {
        return machine_exec_report_get_segment_summary(report, segment_index, out_summary);
    }
    return 1;
}

int machine_exec_dump_report(const MachineExecReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecStringBuilder builder;
    char *raw_text = NULL;
    size_t executable_index;
    size_t filter_index;
    size_t entry_segment_index = 0u;
    const MachineExecSegmentSummary *entry_segment_summary = NULL;
    MachineExecReportOverviewArtifact overview_artifact;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_exec_set_error(error, 0, 0, "MACHINE-EXEC-123: invalid exec report dump contract");
        return 0;
    }
    if (!machine_exec_verify_file(&report->file, error) ||
        !machine_exec_dump_file(&report->file, &raw_text, error) ||
        !raw_text) {
        free(raw_text);
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_exec_report_get_entry_segment_summary_artifact(
            report, &entry_segment_index, &entry_segment_summary) ||
        !entry_segment_summary ||
        !machine_exec_report_get_overview_artifact(report, &overview_artifact)) {
        goto cleanup;
    }
    if (!machine_exec_append_format(&builder, "%s", raw_text) ||
        !machine_exec_append_format(
            &builder,
            "report_overview:\n"
            "  segments=%zu bytes=%zu executable_segments=%zu non_executable_segments=%zu entry=0x%zx entry_segment=%zu\n",
            report->segment_summary_count,
            report->file.image_file.byte_count,
            report->executable_segment_count,
            report->non_executable_segment_count,
            report->header_summary.entry_virtual_address,
            entry_segment_index) ||
        !machine_exec_append_format(&builder, "report_segment_filters:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index <= (size_t)MACHINE_EXEC_SEGMENT_FILTER_ENTRY; ++filter_index) {
        size_t count = 0u;
        size_t filtered_index;
        const MachineExecSegmentSummary *segment_summary = NULL;
        size_t segment_index = 0u;

        if (!machine_exec_report_get_segment_filter_count(
                report,
                (MachineExecReportSegmentFilterKind)filter_index,
                &count) ||
            !machine_exec_append_format(
                &builder,
                "  %s[%zu]",
                machine_exec_segment_filter_kind_name((MachineExecReportSegmentFilterKind)filter_index),
                count)) {
            goto cleanup;
        }
        for (filtered_index = 0u; filtered_index < count; ++filtered_index) {
            if (!machine_exec_report_get_segment_summary_by_filter_index(
                    report,
                    (MachineExecReportSegmentFilterKind)filter_index,
                    filtered_index,
                    &segment_index,
                    &segment_summary) ||
                !segment_summary ||
                !machine_exec_append_format(
                    &builder,
                    " xseg.%zu(%s)",
                    segment_index,
                    segment_summary->name ? segment_summary->name : "<null>")) {
                goto cleanup;
            }
        }
        if (!machine_exec_append_format(&builder, "\n")) {
            goto cleanup;
        }
    }
    if (!machine_exec_append_format(&builder, "report_executable_segments:\n")) {
        goto cleanup;
    }
    for (executable_index = 0u; executable_index < report->executable_segment_count; ++executable_index) {
        size_t segment_index = 0u;
        const MachineExecSegmentSummary *segment_summary = NULL;

        if (!machine_exec_report_overview_artifact_get_segment_summary(
                &overview_artifact,
                MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE,
                executable_index,
                &segment_index,
                &segment_summary) ||
            !segment_summary ||
            !machine_exec_append_format(
                &builder,
                "  xseg.%zu %s perms=%c%c%c\n",
                segment_index,
                segment_summary->name ? segment_summary->name : "<null>",
                segment_summary->readable ? 'r' : '-',
                segment_summary->writable ? 'w' : '-',
                segment_summary->executable ? 'x' : '-')) {
            goto cleanup;
        }
    }
    if (!machine_exec_append_format(&builder, "report_segments:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index < report->segment_summary_count; ++filter_index) {
        MachineExecReportSegmentArtifact segment_artifact;

        if (!machine_exec_report_get_segment_artifact(report, filter_index, &segment_artifact) ||
            !segment_artifact.segment_summary ||
            !machine_exec_append_format(
                &builder,
                "  xseg.%zu %s entry=%s executable=%s perms=%c%c%c\n",
                filter_index,
                segment_artifact.segment_summary->name ? segment_artifact.segment_summary->name : "<null>",
                segment_artifact.is_entry_segment ? "yes" : "no",
                segment_artifact.is_executable_segment ? "yes" : "no",
                segment_artifact.segment_summary->readable ? 'r' : '-',
                segment_artifact.segment_summary->writable ? 'w' : '-',
                segment_artifact.segment_summary->executable ? 'x' : '-')) {
            goto cleanup;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(raw_text);
    free(builder.data);
    return ok;
}

int machine_exec_build_report_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport report;
    int ok;

    machine_exec_report_init(&report);
    ok = machine_exec_build_report_from_machine_image_file(source, &report, error) &&
        machine_exec_dump_report(&report, out_text, error);
    machine_exec_report_free(&report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_image_report(report, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_elf_file(source, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_elf_file_with_profile(source, profile, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_elf_report(report, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_elf_report_with_profile(report, profile, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_elf_bytes(bytes, byte_count, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_ir_report(report, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

int machine_exec_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error) {
    MachineExecReport exec_report;
    int ok;

    machine_exec_report_init(&exec_report);
    ok = machine_exec_build_report_from_machine_ir_report_with_profile(report, profile, &exec_report, error) &&
        machine_exec_dump_report(&exec_report, out_text, error);
    machine_exec_report_free(&exec_report);
    return ok;
}

#endif
