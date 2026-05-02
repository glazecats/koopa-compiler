#if defined(__INTELLISENSE__) && !defined(MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IMAGE_SPLIT_AGGREGATOR)
#define MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_image.c"
#else

int machine_image_dump_file(const MachineImageFile *image_file,
    char **out_text,
    MachineImageError *error) {
    MachineImageStringBuilder builder;
    const MachineImageSymbol *entry_symbol = NULL;
    size_t segment_index;
    size_t symbol_index;
    size_t relocation_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!image_file || !out_text) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-128: invalid image dump contract");
        return 0;
    }
    if (!machine_image_verify_file(image_file, error)) {
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (image_file->has_entry) {
        entry_symbol = &image_file->symbols[image_file->entry_symbol_index];
    }
    if (!machine_image_append_format(
            &builder,
            "machine_image profile=%s base=0x%zx bytes=%zu segments=%zu symbols=%zu relocations=%zu entry=%s\n",
            machine_elf_target_profile_name(image_file->target_profile),
            image_file->base_virtual_address,
            image_file->byte_count,
            image_file->segment_count,
            image_file->symbol_count,
            image_file->relocation_count,
            image_file->has_entry && entry_symbol && entry_symbol->name
                ? entry_symbol->name
                : "-")) {
        goto cleanup;
    }
    if (image_file->has_entry && entry_symbol) {
        if (!machine_image_append_format(&builder, "entry_address: 0x%zx\n", image_file->entry_virtual_address)) {
            goto cleanup;
        }
    }
    if (!machine_image_append_format(&builder, "segments:\n")) {
        goto cleanup;
    }
    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        const MachineImageSegment *segment = &image_file->segments[segment_index];

        if (!machine_image_append_format(
                &builder,
                "  iseg.%zu %s img@%zu vaddr=0x%zx bytes=%zu align=%zu\n",
                segment_index,
                segment->name ? segment->name : "<null>",
                segment->image_offset,
                segment->virtual_address,
                segment->byte_count,
                segment->align)) {
            goto cleanup;
        }
    }
    if (!machine_image_append_format(&builder, "symbols:\n")) {
        goto cleanup;
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        const MachineImageSymbol *symbol = &image_file->symbols[symbol_index];

        if (symbol->is_defined) {
            if (!machine_image_append_format(
                    &builder,
                    "  isym.%zu %s bind=%s type=%u defined=%d seg=%zu off=0x%zx vaddr=0x%zx\n",
                    symbol_index,
                    symbol->name ? symbol->name : "<null>",
                    symbol->binding == MACHINE_ELF_SYMBOL_LOCAL ? "local" : "global",
                    (unsigned)symbol->type,
                    symbol->is_defined,
                    symbol->segment_index,
                    symbol->value_offset,
                    symbol->virtual_address)) {
                goto cleanup;
            }
        } else {
            if (!machine_image_append_format(
                    &builder,
                    "  isym.%zu %s bind=%s type=%u defined=%d seg=- off=- vaddr=-\n",
                    symbol_index,
                    symbol->name ? symbol->name : "<null>",
                    symbol->binding == MACHINE_ELF_SYMBOL_LOCAL ? "local" : "global",
                    (unsigned)symbol->type,
                    symbol->is_defined)) {
                goto cleanup;
            }
        }
    }
    if (!machine_image_append_format(&builder, "relocations:\n")) {
        goto cleanup;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->is_resolved) {
            if (!machine_image_append_format(
                    &builder,
                    "  irel.%zu kind=%s seg=%zu off=0x%x site=0x%zx resolved=yes target=0x%zx sym=%zu type=%u name=%s\n",
                    relocation_index,
                    machine_image_relocation_kind_name(relocation->source_kind),
                    relocation->segment_index,
                    relocation->segment_offset,
                    relocation->site_virtual_address,
                    relocation->target_virtual_address,
                    relocation->symbol_index,
                    relocation->type,
                    relocation->symbol_name ? relocation->symbol_name : "<null>")) {
                goto cleanup;
            }
        } else {
            if (!machine_image_append_format(
                    &builder,
                    "  irel.%zu kind=%s seg=%zu off=0x%x site=0x%zx resolved=no target=- sym=%zu type=%u name=%s\n",
                    relocation_index,
                    machine_image_relocation_kind_name(relocation->source_kind),
                    relocation->segment_index,
                    relocation->segment_offset,
                    relocation->site_virtual_address,
                    relocation->symbol_index,
                    relocation->type,
                    relocation->symbol_name ? relocation->symbol_name : "<null>")) {
                goto cleanup;
            }
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    return ok;
}

int machine_image_build_dump_from_file(const MachineImageFile *source,
    char **out_text,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_clone_file(source, &image_file, error) &&
        machine_image_dump_file(&image_file, out_text, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_file(source, &image_file, error) &&
        machine_image_dump_file(&image_file, out_text, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_report(report, &image_file, error) &&
        machine_image_dump_file(&image_file, out_text, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_report_get_summary(const MachineImageReport *report,
    size_t *out_segment_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_segment_count) {
        *out_segment_count = report->segment_summary_count;
    }
    if (out_symbol_count) {
        *out_symbol_count = report->symbol_summary_count;
    }
    if (out_relocation_count) {
        *out_relocation_count = report->relocation_summary_count;
    }
    if (out_byte_count) {
        *out_byte_count = report->file.byte_count;
    }
    return 1;
}

int machine_image_report_get_overview_artifact(const MachineImageReport *report,
    MachineImageReportOverviewArtifact *out_artifact) {
    if (out_artifact) {
        memset(out_artifact, 0, sizeof(*out_artifact));
    }
    if (!report || !out_artifact) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->header_summary = &report->header_summary;
    out_artifact->target_policy_summary = &report->target_policy_summary;
    out_artifact->segment_count = report->segment_summary_count;
    out_artifact->symbol_count = report->symbol_summary_count;
    out_artifact->relocation_count = report->relocation_summary_count;
    out_artifact->byte_count = report->file.byte_count;
    out_artifact->resolved_relocation_count = report->resolved_relocation_count;
    out_artifact->unresolved_relocation_count = report->unresolved_relocation_count;
    if (report->has_entry_symbol_summary) {
        out_artifact->has_entry_symbol_artifact =
            machine_image_report_get_entry_symbol_artifact(report, NULL, &out_artifact->entry_symbol_artifact);
    }
    return 1;
}

int machine_image_report_get_file(const MachineImageReport *report,
    const MachineImageFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_image_report_get_header_summary_artifact(const MachineImageReport *report,
    const MachineImageHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_image_report_get_target_policy_summary_artifact(const MachineImageReport *report,
    const MachineImageTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_image_report_get_entry_symbol_summary_artifact(const MachineImageReport *report,
    const MachineImageSymbolSummary **out_summary) {
    if (!report || !out_summary || !report->has_entry_symbol_summary) {
        return 0;
    }
    *out_summary = &report->entry_symbol_summary;
    return 1;
}

int machine_image_report_get_entry_symbol_artifact(const MachineImageReport *report,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact) {
    if (out_symbol_index) {
        *out_symbol_index = 0u;
    }
    if (!report || !report->has_entry_symbol_summary || !out_artifact) {
        return 0;
    }
    if (!machine_image_report_get_symbol_artifact(report, report->file.entry_symbol_index, out_artifact)) {
        return 0;
    }
    if (out_symbol_index) {
        *out_symbol_index = report->file.entry_symbol_index;
    }
    return 1;
}

int machine_image_report_get_segment_summary(const MachineImageReport *report,
    size_t segment_index,
    const MachineImageSegmentSummary **out_summary) {
    if (!report || !out_summary || !report->segment_summaries || segment_index >= report->segment_summary_count) {
        return 0;
    }
    *out_summary = &report->segment_summaries[segment_index];
    return 1;
}

int machine_image_report_get_segment_artifact(const MachineImageReport *report,
    size_t segment_index,
    MachineImageReportSegmentArtifact *out_artifact) {
    if (out_artifact) {
        memset(out_artifact, 0, sizeof(*out_artifact));
    }
    if (!report || !out_artifact || segment_index >= report->segment_summary_count ||
        !report->segment_summaries ||
        !report->segment_symbol_start_indices || !report->segment_symbol_counts ||
        !report->segment_relocation_start_indices || !report->segment_relocation_counts ||
        !report->segment_resolved_relocation_start_indices || !report->segment_resolved_relocation_counts ||
        !report->segment_unresolved_relocation_start_indices || !report->segment_unresolved_relocation_counts) {
        return 0;
    }

    out_artifact->report = report;
    out_artifact->segment_index = segment_index;
    out_artifact->segment_summary = &report->segment_summaries[segment_index];
    out_artifact->symbol_count = report->segment_symbol_counts[segment_index];
    out_artifact->symbol_indices = out_artifact->symbol_count > 0u
        ? report->segment_symbol_indices + report->segment_symbol_start_indices[segment_index]
        : NULL;
    out_artifact->relocation_count = report->segment_relocation_counts[segment_index];
    out_artifact->relocation_indices = out_artifact->relocation_count > 0u
        ? report->segment_relocation_indices + report->segment_relocation_start_indices[segment_index]
        : NULL;
    out_artifact->resolved_relocation_count = report->segment_resolved_relocation_counts[segment_index];
    out_artifact->resolved_relocation_indices = out_artifact->resolved_relocation_count > 0u
        ? report->segment_resolved_relocation_indices +
            report->segment_resolved_relocation_start_indices[segment_index]
        : NULL;
    out_artifact->unresolved_relocation_count = report->segment_unresolved_relocation_counts[segment_index];
    out_artifact->unresolved_relocation_indices = out_artifact->unresolved_relocation_count > 0u
        ? report->segment_unresolved_relocation_indices +
            report->segment_unresolved_relocation_start_indices[segment_index]
        : NULL;
    return 1;
}

int machine_image_report_find_segment_summary_by_name(const MachineImageReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineImageSegmentSummary **out_summary) {
    size_t segment_index;

    if (!report || !segment_name || !report->segment_summaries) {
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

int machine_image_report_find_segment_artifact_by_name(const MachineImageReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineImageReportSegmentArtifact *out_artifact) {
    size_t segment_index;

    if (out_segment_index) {
        *out_segment_index = 0u;
    }
    if (!report || !segment_name || !out_artifact) {
        return 0;
    }
    if (!machine_image_report_find_segment_summary_by_name(report, segment_name, &segment_index, NULL) ||
        !machine_image_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_image_report_find_segment_summary_covering_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineImageSegmentSummary **out_summary) {
    size_t segment_index;

    if (!report || !report->segment_summaries) {
        return 0;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        const MachineImageSegmentSummary *summary = &report->segment_summaries[segment_index];

        if (virtual_address >= summary->virtual_address &&
            virtual_address < summary->virtual_address + summary->byte_count) {
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

int machine_image_report_find_segment_artifact_covering_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineImageReportSegmentArtifact *out_artifact) {
    size_t segment_index;

    if (out_segment_index) {
        *out_segment_index = 0u;
    }
    if (!report || !out_artifact) {
        return 0;
    }
    if (!machine_image_report_find_segment_summary_covering_virtual_address(
            report,
            virtual_address,
            &segment_index,
            NULL) ||
        !machine_image_report_get_segment_artifact(report, segment_index, out_artifact)) {
        return 0;
    }
    if (out_segment_index) {
        *out_segment_index = segment_index;
    }
    return 1;
}

int machine_image_report_get_segment_symbol_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || segment_index >= report->segment_summary_count || !report->segment_symbol_counts) {
        return 0;
    }
    *out_count = report->segment_symbol_counts[segment_index];
    return 1;
}

int machine_image_report_segment_artifact_get_symbol_summary(const MachineImageReportSegmentArtifact *artifact,
    size_t segment_symbol_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    size_t symbol_index;

    if (out_symbol_index) {
        *out_symbol_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->segment_summary ||
        segment_symbol_index >= artifact->symbol_count || !artifact->symbol_indices || !out_summary) {
        return 0;
    }
    symbol_index = artifact->symbol_indices[segment_symbol_index];
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    *out_summary = &artifact->report->symbol_summaries[symbol_index];
    return 1;
}

int machine_image_report_get_symbol_summary(const MachineImageReport *report,
    size_t symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    if (!report || !out_summary || !report->symbol_summaries || symbol_index >= report->symbol_summary_count) {
        return 0;
    }
    *out_summary = &report->symbol_summaries[symbol_index];
    return 1;
}

int machine_image_report_get_symbol_artifact(const MachineImageReport *report,
    size_t symbol_index,
    MachineImageReportSymbolArtifact *out_artifact) {
    const MachineImageSymbolSummary *summary = NULL;

    if (out_artifact) {
        memset(out_artifact, 0, sizeof(*out_artifact));
    }
    if (!report || !out_artifact ||
        !machine_image_report_get_symbol_summary(report, symbol_index, &summary) || !summary) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->symbol_index = symbol_index;
    out_artifact->symbol_summary = summary;
    out_artifact->is_entry_symbol = report->has_entry_symbol_summary && report->file.entry_symbol_index == symbol_index;
    if (summary->is_defined && summary->segment_index < report->segment_summary_count) {
        out_artifact->has_segment_artifact =
            machine_image_report_get_segment_artifact(report, summary->segment_index, &out_artifact->segment_artifact);
    }
    if (report->symbol_relocation_start_indices && report->symbol_relocation_counts &&
        report->symbol_relocation_indices) {
        out_artifact->incoming_relocation_count = report->symbol_relocation_counts[symbol_index];
        out_artifact->incoming_relocation_indices = out_artifact->incoming_relocation_count > 0u
            ? report->symbol_relocation_indices + report->symbol_relocation_start_indices[symbol_index]
            : NULL;
    }
    if (report->symbol_resolved_relocation_start_indices && report->symbol_resolved_relocation_counts &&
        report->symbol_resolved_relocation_indices) {
        out_artifact->incoming_resolved_relocation_count = report->symbol_resolved_relocation_counts[symbol_index];
        out_artifact->incoming_resolved_relocation_indices = out_artifact->incoming_resolved_relocation_count > 0u
            ? report->symbol_resolved_relocation_indices +
                report->symbol_resolved_relocation_start_indices[symbol_index]
            : NULL;
    }
    if (report->symbol_unresolved_relocation_start_indices && report->symbol_unresolved_relocation_counts &&
        report->symbol_unresolved_relocation_indices) {
        out_artifact->incoming_unresolved_relocation_count = report->symbol_unresolved_relocation_counts[symbol_index];
        out_artifact->incoming_unresolved_relocation_indices = out_artifact->incoming_unresolved_relocation_count > 0u
            ? report->symbol_unresolved_relocation_indices +
                report->symbol_unresolved_relocation_start_indices[symbol_index]
            : NULL;
    }
    return 1;
}

int machine_image_report_get_symbol_filter_count(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t *out_count) {
    const size_t *indices = NULL;

    return machine_image_report_get_symbol_filter_storage(report, filter_kind, &indices, out_count);
}

int machine_image_report_get_symbol_relocation_summary_count(const MachineImageReport *report,
    size_t symbol_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || symbol_index >= report->symbol_summary_count || !report->symbol_relocation_counts) {
        return 0;
    }
    *out_count = report->symbol_relocation_counts[symbol_index];
    return 1;
}

int machine_image_report_get_symbol_resolved_relocation_summary_count(const MachineImageReport *report,
    size_t symbol_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || symbol_index >= report->symbol_summary_count ||
        !report->symbol_resolved_relocation_counts) {
        return 0;
    }
    *out_count = report->symbol_resolved_relocation_counts[symbol_index];
    return 1;
}

int machine_image_report_get_symbol_unresolved_relocation_summary_count(const MachineImageReport *report,
    size_t symbol_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || symbol_index >= report->symbol_summary_count ||
        !report->symbol_unresolved_relocation_counts) {
        return 0;
    }
    *out_count = report->symbol_unresolved_relocation_counts[symbol_index];
    return 1;
}

int machine_image_report_symbol_artifact_get_relocation_summary(
    const MachineImageReportSymbolArtifact *artifact,
    size_t symbol_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->symbol_summary ||
        symbol_relocation_index >= artifact->incoming_relocation_count ||
        !artifact->incoming_relocation_indices || !out_summary) {
        return 0;
    }
    relocation_index = artifact->incoming_relocation_indices[symbol_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    *out_summary = &artifact->report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_symbol_artifact_get_resolved_relocation_summary(
    const MachineImageReportSymbolArtifact *artifact,
    size_t symbol_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->symbol_summary ||
        symbol_resolved_relocation_index >= artifact->incoming_resolved_relocation_count ||
        !artifact->incoming_resolved_relocation_indices || !out_summary) {
        return 0;
    }
    relocation_index = artifact->incoming_resolved_relocation_indices[symbol_resolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    *out_summary = &artifact->report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_symbol_artifact_get_unresolved_relocation_summary(
    const MachineImageReportSymbolArtifact *artifact,
    size_t symbol_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->symbol_summary ||
        symbol_unresolved_relocation_index >= artifact->incoming_unresolved_relocation_count ||
        !artifact->incoming_unresolved_relocation_indices || !out_summary) {
        return 0;
    }
    relocation_index = artifact->incoming_unresolved_relocation_indices[symbol_unresolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    *out_summary = &artifact->report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_get_symbol_relocation_summary_by_index(const MachineImageReport *report,
    size_t symbol_index,
    size_t symbol_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t relocation_index;

    if (!report || symbol_index >= report->symbol_summary_count ||
        !report->symbol_relocation_start_indices ||
        !report->symbol_relocation_counts ||
        !report->symbol_relocation_indices) {
        return 0;
    }
    start_index = report->symbol_relocation_start_indices[symbol_index];
    count = report->symbol_relocation_counts[symbol_index];
    if (symbol_relocation_index >= count ||
        start_index + symbol_relocation_index >= report->symbol_relocation_index_count) {
        return 0;
    }
    relocation_index = report->symbol_relocation_indices[start_index + symbol_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_symbol_resolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t symbol_index,
    size_t symbol_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t relocation_index;

    if (!report || symbol_index >= report->symbol_summary_count ||
        !report->symbol_resolved_relocation_start_indices ||
        !report->symbol_resolved_relocation_counts ||
        !report->symbol_resolved_relocation_indices) {
        return 0;
    }
    start_index = report->symbol_resolved_relocation_start_indices[symbol_index];
    count = report->symbol_resolved_relocation_counts[symbol_index];
    if (symbol_resolved_relocation_index >= count ||
        start_index + symbol_resolved_relocation_index >= report->symbol_resolved_relocation_index_count) {
        return 0;
    }
    relocation_index = report->symbol_resolved_relocation_indices[start_index + symbol_resolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_symbol_unresolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t symbol_index,
    size_t symbol_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t relocation_index;

    if (!report || symbol_index >= report->symbol_summary_count ||
        !report->symbol_unresolved_relocation_start_indices ||
        !report->symbol_unresolved_relocation_counts ||
        !report->symbol_unresolved_relocation_indices) {
        return 0;
    }
    start_index = report->symbol_unresolved_relocation_start_indices[symbol_index];
    count = report->symbol_unresolved_relocation_counts[symbol_index];
    if (symbol_unresolved_relocation_index >= count ||
        start_index + symbol_unresolved_relocation_index >= report->symbol_unresolved_relocation_index_count) {
        return 0;
    }
    relocation_index = report->symbol_unresolved_relocation_indices[start_index + symbol_unresolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_symbol_summary_by_filter_index(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    const size_t *indices = NULL;
    size_t count = 0u;
    size_t symbol_index;

    if (!machine_image_report_get_symbol_filter_storage(report, filter_kind, &indices, &count) ||
        filtered_index >= count || !indices) {
        return 0;
    }
    symbol_index = indices[filtered_index];
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    if (out_summary) {
        return machine_image_report_get_symbol_summary(report, symbol_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_symbol_artifact_by_filter_index(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact) {
    size_t symbol_index;

    if (!machine_image_report_get_symbol_summary_by_filter_index(
            report,
            filter_kind,
            filtered_index,
            &symbol_index,
            NULL) ||
        !machine_image_report_get_symbol_artifact(report, symbol_index, out_artifact)) {
        return 0;
    }
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    return 1;
}

int machine_image_report_overview_artifact_get_symbol_summary(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_image_report_get_symbol_summary_by_filter_index(
        artifact->report,
        filter_kind,
        filtered_index,
        out_symbol_index,
        out_summary);
}

int machine_image_report_overview_artifact_get_symbol_artifact(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_image_report_get_symbol_artifact_by_filter_index(
        artifact->report,
        filter_kind,
        filtered_index,
        out_symbol_index,
        out_artifact);
}

int machine_image_report_overview_artifact_get_relocation_summary(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_image_report_get_relocation_summary_by_filter_index(
        artifact->report,
        filter_kind,
        filtered_index,
        out_relocation_index,
        out_summary);
}

int machine_image_report_overview_artifact_get_relocation_artifact(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact) {
    if (!artifact || !artifact->report) {
        return 0;
    }
    return machine_image_report_get_relocation_artifact_by_filter_index(
        artifact->report,
        filter_kind,
        filtered_index,
        out_relocation_index,
        out_artifact);
}

int machine_image_report_get_segment_symbol_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_symbol_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t symbol_index;

    if (!report || segment_index >= report->segment_summary_count ||
        !report->segment_symbol_start_indices ||
        !report->segment_symbol_counts ||
        !report->segment_symbol_indices) {
        return 0;
    }
    start_index = report->segment_symbol_start_indices[segment_index];
    count = report->segment_symbol_counts[segment_index];
    if (segment_symbol_index >= count || start_index + segment_symbol_index >= report->segment_symbol_index_count) {
        return 0;
    }
    symbol_index = report->segment_symbol_indices[start_index + segment_symbol_index];
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    if (out_summary) {
        return machine_image_report_get_symbol_summary(report, symbol_index, out_summary);
    }
    return 1;
}

int machine_image_report_find_symbol_summary_by_name(const MachineImageReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    size_t symbol_index;

    if (!report || !symbol_name || !report->symbol_summaries) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < report->symbol_summary_count; ++symbol_index) {
        if (report->symbol_summaries[symbol_index].name &&
            strcmp(report->symbol_summaries[symbol_index].name, symbol_name) == 0) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_summary) {
                *out_summary = &report->symbol_summaries[symbol_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_report_find_symbol_artifact_by_name(const MachineImageReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact) {
    size_t symbol_index;

    if (out_symbol_index) {
        *out_symbol_index = 0u;
    }
    if (!report || !symbol_name || !out_artifact ||
        !machine_image_report_find_symbol_summary_by_name(report, symbol_name, &symbol_index, NULL) ||
        !machine_image_report_get_symbol_artifact(report, symbol_index, out_artifact)) {
        return 0;
    }
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    return 1;
}

int machine_image_report_find_symbol_summary_by_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary) {
    size_t symbol_index;

    if (!report || !report->symbol_summaries) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < report->symbol_summary_count; ++symbol_index) {
        const MachineImageSymbolSummary *summary = &report->symbol_summaries[symbol_index];

        if (summary->is_defined && summary->virtual_address == virtual_address) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_summary) {
                *out_summary = summary;
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_report_find_symbol_artifact_by_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact) {
    size_t symbol_index;

    if (out_symbol_index) {
        *out_symbol_index = 0u;
    }
    if (!report || !out_artifact ||
        !machine_image_report_find_symbol_summary_by_virtual_address(report, virtual_address, &symbol_index, NULL) ||
        !machine_image_report_get_symbol_artifact(report, symbol_index, out_artifact)) {
        return 0;
    }
    if (out_symbol_index) {
        *out_symbol_index = symbol_index;
    }
    return 1;
}

int machine_image_report_get_relocation_summary(const MachineImageReport *report,
    size_t relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    if (!report || !out_summary || !report->relocation_summaries ||
        relocation_index >= report->relocation_summary_count) {
        return 0;
    }
    *out_summary = &report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_get_relocation_artifact(const MachineImageReport *report,
    size_t relocation_index,
    MachineImageReportRelocationArtifact *out_artifact) {
    const MachineImageRelocationSummary *summary = NULL;

    if (out_artifact) {
        memset(out_artifact, 0, sizeof(*out_artifact));
    }
    if (!report || !out_artifact ||
        !machine_image_report_get_relocation_summary(report, relocation_index, &summary) || !summary) {
        return 0;
    }
    out_artifact->report = report;
    out_artifact->relocation_index = relocation_index;
    out_artifact->relocation_summary = summary;
    if (!machine_image_report_get_segment_artifact(report, summary->segment_index, &out_artifact->source_segment_artifact)) {
        return 0;
    }
    if (summary->symbol_index < report->symbol_summary_count &&
        machine_image_report_get_symbol_summary(report, summary->symbol_index, &out_artifact->target_symbol_summary) &&
        out_artifact->target_symbol_summary) {
        out_artifact->has_target_symbol_summary = 1;
        out_artifact->target_symbol_index = summary->symbol_index;
        if (out_artifact->target_symbol_summary->is_defined &&
            out_artifact->target_symbol_summary->segment_index < report->segment_summary_count) {
            out_artifact->has_target_segment_artifact =
                machine_image_report_get_segment_artifact(
                    report,
                    out_artifact->target_symbol_summary->segment_index,
                    &out_artifact->target_segment_artifact);
        }
    }
    return 1;
}

int machine_image_report_get_relocation_filter_count(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t *out_count) {
    const size_t *indices = NULL;

    return machine_image_report_get_relocation_filter_storage(report, filter_kind, &indices, out_count);
}

int machine_image_report_get_segment_relocation_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || segment_index >= report->segment_summary_count ||
        !report->segment_relocation_counts) {
        return 0;
    }
    *out_count = report->segment_relocation_counts[segment_index];
    return 1;
}

int machine_image_report_segment_artifact_get_relocation_summary(const MachineImageReportSegmentArtifact *artifact,
    size_t segment_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->segment_summary ||
        segment_relocation_index >= artifact->relocation_count || !artifact->relocation_indices || !out_summary) {
        return 0;
    }
    relocation_index = artifact->relocation_indices[segment_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    *out_summary = &artifact->report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_get_segment_relocation_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t relocation_index;

    if (!report || segment_index >= report->segment_summary_count ||
        !report->segment_relocation_start_indices ||
        !report->segment_relocation_counts ||
        !report->segment_relocation_indices) {
        return 0;
    }
    start_index = report->segment_relocation_start_indices[segment_index];
    count = report->segment_relocation_counts[segment_index];
    if (segment_relocation_index >= count ||
        start_index + segment_relocation_index >= report->segment_relocation_index_count) {
        return 0;
    }
    relocation_index = report->segment_relocation_indices[start_index + segment_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_segment_resolved_relocation_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || segment_index >= report->segment_summary_count ||
        !report->segment_resolved_relocation_counts) {
        return 0;
    }
    *out_count = report->segment_resolved_relocation_counts[segment_index];
    return 1;
}

int machine_image_report_get_segment_unresolved_relocation_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count) {
    if (out_count) {
        *out_count = 0u;
    }
    if (!report || !out_count || segment_index >= report->segment_summary_count ||
        !report->segment_unresolved_relocation_counts) {
        return 0;
    }
    *out_count = report->segment_unresolved_relocation_counts[segment_index];
    return 1;
}

int machine_image_report_segment_artifact_get_resolved_relocation_summary(
    const MachineImageReportSegmentArtifact *artifact,
    size_t segment_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->segment_summary ||
        segment_resolved_relocation_index >= artifact->resolved_relocation_count ||
        !artifact->resolved_relocation_indices || !out_summary) {
        return 0;
    }
    relocation_index = artifact->resolved_relocation_indices[segment_resolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    *out_summary = &artifact->report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_segment_artifact_get_unresolved_relocation_summary(
    const MachineImageReportSegmentArtifact *artifact,
    size_t segment_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (out_summary) {
        *out_summary = NULL;
    }
    if (!artifact || !artifact->report || !artifact->segment_summary ||
        segment_unresolved_relocation_index >= artifact->unresolved_relocation_count ||
        !artifact->unresolved_relocation_indices || !out_summary) {
        return 0;
    }
    relocation_index = artifact->unresolved_relocation_indices[segment_unresolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    *out_summary = &artifact->report->relocation_summaries[relocation_index];
    return 1;
}

int machine_image_report_get_segment_resolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t relocation_index;

    if (!report || segment_index >= report->segment_summary_count ||
        !report->segment_resolved_relocation_start_indices ||
        !report->segment_resolved_relocation_counts ||
        !report->segment_resolved_relocation_indices) {
        return 0;
    }
    start_index = report->segment_resolved_relocation_start_indices[segment_index];
    count = report->segment_resolved_relocation_counts[segment_index];
    if (segment_resolved_relocation_index >= count ||
        start_index + segment_resolved_relocation_index >= report->segment_resolved_relocation_index_count) {
        return 0;
    }
    relocation_index = report->segment_resolved_relocation_indices[start_index + segment_resolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_segment_unresolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t start_index;
    size_t count;
    size_t relocation_index;

    if (!report || segment_index >= report->segment_summary_count ||
        !report->segment_unresolved_relocation_start_indices ||
        !report->segment_unresolved_relocation_counts ||
        !report->segment_unresolved_relocation_indices) {
        return 0;
    }
    start_index = report->segment_unresolved_relocation_start_indices[segment_index];
    count = report->segment_unresolved_relocation_counts[segment_index];
    if (segment_unresolved_relocation_index >= count ||
        start_index + segment_unresolved_relocation_index >= report->segment_unresolved_relocation_index_count) {
        return 0;
    }
    relocation_index =
        report->segment_unresolved_relocation_indices[start_index + segment_unresolved_relocation_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_find_relocation_summary_by_site_virtual_address(const MachineImageReport *report,
    size_t site_virtual_address,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (!report || !report->relocation_summaries) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < report->relocation_summary_count; ++relocation_index) {
        const MachineImageRelocationSummary *summary = &report->relocation_summaries[relocation_index];

        if (summary->site_virtual_address == site_virtual_address) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_summary) {
                *out_summary = summary;
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_report_find_relocation_artifact_by_site_virtual_address(const MachineImageReport *report,
    size_t site_virtual_address,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (!report || !out_artifact ||
        !machine_image_report_find_relocation_summary_by_site_virtual_address(
            report,
            site_virtual_address,
            &relocation_index,
            NULL) ||
        !machine_image_report_get_relocation_artifact(report, relocation_index, out_artifact)) {
        return 0;
    }
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    return 1;
}

int machine_image_report_get_relocation_summary_by_filter_index(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    const size_t *indices = NULL;
    size_t count = 0u;
    size_t relocation_index;

    if (!machine_image_report_get_relocation_filter_storage(report, filter_kind, &indices, &count) ||
        filtered_index >= count || !indices) {
        return 0;
    }
    relocation_index = indices[filtered_index];
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_relocation_artifact_by_filter_index(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact) {
    size_t relocation_index;

    if (!machine_image_report_get_relocation_summary_by_filter_index(
            report,
            filter_kind,
            filtered_index,
            &relocation_index,
            NULL) ||
        !machine_image_report_get_relocation_artifact(report, relocation_index, out_artifact)) {
        return 0;
    }
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    return 1;
}

int machine_image_report_get_resolved_relocation_count(const MachineImageReport *report,
    size_t *out_count) {
    if (!report || !out_count) {
        return 0;
    }
    *out_count = report->resolved_relocation_count;
    return 1;
}

int machine_image_report_get_resolved_relocation_index(const MachineImageReport *report,
    size_t resolved_index,
    size_t *out_relocation_index) {
    if (!report || !out_relocation_index || !report->resolved_relocation_indices ||
        resolved_index >= report->resolved_relocation_count) {
        return 0;
    }
    *out_relocation_index = report->resolved_relocation_indices[resolved_index];
    return 1;
}

int machine_image_report_get_resolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t resolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (!machine_image_report_get_resolved_relocation_index(report, resolved_index, &relocation_index)) {
        return 0;
    }
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_resolved_relocation_artifact_by_index(const MachineImageReport *report,
    size_t resolved_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (!report || !out_artifact ||
        !machine_image_report_get_resolved_relocation_index(report, resolved_index, &relocation_index) ||
        !machine_image_report_get_relocation_artifact(report, relocation_index, out_artifact)) {
        return 0;
    }
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    return 1;
}

int machine_image_report_get_unresolved_relocation_count(const MachineImageReport *report,
    size_t *out_count) {
    if (!report || !out_count) {
        return 0;
    }
    *out_count = report->unresolved_relocation_count;
    return 1;
}

int machine_image_report_get_unresolved_relocation_index(const MachineImageReport *report,
    size_t unresolved_index,
    size_t *out_relocation_index) {
    if (!report || !out_relocation_index || !report->unresolved_relocation_indices ||
        unresolved_index >= report->unresolved_relocation_count) {
        return 0;
    }
    *out_relocation_index = report->unresolved_relocation_indices[unresolved_index];
    return 1;
}

int machine_image_report_get_unresolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t unresolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary) {
    size_t relocation_index;

    if (!machine_image_report_get_unresolved_relocation_index(report, unresolved_index, &relocation_index)) {
        return 0;
    }
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    if (out_summary) {
        return machine_image_report_get_relocation_summary(report, relocation_index, out_summary);
    }
    return 1;
}

int machine_image_report_get_unresolved_relocation_artifact_by_index(const MachineImageReport *report,
    size_t unresolved_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact) {
    size_t relocation_index;

    if (out_relocation_index) {
        *out_relocation_index = 0u;
    }
    if (!report || !out_artifact ||
        !machine_image_report_get_unresolved_relocation_index(report, unresolved_index, &relocation_index) ||
        !machine_image_report_get_relocation_artifact(report, relocation_index, out_artifact)) {
        return 0;
    }
    if (out_relocation_index) {
        *out_relocation_index = relocation_index;
    }
    return 1;
}

int machine_image_dump_report(const MachineImageReport *report,
    char **out_text,
    MachineImageError *error) {
    MachineImageStringBuilder builder;
    MachineImageReportOverviewArtifact overview_artifact;
    char *raw_text = NULL;
    size_t segment_index;
    size_t filter_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!report || !out_text) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-141: invalid image report dump contract");
        return 0;
    }
    if (!machine_image_verify_file(&report->file, error) ||
        !machine_image_report_get_overview_artifact(report, &overview_artifact) ||
        !machine_image_dump_file(&report->file, &raw_text, error) ||
        !raw_text) {
        free(raw_text);
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_image_append_format(&builder, "%s", raw_text)) {
        goto cleanup;
    }
    if (!machine_image_append_format(
            &builder,
            "report_overview:\n"
            "  segments=%zu symbols=%zu relocations=%zu bytes=%zu resolved=%zu unresolved=%zu\n"
            "  policy: profile=%s base=0x%zx align=%zu\n",
            overview_artifact.segment_count,
            overview_artifact.symbol_count,
            overview_artifact.relocation_count,
            overview_artifact.byte_count,
            overview_artifact.resolved_relocation_count,
            overview_artifact.unresolved_relocation_count,
            machine_elf_target_profile_name(overview_artifact.target_policy_summary->target_profile),
            overview_artifact.target_policy_summary->base_virtual_address,
            overview_artifact.target_policy_summary->segment_alignment)) {
        goto cleanup;
    }
    if (overview_artifact.has_entry_symbol_artifact &&
        overview_artifact.entry_symbol_artifact.symbol_summary &&
        !machine_image_append_format(
            &builder,
            "  entry_artifact: isym.%zu %s seg=%s incoming=%zu\n",
            overview_artifact.entry_symbol_artifact.symbol_index,
            overview_artifact.entry_symbol_artifact.symbol_summary->name
                ? overview_artifact.entry_symbol_artifact.symbol_summary->name
                : "<null>",
            overview_artifact.entry_symbol_artifact.has_segment_artifact &&
                    overview_artifact.entry_symbol_artifact.segment_artifact.segment_summary &&
                    overview_artifact.entry_symbol_artifact.segment_artifact.segment_summary->name
                ? overview_artifact.entry_symbol_artifact.segment_artifact.segment_summary->name
                : "-",
            overview_artifact.entry_symbol_artifact.incoming_relocation_count)) {
        goto cleanup;
    }

    if (!machine_image_append_format(&builder, "report_symbol_filters:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index <= (size_t)MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED_GLOBAL; ++filter_index) {
        size_t count = 0u;
        size_t sample_symbol_index = 0u;
        const MachineImageSymbolSummary *sample_symbol_summary = NULL;

        if (!machine_image_report_get_symbol_filter_count(
                report,
                (MachineImageReportSymbolFilterKind)filter_index,
                &count)) {
            goto cleanup;
        }
        if (!machine_image_append_format(
                &builder,
                "  %s count=%zu",
                machine_image_symbol_filter_kind_name((MachineImageReportSymbolFilterKind)filter_index),
                count)) {
            goto cleanup;
        }
        if (count > 0u &&
            !machine_image_report_get_symbol_summary_by_filter_index(
                report,
                (MachineImageReportSymbolFilterKind)filter_index,
                0u,
                &sample_symbol_index,
                &sample_symbol_summary)) {
            goto cleanup;
        }
        if (count > 0u) {
            if (!machine_image_append_format(
                    &builder,
                    " first=isym.%zu:%s",
                    sample_symbol_index,
                    sample_symbol_summary && sample_symbol_summary->name
                        ? sample_symbol_summary->name
                        : "<null>")) {
                goto cleanup;
            }
        }
        if (!machine_image_append_format(&builder, "\n")) {
            goto cleanup;
        }
    }

    if (!machine_image_append_format(&builder, "report_relocation_filters:\n")) {
        goto cleanup;
    }
    for (filter_index = 0u; filter_index <= (size_t)MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_SECONDARY; ++filter_index) {
        size_t count = 0u;
        size_t sample_relocation_index = 0u;
        const MachineImageRelocationSummary *sample_relocation_summary = NULL;

        if (!machine_image_report_get_relocation_filter_count(
                report,
                (MachineImageReportRelocationFilterKind)filter_index,
                &count)) {
            goto cleanup;
        }
        if (!machine_image_append_format(
                &builder,
                "  %s count=%zu",
                machine_image_relocation_filter_kind_name((MachineImageReportRelocationFilterKind)filter_index),
                count)) {
            goto cleanup;
        }
        if (count > 0u &&
            !machine_image_report_get_relocation_summary_by_filter_index(
                report,
                (MachineImageReportRelocationFilterKind)filter_index,
                0u,
                &sample_relocation_index,
                &sample_relocation_summary)) {
            goto cleanup;
        }
        if (count > 0u) {
            if (!machine_image_append_format(
                    &builder,
                    " first=irel.%zu:%s",
                    sample_relocation_index,
                    sample_relocation_summary && sample_relocation_summary->symbol_name
                        ? sample_relocation_summary->symbol_name
                        : "<null>")) {
                goto cleanup;
            }
        }
        if (!machine_image_append_format(&builder, "\n")) {
            goto cleanup;
        }
    }

    if (!machine_image_append_format(&builder, "report_segments:\n")) {
        goto cleanup;
    }
    for (segment_index = 0u; segment_index < report->segment_summary_count; ++segment_index) {
        MachineImageReportSegmentArtifact segment_artifact;

        if (!machine_image_report_get_segment_artifact(report, segment_index, &segment_artifact) ||
            !machine_image_append_format(
                &builder,
                "  iseg.%zu %s symbols=%zu relocations=%zu resolved=%zu unresolved=%zu\n",
                segment_index,
                segment_artifact.segment_summary && segment_artifact.segment_summary->name
                    ? segment_artifact.segment_summary->name
                    : "<null>",
                segment_artifact.symbol_count,
                segment_artifact.relocation_count,
                segment_artifact.resolved_relocation_count,
                segment_artifact.unresolved_relocation_count)) {
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

int machine_image_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_bytes(bytes, byte_count, &image_file, error) &&
        machine_image_dump_file(&image_file, out_text, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &image_file, error) &&
        machine_image_dump_file(&image_file, out_text, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineImageError *error) {
    return machine_image_build_dump_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_text,
        error);
}

int machine_image_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_ir_report_with_profile(report, profile, &image_file, error) &&
        machine_image_dump_file(&image_file, out_text, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_dump_from_file(const MachineImageFile *source,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report;
    int ok;

    machine_image_report_init(&report);
    ok = machine_image_build_report_from_file(source, &report, error) &&
        machine_image_dump_report(&report, out_text, error);
    machine_image_report_free(&report);
    return ok;
}

int machine_image_build_report_dump_from_file_with_profile(const MachineImageFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report;
    int ok;

    machine_image_report_init(&report);
    ok = machine_image_build_report_from_file_with_profile(source, profile, &report, error) &&
        machine_image_dump_report(&report, out_text, error);
    machine_image_report_free(&report);
    return ok;
}

int machine_image_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report;
    int ok;

    machine_image_report_init(&report);
    ok = machine_image_build_report_from_machine_elf_file(source, &report, error) &&
        machine_image_dump_report(&report, out_text, error);
    machine_image_report_free(&report);
    return ok;
}

int machine_image_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report;
    int ok;

    machine_image_report_init(&report);
    ok = machine_image_build_report_from_machine_elf_file_with_profile(source, profile, &report, error) &&
        machine_image_dump_report(&report, out_text, error);
    machine_image_report_free(&report);
    return ok;
}

int machine_image_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport image_report;
    int ok;

    machine_image_report_init(&image_report);
    ok = machine_image_build_report_from_machine_elf_report(report, &image_report, error) &&
        machine_image_dump_report(&image_report, out_text, error);
    machine_image_report_free(&image_report);
    return ok;
}

int machine_image_build_report_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport image_report;
    int ok;

    machine_image_report_init(&image_report);
    ok = machine_image_build_report_from_machine_elf_report_with_profile(report, profile, &image_report, error) &&
        machine_image_dump_report(&image_report, out_text, error);
    machine_image_report_free(&image_report);
    return ok;
}

int machine_image_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report;
    int ok;

    machine_image_report_init(&report);
    ok = machine_image_build_report_from_machine_elf_bytes(bytes, byte_count, &report, error) &&
        machine_image_dump_report(&report, out_text, error);
    machine_image_report_free(&report);
    return ok;
}

int machine_image_build_report_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report;
    int ok;

    machine_image_report_init(&report);
    ok = machine_image_build_report_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &report, error) &&
        machine_image_dump_report(&report, out_text, error);
    machine_image_report_free(&report);
    return ok;
}

int machine_image_build_report_dump_from_machine_ir_report(
    const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineImageError *error) {
    return machine_image_build_report_dump_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_text,
        error);
}

int machine_image_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error) {
    MachineImageReport report_artifact;
    int ok;

    machine_image_report_init(&report_artifact);
    ok = machine_image_build_report_from_machine_ir_report_with_profile(
            report,
            profile,
            &report_artifact,
            error) &&
        machine_image_dump_report(&report_artifact, out_text, error);
    machine_image_report_free(&report_artifact);
    return ok;
}

#endif
