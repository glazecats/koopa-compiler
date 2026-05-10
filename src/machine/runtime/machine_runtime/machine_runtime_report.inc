#if defined(__INTELLISENSE__) && !defined(MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_RUNTIME_SPLIT_AGGREGATOR)
#define MACHINE_RUNTIME_SPLIT_INTELLISENSE_CONTEXT
#include "machine_runtime.c"

#else

static int machine_runtime_report_has_segment_tables(const MachineRuntimeReport *report) {
    return report &&
        report->segment_summary_count == report->file.segment_count &&
        (report->segment_summary_count == 0u || report->segment_summaries) &&
        (report->file.segment_count == 0u || report->file.segments);
}

int machine_runtime_report_get_summary(const MachineRuntimeReport *report,
    size_t *out_segment_count,
    size_t *out_mapped_byte_count,
    size_t *out_executable_segment_count) {
    if (!report) {
        return 0;
    }
    if (out_segment_count) {
        *out_segment_count = report->segment_summary_count;
    }
    if (out_mapped_byte_count) {
        *out_mapped_byte_count = report->header_summary.mapped_byte_count;
    }
    if (out_executable_segment_count) {
        *out_executable_segment_count = report->executable_segment_count;
    }
    return 1;
}

int machine_runtime_report_get_overview_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportOverviewArtifact *out_artifact) {
    size_t entry_segment_index = 0u;
    const MachineRuntimeSegmentSummary *entry_summary = NULL;
    size_t stack_segment_index = 0u;
    const MachineRuntimeSegmentSummary *stack_summary = NULL;

    if (!report || !out_artifact ||
        !machine_runtime_report_get_entry_segment_summary_artifact(report, &entry_segment_index, &entry_summary)) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->memory_summary = &report->memory_summary;
    out_artifact->stack_summary = &report->stack_summary;
    out_artifact->gap_summary = &report->gap_summary;
    out_artifact->launch_summary = &report->launch_summary;
    out_artifact->initial_stack_summary = &report->initial_stack_summary;
    out_artifact->entry_segment_index = entry_segment_index;
    out_artifact->entry_segment_summary = entry_summary;
    out_artifact->has_stack_segment =
        machine_runtime_report_get_stack_segment_summary_artifact(report, &stack_segment_index, &stack_summary);
    out_artifact->stack_segment_index = stack_segment_index;
    out_artifact->stack_segment_summary = stack_summary;
    out_artifact->executable_segment_indices = report->executable_segment_indices;
    out_artifact->executable_segment_count = report->executable_segment_count;
    out_artifact->non_executable_segment_indices = report->non_executable_segment_indices;
    out_artifact->non_executable_segment_count = report->non_executable_segment_count;
    return 1;
}

int machine_runtime_report_get_file(const MachineRuntimeReport *report,
    const MachineRuntimeFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_runtime_report_get_source_elf_artifact_summary_artifact(const MachineRuntimeReport *report,
    const MachineElfArtifactSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->file.load_file.exec_file.image_file.source_elf_artifact_summary;
    return 1;
}

int machine_runtime_report_get_header_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_runtime_report_get_target_policy_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_runtime_report_get_memory_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeMemorySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->memory_summary;
    return 1;
}

int machine_runtime_report_get_stack_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeStackSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->stack_summary;
    return 1;
}

int machine_runtime_report_get_gap_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeGapSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->gap_summary;
    return 1;
}

int machine_runtime_report_get_launch_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->launch_summary;
    return 1;
}

int machine_runtime_report_get_initial_stack_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->initial_stack_summary;
    return 1;
}

int machine_runtime_report_get_stack_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportStackArtifact *out_artifact) {
    size_t stack_segment_index = 0u;

    if (!report || !out_artifact ||
        !machine_runtime_report_get_stack_segment_summary_artifact(report, &stack_segment_index, NULL)) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->stack_summary = &report->stack_summary;
    if (!machine_runtime_report_get_segment_artifact(report, stack_segment_index, &out_artifact->stack_segment_artifact)) {
        return 0;
    }
    return 1;
}

int machine_runtime_report_get_gap_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportGapArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->gap_summary = &report->gap_summary;
    return 1;
}

int machine_runtime_report_get_launch_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportLaunchArtifact *out_artifact) {
    size_t entry_segment_index = 0u;

    if (!report || !out_artifact ||
        !machine_runtime_report_get_entry_segment_summary_artifact(report, &entry_segment_index, NULL)) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->launch_summary = &report->launch_summary;
    if (!machine_runtime_report_get_segment_artifact(report, entry_segment_index, &out_artifact->entry_segment_artifact) ||
        !machine_runtime_report_get_stack_artifact(report, &out_artifact->stack_artifact)) {
        return 0;
    }
    return 1;
}

int machine_runtime_report_get_initial_stack_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportInitialStackArtifact *out_artifact) {
    if (!report || !out_artifact) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->initial_stack_summary = &report->initial_stack_summary;
    if (!machine_runtime_report_get_stack_artifact(report, &out_artifact->stack_artifact) ||
        !machine_runtime_report_get_launch_artifact(report, &out_artifact->launch_artifact)) {
        return 0;
    }
    return 1;
}

int machine_runtime_report_get_region_count(const MachineRuntimeReport *report,
    size_t *out_count) {
    if (!report || !out_count) {
        return 0;
    }
    return machine_runtime_file_get_region_count(&report->file, out_count);
}

int machine_runtime_report_get_region_summary(const MachineRuntimeReport *report,
    size_t region_index,
    MachineRuntimeRegionSummary *out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    return machine_runtime_file_get_region_summary(&report->file, region_index, out_summary);
}

int machine_runtime_report_get_region_artifact(const MachineRuntimeReport *report,
    size_t region_index,
    MachineRuntimeReportRegionArtifact *out_artifact) {
    MachineRuntimeRegionSummary summary;

    memset(&summary, 0, sizeof(summary));
    if (!report || !out_artifact || !machine_runtime_report_get_region_summary(report, region_index, &summary)) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->summary = summary;
    switch (summary.kind) {
        case MACHINE_RUNTIME_REGION_LOAD:
        case MACHINE_RUNTIME_REGION_STACK:
            out_artifact->has_segment_artifact =
                machine_runtime_report_get_segment_artifact(report, summary.segment_index, &out_artifact->segment_artifact);
            if (summary.kind == MACHINE_RUNTIME_REGION_STACK) {
                out_artifact->has_stack_artifact =
                    machine_runtime_report_get_stack_artifact(report, &out_artifact->stack_artifact);
            }
            return out_artifact->has_segment_artifact;
        case MACHINE_RUNTIME_REGION_GAP:
            out_artifact->has_gap_artifact =
                machine_runtime_report_get_gap_artifact(report, &out_artifact->gap_artifact);
            return out_artifact->has_gap_artifact;
    }
    return 0;
}

int machine_runtime_report_get_entry_segment_summary_artifact(const MachineRuntimeReport *report,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_runtime_report_has_segment_tables(report)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        const MachineRuntimeSegmentSummary *summary = &report->segment_summaries[segment_index];

        if (summary->executable &&
            machine_runtime_segment_covers_virtual_address(
                &report->file.segments[segment_index], report->header_summary.entry_virtual_address)) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_summary) {
                *out_summary = summary;
            }
            return 1;
        }
    }
    return 0;
}

int machine_runtime_report_get_stack_segment_summary_artifact(const MachineRuntimeReport *report,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!machine_runtime_report_has_segment_tables(report) ||
        !report->has_stack_segment ||
        report->stack_segment_index >= report->segment_summary_count) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = report->stack_segment_index;
    }
    if (out_summary) {
        *out_summary = &report->segment_summaries[report->stack_segment_index];
    }
    return 1;
}

int machine_runtime_report_get_segment_summary(const MachineRuntimeReport *report,
    size_t segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!machine_runtime_report_has_segment_tables(report) ||
        segment_index >= report->segment_summary_count ||
        !out_summary) {
        return 0;
    }
    *out_summary = &report->segment_summaries[segment_index];
    return 1;
}

int machine_runtime_report_get_segment_artifact(const MachineRuntimeReport *report,
    size_t segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact) {
    size_t entry_segment_index = 0u;
    size_t stack_segment_index = 0u;

    if (!machine_runtime_report_has_segment_tables(report) ||
        !out_artifact ||
        segment_index >= report->segment_summary_count) {
        return 0;
    }
    memset(out_artifact, 0, sizeof(*out_artifact));
    out_artifact->report = report;
    out_artifact->segment_index = segment_index;
    out_artifact->segment_summary = &report->segment_summaries[segment_index];
    out_artifact->is_entry_segment =
        machine_runtime_report_get_entry_segment_summary_artifact(report, &entry_segment_index, NULL) &&
        entry_segment_index == segment_index;
    out_artifact->is_stack_segment =
        machine_runtime_report_get_stack_segment_summary_artifact(report, &stack_segment_index, NULL) &&
        stack_segment_index == segment_index;
    out_artifact->is_executable_segment = out_artifact->segment_summary->executable;
    return 1;
}

int machine_runtime_report_find_segment_summary_by_name(const MachineRuntimeReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_runtime_report_has_segment_tables(report) || !segment_name) {
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

int machine_runtime_report_find_segment_artifact_by_name(const MachineRuntimeReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!machine_runtime_report_find_segment_summary_by_name(report, segment_name, &segment_index, NULL)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return !out_artifact || machine_runtime_report_get_segment_artifact(report, segment_index, out_artifact);
}

int machine_runtime_report_find_segment_summary_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_runtime_report_has_segment_tables(report)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        if (machine_runtime_segment_covers_virtual_address(&report->file.segments[segment_index], virtual_address)) {
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

int machine_runtime_report_find_segment_artifact_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!machine_runtime_report_find_segment_summary_covering_virtual_address(
            report, virtual_address, &segment_index, NULL)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return !out_artifact || machine_runtime_report_get_segment_artifact(report, segment_index, out_artifact);
}

int machine_runtime_report_find_segment_summary_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    size_t virtual_address;

    if (!report || memory_offset >= report->memory_summary.span_byte_count) {
        return 0;
    }
    virtual_address = report->memory_summary.base_virtual_address + memory_offset;
    return machine_runtime_report_find_segment_summary_covering_virtual_address(
        report, virtual_address, out_segment_index, out_summary);
}

int machine_runtime_report_find_segment_artifact_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact) {
    size_t virtual_address;

    if (!report || memory_offset >= report->memory_summary.span_byte_count) {
        return 0;
    }
    virtual_address = report->memory_summary.base_virtual_address + memory_offset;
    return machine_runtime_report_find_segment_artifact_covering_virtual_address(
        report, virtual_address, out_segment_index, out_artifact);
}

int machine_runtime_report_find_region_summary_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary) {
    if (!report) {
        return 0;
    }
    return machine_runtime_file_find_region_covering_virtual_address(
        &report->file, virtual_address, out_region_index, out_summary);
}

int machine_runtime_report_find_region_artifact_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact) {
    size_t region_index = 0u;

    if (!machine_runtime_report_find_region_summary_covering_virtual_address(
            report, virtual_address, &region_index, NULL)) {
        return 0;
    }
    if (out_region_index) {
        *out_region_index = region_index;
    }
    return !out_artifact || machine_runtime_report_get_region_artifact(report, region_index, out_artifact);
}

int machine_runtime_report_find_region_summary_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary) {
    if (!report) {
        return 0;
    }
    return machine_runtime_file_find_region_covering_memory_offset(
        &report->file, memory_offset, out_region_index, out_summary);
}

int machine_runtime_report_find_region_artifact_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact) {
    size_t region_index = 0u;

    if (!machine_runtime_report_find_region_summary_covering_memory_offset(
            report, memory_offset, &region_index, NULL)) {
        return 0;
    }
    if (out_region_index) {
        *out_region_index = region_index;
    }
    return !out_artifact || machine_runtime_report_get_region_artifact(report, region_index, out_artifact);
}

int machine_runtime_report_get_segment_filter_count(const MachineRuntimeReport *report,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t *out_count) {
    if (!machine_runtime_report_has_segment_tables(report) || !out_count) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE:
            *out_count = report->executable_segment_count;
            return 1;
        case MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE:
            *out_count = report->non_executable_segment_count;
            return 1;
        case MACHINE_RUNTIME_SEGMENT_FILTER_ENTRY:
            *out_count = report->segment_summary_count > 0u ? 1u : 0u;
            return 1;
        case MACHINE_RUNTIME_SEGMENT_FILTER_STACK:
            *out_count = report->has_stack_segment ? 1u : 0u;
            return 1;
    }
    return 0;
}

int machine_runtime_report_get_segment_summary_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    size_t segment_index;

    if (!machine_runtime_report_has_segment_tables(report)) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE:
            if (filtered_index >= report->executable_segment_count ||
                (report->executable_segment_count > 0u && !report->executable_segment_indices)) {
                return 0;
            }
            segment_index = report->executable_segment_indices[filtered_index];
            break;
        case MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE:
            if (filtered_index >= report->non_executable_segment_count ||
                (report->non_executable_segment_count > 0u && !report->non_executable_segment_indices)) {
                return 0;
            }
            segment_index = report->non_executable_segment_indices[filtered_index];
            break;
        case MACHINE_RUNTIME_SEGMENT_FILTER_ENTRY:
            if (filtered_index != 0u ||
                !machine_runtime_report_get_entry_segment_summary_artifact(report, &segment_index, out_summary)) {
                return 0;
            }
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            return 1;
        case MACHINE_RUNTIME_SEGMENT_FILTER_STACK:
            if (filtered_index != 0u ||
                !machine_runtime_report_get_stack_segment_summary_artifact(report, &segment_index, out_summary)) {
                return 0;
            }
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            return 1;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    if (out_summary) {
        *out_summary = &report->segment_summaries[segment_index];
    }
    return 1;
}

int machine_runtime_report_get_segment_artifact_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact) {
    size_t segment_index = 0u;

    if (!machine_runtime_report_get_segment_summary_by_filter_index(
            report, filter_kind, filtered_index, &segment_index, NULL)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return !out_artifact || machine_runtime_report_get_segment_artifact(report, segment_index, out_artifact);
}

int machine_runtime_report_get_region_filter_count(const MachineRuntimeReport *report,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t *out_count) {
    MachineRuntimeGapSummary gap_summary;
    size_t count = 0u;
    size_t segment_index;

    memset(&gap_summary, 0, sizeof(gap_summary));
    if (!machine_runtime_report_has_segment_tables(report) || !out_count) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_RUNTIME_REGION_FILTER_LOAD:
            for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
                if (report->segment_summaries[segment_index].kind == MACHINE_RUNTIME_SEGMENT_KIND_LOAD) {
                    ++count;
                }
            }
            *out_count = count;
            return 1;
        case MACHINE_RUNTIME_REGION_FILTER_GAP:
            *out_count = machine_runtime_file_get_gap_summary(&report->file, &gap_summary) ? 1u : 0u;
            return 1;
        case MACHINE_RUNTIME_REGION_FILTER_STACK:
            *out_count = report->has_stack_segment ? 1u : 0u;
            return 1;
    }
    return 0;
}

int machine_runtime_report_get_region_summary_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary) {
    size_t region_index;
    size_t current_index;
    MachineRuntimeGapSummary gap_summary;

    memset(&gap_summary, 0, sizeof(gap_summary));
    if (!machine_runtime_report_has_segment_tables(report)) {
        return 0;
    }
    switch (filter_kind) {
        case MACHINE_RUNTIME_REGION_FILTER_LOAD:
            current_index = 0u;
            for (region_index = 0u; region_index < report->segment_summary_count; ++region_index) {
                if (report->segment_summaries[region_index].kind != MACHINE_RUNTIME_SEGMENT_KIND_LOAD) {
                    continue;
                }
                if (current_index == filtered_index) {
                    if (out_region_index) {
                        *out_region_index = region_index;
                    }
                    return !out_summary || machine_runtime_report_get_region_summary(report, region_index, out_summary);
                }
                ++current_index;
            }
            return 0;
        case MACHINE_RUNTIME_REGION_FILTER_GAP:
            if (filtered_index != 0u ||
                !machine_runtime_file_get_gap_summary(&report->file, &gap_summary)) {
                return 0;
            }
            region_index = report->segment_summary_count;
            break;
        case MACHINE_RUNTIME_REGION_FILTER_STACK:
            if (filtered_index != 0u || !report->has_stack_segment) {
                return 0;
            }
            region_index = report->stack_segment_index;
            break;
        default:
            return 0;
    }
    if (out_region_index) {
        *out_region_index = region_index;
    }
    return !out_summary || machine_runtime_report_get_region_summary(report, region_index, out_summary);
}

int machine_runtime_report_get_region_artifact_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact) {
    size_t region_index = 0u;

    if (!machine_runtime_report_get_region_summary_by_filter_index(
            report, filter_kind, filtered_index, &region_index, NULL)) {
        return 0;
    }
    if (out_region_index) {
        *out_region_index = region_index;
    }
    return !out_artifact || machine_runtime_report_get_region_artifact(report, region_index, out_artifact);
}

int machine_runtime_report_overview_artifact_get_segment_summary(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_runtime_report_get_segment_summary_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_segment_index, out_summary);
}

int machine_runtime_report_overview_artifact_get_segment_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_runtime_report_get_segment_artifact_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_segment_index, out_artifact);
}

int machine_runtime_report_overview_artifact_get_stack_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeStackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->stack_summary;
    return 1;
}

int machine_runtime_report_overview_artifact_get_gap_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeGapSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->gap_summary;
    return 1;
}

int machine_runtime_report_overview_artifact_get_launch_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->launch_summary;
    return 1;
}

int machine_runtime_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary) {
    if (!artifact || !out_summary) {
        return 0;
    }
    *out_summary = artifact->initial_stack_summary;
    return 1;
}

int machine_runtime_report_overview_artifact_get_stack_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportStackArtifact *out_artifact) {
    if (!artifact || !artifact->report || !out_artifact) {
        return 0;
    }
    return machine_runtime_report_get_stack_artifact(artifact->report, out_artifact);
}

int machine_runtime_report_overview_artifact_get_gap_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportGapArtifact *out_artifact) {
    if (!artifact || !artifact->report || !out_artifact) {
        return 0;
    }
    return machine_runtime_report_get_gap_artifact(artifact->report, out_artifact);
}

int machine_runtime_report_overview_artifact_get_launch_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportLaunchArtifact *out_artifact) {
    if (!artifact || !artifact->report || !out_artifact) {
        return 0;
    }
    return machine_runtime_report_get_launch_artifact(artifact->report, out_artifact);
}

int machine_runtime_report_overview_artifact_get_initial_stack_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportInitialStackArtifact *out_artifact) {
    if (!artifact || !artifact->report || !out_artifact) {
        return 0;
    }
    return machine_runtime_report_get_initial_stack_artifact(artifact->report, out_artifact);
}

int machine_runtime_report_overview_artifact_get_region_summary(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_runtime_report_get_region_summary_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_region_index, out_summary);
}

int machine_runtime_report_overview_artifact_get_region_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_runtime_report_get_region_artifact_by_filter_index(
        artifact->report, filter_kind, filtered_index, out_region_index, out_artifact);
}

int machine_runtime_report_get_executable_segment_count(const MachineRuntimeReport *report,
    size_t *out_count) {
    return machine_runtime_report_get_segment_filter_count(
        report, MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE, out_count);
}

int machine_runtime_report_get_executable_segment_summary_by_index(const MachineRuntimeReport *report,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary) {
    return machine_runtime_report_get_segment_summary_by_filter_index(
        report, MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE, executable_index, out_segment_index, out_summary);
}

int machine_runtime_dump_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeStringBuilder builder;
    MachineRuntimeReportOverviewArtifact overview_artifact;
    size_t segment_index;

    memset(&builder, 0, sizeof(builder));
    memset(&overview_artifact, 0, sizeof(overview_artifact));
    if (!report || !out_text ||
        !machine_runtime_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_runtime_append_format(
            &builder,
            "machine_runtime profile=%s elf_origin=%s elf_semantics=%s base=0x%zx entry=0x%zx sp=0x%zx segments=%zu mapped_bytes=%zu\n",
            machine_elf_target_profile_name(report->header_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics),
            report->header_summary.base_virtual_address,
            report->header_summary.entry_virtual_address,
            report->header_summary.initial_stack_pointer,
            report->header_summary.segment_count,
            report->header_summary.mapped_byte_count) ||
        !machine_runtime_append_format(
            &builder,
            "entry_segment: rseg.%zu %s perms=%c%c%c\n",
            overview_artifact.entry_segment_index,
            overview_artifact.entry_segment_summary ? overview_artifact.entry_segment_summary->name : "",
            overview_artifact.entry_segment_summary && overview_artifact.entry_segment_summary->readable ? 'r' : '-',
            overview_artifact.entry_segment_summary && overview_artifact.entry_segment_summary->writable ? 'w' : '-',
            overview_artifact.entry_segment_summary && overview_artifact.entry_segment_summary->executable ? 'x' : '-') ||
        !machine_runtime_append_format(
            &builder,
            "stack_segment: rseg.%zu %s perms=%c%c%c\n",
            overview_artifact.stack_segment_index,
            overview_artifact.stack_segment_summary ? overview_artifact.stack_segment_summary->name : "",
            overview_artifact.stack_segment_summary && overview_artifact.stack_segment_summary->readable ? 'r' : '-',
            overview_artifact.stack_segment_summary && overview_artifact.stack_segment_summary->writable ? 'w' : '-',
            overview_artifact.stack_segment_summary && overview_artifact.stack_segment_summary->executable ? 'x' : '-') ||
        !machine_runtime_append_format(&builder, "segments:\n")) {
        free(builder.data);
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-152: out of memory building runtime report dump");
        return 0;
    }

    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        const MachineRuntimeSegmentSummary *summary = &report->segment_summaries[segment_index];

        if (!machine_runtime_append_format(
                &builder,
                "  rseg.%zu %s kind=%s vaddr=0x%zx bytes=%zu perms=%c%c%c\n",
                segment_index,
                summary->name ? summary->name : "",
                machine_runtime_segment_kind_name(summary->kind),
                summary->virtual_address,
                summary->byte_count,
                summary->readable ? 'r' : '-',
                summary->writable ? 'w' : '-',
                summary->executable ? 'x' : '-')) {
            free(builder.data);
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-153: out of memory appending runtime report segment");
            return 0;
        }
    }

    if (!machine_runtime_append_format(&builder, "report_overview:\n") ||
        !machine_runtime_append_format(
            &builder,
            "  segments=%zu mapped_bytes=%zu executable_segments=%zu non_executable_segments=%zu entry=0x%zx sp=0x%zx stack_segment=%zu\n",
            report->header_summary.segment_count,
            report->header_summary.mapped_byte_count,
            report->executable_segment_count,
            report->non_executable_segment_count,
            report->header_summary.entry_virtual_address,
            report->header_summary.initial_stack_pointer,
            report->stack_segment_index) ||
        !machine_runtime_append_format(
            &builder,
            "  elf_source: target=%s origin=%s semantics=%s\n",
            machine_elf_target_profile_name(
                report->file.load_file.exec_file.image_file.source_elf_artifact_summary.target_profile),
            machine_elf_target_profile_name(
                report->file.load_file.exec_file.image_file.source_elf_artifact_summary.origin_profile),
            machine_elf_relocation_semantics_name(
                report->file.load_file.exec_file.image_file.source_elf_artifact_summary.relocation_semantics)) ||
        !machine_runtime_append_format(
            &builder,
            "  policy: profile=%s base=0x%zx stack-align=%zu stack-size=%zu stack-gap=%zu\n",
            machine_elf_target_profile_name(report->target_policy_summary.target_profile),
            report->target_policy_summary.base_virtual_address,
            report->target_policy_summary.stack_alignment,
            report->target_policy_summary.stack_byte_count,
            report->target_policy_summary.stack_gap_byte_count) ||
        !machine_runtime_append_format(
            &builder,
            "  memory: base=0x%zx end=0x%zx span-bytes=%zu mapped-bytes=%zu entry-offset=0x%zx sp-offset=0x%zx\n",
            report->memory_summary.base_virtual_address,
            report->memory_summary.end_virtual_address,
            report->memory_summary.span_byte_count,
            report->memory_summary.mapped_byte_count,
            report->memory_summary.entry_offset,
            report->memory_summary.initial_stack_pointer_offset) ||
        !machine_runtime_append_format(
            &builder,
            "  stack: segment=%zu base=0x%zx end=0x%zx bytes=%zu sp=0x%zx sp-offset=0x%zx\n",
            report->stack_summary.stack_segment_index,
            report->stack_summary.base_virtual_address,
            report->stack_summary.end_virtual_address,
            report->stack_summary.byte_count,
            report->stack_summary.initial_stack_pointer,
            report->stack_summary.initial_stack_pointer_offset) ||
        !machine_runtime_append_format(
            &builder,
            "  gap: base=0x%zx end=0x%zx bytes=%zu\n",
            report->gap_summary.base_virtual_address,
            report->gap_summary.end_virtual_address,
            report->gap_summary.byte_count) ||
        !machine_runtime_append_format(
            &builder,
            "  launch: pc=0x%zx sp=0x%zx stack-segment=%zu stack-base=0x%zx stack-end=0x%zx stack-bytes=%zu\n",
            report->launch_summary.entry_virtual_address,
            report->launch_summary.initial_stack_pointer,
            report->launch_summary.stack_segment_index,
            report->launch_summary.stack_base_virtual_address,
            report->launch_summary.stack_end_virtual_address,
            report->launch_summary.stack_byte_count) ||
        !machine_runtime_append_format(
            &builder,
            "  initial-stack: base=0x%zx end=0x%zx bytes=%zu word=%zu argc=%zu argv-null=0x%zx env-null=0x%zx auxv-null=0x%zx\n",
            report->initial_stack_summary.image_base_virtual_address,
            report->initial_stack_summary.image_end_virtual_address,
            report->initial_stack_summary.image_byte_count,
            report->initial_stack_summary.word_byte_count,
            report->initial_stack_summary.argc,
            report->initial_stack_summary.argv_terminator_virtual_address,
            report->initial_stack_summary.envp_terminator_virtual_address,
            report->initial_stack_summary.auxv_terminator_virtual_address) ||
        !machine_runtime_append_format(&builder, "report_segment_filters:\n")) {
        free(builder.data);
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-154: out of memory appending runtime report overview");
        return 0;
    }

    {
        MachineRuntimeReportSegmentFilterKind filter_kinds[4] = {
            MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE,
            MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE,
            MACHINE_RUNTIME_SEGMENT_FILTER_ENTRY,
            MACHINE_RUNTIME_SEGMENT_FILTER_STACK
        };
        size_t filter_kind_index;

        for (filter_kind_index = 0u; filter_kind_index < 4u; ++filter_kind_index) {
            MachineRuntimeReportSegmentFilterKind filter_kind = filter_kinds[filter_kind_index];
            size_t count = 0u;
            size_t filtered_index;

            if (!machine_runtime_report_get_segment_filter_count(report, filter_kind, &count) ||
                !machine_runtime_append_format(
                    &builder,
                    "  %s[%zu]",
                    machine_runtime_segment_filter_kind_name(filter_kind),
                    count)) {
                free(builder.data);
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-155: failed appending runtime filter header");
                return 0;
            }
            for (filtered_index = 0u; filtered_index < count; ++filtered_index) {
                size_t filtered_segment_index = 0u;
                const MachineRuntimeSegmentSummary *filtered_summary = NULL;

                if (!machine_runtime_report_get_segment_summary_by_filter_index(
                        report, filter_kind, filtered_index, &filtered_segment_index, &filtered_summary) ||
                    !machine_runtime_append_format(
                        &builder,
                        "%s rseg.%zu(%s)",
                        filtered_index == 0u ? "" : ",",
                        filtered_segment_index,
                        filtered_summary && filtered_summary->name ? filtered_summary->name : "")) {
                    free(builder.data);
                    machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-156: failed appending runtime filter item");
                    return 0;
                }
            }
            if (!machine_runtime_append_format(&builder, "\n")) {
                free(builder.data);
                machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-157: failed terminating runtime filter line");
                return 0;
            }
        }
    }

    if (!machine_runtime_append_format(&builder, "report_segments:\n")) {
        free(builder.data);
        machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-158: failed appending runtime segments header");
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        MachineRuntimeReportSegmentArtifact segment_artifact;

        memset(&segment_artifact, 0, sizeof(segment_artifact));
        if (!machine_runtime_report_get_segment_artifact(report, segment_index, &segment_artifact) ||
            !machine_runtime_append_format(
                &builder,
                "  rseg.%zu %s kind=%s entry=%s stack=%s executable=%s bytes=%zu perms=%c%c%c\n",
                segment_index,
                segment_artifact.segment_summary && segment_artifact.segment_summary->name
                    ? segment_artifact.segment_summary->name
                    : "",
                segment_artifact.segment_summary
                    ? machine_runtime_segment_kind_name(segment_artifact.segment_summary->kind)
                    : "unknown",
                segment_artifact.is_entry_segment ? "yes" : "no",
                segment_artifact.is_stack_segment ? "yes" : "no",
                segment_artifact.is_executable_segment ? "yes" : "no",
                segment_artifact.segment_summary ? segment_artifact.segment_summary->byte_count : 0u,
                segment_artifact.segment_summary && segment_artifact.segment_summary->readable ? 'r' : '-',
                segment_artifact.segment_summary && segment_artifact.segment_summary->writable ? 'w' : '-',
                segment_artifact.segment_summary && segment_artifact.segment_summary->executable ? 'x' : '-')) {
            free(builder.data);
            machine_runtime_set_error(error, 0, 0, "MACHINE-RUNTIME-159: failed appending runtime report artifact");
            return 0;
        }
    }

    *out_text = builder.data;
    return 1;
}

int machine_runtime_build_report_dump_from_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_file(source, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

#define MACHINE_RUNTIME_DEFINE_REPORT_DUMP_WRAPPER(FN_NAME, BUILD_REPORT_CALL) \
int FN_NAME(const MachineLoadFile *source, char **out_text, MachineRuntimeError *error); /* suppress */

int machine_runtime_build_report_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_load_file(source, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_load_report(report, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_exec_file(source, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_exec_report(report, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_image_file(source, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_image_report(report, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_elf_file(source, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_elf_file_with_profile(source, profile, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_elf_report(report, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_elf_report_with_profile(report, profile, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_elf_bytes(bytes, byte_count, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport report;
    int ok;

    machine_runtime_report_init(&report);
    ok = machine_runtime_build_report_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &report, error) &&
        machine_runtime_dump_report(&report, out_text, error);
    machine_runtime_report_free(&report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_ir_report(report, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

int machine_runtime_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error) {
    MachineRuntimeReport runtime_report;
    int ok;

    machine_runtime_report_init(&runtime_report);
    ok = machine_runtime_build_report_from_machine_ir_report_with_profile(report, profile, &runtime_report, error) &&
        machine_runtime_dump_report(&runtime_report, out_text, error);
    machine_runtime_report_free(&runtime_report);
    return ok;
}

#endif
