#if defined(__INTELLISENSE__) && !defined(MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ELF_SPLIT_AGGREGATOR)
#define MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT
#include "machine_elf.c"
#else

int machine_elf_build_report_from_file(const MachineElfFile *source,
    MachineElfReport *out_report,
    MachineElfError *error) {
    size_t section_index;
    size_t symbol_index;
    size_t relocation_index;

    if (!source || !out_report) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-107: invalid elf report build contract");
        return 0;
    }

    machine_elf_report_free(out_report);
    if (!machine_elf_clone_file(source, &out_report->file, error) ||
        !machine_elf_file_get_header_summary(&out_report->file, &out_report->header_summary) ||
        !machine_elf_file_get_target_policy_summary(&out_report->file, &out_report->target_policy_summary)) {
        machine_elf_report_free(out_report);
        if (!error || error->message[0] == '\0') {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-108: failed to source header/policy summary for report");
        }
        return 0;
    }

    out_report->section_summary_count = out_report->file.section_count;
    out_report->symbol_summary_count = out_report->file.symbol_count;
    out_report->relocation_summary_count = out_report->file.relocation_count;
    out_report->section_summaries = out_report->section_summary_count > 0u
        ? (MachineElfSectionSummary *)calloc(out_report->section_summary_count, sizeof(MachineElfSectionSummary))
        : NULL;
    out_report->symbol_summaries = out_report->symbol_summary_count > 0u
        ? (MachineElfSymbolSummary *)calloc(out_report->symbol_summary_count, sizeof(MachineElfSymbolSummary))
        : NULL;
    out_report->relocation_summaries = out_report->relocation_summary_count > 0u
        ? (MachineElfRelocationSummary *)calloc(
            out_report->relocation_summary_count,
            sizeof(MachineElfRelocationSummary))
        : NULL;

    if ((out_report->section_summary_count > 0u && !out_report->section_summaries) ||
        (out_report->symbol_summary_count > 0u && !out_report->symbol_summaries) ||
        (out_report->relocation_summary_count > 0u && !out_report->relocation_summaries)) {
        machine_elf_report_free(out_report);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-109: out of memory building elf report summaries");
        return 0;
    }

    for (section_index = 0u; section_index < out_report->section_summary_count; ++section_index) {
        if (!machine_elf_section_get_summary(
                &out_report->file.sections[section_index],
                &out_report->section_summaries[section_index])) {
            machine_elf_report_free(out_report);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-110: failed to summarize elf section");
            return 0;
        }
    }
    for (symbol_index = 0u; symbol_index < out_report->symbol_summary_count; ++symbol_index) {
        if (!machine_elf_symbol_get_summary(
                &out_report->file.symbols[symbol_index],
                &out_report->symbol_summaries[symbol_index])) {
            machine_elf_report_free(out_report);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-111: failed to summarize elf symbol");
            return 0;
        }
    }
    for (relocation_index = 0u; relocation_index < out_report->relocation_summary_count; ++relocation_index) {
        if (!machine_elf_relocation_get_summary(
                &out_report->file.relocations[relocation_index],
                &out_report->relocation_summaries[relocation_index])) {
            machine_elf_report_free(out_report);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-112: failed to summarize elf relocation");
            return 0;
        }
    }

    return 1;
}

#endif

int machine_elf_build_report_from_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error) {
    MachineElfFile cloned_file;
    int ok;

    machine_elf_file_init(&cloned_file);
    ok = machine_elf_clone_file(source, &cloned_file, error);
    if (ok && profile != (MachineElfTargetProfile)-1) {
        ok = machine_elf_reprofile_file(&cloned_file, profile, error);
    }
    ok = ok && machine_elf_build_report_from_file(&cloned_file, out_report, error);
    machine_elf_file_free(&cloned_file);
    return ok;
}

int machine_elf_build_bytes_from_file(const MachineElfFile *source,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    return machine_elf_build_bytes_from_file_with_profile(
        source,
        (MachineElfTargetProfile)-1,
        out_bytes,
        out_byte_count,
        error);
}

int machine_elf_build_bytes_from_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    MachineElfFile cloned_file;
    int ok;

    machine_elf_file_init(&cloned_file);
    ok = machine_elf_clone_file(source, &cloned_file, error);
    if (ok && profile != (MachineElfTargetProfile)-1) {
        ok = machine_elf_reprofile_file(&cloned_file, profile, error);
    }
    ok = ok && machine_elf_file_copy_bytes(&cloned_file, out_bytes, out_byte_count, error);
    machine_elf_file_free(&cloned_file);
    return ok;
}

int machine_elf_build_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    const MachineRelocFile *reloc_file = NULL;
    const MachineObjectFile *object_file = NULL;
    const MachineObjectSection *text_section = NULL;
    MachineElfTargetPolicy policy;
    size_t text_size;
    size_t strtab_size = 1u;
    size_t shstrtab_size = 1u;
    size_t symbol_count;
    size_t relocation_count;
    size_t text_offset;
    size_t strtab_offset;
    size_t symtab_offset;
    size_t rel_offset;
    size_t shstrtab_offset;
    size_t shoff;
    size_t byte_count;
    size_t string_cursor;
    size_t shstring_cursor;
    size_t block_symbol_count = 0u;
    size_t first_global_symbol_index = 1u;
    size_t symbol_index;
    size_t relocation_index;
    size_t next_local_symbol_index;
    size_t next_global_symbol_index;
    size_t *object_symbol_to_elf_index = NULL;
    size_t *elf_symbol_to_object_index = NULL;
    unsigned char *bytes;
    uint32_t text_name_offset;
    uint32_t strtab_name_offset;
    uint32_t symtab_name_offset;
    uint32_t reltext_name_offset;
    uint32_t shstrtab_name_offset;

    if (!container_file || !out_elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-102: invalid elf build contract");
        return 0;
    }
    if (!machine_elf_get_target_policy(profile, &policy)) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-103: unknown elf target profile");
        return 0;
    }
    machine_elf_file_free(out_elf_file);

    if (!machine_container_file_get_reloc_file(container_file, &reloc_file) || !reloc_file ||
        !machine_reloc_file_get_object_file(reloc_file, &object_file) || !object_file ||
        object_file->section_count != 1u ||
        !machine_object_file_get_section(object_file, 0u, &text_section) || !text_section) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-104: invalid machine-container source for elf build");
        return 0;
    }

    text_size = text_section->byte_count;
    symbol_count = object_file->symbol_count + 1u;
    relocation_count = reloc_file->relocation_count;

    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        if (object_file->symbols[symbol_index].kind == MACHINE_BYTES_SYMBOL_BLOCK) {
            ++block_symbol_count;
        }
    }
    first_global_symbol_index = 1u + block_symbol_count;

    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        strtab_size += strlen(object_file->symbols[symbol_index].name) + 1u;
    }

    shstrtab_size += strlen(".text") + 1u;
    shstrtab_size += strlen(".strtab") + 1u;
    shstrtab_size += strlen(".symtab") + 1u;
    shstrtab_size += strlen(".rel.text") + 1u;
    shstrtab_size += strlen(".shstrtab") + 1u;

    text_offset = MACHINE_ELF_HEADER_SIZE;
    strtab_offset = text_offset + text_size;
    symtab_offset = machine_elf_align_up(strtab_offset + strtab_size, 4u);
    rel_offset = machine_elf_align_up(symtab_offset + symbol_count * MACHINE_ELF_SYMBOL_ENTRY_SIZE, 4u);
    shstrtab_offset = rel_offset + relocation_count * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
    shoff = machine_elf_align_up(shstrtab_offset + shstrtab_size, 4u);
    byte_count = shoff + 6u * MACHINE_ELF_SECTION_HEADER_SIZE;

    out_elf_file->sections = (MachineElfSection *)calloc(6u, sizeof(MachineElfSection));
    out_elf_file->symbols = (MachineElfSymbol *)calloc(symbol_count, sizeof(MachineElfSymbol));
    out_elf_file->relocations =
        relocation_count > 0u ? (MachineElfRelocation *)calloc(relocation_count, sizeof(MachineElfRelocation)) : NULL;
    bytes = (unsigned char *)calloc(byte_count == 0u ? 1u : byte_count, 1u);
    if (!out_elf_file->sections || !out_elf_file->symbols ||
        (relocation_count > 0u && !out_elf_file->relocations) || !bytes) {
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-105: out of memory building elf artifact");
        return 0;
    }

    out_elf_file->target_profile = profile;
    out_elf_file->section_count = 6u;
    out_elf_file->section_capacity = 6u;
    out_elf_file->symbol_count = symbol_count;
    out_elf_file->symbol_capacity = symbol_count;
    out_elf_file->relocation_count = relocation_count;
    out_elf_file->relocation_capacity = relocation_count;
    out_elf_file->bytes = bytes;
    out_elf_file->byte_count = byte_count;
    out_elf_file->section_header_offset = shoff;
    out_elf_file->text_section_index = 1u;
    out_elf_file->strtab_section_index = 2u;
    out_elf_file->symtab_section_index = 3u;
    out_elf_file->rel_text_section_index = 4u;
    out_elf_file->shstrtab_section_index = 5u;

    out_elf_file->sections[0].name = machine_elf_strdup("");
    out_elf_file->sections[0].type = MACHINE_ELF_SECTION_NULL;
    out_elf_file->sections[1].name = machine_elf_strdup(".text");
    out_elf_file->sections[1].type = MACHINE_ELF_SECTION_PROGBITS;
    out_elf_file->sections[1].file_offset = text_offset;
    out_elf_file->sections[1].byte_count = text_size;
    out_elf_file->sections[1].flags = MACHINE_ELF_SHF_ALLOC | MACHINE_ELF_SHF_EXECINSTR;
    out_elf_file->sections[1].align = 1u;
    out_elf_file->sections[2].name = machine_elf_strdup(".strtab");
    out_elf_file->sections[2].type = MACHINE_ELF_SECTION_STRTAB;
    out_elf_file->sections[2].file_offset = strtab_offset;
    out_elf_file->sections[2].byte_count = strtab_size;
    out_elf_file->sections[2].align = 1u;
    out_elf_file->sections[3].name = machine_elf_strdup(".symtab");
    out_elf_file->sections[3].type = MACHINE_ELF_SECTION_SYMTAB;
    out_elf_file->sections[3].file_offset = symtab_offset;
    out_elf_file->sections[3].byte_count = symbol_count * MACHINE_ELF_SYMBOL_ENTRY_SIZE;
    out_elf_file->sections[3].link = (uint32_t)out_elf_file->strtab_section_index;
    out_elf_file->sections[3].info = (uint32_t)first_global_symbol_index;
    out_elf_file->sections[3].align = 4u;
    out_elf_file->sections[3].entry_size = MACHINE_ELF_SYMBOL_ENTRY_SIZE;
    out_elf_file->sections[4].name = machine_elf_strdup(".rel.text");
    out_elf_file->sections[4].type = MACHINE_ELF_SECTION_REL;
    out_elf_file->sections[4].file_offset = rel_offset;
    out_elf_file->sections[4].byte_count = relocation_count * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
    out_elf_file->sections[4].link = (uint32_t)out_elf_file->symtab_section_index;
    out_elf_file->sections[4].info = (uint32_t)out_elf_file->text_section_index;
    out_elf_file->sections[4].align = 4u;
    out_elf_file->sections[4].entry_size = MACHINE_ELF_RELOCATION_ENTRY_SIZE;
    out_elf_file->sections[5].name = machine_elf_strdup(".shstrtab");
    out_elf_file->sections[5].type = MACHINE_ELF_SECTION_STRTAB;
    out_elf_file->sections[5].file_offset = shstrtab_offset;
    out_elf_file->sections[5].byte_count = shstrtab_size;
    out_elf_file->sections[5].align = 1u;

    for (symbol_index = 0; symbol_index < 6u; ++symbol_index) {
        if (!out_elf_file->sections[symbol_index].name) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-106: out of memory copying elf section names");
            return 0;
        }
    }

    memcpy(bytes + text_offset, text_section->bytes, text_size);
    string_cursor = 1u;
    bytes[strtab_offset] = '\0';
    if (object_file->symbol_count > 0u) {
        object_symbol_to_elf_index = (size_t *)calloc(object_file->symbol_count, sizeof(size_t));
        elf_symbol_to_object_index = (size_t *)calloc(symbol_count, sizeof(size_t));
        if (!object_symbol_to_elf_index || !elf_symbol_to_object_index) {
            free(elf_symbol_to_object_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-107: out of memory building elf symbol index map");
            return 0;
        }
    }
    out_elf_file->symbols[0].name = machine_elf_strdup("");
    out_elf_file->symbols[0].binding = MACHINE_ELF_SYMBOL_LOCAL;
    out_elf_file->symbols[0].type = MACHINE_ELF_SYMBOL_NOTYPE;
    if (!out_elf_file->symbols[0].name) {
        free(object_symbol_to_elf_index);
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-108: out of memory initializing null symbol");
        return 0;
    }

    next_local_symbol_index = 1u;
    next_global_symbol_index = first_global_symbol_index;
    for (symbol_index = 0; symbol_index < object_file->symbol_count; ++symbol_index) {
        const MachineObjectSymbol *object_symbol = &object_file->symbols[symbol_index];
        size_t elf_symbol_index;

        if (object_symbol->kind == MACHINE_BYTES_SYMBOL_BLOCK) {
            elf_symbol_index = next_local_symbol_index++;
        } else {
            elf_symbol_index = next_global_symbol_index++;
        }
        object_symbol_to_elf_index[symbol_index] = elf_symbol_index;
        elf_symbol_to_object_index[elf_symbol_index] = symbol_index;
    }
    for (symbol_index = 1u; symbol_index < symbol_count; ++symbol_index) {
        const MachineObjectSymbol *object_symbol = &object_file->symbols[elf_symbol_to_object_index[symbol_index]];
        size_t elf_symbol_index = symbol_index;
        MachineElfSymbol *elf_symbol;
        size_t entry_offset;
        uint32_t st_name =
            machine_elf_append_string(bytes, strtab_offset, strtab_size, &string_cursor, object_symbol->name);
        uint8_t info = 0u;

        elf_symbol = &out_elf_file->symbols[elf_symbol_index];
        entry_offset = symtab_offset + elf_symbol_index * MACHINE_ELF_SYMBOL_ENTRY_SIZE;
        elf_symbol->name = machine_elf_strdup(object_symbol->name);
        elf_symbol->binding = object_symbol->kind == MACHINE_BYTES_SYMBOL_BLOCK
            ? MACHINE_ELF_SYMBOL_LOCAL
            : MACHINE_ELF_SYMBOL_GLOBAL;
        elf_symbol->type = object_symbol->kind == MACHINE_BYTES_SYMBOL_FUNCTION
            ? MACHINE_ELF_SYMBOL_FUNC
            : MACHINE_ELF_SYMBOL_NOTYPE;
        elf_symbol->is_defined = object_symbol->is_defined;
        elf_symbol->section_index = object_symbol->is_defined ? out_elf_file->text_section_index : 0u;
        elf_symbol->value = (uint32_t)object_symbol->byte_offset_in_section;
        elf_symbol->size = (uint32_t)object_symbol->byte_count;
        if (!elf_symbol->name) {
            free(object_symbol_to_elf_index);
            free(elf_symbol_to_object_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-109: out of memory copying elf symbol names");
            return 0;
        }
        info = (uint8_t)(((uint32_t)elf_symbol->binding << 4u) | (uint32_t)elf_symbol->type);
        machine_elf_write_u32_le(bytes, entry_offset + 0u, st_name);
        machine_elf_write_u32_le(bytes, entry_offset + 4u, elf_symbol->value);
        machine_elf_write_u32_le(bytes, entry_offset + 8u, elf_symbol->size);
        bytes[entry_offset + 12u] = info;
        bytes[entry_offset + 13u] = 0u;
        machine_elf_write_u16_le(
            bytes,
            entry_offset + 14u,
            (uint16_t)(elf_symbol->is_defined ? elf_symbol->section_index : MACHINE_ELF_SHN_UNDEF));
    }

    for (relocation_index = 0; relocation_index < reloc_file->relocation_count; ++relocation_index) {
        const MachineRelocation *relocation = &reloc_file->relocations[relocation_index];
        MachineElfRelocation *elf_relocation = &out_elf_file->relocations[relocation_index];
        size_t entry_offset = rel_offset + relocation_index * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
        uint32_t elf_symbol_index =
            relocation->has_target_symbol_index
                ? (uint32_t)object_symbol_to_elf_index[relocation->target_symbol_index]
                : 0u;
        uint32_t elf_type = machine_elf_map_relocation_type(profile, relocation->kind);

        elf_relocation->source_kind = relocation->kind;
        elf_relocation->section_index = out_elf_file->rel_text_section_index;
        elf_relocation->offset = relocation->patch_byte_offset;
        elf_relocation->symbol_index = elf_symbol_index;
        elf_relocation->type = elf_type;
        elf_relocation->symbol_name = machine_elf_strdup(relocation->target_name);
        if (relocation->target_name && !elf_relocation->symbol_name) {
            free(object_symbol_to_elf_index);
            free(elf_symbol_to_object_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-110: out of memory copying elf relocation symbol names");
            return 0;
        }
        machine_elf_write_u32_le(bytes, entry_offset + 0u, relocation->patch_byte_offset);
        machine_elf_write_u32_le(bytes, entry_offset + 4u, (elf_symbol_index << 8u) | (elf_type & 0xFFu));
    }

    shstring_cursor = 1u;
    bytes[shstrtab_offset] = '\0';
    text_name_offset = machine_elf_append_string(bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".text");
    strtab_name_offset = machine_elf_append_string(bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".strtab");
    symtab_name_offset = machine_elf_append_string(bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".symtab");
    reltext_name_offset = machine_elf_append_string(bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".rel.text");
    shstrtab_name_offset = machine_elf_append_string(bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".shstrtab");

    bytes[0u] = 0x7Fu;
    bytes[1u] = 'E';
    bytes[2u] = 'L';
    bytes[3u] = 'F';
    bytes[4u] = 1u;
    bytes[5u] = 1u;
    bytes[6u] = 1u;
    bytes[7u] = 0u;
    memset(bytes + 8u, 0, 8u);
    machine_elf_write_u16_le(bytes, 16u, MACHINE_ELF_ET_REL);
    machine_elf_write_u16_le(bytes, 18u, policy.machine);
    machine_elf_write_u32_le(bytes, 20u, MACHINE_ELF_EV_CURRENT);
    machine_elf_write_u32_le(bytes, 24u, 0u);
    machine_elf_write_u32_le(bytes, 28u, 0u);
    machine_elf_write_u32_le(bytes, 32u, (uint32_t)shoff);
    machine_elf_write_u32_le(bytes, 36u, policy.flags);
    machine_elf_write_u16_le(bytes, 40u, MACHINE_ELF_HEADER_SIZE);
    machine_elf_write_u16_le(bytes, 42u, 0u);
    machine_elf_write_u16_le(bytes, 44u, 0u);
    machine_elf_write_u16_le(bytes, 46u, MACHINE_ELF_SECTION_HEADER_SIZE);
    machine_elf_write_u16_le(bytes, 48u, 6u);
    machine_elf_write_u16_le(bytes, 50u, (uint16_t)out_elf_file->shstrtab_section_index);

    machine_elf_write_u32_le(bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, text_name_offset);
    machine_elf_write_u32_le(bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_PROGBITS);
    machine_elf_write_u32_le(bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 8u, MACHINE_ELF_SHF_ALLOC | MACHINE_ELF_SHF_EXECINSTR);
    machine_elf_write_u32_le(bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)text_offset);
    machine_elf_write_u32_le(bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)text_size);
    machine_elf_write_u32_le(bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 1u);

    machine_elf_write_u32_le(bytes, shoff + 2u * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, strtab_name_offset);
    machine_elf_write_u32_le(bytes, shoff + 2u * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_STRTAB);
    machine_elf_write_u32_le(bytes, shoff + 2u * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)strtab_offset);
    machine_elf_write_u32_le(bytes, shoff + 2u * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)strtab_size);
    machine_elf_write_u32_le(bytes, shoff + 2u * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 1u);

    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, symtab_name_offset);
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_SYMTAB);
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)symtab_offset);
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)(symbol_count * MACHINE_ELF_SYMBOL_ENTRY_SIZE));
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 24u, (uint32_t)out_elf_file->strtab_section_index);
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 28u, out_elf_file->sections[3].info);
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 4u);
    machine_elf_write_u32_le(bytes, shoff + 3u * MACHINE_ELF_SECTION_HEADER_SIZE + 36u, MACHINE_ELF_SYMBOL_ENTRY_SIZE);

    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, reltext_name_offset);
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_REL);
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)rel_offset);
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)(relocation_count * MACHINE_ELF_RELOCATION_ENTRY_SIZE));
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 24u, (uint32_t)out_elf_file->symtab_section_index);
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 28u, (uint32_t)out_elf_file->text_section_index);
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 4u);
    machine_elf_write_u32_le(bytes, shoff + 4u * MACHINE_ELF_SECTION_HEADER_SIZE + 36u, MACHINE_ELF_RELOCATION_ENTRY_SIZE);

    machine_elf_write_u32_le(bytes, shoff + 5u * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, shstrtab_name_offset);
    machine_elf_write_u32_le(bytes, shoff + 5u * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_STRTAB);
    machine_elf_write_u32_le(bytes, shoff + 5u * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)shstrtab_offset);
    machine_elf_write_u32_le(bytes, shoff + 5u * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)shstrtab_size);
    machine_elf_write_u32_le(bytes, shoff + 5u * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 1u);

    if (string_cursor != strtab_size || shstring_cursor != shstrtab_size) {
        free(object_symbol_to_elf_index);
        free(elf_symbol_to_object_index);
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-111: string table layout mismatch");
        return 0;
    }

    free(object_symbol_to_elf_index);
    free(elf_symbol_to_object_index);
    return machine_elf_verify_file(out_elf_file, error);
}

int machine_elf_build_from_machine_container_file(const MachineContainerFile *container_file,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    return machine_elf_build_from_machine_container_file_with_profile(
        container_file,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_elf_file,
        error);
}

int machine_elf_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    MachineContainerFile container_file;
    MachineContainerError container_error;
    int ok;

    if (!report || !out_elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-112: invalid machine-ir elf build contract");
        return 0;
    }
    machine_container_file_init(&container_file);
    memset(&container_error, 0, sizeof(container_error));
    ok = machine_container_build_from_machine_ir_report_with_profile(
        report,
        machine_elf_bytes_target_profile(profile),
        &container_file,
        &container_error);
    if (!ok) {
        machine_elf_set_error(error, container_error.line, container_error.column, "%s", container_error.message);
        machine_container_file_free(&container_file);
        return 0;
    }
    ok = machine_elf_build_from_machine_container_file_with_profile(&container_file, profile, out_elf_file, error);
    machine_container_file_free(&container_file);
    return ok;
}

int machine_elf_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    return machine_elf_build_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_elf_file,
        error);
}

int machine_elf_build_bytes_from_machine_container_file(const MachineContainerFile *container_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    return machine_elf_build_bytes_from_machine_container_file_with_profile(
        container_file,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_bytes,
        out_byte_count,
        error);
}

int machine_elf_build_bytes_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_container_file_with_profile(container_file, profile, &elf_file, error) &&
        machine_elf_file_copy_bytes(&elf_file, out_bytes, out_byte_count, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_bytes_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    return machine_elf_build_bytes_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_bytes,
        out_byte_count,
        error);
}

int machine_elf_build_bytes_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_ir_report_with_profile(report, profile, &elf_file, error) &&
        machine_elf_file_copy_bytes(&elf_file, out_bytes, out_byte_count, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_file_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    return machine_elf_build_file_from_bytes_with_profile(
        bytes,
        byte_count,
        (MachineElfTargetProfile)-1,
        out_elf_file,
        error);
}

int machine_elf_build_file_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    int ok;

    if (!bytes || !out_elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-131: invalid elf build-from-bytes contract");
        return 0;
    }

    ok = machine_elf_parse_file_from_bytes(bytes, byte_count, out_elf_file, error);
    if (ok && profile != (MachineElfTargetProfile)-1) {
        out_elf_file->target_profile = profile;
        ok = machine_elf_refresh_bytes(out_elf_file, error);
    }
    return ok;
}

int machine_elf_build_report_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineElfReport *out_report,
    MachineElfError *error) {
    return machine_elf_build_report_from_bytes_with_profile(
        bytes,
        byte_count,
        (MachineElfTargetProfile)-1,
        out_report,
        error);
}

int machine_elf_build_report_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_file_from_bytes_with_profile(bytes, byte_count, profile, &elf_file, error) &&
        machine_elf_build_report_from_file(&elf_file, out_report, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_report_from_machine_container_file(const MachineContainerFile *container_file,
    MachineElfReport *out_report,
    MachineElfError *error) {
    return machine_elf_build_report_from_machine_container_file_with_profile(
        container_file,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_report,
        error);
}

int machine_elf_build_report_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_container_file_with_profile(container_file, profile, &elf_file, error) &&
        machine_elf_build_report_from_file(&elf_file, out_report, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineElfReport *out_report,
    MachineElfError *error) {
    return machine_elf_build_report_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_report,
        error);
}

int machine_elf_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_ir_report_with_profile(report, profile, &elf_file, error) &&
        machine_elf_build_report_from_file(&elf_file, out_report, error);
    machine_elf_file_free(&elf_file);
    return ok;
}
