#if defined(__INTELLISENSE__) && !defined(MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IMAGE_SPLIT_AGGREGATOR)
#define MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_image.c"
#else

void machine_image_file_init(MachineImageFile *image_file) {
    if (!image_file) {
        return;
    }
    image_file->target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file->source_elf_artifact_summary.target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file->source_elf_artifact_summary.origin_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    image_file->source_elf_artifact_summary.relocation_semantics = MACHINE_ELF_RELOCATION_SEMANTICS_UNKNOWN;
    image_file->base_virtual_address = 0u;
    image_file->has_entry = 0;
    image_file->entry_symbol_index = 0u;
    image_file->entry_virtual_address = 0u;
    image_file->segments = NULL;
    image_file->segment_count = 0u;
    image_file->segment_capacity = 0u;
    image_file->symbols = NULL;
    image_file->symbol_count = 0u;
    image_file->symbol_capacity = 0u;
    image_file->relocations = NULL;
    image_file->relocation_count = 0u;
    image_file->relocation_capacity = 0u;
    image_file->bytes = NULL;
    image_file->byte_count = 0u;
}

static int machine_image_try_add_size(size_t base, size_t delta, size_t *out_sum) {
    if (!out_sum) {
        return 0;
    }
    if (delta > ((size_t)-1) - base) {
        return 0;
    }
    *out_sum = base + delta;
    return 1;
}

void machine_image_report_init(MachineImageReport *report) {
    if (!report) {
        return;
    }
    machine_image_file_init(&report->file);
    memset(&report->source_elf_artifact_summary, 0, sizeof(report->source_elf_artifact_summary));
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    memset(&report->entry_symbol_summary, 0, sizeof(report->entry_symbol_summary));
    report->has_entry_symbol_summary = 0;
    report->segment_summaries = NULL;
    report->segment_summary_count = 0u;
    report->segment_symbol_start_indices = NULL;
    report->segment_symbol_counts = NULL;
    report->segment_symbol_indices = NULL;
    report->segment_symbol_index_count = 0u;
    report->symbol_summaries = NULL;
    report->symbol_summary_count = 0u;
    report->defined_symbol_indices = NULL;
    report->defined_symbol_count = 0u;
    report->undefined_symbol_indices = NULL;
    report->undefined_symbol_count = 0u;
    report->global_symbol_indices = NULL;
    report->global_symbol_count = 0u;
    report->local_symbol_indices = NULL;
    report->local_symbol_count = 0u;
    report->defined_global_symbol_indices = NULL;
    report->defined_global_symbol_count = 0u;
    report->undefined_global_symbol_indices = NULL;
    report->undefined_global_symbol_count = 0u;
    report->segment_relocation_start_indices = NULL;
    report->segment_relocation_counts = NULL;
    report->segment_relocation_indices = NULL;
    report->segment_relocation_index_count = 0u;
    report->segment_resolved_relocation_start_indices = NULL;
    report->segment_resolved_relocation_counts = NULL;
    report->segment_resolved_relocation_indices = NULL;
    report->segment_resolved_relocation_index_count = 0u;
    report->segment_unresolved_relocation_start_indices = NULL;
    report->segment_unresolved_relocation_counts = NULL;
    report->segment_unresolved_relocation_indices = NULL;
    report->segment_unresolved_relocation_index_count = 0u;
    report->symbol_relocation_start_indices = NULL;
    report->symbol_relocation_counts = NULL;
    report->symbol_relocation_indices = NULL;
    report->symbol_relocation_index_count = 0u;
    report->symbol_resolved_relocation_start_indices = NULL;
    report->symbol_resolved_relocation_counts = NULL;
    report->symbol_resolved_relocation_indices = NULL;
    report->symbol_resolved_relocation_index_count = 0u;
    report->symbol_unresolved_relocation_start_indices = NULL;
    report->symbol_unresolved_relocation_counts = NULL;
    report->symbol_unresolved_relocation_indices = NULL;
    report->symbol_unresolved_relocation_index_count = 0u;
    report->relocation_summaries = NULL;
    report->relocation_summary_count = 0u;
    report->resolved_relocation_indices = NULL;
    report->resolved_relocation_count = 0u;
    report->unresolved_relocation_indices = NULL;
    report->unresolved_relocation_count = 0u;
    report->call_relocation_indices = NULL;
    report->call_relocation_count = 0u;
    report->control_primary_relocation_indices = NULL;
    report->control_primary_relocation_count = 0u;
    report->control_secondary_relocation_indices = NULL;
    report->control_secondary_relocation_count = 0u;
}

static int machine_image_file_has_tables(const MachineImageFile *image_file) {
    return image_file &&
        (image_file->segment_count == 0u || image_file->segments) &&
        (image_file->symbol_count == 0u || image_file->symbols) &&
        (image_file->relocation_count == 0u || image_file->relocations);
}

static int machine_image_report_has_tables(const MachineImageReport *report) {
    return report &&
        machine_image_file_has_tables(&report->file) &&
        report->segment_summary_count == report->file.segment_count &&
        report->symbol_summary_count == report->file.symbol_count &&
        report->relocation_summary_count == report->file.relocation_count &&
        (report->segment_summary_count == 0u || report->segment_summaries) &&
        (report->segment_summary_count == 0u || report->segment_symbol_start_indices) &&
        (report->segment_summary_count == 0u || report->segment_symbol_counts) &&
        (report->segment_summary_count == 0u || report->segment_relocation_start_indices) &&
        (report->segment_summary_count == 0u || report->segment_relocation_counts) &&
        (report->segment_summary_count == 0u || report->segment_resolved_relocation_start_indices) &&
        (report->segment_summary_count == 0u || report->segment_resolved_relocation_counts) &&
        (report->segment_summary_count == 0u || report->segment_unresolved_relocation_start_indices) &&
        (report->segment_summary_count == 0u || report->segment_unresolved_relocation_counts) &&
        (report->symbol_summary_count == 0u || report->symbol_summaries) &&
        (report->symbol_summary_count == 0u || report->symbol_relocation_start_indices) &&
        (report->symbol_summary_count == 0u || report->symbol_relocation_counts) &&
        (report->symbol_summary_count == 0u || report->symbol_resolved_relocation_start_indices) &&
        (report->symbol_summary_count == 0u || report->symbol_resolved_relocation_counts) &&
        (report->symbol_summary_count == 0u || report->symbol_unresolved_relocation_start_indices) &&
        (report->symbol_summary_count == 0u || report->symbol_unresolved_relocation_counts) &&
        (report->relocation_summary_count == 0u || report->relocation_summaries) &&
        (report->defined_symbol_count == 0u || report->defined_symbol_indices) &&
        (report->undefined_symbol_count == 0u || report->undefined_symbol_indices) &&
        (report->global_symbol_count == 0u || report->global_symbol_indices) &&
        (report->local_symbol_count == 0u || report->local_symbol_indices) &&
        (report->defined_global_symbol_count == 0u || report->defined_global_symbol_indices) &&
        (report->undefined_global_symbol_count == 0u || report->undefined_global_symbol_indices) &&
        (report->resolved_relocation_count == 0u || report->resolved_relocation_indices) &&
        (report->unresolved_relocation_count == 0u || report->unresolved_relocation_indices) &&
        (report->call_relocation_count == 0u || report->call_relocation_indices) &&
        (report->control_primary_relocation_count == 0u || report->control_primary_relocation_indices) &&
        (report->control_secondary_relocation_count == 0u || report->control_secondary_relocation_indices) &&
        (report->segment_symbol_index_count == 0u || report->segment_symbol_indices) &&
        (report->segment_relocation_index_count == 0u || report->segment_relocation_indices) &&
        (report->segment_resolved_relocation_index_count == 0u || report->segment_resolved_relocation_indices) &&
        (report->segment_unresolved_relocation_index_count == 0u || report->segment_unresolved_relocation_indices) &&
        (report->symbol_relocation_index_count == 0u || report->symbol_relocation_indices) &&
        (report->symbol_resolved_relocation_index_count == 0u || report->symbol_resolved_relocation_indices) &&
        (report->symbol_unresolved_relocation_index_count == 0u || report->symbol_unresolved_relocation_indices);
}

void machine_image_file_free(MachineImageFile *image_file) {
    size_t segment_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!image_file) {
        return;
    }

    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        free(image_file->segments[segment_index].name);
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        free(image_file->symbols[symbol_index].name);
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        free(image_file->relocations[relocation_index].symbol_name);
    }
    free(image_file->segments);
    free(image_file->symbols);
    free(image_file->relocations);
    free(image_file->bytes);
    machine_image_file_init(image_file);
}

int machine_image_clone_file(const MachineImageFile *source,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    size_t segment_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!source || !out_image_file) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-143: invalid image clone contract");
        return 0;
    }
    if (!machine_image_verify_file(source, error)) {
        return 0;
    }

    machine_image_file_free(out_image_file);
    out_image_file->target_profile = source->target_profile;
    out_image_file->source_elf_artifact_summary = source->source_elf_artifact_summary;
    out_image_file->base_virtual_address = source->base_virtual_address;
    out_image_file->has_entry = source->has_entry;
    out_image_file->entry_symbol_index = source->entry_symbol_index;
    out_image_file->entry_virtual_address = source->entry_virtual_address;
    out_image_file->segment_count = source->segment_count;
    out_image_file->segment_capacity = source->segment_count;
    out_image_file->symbol_count = source->symbol_count;
    out_image_file->symbol_capacity = source->symbol_count;
    out_image_file->relocation_count = source->relocation_count;
    out_image_file->relocation_capacity = source->relocation_count;
    out_image_file->byte_count = source->byte_count;
    out_image_file->segments = source->segment_count > 0u
        ? (MachineImageSegment *)calloc(source->segment_count, sizeof(MachineImageSegment))
        : NULL;
    out_image_file->symbols = source->symbol_count > 0u
        ? (MachineImageSymbol *)calloc(source->symbol_count, sizeof(MachineImageSymbol))
        : NULL;
    out_image_file->relocations = source->relocation_count > 0u
        ? (MachineImageRelocation *)calloc(source->relocation_count, sizeof(MachineImageRelocation))
        : NULL;
    out_image_file->bytes = source->byte_count > 0u
        ? (unsigned char *)malloc(source->byte_count)
        : NULL;
    if ((source->segment_count > 0u && !out_image_file->segments) ||
        (source->symbol_count > 0u && !out_image_file->symbols) ||
        (source->relocation_count > 0u && !out_image_file->relocations) ||
        (source->byte_count > 0u && !out_image_file->bytes)) {
        machine_image_file_free(out_image_file);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-144: out of memory cloning image file");
        return 0;
    }
    if (source->byte_count > 0u) {
        memcpy(out_image_file->bytes, source->bytes, source->byte_count);
    }
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        out_image_file->segments[segment_index].name =
            machine_image_strdup(source->segments[segment_index].name ? source->segments[segment_index].name : "");
        out_image_file->segments[segment_index].image_offset = source->segments[segment_index].image_offset;
        out_image_file->segments[segment_index].virtual_address = source->segments[segment_index].virtual_address;
        out_image_file->segments[segment_index].byte_count = source->segments[segment_index].byte_count;
        out_image_file->segments[segment_index].align = source->segments[segment_index].align;
        if (!out_image_file->segments[segment_index].name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-145: out of memory cloning image segment name");
            return 0;
        }
    }
    for (symbol_index = 0u; symbol_index < source->symbol_count; ++symbol_index) {
        out_image_file->symbols[symbol_index].name =
            machine_image_strdup(source->symbols[symbol_index].name ? source->symbols[symbol_index].name : "");
        out_image_file->symbols[symbol_index].binding = source->symbols[symbol_index].binding;
        out_image_file->symbols[symbol_index].type = source->symbols[symbol_index].type;
        out_image_file->symbols[symbol_index].is_defined = source->symbols[symbol_index].is_defined;
        out_image_file->symbols[symbol_index].segment_index = source->symbols[symbol_index].segment_index;
        out_image_file->symbols[symbol_index].value_offset = source->symbols[symbol_index].value_offset;
        out_image_file->symbols[symbol_index].virtual_address = source->symbols[symbol_index].virtual_address;
        if (!out_image_file->symbols[symbol_index].name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-146: out of memory cloning image symbol name");
            return 0;
        }
    }
    for (relocation_index = 0u; relocation_index < source->relocation_count; ++relocation_index) {
        out_image_file->relocations[relocation_index].source_kind = source->relocations[relocation_index].source_kind;
        out_image_file->relocations[relocation_index].segment_index = source->relocations[relocation_index].segment_index;
        out_image_file->relocations[relocation_index].segment_offset = source->relocations[relocation_index].segment_offset;
        out_image_file->relocations[relocation_index].site_virtual_address =
            source->relocations[relocation_index].site_virtual_address;
        out_image_file->relocations[relocation_index].is_resolved = source->relocations[relocation_index].is_resolved;
        out_image_file->relocations[relocation_index].target_virtual_address =
            source->relocations[relocation_index].target_virtual_address;
        out_image_file->relocations[relocation_index].type = source->relocations[relocation_index].type;
        out_image_file->relocations[relocation_index].symbol_index = source->relocations[relocation_index].symbol_index;
        out_image_file->relocations[relocation_index].symbol_name =
            machine_image_strdup(source->relocations[relocation_index].symbol_name
                ? source->relocations[relocation_index].symbol_name
                : "");
        if (!out_image_file->relocations[relocation_index].symbol_name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-147: out of memory cloning image relocation name");
            return 0;
        }
    }

    return 1;
}

void machine_image_report_free(MachineImageReport *report) {
    if (!report) {
        return;
    }
    machine_image_file_free(&report->file);
    free(report->segment_summaries);
    free(report->segment_symbol_start_indices);
    free(report->segment_symbol_counts);
    free(report->segment_symbol_indices);
    free(report->symbol_summaries);
    free(report->defined_symbol_indices);
    free(report->undefined_symbol_indices);
    free(report->global_symbol_indices);
    free(report->local_symbol_indices);
    free(report->defined_global_symbol_indices);
    free(report->undefined_global_symbol_indices);
    free(report->segment_relocation_start_indices);
    free(report->segment_relocation_counts);
    free(report->segment_relocation_indices);
    free(report->segment_resolved_relocation_start_indices);
    free(report->segment_resolved_relocation_counts);
    free(report->segment_resolved_relocation_indices);
    free(report->segment_unresolved_relocation_start_indices);
    free(report->segment_unresolved_relocation_counts);
    free(report->segment_unresolved_relocation_indices);
    free(report->symbol_relocation_start_indices);
    free(report->symbol_relocation_counts);
    free(report->symbol_relocation_indices);
    free(report->symbol_resolved_relocation_start_indices);
    free(report->symbol_resolved_relocation_counts);
    free(report->symbol_resolved_relocation_indices);
    free(report->symbol_unresolved_relocation_start_indices);
    free(report->symbol_unresolved_relocation_counts);
    free(report->symbol_unresolved_relocation_indices);
    free(report->relocation_summaries);
    free(report->resolved_relocation_indices);
    free(report->unresolved_relocation_indices);
    free(report->call_relocation_indices);
    free(report->control_primary_relocation_indices);
    free(report->control_secondary_relocation_indices);
    machine_image_report_init(report);
}

int machine_image_file_get_summary(const MachineImageFile *image_file,
    size_t *out_segment_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count) {
    if (!machine_image_file_has_tables(image_file)) {
        return 0;
    }
    if (out_segment_count) {
        *out_segment_count = image_file->segment_count;
    }
    if (out_symbol_count) {
        *out_symbol_count = image_file->symbol_count;
    }
    if (out_relocation_count) {
        *out_relocation_count = image_file->relocation_count;
    }
    if (out_byte_count) {
        *out_byte_count = image_file->byte_count;
    }
    return 1;
}

int machine_image_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineImageTargetPolicySummary *out_summary) {
    if (!out_summary) {
        return 0;
    }
    out_summary->target_profile = profile;
    out_summary->base_virtual_address = machine_image_base_virtual_address(profile);
    out_summary->segment_alignment = machine_image_segment_alignment(profile);
    return 1;
}

int machine_image_file_get_target_profile(const MachineImageFile *image_file,
    MachineElfTargetProfile *out_profile) {
    if (!image_file || !out_profile) {
        return 0;
    }
    *out_profile = image_file->target_profile;
    return 1;
}

int machine_image_file_get_source_elf_artifact_summary(const MachineImageFile *image_file,
    MachineElfArtifactSummary *out_summary) {
    if (!image_file || !out_summary) {
        return 0;
    }
    *out_summary = image_file->source_elf_artifact_summary;
    return 1;
}

int machine_image_file_get_target_policy_summary(const MachineImageFile *image_file,
    MachineImageTargetPolicySummary *out_summary) {
    if (!image_file || !out_summary) {
        return 0;
    }
    return machine_image_get_target_policy_summary(image_file->target_profile, out_summary);
}

int machine_image_file_get_header_summary(const MachineImageFile *image_file,
    MachineImageHeaderSummary *out_summary) {
    if (!machine_image_file_has_tables(image_file) || !out_summary) {
        return 0;
    }
    out_summary->target_profile = image_file->target_profile;
    out_summary->base_virtual_address = image_file->base_virtual_address;
    out_summary->has_entry = image_file->has_entry;
    out_summary->entry_symbol_index = image_file->entry_symbol_index;
    out_summary->entry_virtual_address = image_file->entry_virtual_address;
    out_summary->segment_count = image_file->segment_count;
    out_summary->byte_count = image_file->byte_count;
    return 1;
}

static int machine_image_apply_profile(MachineImageFile *image_file,
    MachineElfTargetProfile profile,
    MachineImageError *error) {
    MachineElfTargetPolicySummary elf_policy;
    size_t segment_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!image_file) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-149: invalid image reprofile contract");
        return 0;
    }
    if (!machine_image_verify_file(image_file, error) ||
        !machine_elf_get_target_policy_summary(profile, &elf_policy)) {
        if (!error || error->message[0] == '\0') {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-150: unknown image target profile");
        }
        return 0;
    }

    image_file->target_profile = profile;
    image_file->source_elf_artifact_summary.target_profile = profile;
    image_file->base_virtual_address = machine_image_base_virtual_address(profile);
    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        MachineImageSegment *segment = &image_file->segments[segment_index];
        segment->align = machine_image_segment_alignment(profile);
        segment->virtual_address = image_file->base_virtual_address + segment->image_offset;
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        MachineImageSymbol *symbol = &image_file->symbols[symbol_index];
        if (!symbol->is_defined || symbol->segment_index >= image_file->segment_count) {
            symbol->virtual_address = 0u;
            continue;
        }
        symbol->virtual_address =
            image_file->segments[symbol->segment_index].virtual_address + symbol->value_offset;
    }
    if (image_file->has_entry && image_file->entry_symbol_index < image_file->symbol_count) {
        image_file->entry_virtual_address = image_file->symbols[image_file->entry_symbol_index].virtual_address;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        MachineImageRelocation *relocation = &image_file->relocations[relocation_index];
        const MachineImageSymbol *target_symbol;
        uint32_t relocation_type = 0u;

        if (relocation->segment_index >= image_file->segment_count ||
            relocation->symbol_index >= image_file->symbol_count) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-151: invalid image relocation while applying profile");
            return 0;
        }
        target_symbol = &image_file->symbols[relocation->symbol_index];
        relocation->site_virtual_address =
            image_file->segments[relocation->segment_index].virtual_address + relocation->segment_offset;
        relocation->is_resolved = target_symbol->is_defined && target_symbol->segment_index != (size_t)-1;
        relocation->target_virtual_address = relocation->is_resolved ? target_symbol->virtual_address : 0u;
        switch (relocation->source_kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                relocation_type = elf_policy.call_relocation_type;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                relocation_type = elf_policy.primary_control_relocation_type;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                relocation_type = elf_policy.secondary_control_relocation_type;
                break;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
                relocation_type = elf_policy.data_address_relocation_type;
                break;
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
                relocation_type = elf_policy.data_load_relocation_type;
                break;
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                relocation_type = elf_policy.data_store_relocation_type;
                break;
        }
        relocation->type = relocation_type;
    }

    return machine_image_verify_file(image_file, error);
}

int machine_image_file_copy_bytes(const MachineImageFile *image_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineImageError *error) {
    unsigned char *copy;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0u;
    }
    if (!image_file || !out_bytes) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-100: invalid image byte-copy contract");
        return 0;
    }
    if (image_file->byte_count == 0u) {
        return 1;
    }
    if (!image_file->bytes) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-101: image has byte count without byte storage");
        return 0;
    }
    copy = (unsigned char *)malloc(image_file->byte_count);
    if (!copy) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-102: out of memory copying image bytes");
        return 0;
    }
    memcpy(copy, image_file->bytes, image_file->byte_count);
    *out_bytes = copy;
    if (out_byte_count) {
        *out_byte_count = image_file->byte_count;
    }
    return 1;
}

int machine_image_file_get_segment(const MachineImageFile *image_file,
    size_t segment_index,
    const MachineImageSegment **out_segment) {
    if (!machine_image_file_has_tables(image_file) ||
        !out_segment ||
        segment_index >= image_file->segment_count) {
        return 0;
    }
    *out_segment = &image_file->segments[segment_index];
    return 1;
}

int machine_image_file_find_segment_by_name(const MachineImageFile *image_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineImageSegment **out_segment) {
    size_t segment_index;

    if (!machine_image_file_has_tables(image_file) || !segment_name) {
        return 0;
    }
    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        if (image_file->segments[segment_index].name &&
            strcmp(image_file->segments[segment_index].name, segment_name) == 0) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = &image_file->segments[segment_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_file_find_segment_covering_virtual_address(const MachineImageFile *image_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineImageSegment **out_segment) {
    size_t segment_index;

    if (!machine_image_file_has_tables(image_file)) {
        return 0;
    }
    for (segment_index = 0u; segment_index < image_file->segment_count; ++segment_index) {
        const MachineImageSegment *segment = &image_file->segments[segment_index];
        size_t segment_end = 0u;

        if (!machine_image_try_add_size(segment->virtual_address, segment->byte_count, &segment_end)) {
            return 0;
        }

        if (virtual_address >= segment->virtual_address &&
            virtual_address < segment_end) {
            if (out_segment_index) {
                *out_segment_index = segment_index;
            }
            if (out_segment) {
                *out_segment = segment;
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_segment_copy_bytes(const MachineImageFile *image_file,
    size_t segment_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineImageError *error) {
    const MachineImageSegment *segment;
    unsigned char *copy;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0u;
    }
    if (!machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-103: invalid image segment byte-copy contract");
        return 0;
    }
    if (segment->byte_count == 0u) {
        return 1;
    }
    {
        size_t image_end = 0u;

        if (!image_file->bytes ||
            !machine_image_try_add_size(segment->image_offset, segment->byte_count, &image_end) ||
            image_end > image_file->byte_count) {
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-104: invalid image segment byte span");
            return 0;
        }
    }
    copy = (unsigned char *)malloc(segment->byte_count);
    if (!copy) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-105: out of memory copying segment bytes");
        return 0;
    }
    memcpy(copy, image_file->bytes + segment->image_offset, segment->byte_count);
    *out_bytes = copy;
    if (out_byte_count) {
        *out_byte_count = segment->byte_count;
    }
    return 1;
}

int machine_image_file_get_segment_symbol_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count) {
    size_t symbol_index;
    size_t count = 0u;
    const MachineImageSegment *segment;

    if (out_count) {
        *out_count = 0u;
    }
    if (!machine_image_file_has_tables(image_file) ||
        !out_count ||
        !machine_image_file_get_segment(image_file, segment_index, &segment) ||
        !segment) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        const MachineImageSymbol *symbol = &image_file->symbols[symbol_index];

        if (symbol->is_defined && symbol->segment_index == segment_index) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_symbol(const MachineImageFile *image_file,
    size_t symbol_index,
    const MachineImageSymbol **out_symbol) {
    if (!machine_image_file_has_tables(image_file) ||
        !out_symbol ||
        symbol_index >= image_file->symbol_count) {
        return 0;
    }
    *out_symbol = &image_file->symbols[symbol_index];
    return 1;
}

int machine_image_file_get_symbol_relocation_count(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;
    const MachineImageSymbol *symbol;

    if (out_count) {
        *out_count = 0u;
    }
    if (!image_file || !out_count || !machine_image_file_get_symbol(image_file, symbol_index, &symbol) || !symbol) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        if (image_file->relocations[relocation_index].symbol_index == symbol_index) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_symbol_resolved_relocation_count(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;
    const MachineImageSymbol *symbol;

    if (out_count) {
        *out_count = 0u;
    }
    if (!image_file || !out_count || !machine_image_file_get_symbol(image_file, symbol_index, &symbol) || !symbol) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->symbol_index == symbol_index && relocation->is_resolved) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_symbol_unresolved_relocation_count(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;
    const MachineImageSymbol *symbol;

    if (out_count) {
        *out_count = 0u;
    }
    if (!image_file || !out_count || !machine_image_file_get_symbol(image_file, symbol_index, &symbol) || !symbol) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->symbol_index == symbol_index && !relocation->is_resolved) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_symbol_relocation_by_index(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t symbol_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;
    const MachineImageSymbol *symbol;

    if (!image_file || !machine_image_file_get_symbol(image_file, symbol_index, &symbol) || !symbol) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->symbol_index != symbol_index) {
            continue;
        }
        if (current_index == symbol_relocation_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_get_symbol_resolved_relocation_by_index(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t symbol_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;
    const MachineImageSymbol *symbol;

    if (!image_file || !machine_image_file_get_symbol(image_file, symbol_index, &symbol) || !symbol) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->symbol_index != symbol_index || !relocation->is_resolved) {
            continue;
        }
        if (current_index == symbol_resolved_relocation_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_get_symbol_unresolved_relocation_by_index(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t symbol_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;
    const MachineImageSymbol *symbol;

    if (!image_file || !machine_image_file_get_symbol(image_file, symbol_index, &symbol) || !symbol) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->symbol_index != symbol_index || relocation->is_resolved) {
            continue;
        }
        if (current_index == symbol_unresolved_relocation_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_get_segment_symbol_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_symbol_index,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol) {
    size_t symbol_index;
    size_t current_index = 0u;
    const MachineImageSegment *segment;

    if (!image_file || !machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        const MachineImageSymbol *symbol = &image_file->symbols[symbol_index];

        if (!symbol->is_defined || symbol->segment_index != segment_index) {
            continue;
        }
        if (current_index == segment_symbol_index) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_symbol) {
                *out_symbol = symbol;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_find_symbol_by_name(const MachineImageFile *image_file,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol) {
    size_t symbol_index;

    if (!machine_image_file_has_tables(image_file) || !symbol_name) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        if (image_file->symbols[symbol_index].name &&
            strcmp(image_file->symbols[symbol_index].name, symbol_name) == 0) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_symbol) {
                *out_symbol = &image_file->symbols[symbol_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_file_find_symbol_by_virtual_address(const MachineImageFile *image_file,
    size_t virtual_address,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol) {
    size_t symbol_index;

    if (!machine_image_file_has_tables(image_file)) {
        return 0;
    }
    for (symbol_index = 0u; symbol_index < image_file->symbol_count; ++symbol_index) {
        const MachineImageSymbol *symbol = &image_file->symbols[symbol_index];

        if (symbol->is_defined && symbol->virtual_address == virtual_address) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_symbol) {
                *out_symbol = symbol;
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_file_get_entry_symbol(const MachineImageFile *image_file,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol) {
    if (!image_file || !image_file->has_entry) {
        return 0;
    }
    if (out_symbol_index) {
        *out_symbol_index = image_file->entry_symbol_index;
    }
    return machine_image_file_get_symbol(image_file, image_file->entry_symbol_index, out_symbol);
}

int machine_image_file_get_relocation(const MachineImageFile *image_file,
    size_t relocation_index,
    const MachineImageRelocation **out_relocation) {
    if (!machine_image_file_has_tables(image_file) || !out_relocation ||
        relocation_index >= image_file->relocation_count) {
        return 0;
    }
    *out_relocation = &image_file->relocations[relocation_index];
    return 1;
}

int machine_image_file_get_segment_relocation_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;
    const MachineImageSegment *segment;

    if (out_count) {
        *out_count = 0u;
    }
    if (!machine_image_file_has_tables(image_file) ||
        !out_count ||
        !machine_image_file_get_segment(image_file, segment_index, &segment) ||
        !segment) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        if (image_file->relocations[relocation_index].segment_index == segment_index) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_segment_relocation_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;
    const MachineImageSegment *segment;

    if (!image_file || !machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->segment_index != segment_index) {
            continue;
        }
        if (current_index == segment_relocation_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_get_segment_resolved_relocation_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;
    const MachineImageSegment *segment;

    if (out_count) {
        *out_count = 0u;
    }
    if (!image_file || !out_count || !machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->segment_index == segment_index && relocation->is_resolved) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_segment_unresolved_relocation_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;
    const MachineImageSegment *segment;

    if (out_count) {
        *out_count = 0u;
    }
    if (!image_file || !out_count || !machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->segment_index == segment_index && !relocation->is_resolved) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_segment_resolved_relocation_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;
    const MachineImageSegment *segment;

    if (!image_file || !machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->segment_index != segment_index || !relocation->is_resolved) {
            continue;
        }
        if (current_index == segment_resolved_relocation_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_get_segment_unresolved_relocation_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;
    const MachineImageSegment *segment;

    if (!image_file || !machine_image_file_get_segment(image_file, segment_index, &segment) || !segment) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->segment_index != segment_index || relocation->is_resolved) {
            continue;
        }
        if (current_index == segment_unresolved_relocation_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_find_relocation_by_site_virtual_address(const MachineImageFile *image_file,
    size_t site_virtual_address,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;

    if (!machine_image_file_has_tables(image_file)) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        const MachineImageRelocation *relocation = &image_file->relocations[relocation_index];

        if (relocation->site_virtual_address == site_virtual_address) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = relocation;
            }
            return 1;
        }
    }
    return 0;
}

int machine_image_file_get_resolved_relocation_count(const MachineImageFile *image_file,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;

    if (!machine_image_file_has_tables(image_file) || !out_count) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        if (image_file->relocations[relocation_index].is_resolved) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_unresolved_relocation_count(const MachineImageFile *image_file,
    size_t *out_count) {
    size_t relocation_index;
    size_t count = 0u;

    if (!machine_image_file_has_tables(image_file) || !out_count) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        if (!image_file->relocations[relocation_index].is_resolved) {
            ++count;
        }
    }
    *out_count = count;
    return 1;
}

int machine_image_file_get_resolved_relocation_by_index(const MachineImageFile *image_file,
    size_t resolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;

    if (!machine_image_file_has_tables(image_file)) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        if (!image_file->relocations[relocation_index].is_resolved) {
            continue;
        }
        if (current_index == resolved_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = &image_file->relocations[relocation_index];
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_file_get_unresolved_relocation_by_index(const MachineImageFile *image_file,
    size_t unresolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation) {
    size_t relocation_index;
    size_t current_index = 0u;

    if (!machine_image_file_has_tables(image_file)) {
        return 0;
    }
    for (relocation_index = 0u; relocation_index < image_file->relocation_count; ++relocation_index) {
        if (image_file->relocations[relocation_index].is_resolved) {
            continue;
        }
        if (current_index == unresolved_index) {
            if (out_relocation_index) {
                *out_relocation_index = relocation_index;
            }
            if (out_relocation) {
                *out_relocation = &image_file->relocations[relocation_index];
            }
            return 1;
        }
        ++current_index;
    }
    return 0;
}

int machine_image_segment_get_summary(const MachineImageSegment *segment,
    MachineImageSegmentSummary *out_summary) {
    if (!segment || !out_summary) {
        return 0;
    }
    out_summary->name = segment->name;
    out_summary->image_offset = segment->image_offset;
    out_summary->virtual_address = segment->virtual_address;
    out_summary->byte_count = segment->byte_count;
    out_summary->align = segment->align;
    return 1;
}

int machine_image_symbol_get_summary(const MachineImageSymbol *symbol,
    MachineImageSymbolSummary *out_summary) {
    if (!symbol || !out_summary) {
        return 0;
    }
    out_summary->name = symbol->name;
    out_summary->binding = symbol->binding;
    out_summary->type = symbol->type;
    out_summary->is_defined = symbol->is_defined;
    out_summary->segment_index = symbol->segment_index;
    out_summary->value_offset = symbol->value_offset;
    out_summary->virtual_address = symbol->virtual_address;
    return 1;
}

int machine_image_relocation_get_summary(const MachineImageRelocation *relocation,
    MachineImageRelocationSummary *out_summary) {
    if (!relocation || !out_summary) {
        return 0;
    }
    out_summary->source_kind = relocation->source_kind;
    out_summary->segment_index = relocation->segment_index;
    out_summary->segment_offset = relocation->segment_offset;
    out_summary->site_virtual_address = relocation->site_virtual_address;
    out_summary->is_resolved = relocation->is_resolved;
    out_summary->target_virtual_address = relocation->target_virtual_address;
    out_summary->type = relocation->type;
    out_summary->symbol_index = relocation->symbol_index;
    out_summary->symbol_name = relocation->symbol_name;
    return 1;
}

#endif
