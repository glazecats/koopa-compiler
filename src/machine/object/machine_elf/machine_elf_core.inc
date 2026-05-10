#if defined(__INTELLISENSE__) && !defined(MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ELF_SPLIT_AGGREGATOR)
#define MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT
#include "machine_elf.c"
#else

void machine_elf_file_init(MachineElfFile *elf_file) {
    if (!elf_file) {
        return;
    }
    elf_file->target_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    elf_file->origin_profile = MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32;
    elf_file->relocation_semantics = MACHINE_ELF_RELOCATION_SEMANTICS_UNKNOWN;
    elf_file->sections = NULL;
    elf_file->section_count = 0;
    elf_file->section_capacity = 0;
    elf_file->symbols = NULL;
    elf_file->symbol_count = 0;
    elf_file->symbol_capacity = 0;
    elf_file->relocations = NULL;
    elf_file->relocation_count = 0;
    elf_file->relocation_capacity = 0;
    elf_file->bytes = NULL;
    elf_file->byte_count = 0;
    elf_file->section_header_offset = 0;
    elf_file->text_section_index = 0;
    elf_file->strtab_section_index = 0;
    elf_file->symtab_section_index = 0;
    elf_file->rel_text_section_index = 0;
    elf_file->shstrtab_section_index = 0;
}

#endif

void machine_elf_file_free(MachineElfFile *elf_file) {
    size_t section_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!elf_file) {
        return;
    }
    for (section_index = 0; section_index < elf_file->section_count; ++section_index) {
        free(elf_file->sections[section_index].name);
    }
    for (symbol_index = 0; symbol_index < elf_file->symbol_count; ++symbol_index) {
        free(elf_file->symbols[symbol_index].name);
    }
    for (relocation_index = 0; relocation_index < elf_file->relocation_count; ++relocation_index) {
        free(elf_file->relocations[relocation_index].symbol_name);
    }
    free(elf_file->sections);
    free(elf_file->symbols);
    free(elf_file->relocations);
    free(elf_file->bytes);
    machine_elf_file_init(elf_file);
}

int machine_elf_clone_file(const MachineElfFile *source,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!source || !out_elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-102: invalid elf clone contract");
        return 0;
    }

    machine_elf_file_free(out_elf_file);
    out_elf_file->target_profile = source->target_profile;
    out_elf_file->origin_profile = source->origin_profile;
    out_elf_file->relocation_semantics = source->relocation_semantics;
    out_elf_file->section_count = source->section_count;
    out_elf_file->section_capacity = source->section_count;
    out_elf_file->symbol_count = source->symbol_count;
    out_elf_file->symbol_capacity = source->symbol_count;
    out_elf_file->relocation_count = source->relocation_count;
    out_elf_file->relocation_capacity = source->relocation_count;
    out_elf_file->byte_count = source->byte_count;
    out_elf_file->section_header_offset = source->section_header_offset;
    out_elf_file->text_section_index = source->text_section_index;
    out_elf_file->strtab_section_index = source->strtab_section_index;
    out_elf_file->symtab_section_index = source->symtab_section_index;
    out_elf_file->rel_text_section_index = source->rel_text_section_index;
    out_elf_file->shstrtab_section_index = source->shstrtab_section_index;

    out_elf_file->sections = source->section_count > 0u
        ? (MachineElfSection *)calloc(source->section_count, sizeof(MachineElfSection))
        : NULL;
    out_elf_file->symbols = source->symbol_count > 0u
        ? (MachineElfSymbol *)calloc(source->symbol_count, sizeof(MachineElfSymbol))
        : NULL;
    out_elf_file->relocations = source->relocation_count > 0u
        ? (MachineElfRelocation *)calloc(source->relocation_count, sizeof(MachineElfRelocation))
        : NULL;
    out_elf_file->bytes = source->byte_count > 0u
        ? (unsigned char *)malloc(source->byte_count)
        : NULL;

    if ((source->section_count > 0u && !out_elf_file->sections) ||
        (source->symbol_count > 0u && !out_elf_file->symbols) ||
        (source->relocation_count > 0u && !out_elf_file->relocations) ||
        (source->byte_count > 0u && !out_elf_file->bytes)) {
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-103: out of memory cloning elf artifact");
        return 0;
    }

    if (source->byte_count > 0u) {
        memcpy(out_elf_file->bytes, source->bytes, source->byte_count);
    }

    for (section_index = 0u; section_index < source->section_count; ++section_index) {
        out_elf_file->sections[section_index] = source->sections[section_index];
        out_elf_file->sections[section_index].name = machine_elf_strdup(source->sections[section_index].name);
        if (source->sections[section_index].name && !out_elf_file->sections[section_index].name) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-104: out of memory cloning section names");
            return 0;
        }
    }

    for (symbol_index = 0u; symbol_index < source->symbol_count; ++symbol_index) {
        out_elf_file->symbols[symbol_index] = source->symbols[symbol_index];
        out_elf_file->symbols[symbol_index].name = machine_elf_strdup(source->symbols[symbol_index].name);
        if (source->symbols[symbol_index].name && !out_elf_file->symbols[symbol_index].name) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-105: out of memory cloning symbol names");
            return 0;
        }
    }

    for (relocation_index = 0u; relocation_index < source->relocation_count; ++relocation_index) {
        out_elf_file->relocations[relocation_index] = source->relocations[relocation_index];
        out_elf_file->relocations[relocation_index].symbol_name =
            machine_elf_strdup(source->relocations[relocation_index].symbol_name);
        if (source->relocations[relocation_index].symbol_name &&
            !out_elf_file->relocations[relocation_index].symbol_name) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-106: out of memory cloning relocation names");
            return 0;
        }
    }

    return machine_elf_verify_file(out_elf_file, error);
}

int machine_elf_reprofile_file(MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineElfError *error) {
    if (!elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-107: invalid elf reprofile contract");
        return 0;
    }
    elf_file->target_profile = profile;
    return machine_elf_refresh_bytes(elf_file, error);
}

void machine_elf_report_init(MachineElfReport *report) {
    if (!report) {
        return;
    }
    machine_elf_file_init(&report->file);
    memset(&report->artifact_summary, 0, sizeof(report->artifact_summary));
    memset(&report->relocation_family_summary, 0, sizeof(report->relocation_family_summary));
    memset(&report->header_summary, 0, sizeof(report->header_summary));
    memset(&report->target_policy_summary, 0, sizeof(report->target_policy_summary));
    report->section_summaries = NULL;
    report->section_summary_count = 0u;
    report->symbol_summaries = NULL;
    report->symbol_summary_count = 0u;
    report->relocation_summaries = NULL;
    report->relocation_summary_count = 0u;
}

void machine_elf_report_free(MachineElfReport *report) {
    if (!report) {
        return;
    }
    machine_elf_file_free(&report->file);
    free(report->section_summaries);
    free(report->symbol_summaries);
    free(report->relocation_summaries);
    machine_elf_report_init(report);
}

int machine_elf_file_get_summary(const MachineElfFile *elf_file,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count) {
    if (!elf_file) {
        return 0;
    }
    if (out_section_count) {
        *out_section_count = elf_file->section_count;
    }
    if (out_symbol_count) {
        *out_symbol_count = elf_file->symbol_count;
    }
    if (out_relocation_count) {
        *out_relocation_count = elf_file->relocation_count;
    }
    if (out_byte_count) {
        *out_byte_count = elf_file->byte_count;
    }
    return 1;
}

int machine_elf_file_get_target_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile *out_profile) {
    if (!elf_file || !out_profile) {
        return 0;
    }
    *out_profile = elf_file->target_profile;
    return 1;
}

int machine_elf_file_get_artifact_summary(const MachineElfFile *elf_file,
    MachineElfArtifactSummary *out_summary) {
    if (!elf_file || !out_summary) {
        return 0;
    }
    out_summary->target_profile = elf_file->target_profile;
    out_summary->origin_profile = elf_file->origin_profile;
    out_summary->relocation_semantics = elf_file->relocation_semantics;
    return 1;
}

int machine_elf_file_get_relocation_family_summary(const MachineElfFile *elf_file,
    MachineElfRelocationFamilySummary *out_summary) {
    size_t relocation_index;

    if (!elf_file || !out_summary) {
        return 0;
    }
    memset(out_summary, 0, sizeof(*out_summary));
    for (relocation_index = 0u; relocation_index < elf_file->relocation_count; ++relocation_index) {
        switch (elf_file->relocations[relocation_index].source_kind) {
            case MACHINE_BYTES_FIXUP_CALL_TARGET:
                out_summary->call_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_PRIMARY:
                out_summary->primary_control_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_CONTROL_SECONDARY:
                out_summary->secondary_control_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET:
                out_summary->data_address_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET:
                out_summary->data_load_relocation_count += 1u;
                break;
            case MACHINE_BYTES_FIXUP_DATA_STORE_TARGET:
                out_summary->data_store_relocation_count += 1u;
                break;
            default:
                break;
        }
    }
    return 1;
}

int machine_elf_file_get_target_policy_summary(const MachineElfFile *elf_file,
    MachineElfTargetPolicySummary *out_summary) {
    if (!elf_file || !out_summary) {
        return 0;
    }
    return machine_elf_get_target_policy_summary(elf_file->target_profile, out_summary);
}

int machine_elf_file_get_header_summary(const MachineElfFile *elf_file,
    MachineElfHeaderSummary *out_summary) {
    if (!elf_file || !out_summary || !elf_file->bytes || elf_file->byte_count < MACHINE_ELF_HEADER_SIZE) {
        return 0;
    }
    out_summary->target_profile = elf_file->target_profile;
    out_summary->elf_class = elf_file->bytes[4u];
    out_summary->data_encoding = elf_file->bytes[5u];
    out_summary->ident_version = elf_file->bytes[6u];
    out_summary->object_type = machine_elf_read_u16_le(elf_file->bytes, 16u);
    out_summary->machine = machine_elf_read_u16_le(elf_file->bytes, 18u);
    out_summary->elf_version = machine_elf_read_u32_le(elf_file->bytes, 20u);
    out_summary->flags = machine_elf_read_u32_le(elf_file->bytes, 36u);
    out_summary->section_header_offset = machine_elf_read_u32_le(elf_file->bytes, 32u);
    out_summary->section_header_entry_size = machine_elf_read_u16_le(elf_file->bytes, 46u);
    out_summary->section_header_count = machine_elf_read_u16_le(elf_file->bytes, 48u);
    out_summary->section_name_string_table_index = machine_elf_read_u16_le(elf_file->bytes, 50u);
    return 1;
}

int machine_elf_file_copy_bytes(const MachineElfFile *elf_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    unsigned char *copy;

    if (out_bytes) {
        *out_bytes = NULL;
    }
    if (out_byte_count) {
        *out_byte_count = 0;
    }
    if (!elf_file || !out_bytes) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-100: invalid elf byte-copy contract");
        return 0;
    }
    if (elf_file->byte_count == 0u) {
        return 1;
    }
    if (!elf_file->bytes) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-100: invalid elf byte-copy contract");
        return 0;
    }
    copy = (unsigned char *)malloc(elf_file->byte_count);
    if (!copy) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-101: out of memory copying elf bytes");
        return 0;
    }
    memcpy(copy, elf_file->bytes, elf_file->byte_count);
    *out_bytes = copy;
    if (out_byte_count) {
        *out_byte_count = elf_file->byte_count;
    }
    return 1;
}

int machine_elf_file_get_section(const MachineElfFile *elf_file,
    size_t section_index,
    const MachineElfSection **out_section) {
    if (!elf_file || !out_section || section_index >= elf_file->section_count || !elf_file->sections) {
        return 0;
    }
    *out_section = &elf_file->sections[section_index];
    return 1;
}

int machine_elf_file_find_section_by_name(const MachineElfFile *elf_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineElfSection **out_section) {
    size_t section_index;

    if (!elf_file || !section_name || !elf_file->sections) {
        return 0;
    }
    for (section_index = 0; section_index < elf_file->section_count; ++section_index) {
        if (elf_file->sections[section_index].name &&
            strcmp(elf_file->sections[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_section) {
                *out_section = &elf_file->sections[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_elf_file_get_text_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section) {
    int ok;

    if (out_section_index) {
        *out_section_index = 0u;
    }
    ok = machine_elf_file_get_section(elf_file, elf_file ? elf_file->text_section_index : 0u, out_section);
    if (ok && out_section_index && elf_file) {
        *out_section_index = elf_file->text_section_index;
    }
    return ok;
}

int machine_elf_file_get_strtab_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section) {
    int ok;

    if (out_section_index) {
        *out_section_index = 0u;
    }
    ok = machine_elf_file_get_section(elf_file, elf_file ? elf_file->strtab_section_index : 0u, out_section);
    if (ok && out_section_index && elf_file) {
        *out_section_index = elf_file->strtab_section_index;
    }
    return ok;
}

int machine_elf_file_get_symtab_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section) {
    int ok;

    if (out_section_index) {
        *out_section_index = 0u;
    }
    ok = machine_elf_file_get_section(elf_file, elf_file ? elf_file->symtab_section_index : 0u, out_section);
    if (ok && out_section_index && elf_file) {
        *out_section_index = elf_file->symtab_section_index;
    }
    return ok;
}

int machine_elf_file_get_rel_text_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section) {
    int ok;

    if (out_section_index) {
        *out_section_index = 0u;
    }
    ok = machine_elf_file_get_section(elf_file, elf_file ? elf_file->rel_text_section_index : 0u, out_section);
    if (ok && out_section_index && elf_file) {
        *out_section_index = elf_file->rel_text_section_index;
    }
    return ok;
}

int machine_elf_file_get_shstrtab_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section) {
    int ok;

    if (out_section_index) {
        *out_section_index = 0u;
    }
    ok = machine_elf_file_get_section(elf_file, elf_file ? elf_file->shstrtab_section_index : 0u, out_section);
    if (ok && out_section_index && elf_file) {
        *out_section_index = elf_file->shstrtab_section_index;
    }
    return ok;
}

int machine_elf_file_get_symbol(const MachineElfFile *elf_file,
    size_t symbol_index,
    const MachineElfSymbol **out_symbol) {
    if (!elf_file || !out_symbol || symbol_index >= elf_file->symbol_count || !elf_file->symbols) {
        return 0;
    }
    *out_symbol = &elf_file->symbols[symbol_index];
    return 1;
}

int machine_elf_file_find_symbol_by_name(const MachineElfFile *elf_file,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineElfSymbol **out_symbol) {
    size_t symbol_index;

    if (!elf_file || !symbol_name || !elf_file->symbols) {
        return 0;
    }
    for (symbol_index = 0; symbol_index < elf_file->symbol_count; ++symbol_index) {
        if (elf_file->symbols[symbol_index].name &&
            strcmp(elf_file->symbols[symbol_index].name, symbol_name) == 0) {
            if (out_symbol_index) {
                *out_symbol_index = symbol_index;
            }
            if (out_symbol) {
                *out_symbol = &elf_file->symbols[symbol_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_elf_file_get_first_global_symbol_index(const MachineElfFile *elf_file,
    size_t *out_symbol_index) {
    if (out_symbol_index) {
        *out_symbol_index = 0u;
    }
    if (!elf_file || !out_symbol_index || !elf_file->sections ||
        elf_file->symtab_section_index >= elf_file->section_count) {
        return 0;
    }
    if ((size_t)elf_file->sections[elf_file->symtab_section_index].info > elf_file->symbol_count) {
        return 0;
    }
    *out_symbol_index = (size_t)elf_file->sections[elf_file->symtab_section_index].info;
    return 1;
}

int machine_elf_file_get_relocation(const MachineElfFile *elf_file,
    size_t relocation_index,
    const MachineElfRelocation **out_relocation) {
    if (!elf_file || !out_relocation || relocation_index >= elf_file->relocation_count || !elf_file->relocations) {
        return 0;
    }
    *out_relocation = &elf_file->relocations[relocation_index];
    return 1;
}

int machine_elf_section_get_summary(const MachineElfSection *section,
    MachineElfSectionSummary *out_summary) {
    if (!section || !out_summary) {
        return 0;
    }
    out_summary->name = section->name;
    out_summary->type = section->type;
    out_summary->file_offset = section->file_offset;
    out_summary->byte_count = section->byte_count;
    out_summary->flags = section->flags;
    out_summary->link = section->link;
    out_summary->info = section->info;
    out_summary->align = section->align;
    out_summary->entry_size = section->entry_size;
    return 1;
}

int machine_elf_symbol_get_summary(const MachineElfSymbol *symbol,
    MachineElfSymbolSummary *out_summary) {
    if (!symbol || !out_summary) {
        return 0;
    }
    out_summary->name = symbol->name;
    out_summary->binding = symbol->binding;
    out_summary->type = symbol->type;
    out_summary->is_defined = symbol->is_defined;
    out_summary->section_index = symbol->section_index;
    out_summary->value = symbol->value;
    out_summary->size = symbol->size;
    return 1;
}

int machine_elf_relocation_get_summary(const MachineElfRelocation *relocation,
    MachineElfRelocationSummary *out_summary) {
    if (!relocation || !out_summary) {
        return 0;
    }
    out_summary->source_kind = relocation->source_kind;
    out_summary->section_index = relocation->section_index;
    out_summary->offset = relocation->offset;
    out_summary->symbol_index = relocation->symbol_index;
    out_summary->type = relocation->type;
    out_summary->symbol_name = relocation->symbol_name;
    return 1;
}
