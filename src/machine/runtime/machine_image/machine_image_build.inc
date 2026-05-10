#if defined(__INTELLISENSE__) && !defined(MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_IMAGE_SPLIT_AGGREGATOR)
#define MACHINE_IMAGE_SPLIT_INTELLISENSE_CONTEXT
#include "machine_image.c"
#else

int machine_image_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfError elf_error;
    MachineElfArtifactSummary artifact_summary;
    MachineElfTargetProfile profile;
    size_t text_section_index = 0u;
    const MachineElfSection *text_section = NULL;
    const MachineElfSection *sbss_section = NULL;
    const MachineElfSection *sdata_section = NULL;
    size_t sbss_section_index = 0u;
    size_t sdata_section_index = 0u;
    size_t image_segment_count = 1u;
    size_t image_byte_count = 0u;
    size_t image_cursor = 0u;
    size_t segment_alignment = 0u;
    size_t sbss_image_offset = 0u;
    size_t sdata_image_offset = 0u;
    size_t symbol_index;
    size_t relocation_index;
    size_t section_index;
    int entry_found = 0;

    memset(&elf_error, 0, sizeof(elf_error));
    memset(&artifact_summary, 0, sizeof(artifact_summary));
    if (!elf_file || !out_image_file) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-106: invalid image build contract");
        return 0;
    }
    if (!machine_elf_verify_file(elf_file, &elf_error)) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-107: invalid elf input: %s", elf_error.message);
        return 0;
    }
    if (!machine_elf_file_get_target_profile(elf_file, &profile) ||
        !machine_elf_file_get_artifact_summary(elf_file, &artifact_summary) ||
        !machine_elf_file_get_text_section(elf_file, NULL, &text_section) ||
        !text_section || !elf_file->bytes ||
        text_section->file_offset + text_section->byte_count > elf_file->byte_count) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-108: unsupported elf input for image build");
        return 0;
    }
    text_section_index = elf_file->text_section_index;
    for (section_index = 0u; section_index < elf_file->section_count; ++section_index) {
        const MachineElfSection *section = &elf_file->sections[section_index];

        if (!section->name || section->name[0] == '\0') {
            continue;
        }
        if (strcmp(section->name, ".sbss") == 0) {
            sbss_section = section;
            sbss_section_index = section_index;
            ++image_segment_count;
        } else if (strcmp(section->name, ".sdata") == 0) {
            sdata_section = section;
            sdata_section_index = section_index;
            ++image_segment_count;
        }
    }

    machine_image_file_free(out_image_file);
    out_image_file->target_profile = profile;
    out_image_file->source_elf_artifact_summary = artifact_summary;
    out_image_file->base_virtual_address = machine_image_base_virtual_address(profile);
    segment_alignment = machine_image_segment_alignment(profile);
    image_cursor = text_section->byte_count;
    if (sbss_section) {
        if (segment_alignment > 0u) {
            size_t remainder = image_cursor % segment_alignment;
            if (remainder != 0u) {
                image_cursor += segment_alignment - remainder;
            }
        }
        sbss_image_offset = image_cursor;
        image_cursor += sbss_section->byte_count;
    }
    if (sdata_section) {
        if (segment_alignment > 0u) {
            size_t remainder = image_cursor % segment_alignment;
            if (remainder != 0u) {
                image_cursor += segment_alignment - remainder;
            }
        }
        sdata_image_offset = image_cursor;
        image_cursor += sdata_section->byte_count;
    }
    image_byte_count = image_cursor;
    out_image_file->segment_count = image_segment_count;
    out_image_file->segment_capacity = image_segment_count;
    out_image_file->segments = (MachineImageSegment *)calloc(image_segment_count, sizeof(MachineImageSegment));
    out_image_file->symbol_count = elf_file->symbol_count;
    out_image_file->symbol_capacity = elf_file->symbol_count;
    out_image_file->symbols = elf_file->symbol_count > 0u
        ? (MachineImageSymbol *)calloc(elf_file->symbol_count, sizeof(MachineImageSymbol))
        : NULL;
    out_image_file->relocation_count = elf_file->relocation_count;
    out_image_file->relocation_capacity = elf_file->relocation_count;
    out_image_file->relocations = elf_file->relocation_count > 0u
        ? (MachineImageRelocation *)calloc(elf_file->relocation_count, sizeof(MachineImageRelocation))
        : NULL;
    out_image_file->byte_count = image_byte_count;
    out_image_file->bytes = image_byte_count > 0u
        ? (unsigned char *)calloc(image_byte_count, 1u)
        : NULL;

    if (!out_image_file->segments ||
        (elf_file->symbol_count > 0u && !out_image_file->symbols) ||
        (elf_file->relocation_count > 0u && !out_image_file->relocations) ||
        (text_section->byte_count > 0u && !out_image_file->bytes)) {
        machine_image_file_free(out_image_file);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-109: out of memory building image");
        return 0;
    }

    if (text_section->byte_count > 0u) {
        memcpy(out_image_file->bytes, elf_file->bytes + text_section->file_offset, text_section->byte_count);
    }
    out_image_file->segments[0].name = machine_image_strdup(".text");
    out_image_file->segments[0].image_offset = 0u;
    out_image_file->segments[0].virtual_address = out_image_file->base_virtual_address;
    out_image_file->segments[0].byte_count = text_section->byte_count;
    out_image_file->segments[0].align = machine_image_segment_alignment(profile);
    if (!out_image_file->segments[0].name) {
        machine_image_file_free(out_image_file);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-110: out of memory naming image segment");
        return 0;
    }
    if (sbss_section) {
        out_image_file->segments[1u].name = machine_image_strdup(".sbss");
        out_image_file->segments[1u].image_offset = sbss_image_offset;
        out_image_file->segments[1u].virtual_address = out_image_file->base_virtual_address + sbss_image_offset;
        out_image_file->segments[1u].byte_count = sbss_section->byte_count;
        out_image_file->segments[1u].align = machine_image_segment_alignment(profile);
        if (!out_image_file->segments[1u].name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-110: out of memory naming image segment");
            return 0;
        }
    }
    if (sdata_section) {
        size_t segment_index = 1u + (sbss_section ? 1u : 0u);

        out_image_file->segments[segment_index].name = machine_image_strdup(".sdata");
        out_image_file->segments[segment_index].image_offset = sdata_image_offset;
        out_image_file->segments[segment_index].virtual_address = out_image_file->base_virtual_address + sdata_image_offset;
        out_image_file->segments[segment_index].byte_count = sdata_section->byte_count;
        out_image_file->segments[segment_index].align = machine_image_segment_alignment(profile);
        if (!out_image_file->segments[segment_index].name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-110: out of memory naming image segment");
            return 0;
        }
        if (sdata_section->byte_count > 0u) {
            memcpy(
                out_image_file->bytes + sdata_image_offset,
                elf_file->bytes + sdata_section->file_offset,
                sdata_section->byte_count);
        }
    }

    for (symbol_index = 0u; symbol_index < elf_file->symbol_count; ++symbol_index) {
        const MachineElfSymbol *elf_symbol = &elf_file->symbols[symbol_index];
        MachineImageSymbol *image_symbol = &out_image_file->symbols[symbol_index];

        image_symbol->name = machine_image_strdup(elf_symbol->name ? elf_symbol->name : "");
        image_symbol->binding = elf_symbol->binding;
        image_symbol->type = elf_symbol->type;
        image_symbol->is_defined = 0;
        image_symbol->segment_index = (size_t)-1;
        image_symbol->value_offset = 0u;
        image_symbol->virtual_address = 0u;
        if (!image_symbol->name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-111: out of memory naming image symbol");
            return 0;
        }
        if (elf_symbol->is_defined && elf_symbol->section_index == text_section_index) {
            image_symbol->is_defined = 1;
            image_symbol->segment_index = 0u;
            image_symbol->value_offset = elf_symbol->value;
            image_symbol->virtual_address = out_image_file->base_virtual_address + (size_t)elf_symbol->value;
        } else if (sbss_section && elf_symbol->is_defined && elf_symbol->section_index == sbss_section_index) {
            image_symbol->is_defined = 1;
            image_symbol->segment_index = 1u;
            image_symbol->value_offset = elf_symbol->value;
            image_symbol->virtual_address =
                out_image_file->base_virtual_address + sbss_image_offset + (size_t)elf_symbol->value;
        } else if (sdata_section && elf_symbol->is_defined && elf_symbol->section_index == sdata_section_index) {
            image_symbol->is_defined = 1;
            image_symbol->segment_index = 1u + (sbss_section ? 1u : 0u);
            image_symbol->value_offset = elf_symbol->value;
            image_symbol->virtual_address =
                out_image_file->base_virtual_address + sdata_image_offset + (size_t)elf_symbol->value;
        }
        if (!entry_found &&
            image_symbol->is_defined &&
            image_symbol->segment_index == 0u &&
            image_symbol->binding == MACHINE_ELF_SYMBOL_GLOBAL &&
            image_symbol->type == MACHINE_ELF_SYMBOL_FUNC &&
            image_symbol->name &&
            strcmp(image_symbol->name, "main") == 0) {
            out_image_file->has_entry = 1;
            out_image_file->entry_symbol_index = symbol_index;
            out_image_file->entry_virtual_address = image_symbol->virtual_address;
            entry_found = 1;
        }
    }
    if (!entry_found) {
        for (symbol_index = 0u; symbol_index < out_image_file->symbol_count; ++symbol_index) {
            const MachineImageSymbol *image_symbol = &out_image_file->symbols[symbol_index];

            if (image_symbol->is_defined &&
                image_symbol->segment_index == 0u &&
                image_symbol->binding == MACHINE_ELF_SYMBOL_GLOBAL &&
                image_symbol->type == MACHINE_ELF_SYMBOL_FUNC) {
                out_image_file->has_entry = 1;
                out_image_file->entry_symbol_index = symbol_index;
                out_image_file->entry_virtual_address = image_symbol->virtual_address;
                break;
            }
        }
    }

    for (relocation_index = 0u; relocation_index < elf_file->relocation_count; ++relocation_index) {
        const MachineElfRelocation *elf_relocation = &elf_file->relocations[relocation_index];
        MachineImageRelocation *image_relocation = &out_image_file->relocations[relocation_index];
        const MachineImageSymbol *target_symbol = NULL;

        if (elf_relocation->symbol_index >= out_image_file->symbol_count) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-112: relocation references out-of-range image symbol");
            return 0;
        }
        target_symbol = &out_image_file->symbols[elf_relocation->symbol_index];
        image_relocation->source_kind = elf_relocation->source_kind;
        image_relocation->segment_index = 0u;
        image_relocation->segment_offset = elf_relocation->offset;
        image_relocation->site_virtual_address = out_image_file->base_virtual_address + (size_t)elf_relocation->offset;
        image_relocation->is_resolved = target_symbol->is_defined && target_symbol->segment_index != (size_t)-1;
        image_relocation->target_virtual_address =
            image_relocation->is_resolved ? target_symbol->virtual_address : 0u;
        image_relocation->type = elf_relocation->type;
        image_relocation->symbol_index = elf_relocation->symbol_index;
        image_relocation->symbol_name = machine_image_strdup(elf_relocation->symbol_name ? elf_relocation->symbol_name : "");
        if (!image_relocation->symbol_name) {
            machine_image_file_free(out_image_file);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-113: out of memory naming image relocation");
            return 0;
        }
    }

    if (!machine_image_verify_file(out_image_file, error)) {
        machine_image_file_free(out_image_file);
        return 0;
    }
    return 1;
}

int machine_image_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfReport elf_report;
    MachineElfError elf_error;
    int ok;

    memset(&elf_error, 0, sizeof(elf_error));
    machine_elf_report_init(&elf_report);
    ok = machine_elf_build_report_from_file_with_profile(elf_file, profile, &elf_report, &elf_error);
    if (!ok) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-152: failed building image from reprfiled elf file: %s", elf_error.message);
        machine_elf_report_free(&elf_report);
        return 0;
    }
    ok = machine_image_build_from_machine_elf_report(&elf_report, out_image_file, error);
    machine_elf_report_free(&elf_report);
    return ok;
}

int machine_image_build_from_machine_elf_report(const MachineElfReport *report,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    const MachineElfFile *elf_file = NULL;

    if (!report) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-129: invalid image build-from-elf-report contract");
        return 0;
    }
    if (!machine_elf_report_get_file(report, &elf_file) || !elf_file) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-130: failed to recover elf file from elf report");
        return 0;
    }
    return machine_image_build_from_machine_elf_file(elf_file, out_image_file, error);
}

int machine_image_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfReport elf_report;
    MachineElfError elf_error;
    int ok;

    memset(&elf_error, 0, sizeof(elf_error));
    machine_elf_report_init(&elf_report);
    ok = machine_elf_build_report_from_file_with_profile(&report->file, profile, &elf_report, &elf_error);
    if (!ok) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-153: failed building image from reprfiled elf report: %s", elf_error.message);
        machine_elf_report_free(&elf_report);
        return 0;
    }
    ok = machine_image_build_from_machine_elf_report(&elf_report, out_image_file, error);
    machine_elf_report_free(&elf_report);
    return ok;
}

int machine_image_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfFile elf_file;
    MachineElfError elf_error;
    int ok;

    memset(&elf_error, 0, sizeof(elf_error));
    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_file_from_bytes(bytes, byte_count, &elf_file, &elf_error);
    if (!ok) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-142: failed building image from elf bytes: %s", elf_error.message);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    ok = machine_image_build_from_machine_elf_file(&elf_file, out_image_file, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_image_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfFile elf_file;
    MachineElfError elf_error;
    int ok;

    memset(&elf_error, 0, sizeof(elf_error));
    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_file_from_bytes_with_profile(bytes, byte_count, profile, &elf_file, &elf_error);
    if (!ok) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-154: failed building image from reprfiled elf bytes: %s", elf_error.message);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    ok = machine_image_build_from_machine_elf_file(&elf_file, out_image_file, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_image_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfFile elf_file;
    MachineElfError elf_error;
    int ok;

    memset(&elf_error, 0, sizeof(elf_error));
    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_ir_report(report, &elf_file, &elf_error);
    if (!ok) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-114: failed building elf bridge: %s", elf_error.message);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    ok = machine_image_build_from_machine_elf_file(&elf_file, out_image_file, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_image_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error) {
    MachineElfFile elf_file;
    MachineElfError elf_error;
    int ok;

    memset(&elf_error, 0, sizeof(elf_error));
    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_ir_report_with_profile(report, profile, &elf_file, &elf_error);
    if (!ok) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-155: failed building image from reprfiled machine-ir report: %s", elf_error.message);
        machine_elf_file_free(&elf_file);
        return 0;
    }
    ok = machine_image_build_from_machine_elf_file(&elf_file, out_image_file, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_image_build_report_from_file(const MachineImageFile *source,
    MachineImageReport *out_report,
    MachineImageError *error) {
    size_t segment_index;
    size_t symbol_index;
    size_t relocation_index;
    size_t segment_symbol_total = 0u;
    size_t segment_relocation_total = 0u;
    size_t segment_resolved_relocation_total = 0u;
    size_t segment_unresolved_relocation_total = 0u;
    size_t defined_symbol_count = 0u;
    size_t undefined_symbol_count = 0u;
    size_t global_symbol_count = 0u;
    size_t local_symbol_count = 0u;
    size_t defined_global_symbol_count = 0u;
    size_t undefined_global_symbol_count = 0u;
    size_t symbol_relocation_total = 0u;
    size_t symbol_resolved_relocation_total = 0u;
    size_t symbol_unresolved_relocation_total = 0u;
    size_t call_relocation_count = 0u;
    size_t control_primary_relocation_count = 0u;
    size_t control_secondary_relocation_count = 0u;
    size_t resolved_count = 0u;
    size_t unresolved_count = 0u;

    if (!source || !out_report) {
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-131: invalid image report build contract");
        return 0;
    }
    if (!machine_image_verify_file(source, error)) {
        return 0;
    }

    machine_image_report_free(out_report);
    machine_image_file_free(&out_report->file);
    out_report->file.target_profile = source->target_profile;
    out_report->file.source_elf_artifact_summary = source->source_elf_artifact_summary;
    out_report->file.base_virtual_address = source->base_virtual_address;
    out_report->file.has_entry = source->has_entry;
    out_report->file.entry_symbol_index = source->entry_symbol_index;
    out_report->file.entry_virtual_address = source->entry_virtual_address;
    out_report->file.segment_count = source->segment_count;
    out_report->file.segment_capacity = source->segment_count;
    out_report->file.symbol_count = source->symbol_count;
    out_report->file.symbol_capacity = source->symbol_count;
    out_report->file.relocation_count = source->relocation_count;
    out_report->file.relocation_capacity = source->relocation_count;
    out_report->file.byte_count = source->byte_count;
    out_report->file.segments = source->segment_count > 0u
        ? (MachineImageSegment *)calloc(source->segment_count, sizeof(MachineImageSegment))
        : NULL;
    out_report->file.symbols = source->symbol_count > 0u
        ? (MachineImageSymbol *)calloc(source->symbol_count, sizeof(MachineImageSymbol))
        : NULL;
    out_report->file.relocations = source->relocation_count > 0u
        ? (MachineImageRelocation *)calloc(source->relocation_count, sizeof(MachineImageRelocation))
        : NULL;
    out_report->file.bytes = source->byte_count > 0u
        ? (unsigned char *)malloc(source->byte_count)
        : NULL;
    if ((source->segment_count > 0u && !out_report->file.segments) ||
        (source->symbol_count > 0u && !out_report->file.symbols) ||
        (source->relocation_count > 0u && !out_report->file.relocations) ||
        (source->byte_count > 0u && !out_report->file.bytes)) {
        machine_image_report_free(out_report);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-132: out of memory cloning image report file");
        return 0;
    }
    if (source->byte_count > 0u) {
        memcpy(out_report->file.bytes, source->bytes, source->byte_count);
    }
    for (segment_index = 0u; segment_index < source->segment_count; ++segment_index) {
        MachineImageSegment *dst = &out_report->file.segments[segment_index];
        const MachineImageSegment *src = &source->segments[segment_index];

        dst->name = machine_image_strdup(src->name ? src->name : "");
        dst->image_offset = src->image_offset;
        dst->virtual_address = src->virtual_address;
        dst->byte_count = src->byte_count;
        dst->align = src->align;
        if (!dst->name) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-133: out of memory cloning image segment name");
            return 0;
        }
    }
    for (symbol_index = 0u; symbol_index < source->symbol_count; ++symbol_index) {
        MachineImageSymbol *dst = &out_report->file.symbols[symbol_index];
        const MachineImageSymbol *src = &source->symbols[symbol_index];

        dst->name = machine_image_strdup(src->name ? src->name : "");
        dst->binding = src->binding;
        dst->type = src->type;
        dst->is_defined = src->is_defined;
        dst->segment_index = src->segment_index;
        dst->value_offset = src->value_offset;
        dst->virtual_address = src->virtual_address;
        if (!dst->name) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-134: out of memory cloning image symbol name");
            return 0;
        }
    }
    for (relocation_index = 0u; relocation_index < source->relocation_count; ++relocation_index) {
        MachineImageRelocation *dst = &out_report->file.relocations[relocation_index];
        const MachineImageRelocation *src = &source->relocations[relocation_index];

        dst->source_kind = src->source_kind;
        dst->segment_index = src->segment_index;
        dst->segment_offset = src->segment_offset;
        dst->site_virtual_address = src->site_virtual_address;
        dst->is_resolved = src->is_resolved;
        dst->target_virtual_address = src->target_virtual_address;
        dst->type = src->type;
        dst->symbol_index = src->symbol_index;
        dst->symbol_name = machine_image_strdup(src->symbol_name ? src->symbol_name : "");
        if (!dst->symbol_name) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-135: out of memory cloning image relocation name");
            return 0;
        }
        if (dst->is_resolved) {
            ++resolved_count;
        } else {
            ++unresolved_count;
        }
    }

    if (!machine_image_file_get_header_summary(&out_report->file, &out_report->header_summary)) {
        machine_image_report_free(out_report);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-136: failed to summarize image header");
        return 0;
    }
    out_report->source_elf_artifact_summary = out_report->file.source_elf_artifact_summary;
    if (!machine_image_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary)) {
        machine_image_report_free(out_report);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-156: failed to summarize image target policy");
        return 0;
    }
    out_report->segment_summary_count = out_report->file.segment_count;
    out_report->symbol_summary_count = out_report->file.symbol_count;
    out_report->relocation_summary_count = out_report->file.relocation_count;
    out_report->segment_summaries = out_report->segment_summary_count > 0u
        ? (MachineImageSegmentSummary *)calloc(out_report->segment_summary_count, sizeof(MachineImageSegmentSummary))
        : NULL;
    out_report->segment_symbol_start_indices = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_symbol_counts = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_summaries = out_report->symbol_summary_count > 0u
        ? (MachineImageSymbolSummary *)calloc(out_report->symbol_summary_count, sizeof(MachineImageSymbolSummary))
        : NULL;
    out_report->defined_symbol_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->undefined_symbol_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->global_symbol_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->local_symbol_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->defined_global_symbol_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->undefined_global_symbol_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_relocation_start_indices = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_relocation_counts = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_resolved_relocation_start_indices = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_resolved_relocation_counts = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_unresolved_relocation_start_indices = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->segment_unresolved_relocation_counts = out_report->segment_summary_count > 0u
        ? (size_t *)calloc(out_report->segment_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_relocation_start_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_relocation_counts = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_resolved_relocation_start_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_resolved_relocation_counts = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_unresolved_relocation_start_indices = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->symbol_unresolved_relocation_counts = out_report->symbol_summary_count > 0u
        ? (size_t *)calloc(out_report->symbol_summary_count, sizeof(size_t))
        : NULL;
    out_report->relocation_summaries = out_report->relocation_summary_count > 0u
        ? (MachineImageRelocationSummary *)calloc(
            out_report->relocation_summary_count,
            sizeof(MachineImageRelocationSummary))
        : NULL;
    out_report->resolved_relocation_count = resolved_count;
    out_report->resolved_relocation_indices = resolved_count > 0u
        ? (size_t *)calloc(resolved_count, sizeof(size_t))
        : NULL;
    out_report->unresolved_relocation_count = unresolved_count;
    out_report->unresolved_relocation_indices = unresolved_count > 0u
        ? (size_t *)calloc(unresolved_count, sizeof(size_t))
        : NULL;
    out_report->call_relocation_indices = out_report->relocation_summary_count > 0u
        ? (size_t *)calloc(out_report->relocation_summary_count, sizeof(size_t))
        : NULL;
    out_report->control_primary_relocation_indices = out_report->relocation_summary_count > 0u
        ? (size_t *)calloc(out_report->relocation_summary_count, sizeof(size_t))
        : NULL;
    out_report->control_secondary_relocation_indices = out_report->relocation_summary_count > 0u
        ? (size_t *)calloc(out_report->relocation_summary_count, sizeof(size_t))
        : NULL;
    if ((out_report->segment_summary_count > 0u && !out_report->segment_summaries) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_symbol_start_indices) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_symbol_counts) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_summaries) ||
        (out_report->symbol_summary_count > 0u && !out_report->defined_symbol_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->undefined_symbol_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->global_symbol_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->local_symbol_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->defined_global_symbol_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->undefined_global_symbol_indices) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_relocation_start_indices) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_relocation_counts) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_resolved_relocation_start_indices) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_resolved_relocation_counts) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_unresolved_relocation_start_indices) ||
        (out_report->segment_summary_count > 0u && !out_report->segment_unresolved_relocation_counts) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_relocation_start_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_relocation_counts) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_resolved_relocation_start_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_resolved_relocation_counts) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_unresolved_relocation_start_indices) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_unresolved_relocation_counts) ||
        (out_report->relocation_summary_count > 0u && !out_report->relocation_summaries) ||
        (resolved_count > 0u && !out_report->resolved_relocation_indices) ||
        (unresolved_count > 0u && !out_report->unresolved_relocation_indices) ||
        (out_report->relocation_summary_count > 0u && !out_report->call_relocation_indices) ||
        (out_report->relocation_summary_count > 0u && !out_report->control_primary_relocation_indices) ||
        (out_report->relocation_summary_count > 0u && !out_report->control_secondary_relocation_indices)) {
        machine_image_report_free(out_report);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-137: out of memory building image report summaries");
        return 0;
    }

    for (segment_index = 0u; segment_index < out_report->segment_summary_count; ++segment_index) {
        size_t segment_symbol_count = 0u;
        size_t segment_relocation_count = 0u;
        size_t segment_resolved_relocation_count = 0u;
        size_t segment_unresolved_relocation_count = 0u;

        if (!machine_image_file_get_segment_symbol_count(&out_report->file, segment_index, &segment_symbol_count) ||
            !machine_image_file_get_segment_relocation_count(&out_report->file, segment_index, &segment_relocation_count) ||
            !machine_image_file_get_segment_resolved_relocation_count(
                &out_report->file,
                segment_index,
                &segment_resolved_relocation_count) ||
            !machine_image_file_get_segment_unresolved_relocation_count(
                &out_report->file,
                segment_index,
                &segment_unresolved_relocation_count)) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-157: failed to build segment-local image summary counts");
            return 0;
        }
        out_report->segment_symbol_start_indices[segment_index] = segment_symbol_total;
        out_report->segment_symbol_counts[segment_index] = segment_symbol_count;
        segment_symbol_total += segment_symbol_count;
        out_report->segment_relocation_start_indices[segment_index] = segment_relocation_total;
        out_report->segment_relocation_counts[segment_index] = segment_relocation_count;
        segment_relocation_total += segment_relocation_count;
        out_report->segment_resolved_relocation_start_indices[segment_index] = segment_resolved_relocation_total;
        out_report->segment_resolved_relocation_counts[segment_index] = segment_resolved_relocation_count;
        segment_resolved_relocation_total += segment_resolved_relocation_count;
        out_report->segment_unresolved_relocation_start_indices[segment_index] = segment_unresolved_relocation_total;
        out_report->segment_unresolved_relocation_counts[segment_index] = segment_unresolved_relocation_count;
        segment_unresolved_relocation_total += segment_unresolved_relocation_count;
    }
    for (symbol_index = 0u; symbol_index < out_report->symbol_summary_count; ++symbol_index) {
        size_t symbol_relocation_count = 0u;
        size_t symbol_resolved_relocation_count = 0u;
        size_t symbol_unresolved_relocation_count = 0u;

        if (!machine_image_file_get_symbol_relocation_count(&out_report->file, symbol_index, &symbol_relocation_count) ||
            !machine_image_file_get_symbol_resolved_relocation_count(
                &out_report->file,
                symbol_index,
                &symbol_resolved_relocation_count) ||
            !machine_image_file_get_symbol_unresolved_relocation_count(
                &out_report->file,
                symbol_index,
                &symbol_unresolved_relocation_count)) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-159: failed to build symbol-local image summary counts");
            return 0;
        }
        out_report->symbol_relocation_start_indices[symbol_index] = symbol_relocation_total;
        out_report->symbol_relocation_counts[symbol_index] = symbol_relocation_count;
        symbol_relocation_total += symbol_relocation_count;
        out_report->symbol_resolved_relocation_start_indices[symbol_index] = symbol_resolved_relocation_total;
        out_report->symbol_resolved_relocation_counts[symbol_index] = symbol_resolved_relocation_count;
        symbol_resolved_relocation_total += symbol_resolved_relocation_count;
        out_report->symbol_unresolved_relocation_start_indices[symbol_index] = symbol_unresolved_relocation_total;
        out_report->symbol_unresolved_relocation_counts[symbol_index] = symbol_unresolved_relocation_count;
        symbol_unresolved_relocation_total += symbol_unresolved_relocation_count;
    }

    out_report->segment_symbol_index_count = segment_symbol_total;
    out_report->segment_symbol_indices = segment_symbol_total > 0u
        ? (size_t *)calloc(segment_symbol_total, sizeof(size_t))
        : NULL;
    out_report->segment_relocation_index_count = segment_relocation_total;
    out_report->segment_relocation_indices = segment_relocation_total > 0u
        ? (size_t *)calloc(segment_relocation_total, sizeof(size_t))
        : NULL;
    out_report->segment_resolved_relocation_index_count = segment_resolved_relocation_total;
    out_report->segment_resolved_relocation_indices = segment_resolved_relocation_total > 0u
        ? (size_t *)calloc(segment_resolved_relocation_total, sizeof(size_t))
        : NULL;
    out_report->segment_unresolved_relocation_index_count = segment_unresolved_relocation_total;
    out_report->segment_unresolved_relocation_indices = segment_unresolved_relocation_total > 0u
        ? (size_t *)calloc(segment_unresolved_relocation_total, sizeof(size_t))
        : NULL;
    out_report->symbol_relocation_index_count = symbol_relocation_total;
    out_report->symbol_relocation_indices = symbol_relocation_total > 0u
        ? (size_t *)calloc(symbol_relocation_total, sizeof(size_t))
        : NULL;
    out_report->symbol_resolved_relocation_index_count = symbol_resolved_relocation_total;
    out_report->symbol_resolved_relocation_indices = symbol_resolved_relocation_total > 0u
        ? (size_t *)calloc(symbol_resolved_relocation_total, sizeof(size_t))
        : NULL;
    out_report->symbol_unresolved_relocation_index_count = symbol_unresolved_relocation_total;
    out_report->symbol_unresolved_relocation_indices = symbol_unresolved_relocation_total > 0u
        ? (size_t *)calloc(symbol_unresolved_relocation_total, sizeof(size_t))
        : NULL;
    if ((segment_symbol_total > 0u && !out_report->segment_symbol_indices) ||
        (segment_relocation_total > 0u && !out_report->segment_relocation_indices) ||
        (segment_resolved_relocation_total > 0u && !out_report->segment_resolved_relocation_indices) ||
        (segment_unresolved_relocation_total > 0u && !out_report->segment_unresolved_relocation_indices) ||
        (symbol_relocation_total > 0u && !out_report->symbol_relocation_indices) ||
        (symbol_resolved_relocation_total > 0u && !out_report->symbol_resolved_relocation_indices) ||
        (symbol_unresolved_relocation_total > 0u && !out_report->symbol_unresolved_relocation_indices)) {
        machine_image_report_free(out_report);
        machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-158: out of memory building cached image indices");
        return 0;
    }

    resolved_count = 0u;
    unresolved_count = 0u;
    for (segment_index = 0u; segment_index < out_report->segment_summary_count; ++segment_index) {
        if (!machine_image_segment_get_summary(
                &out_report->file.segments[segment_index],
                &out_report->segment_summaries[segment_index])) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-138: failed to summarize image segment");
            return 0;
        }
    }
    for (symbol_index = 0u; symbol_index < out_report->symbol_summary_count; ++symbol_index) {
        if (!machine_image_symbol_get_summary(
                &out_report->file.symbols[symbol_index],
                &out_report->symbol_summaries[symbol_index])) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-139: failed to summarize image symbol");
            return 0;
        }
        if (out_report->file.symbols[symbol_index].is_defined) {
            out_report->defined_symbol_indices[defined_symbol_count++] = symbol_index;
        } else {
            out_report->undefined_symbol_indices[undefined_symbol_count++] = symbol_index;
        }
        if (out_report->file.symbols[symbol_index].binding == MACHINE_ELF_SYMBOL_GLOBAL) {
            out_report->global_symbol_indices[global_symbol_count++] = symbol_index;
            if (out_report->file.symbols[symbol_index].is_defined) {
                out_report->defined_global_symbol_indices[defined_global_symbol_count++] = symbol_index;
            } else {
                out_report->undefined_global_symbol_indices[undefined_global_symbol_count++] = symbol_index;
            }
        } else {
            out_report->local_symbol_indices[local_symbol_count++] = symbol_index;
        }
    }
    out_report->defined_symbol_count = defined_symbol_count;
    out_report->undefined_symbol_count = undefined_symbol_count;
    out_report->global_symbol_count = global_symbol_count;
    out_report->local_symbol_count = local_symbol_count;
    out_report->defined_global_symbol_count = defined_global_symbol_count;
    out_report->undefined_global_symbol_count = undefined_global_symbol_count;
    if (out_report->file.has_entry) {
        out_report->entry_symbol_summary = out_report->symbol_summaries[out_report->file.entry_symbol_index];
        out_report->has_entry_symbol_summary = 1;
    }
    for (relocation_index = 0u; relocation_index < out_report->relocation_summary_count; ++relocation_index) {
        if (!machine_image_relocation_get_summary(
                &out_report->file.relocations[relocation_index],
                &out_report->relocation_summaries[relocation_index])) {
            machine_image_report_free(out_report);
            machine_image_set_error(error, 0, 0, "MACHINE-IMAGE-140: failed to summarize image relocation");
            return 0;
        }
        if (out_report->file.relocations[relocation_index].is_resolved) {
            out_report->resolved_relocation_indices[resolved_count++] = relocation_index;
        } else {
            out_report->unresolved_relocation_indices[unresolved_count++] = relocation_index;
        }
        switch (out_report->file.relocations[relocation_index].source_kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                out_report->call_relocation_indices[call_relocation_count++] = relocation_index;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                out_report->control_primary_relocation_indices[control_primary_relocation_count++] = relocation_index;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                out_report->control_secondary_relocation_indices[control_secondary_relocation_count++] =
                    relocation_index;
                break;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                break;
        }
    }

    for (segment_index = 0u; segment_index < out_report->segment_summary_count; ++segment_index) {
        size_t segment_symbol_fill_index = out_report->segment_symbol_start_indices[segment_index];
        size_t segment_relocation_fill_index = out_report->segment_relocation_start_indices[segment_index];
        size_t segment_resolved_relocation_fill_index =
            out_report->segment_resolved_relocation_start_indices[segment_index];
        size_t segment_unresolved_relocation_fill_index =
            out_report->segment_unresolved_relocation_start_indices[segment_index];

        for (symbol_index = 0u; symbol_index < out_report->symbol_summary_count; ++symbol_index) {
            const MachineImageSymbol *symbol = &out_report->file.symbols[symbol_index];

            if (symbol->is_defined && symbol->segment_index == segment_index) {
                out_report->segment_symbol_indices[segment_symbol_fill_index++] = symbol_index;
            }
        }
        for (relocation_index = 0u; relocation_index < out_report->relocation_summary_count; ++relocation_index) {
            const MachineImageRelocation *relocation = &out_report->file.relocations[relocation_index];

            if (relocation->segment_index != segment_index) {
                continue;
            }
            out_report->segment_relocation_indices[segment_relocation_fill_index++] = relocation_index;
            if (relocation->is_resolved) {
                out_report->segment_resolved_relocation_indices[segment_resolved_relocation_fill_index++] =
                    relocation_index;
            } else {
                out_report->segment_unresolved_relocation_indices[segment_unresolved_relocation_fill_index++] =
                    relocation_index;
            }
        }
    }
    for (symbol_index = 0u; symbol_index < out_report->symbol_summary_count; ++symbol_index) {
        size_t symbol_relocation_fill_index = out_report->symbol_relocation_start_indices[symbol_index];
        size_t symbol_resolved_relocation_fill_index =
            out_report->symbol_resolved_relocation_start_indices[symbol_index];
        size_t symbol_unresolved_relocation_fill_index =
            out_report->symbol_unresolved_relocation_start_indices[symbol_index];

        for (relocation_index = 0u; relocation_index < out_report->relocation_summary_count; ++relocation_index) {
            const MachineImageRelocation *relocation = &out_report->file.relocations[relocation_index];

            if (relocation->symbol_index != symbol_index) {
                continue;
            }
            out_report->symbol_relocation_indices[symbol_relocation_fill_index++] = relocation_index;
            if (relocation->is_resolved) {
                out_report->symbol_resolved_relocation_indices[symbol_resolved_relocation_fill_index++] =
                    relocation_index;
            } else {
                out_report->symbol_unresolved_relocation_indices[symbol_unresolved_relocation_fill_index++] =
                    relocation_index;
            }
        }
    }
    out_report->call_relocation_count = call_relocation_count;
    out_report->control_primary_relocation_count = control_primary_relocation_count;
    out_report->control_secondary_relocation_count = control_secondary_relocation_count;

    return 1;
}

int machine_image_build_report_from_file_with_profile(const MachineImageFile *source,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile cloned_file;
    int ok;

    machine_image_file_init(&cloned_file);
    ok = machine_image_clone_file(source, &cloned_file, error);
    if (ok) {
        ok = machine_image_apply_profile(&cloned_file, profile, error);
    }
    ok = ok && machine_image_build_report_from_file(&cloned_file, out_report, error);
    machine_image_file_free(&cloned_file);
    return ok;
}

int machine_image_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_file(source, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

#endif

int machine_image_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_file_with_profile(source, profile, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_report(report, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_report_with_profile(report, profile, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_bytes(bytes, byte_count, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_elf_bytes_with_profile(bytes, byte_count, profile, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_ir_report(report, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}

int machine_image_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error) {
    MachineImageFile image_file;
    int ok;

    machine_image_file_init(&image_file);
    ok = machine_image_build_from_machine_ir_report_with_profile(report, profile, &image_file, error) &&
        machine_image_build_report_from_file(&image_file, out_report, error);
    machine_image_file_free(&image_file);
    return ok;
}
