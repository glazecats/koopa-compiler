#if defined(__INTELLISENSE__) && !defined(MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_LOAD_SPLIT_AGGREGATOR)
#define MACHINE_LOAD_SPLIT_INTELLISENSE_CONTEXT
#include "machine_load.c"

#else

static int machine_load_report_has_segment_tables(const MachineLoadReport *report) {
    return report &&
        report->segment_summary_count == report->file.segment_count &&
        (report->segment_summary_count == 0u || report->segment_summaries) &&
        (report->file.segment_count == 0u || report->file.segments) &&
        (report->executable_segment_count == 0u || report->executable_segment_indices) &&
        (report->non_executable_segment_count == 0u || report->non_executable_segment_indices);
}

int machine_load_report_get_summary(const MachineLoadReport *report,
    size_t *out_segment_count,
    size_t *out_file_byte_count,
    size_t *out_memory_byte_count,
    size_t *out_executable_segment_count) {
    if (!machine_load_report_has_segment_tables(report)) {
        return 0;
    }
    if (out_segment_count) {
        *out_segment_count = report->segment_summary_count;
    }
    if (out_file_byte_count) {
        *out_file_byte_count = report->file.exec_file.image_file.byte_count;
    }
    if (out_memory_byte_count) {
        *out_memory_byte_count = report->file.total_memory_byte_count;
    }
    if (out_executable_segment_count) {
        *out_executable_segment_count = report->executable_segment_count;
    }
    return 1;
}

int machine_load_report_get_overview_artifact(const MachineLoadReport *report,
    MachineLoadReportOverviewArtifact *out_artifact) {
    size_t entry_segment_index = 0u;
    const MachineLoadSegmentSummary *entry_segment_summary = NULL;

    if (!report || !out_artifact) {
        return 0;
    }
    if (!machine_load_report_get_entry_segment_summary_artifact(
            report, &entry_segment_index, &entry_segment_summary) ||
        !entry_segment_summary) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->memory_summary = &report->memory_summary;
    out_artifact->entry_segment_index = entry_segment_index;
    out_artifact->entry_segment_summary = entry_segment_summary;
    out_artifact->executable_segment_indices = report->executable_segment_indices;
    out_artifact->executable_segment_count = report->executable_segment_count;
    out_artifact->non_executable_segment_indices = report->non_executable_segment_indices;
    out_artifact->non_executable_segment_count = report->non_executable_segment_count;
    return 1;
}

int machine_load_report_get_file(const MachineLoadReport *report,
    const MachineLoadFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_load_report_get_source_elf_artifact_summary_artifact(const MachineLoadReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_load_report_get_header_summary_artifact(const MachineLoadReport *report,
    const MachineLoadHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_load_report_get_target_policy_summary_artifact(const MachineLoadReport *report,
    const MachineLoadTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_load_report_get_memory_summary_artifact(const MachineLoadReport *report,
    const MachineLoadMemorySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->memory_summary;
    return 1;
}

int machine_load_report_get_entry_segment_summary_artifact(const MachineLoadReport *report,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    size_t segment_index = 0u;
    const MachineLoadSegment *segment = NULL;

    if (!machine_load_report_has_segment_tables(report)) {
        return 0;
    }
    if (!machine_load_file_get_entry_segment(&report->file, &segment_index, &segment) || !segment) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    if (out_summary) {
        return machine_load_report_get_segment_summary(report, segment_index, out_summary);
    }
    return 1;
}

int machine_load_report_get_segment_summary(const MachineLoadReport *report,
    size_t segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    if (!machine_load_report_has_segment_tables(report) ||
        !out_summary ||
        segment_index >= report->segment_summary_count) {
        return 0;
    }
    *out_summary = &report->segment_summaries[segment_index];
    return 1;
}

int machine_load_report_get_segment_artifact(const MachineLoadReport *report,
    size_t segment_index,
    MachineLoadReportSegmentArtifact *out_artifact) {
    const MachineLoadSegmentSummary *summary = NULL;
    size_t entry_segment_index = 0u;

    if (!machine_load_report_has_segment_tables(report) ||
        !out_artifact ||
        !machine_load_report_get_segment_summary(report, segment_index, &summary) ||
        !summary) {
        return 0;
    }
    if (!machine_load_report_get_entry_segment_summary_artifact(report, &entry_segment_index, NULL)) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->segment_index = segment_index;
    out_artifact->segment_summary = summary;
    out_artifact->is_entry_segment = (int)(segment_index == entry_segment_index);
    out_artifact->is_executable_segment = (int)summary->executable;
    return 1;
}

int machine_load_report_find_segment_summary_by_name(const MachineLoadReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_load_report_has_segment_tables(report) || !segment_name) {
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

int machine_load_report_find_segment_artifact_by_name(const MachineLoadReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!report || !segment_name || !out_artifact ||
        !machine_load_report_find_segment_summary_by_name(report, segment_name, &segment_index, NULL) ||
        !machine_load_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_load_report_find_segment_summary_covering_virtual_address(const MachineLoadReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_load_report_has_segment_tables(report)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        const MachineLoadSegmentSummary *summary = &report->segment_summaries[segment_index];
        size_t delta;

        if (summary->memory_byte_count == 0u || virtual_address < summary->virtual_address) {
            continue;
        }
        delta = virtual_address - summary->virtual_address;
        if (delta >= summary->memory_byte_count) {
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

int machine_load_report_find_segment_artifact_covering_virtual_address(const MachineLoadReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!report || !out_artifact ||
        !machine_load_report_find_segment_summary_covering_virtual_address(
            report, virtual_address, &segment_index, NULL) ||
        !machine_load_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_load_report_get_segment_filter_count(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t *out_count) {
    if (!machine_load_report_has_segment_tables(report)) {
        return 0;
    }
    return machine_load_report_get_segment_filter_storage(report, filter_kind, NULL, out_count);
}

int machine_load_report_get_segment_summary_by_filter_index(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    const size_t *indices = NULL;
    size_t count = 0u;
    size_t segment_index = 0u;

    if (!machine_load_report_has_segment_tables(report)) {
        return 0;
    }
    if (!machine_load_report_get_segment_filter_storage(report, filter_kind, &indices, &count) ||
        filtered_index >= count) {
        return 0;
    }
    if (filter_kind == MACHINE_LOAD_SEGMENT_FILTER_ENTRY) {
        if (!machine_load_report_get_entry_segment_summary_artifact(report, &segment_index, out_summary)) {
            return 0;
        }
    } else {
        if (!indices) {
            return 0;
        }
        segment_index = indices[filtered_index];
        if (out_summary && !machine_load_report_get_segment_summary(report, segment_index, out_summary)) {
            return 0;
        }
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_load_report_get_segment_artifact_by_filter_index(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!report || !out_artifact ||
        !machine_load_report_get_segment_summary_by_filter_index(
            report, filter_kind, filtered_index, &segment_index, NULL) ||
        !machine_load_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_load_report_overview_artifact_get_segment_summary(
    const MachineLoadReportOverviewArtifact *artifact,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_load_report_get_segment_summary_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_segment_index, out_summary);
}

int machine_load_report_overview_artifact_get_segment_artifact(
    const MachineLoadReportOverviewArtifact *artifact,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_load_report_get_segment_artifact_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_segment_index, out_artifact);
}

int machine_load_report_get_executable_segment_count(const MachineLoadReport *report,
    size_t *out_count) {
    if (!machine_load_report_has_segment_tables(report) || !out_count) {
        return 0;
    }
    *out_count = report->executable_segment_count;
    return 1;
}

int machine_load_report_get_executable_segment_summary_by_index(const MachineLoadReport *report,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_load_report_has_segment_tables(report) ||
        !report->executable_segment_indices ||
        executable_index >= report->executable_segment_count) {
        return 0;
    }
    segment_index = report->executable_segment_indices[executable_index];
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    if (out_summary) {
        return machine_load_report_get_segment_summary(report, segment_index, out_summary);
    }
    return 1;
}

int machine_load_dump_report(const MachineLoadReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadStringBuilder builder;
    char *raw_text = NULL;
    size_t entry_segment_index = 0u;
    const MachineLoadSegmentSummary *entry_segment_summary = NULL;
    MachineLoadReportOverviewArtifact overview_artifact;
    size_t filter_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_load_set_error(error, 0, 0, "MACHINE-LOAD-134: invalid load report dump contract");
        return 0;
    }
    if (!machine_load_verify_file(&report->file, error) ||
        !machine_load_dump_file(&report->file, &raw_text, error) ||
        !raw_text ||
        !machine_load_report_get_entry_segment_summary_artifact(
            report, &entry_segment_index, &entry_segment_summary) ||
        !entry_segment_summary ||
        !machine_load_report_get_overview_artifact(report, &overview_artifact)) {
        free(raw_text);
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_load_append_format(&builder, "%s", raw_text) ||
        !machine_load_append_format(
            &builder,
            "report_overview:\n"
            "  segments=%zu file_bytes=%zu memory_bytes=%zu executable_segments=%zu non_executable_segments=%zu entry=0x%zx entry_segment=%zu\n"
            "  elf_source: target=%s origin=%s semantics=%s\n"
            "  policy: profile=%s base=0x%zx align=%zu\n"
            "  memory: base=0x%zx end=0x%zx entry-offset=0x%zx\n",
            report->segment_summary_count,
            report->file.exec_file.image_file.byte_count,
            report->file.total_memory_byte_count,
            report->executable_segment_count,
            report->non_executable_segment_count,
            report->header_summary.entry_virtual_address,
            entry_segment_index,
            machine_elf_target_profile_name(report->file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(report->file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            machine_elf_target_profile_name(overview_artifact.target_policy_summary->target_profile),
            overview_artifact.target_policy_summary->base_virtual_address,
            overview_artifact.target_policy_summary->segment_alignment,
            overview_artifact.memory_summary->base_virtual_address,
            overview_artifact.memory_summary->end_virtual_address,
            overview_artifact.memory_summary->entry_offset) ||
        !machine_load_append_format(&builder, "report_segment_filters:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index <= (size_t)MACHINE_LOAD_SEGMENT_FILTER_ENTRY; ++filter_index) {
        size_t count = 0u;
        size_t filtered_index;
        const MachineLoadSegmentSummary *segment_summary = NULL;
        size_t segment_index = 0u;

        if (!machine_load_report_get_segment_filter_count(
                report,
                (MachineLoadReportSegmentFilterKind)filter_index,
                &count) ||
            !machine_load_append_format(
                &builder,
                "  %s[%zu]",
                machine_load_segment_filter_kind_name((MachineLoadReportSegmentFilterKind)filter_index),
                count)) {
            goto cleanup;
        }
        for (filtered_index = 0u; filtered_index < count; ++filtered_index) {
            if (!machine_load_report_get_segment_summary_by_filter_index(
                    report,
                    (MachineLoadReportSegmentFilterKind)filter_index,
                    filtered_index,
                    &segment_index,
                    &segment_summary) ||
                !segment_summary ||
                !machine_load_append_format(
                    &builder,
                    " lseg.%zu(%s)",
                    segment_index,
                    segment_summary->name ? segment_summary->name : "<null>")) {
                goto cleanup;
            }
        }
        if (!machine_load_append_format(&builder, "\n")) {
            goto cleanup;
        }
    }
    if (!machine_load_append_format(&builder, "report_executable_segments:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index < report->executable_segment_count; ++filter_index) {
        size_t segment_index = 0u;
        const MachineLoadSegmentSummary *segment_summary = NULL;

        if (!machine_load_report_overview_artifact_get_segment_summary(
                &overview_artifact,
                MACHINE_LOAD_SEGMENT_FILTER_EXECUTABLE,
                filter_index,
                &segment_index,
                &segment_summary) ||
            !segment_summary ||
            !machine_load_append_format(
                &builder,
                "  lseg.%zu %s mem-bytes=%zu zero-fill=%zu perms=%c%c%c\n",
                segment_index,
                segment_summary->name ? segment_summary->name : "<null>",
                segment_summary->memory_byte_count,
                segment_summary->memory_byte_count - segment_summary->file_byte_count,
                segment_summary->readable ? 'r' : '-',
                segment_summary->writable ? 'w' : '-',
                segment_summary->executable ? 'x' : '-')) {
            goto cleanup;
        }
    }
    if (!machine_load_append_format(&builder, "report_segments:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index < report->segment_summary_count; ++filter_index) {
        MachineLoadReportSegmentArtifact segment_artifact;

        if (!machine_load_report_get_segment_artifact(report, filter_index, &segment_artifact) ||
            !segment_artifact.segment_summary ||
            !machine_load_append_format(
                &builder,
                "  lseg.%zu %s entry=%s executable=%s file-bytes=%zu mem-bytes=%zu zero-fill=%zu perms=%c%c%c\n",
                filter_index,
                segment_artifact.segment_summary->name ? segment_artifact.segment_summary->name : "<null>",
                segment_artifact.is_entry_segment ? "yes" : "no",
                segment_artifact.is_executable_segment ? "yes" : "no",
                segment_artifact.segment_summary->file_byte_count,
                segment_artifact.segment_summary->memory_byte_count,
                segment_artifact.segment_summary->memory_byte_count -
                    segment_artifact.segment_summary->file_byte_count,
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

int machine_load_build_report_dump_from_file(const MachineLoadFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_file(source, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_machine_exec_file(source, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport load_report;
    int ok;

    machine_load_report_init(&load_report);
    ok = machine_load_build_report_from_machine_exec_report(report, &load_report, error) &&
        machine_load_dump_report(&load_report, out_text, error);
    machine_load_report_free(&load_report);
    return ok;
}

int machine_load_build_report_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_machine_image_file(source, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport load_report;
    int ok;

    machine_load_report_init(&load_report);
    ok = machine_load_build_report_from_machine_image_report(report, &load_report, error) &&
        machine_load_dump_report(&load_report, out_text, error);
    machine_load_report_free(&load_report);
    return ok;
}

int machine_load_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_machine_elf_file(source, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_machine_elf_file_with_profile(source, profile, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport load_report;
    int ok;

    machine_load_report_init(&load_report);
    ok = machine_load_build_report_from_machine_elf_report(report, &load_report, error) &&
        machine_load_dump_report(&load_report, out_text, error);
    machine_load_report_free(&load_report);
    return ok;
}

int machine_load_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport load_report;
    int ok;

    machine_load_report_init(&load_report);
    ok = machine_load_build_report_from_machine_elf_report_with_profile(report, profile, &load_report, error) &&
        machine_load_dump_report(&load_report, out_text, error);
    machine_load_report_free(&load_report);
    return ok;
}

int machine_load_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_machine_elf_bytes(bytes, byte_count, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport report;
    int ok;

    machine_load_report_init(&report);
    ok = machine_load_build_report_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &report, error) &&
        machine_load_dump_report(&report, out_text, error);
    machine_load_report_free(&report);
    return ok;
}

int machine_load_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport load_report;
    int ok;

    machine_load_report_init(&load_report);
    ok = machine_load_build_report_from_machine_ir_report(report, &load_report, error) &&
        machine_load_dump_report(&load_report, out_text, error);
    machine_load_report_free(&load_report);
    return ok;
}

int machine_load_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error) {
    MachineLoadReport load_report;
    int ok;

    machine_load_report_init(&load_report);
    ok = machine_load_build_report_from_machine_ir_report_with_profile(report, profile, &load_report, error) &&
        machine_load_dump_report(&load_report, out_text, error);
    machine_load_report_free(&load_report);
    return ok;
}

#endif
