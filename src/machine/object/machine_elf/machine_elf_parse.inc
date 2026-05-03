#if defined(__INTELLISENSE__) && !defined(MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ELF_SPLIT_AGGREGATOR)
#define MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT
#include "machine_elf.c"
#else

int machine_elf_parse_file_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineElfFile *out_elf_file,
    MachineElfError *error) {
    MachineElfTargetProfile profile;
    uint16_t object_type;
    uint16_t machine;
    uint32_t version;
    uint32_t section_header_offset;
    uint32_t flags;
    uint16_t section_header_entry_size;
    uint16_t section_count;
    uint16_t section_name_string_table_index;
    size_t section_index;
    size_t symbol_index;
    size_t relocation_index;
    size_t shstrtab_offset;
    size_t shstrtab_size;
    size_t *old_to_new_section_index = NULL;
    size_t first_null_index = (size_t)-1;
    size_t first_text_index = (size_t)-1;
    size_t first_sbss_index = (size_t)-1;
    size_t first_sdata_index = (size_t)-1;
    size_t first_strtab_index = (size_t)-1;
    size_t first_symtab_index = (size_t)-1;
    size_t first_rel_text_index = (size_t)-1;
    size_t first_shstrtab_index = (size_t)-1;

    if (!bytes || !out_elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-131: invalid elf parse contract");
        return 0;
    }
    if (byte_count < MACHINE_ELF_HEADER_SIZE ||
        bytes[0u] != 0x7Fu || bytes[1u] != 'E' || bytes[2u] != 'L' || bytes[3u] != 'F' ||
        bytes[4u] != 1u || bytes[5u] != 1u || bytes[6u] != MACHINE_ELF_EV_CURRENT) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-132: invalid elf parse header");
        return 0;
    }

    object_type = machine_elf_read_u16_le(bytes, 16u);
    machine = machine_elf_read_u16_le(bytes, 18u);
    version = machine_elf_read_u32_le(bytes, 20u);
    section_header_offset = machine_elf_read_u32_le(bytes, 32u);
    flags = machine_elf_read_u32_le(bytes, 36u);
    section_header_entry_size = machine_elf_read_u16_le(bytes, 46u);
    section_count = machine_elf_read_u16_le(bytes, 48u);
    section_name_string_table_index = machine_elf_read_u16_le(bytes, 50u);
    profile = machine_elf_profile_from_header(machine, flags);

    if (object_type != MACHINE_ELF_ET_REL ||
        version != MACHINE_ELF_EV_CURRENT ||
        machine_elf_read_u32_le(bytes, 24u) != 0u ||
        machine_elf_read_u32_le(bytes, 28u) != 0u ||
        machine_elf_read_u16_le(bytes, 40u) != MACHINE_ELF_HEADER_SIZE ||
        machine_elf_read_u16_le(bytes, 42u) != 0u ||
        machine_elf_read_u16_le(bytes, 44u) != 0u ||
        section_header_entry_size != MACHINE_ELF_SECTION_HEADER_SIZE ||
        section_count < 5u ||
        section_name_string_table_index >= section_count ||
        section_header_offset > byte_count ||
        byte_count - section_header_offset < (size_t)section_count * MACHINE_ELF_SECTION_HEADER_SIZE ||
        profile == (MachineElfTargetProfile)-1) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-133: unsupported elf parse layout");
        return 0;
    }

    shstrtab_offset = machine_elf_read_u32_le(
        bytes,
        section_header_offset +
            (size_t)section_name_string_table_index * MACHINE_ELF_SECTION_HEADER_SIZE +
            16u);
    shstrtab_size = machine_elf_read_u32_le(
        bytes,
        section_header_offset +
            (size_t)section_name_string_table_index * MACHINE_ELF_SECTION_HEADER_SIZE +
            20u);
    if (shstrtab_offset > byte_count || shstrtab_size > byte_count - shstrtab_offset) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-134: invalid shstrtab span");
        return 0;
    }

    machine_elf_file_free(out_elf_file);
    out_elf_file->target_profile = profile;
    out_elf_file->origin_profile = profile;
    out_elf_file->relocation_semantics = MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE;
    out_elf_file->bytes = (unsigned char *)malloc(byte_count == 0u ? 1u : byte_count);
    out_elf_file->sections = (MachineElfSection *)calloc(section_count, sizeof(MachineElfSection));
    if (!out_elf_file->bytes || !out_elf_file->sections) {
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-135: out of memory parsing elf sections");
        return 0;
    }
    memcpy(out_elf_file->bytes, bytes, byte_count);
    out_elf_file->byte_count = byte_count;
    out_elf_file->section_count = section_count;
    out_elf_file->section_capacity = section_count;
    out_elf_file->section_header_offset = section_header_offset;
    out_elf_file->shstrtab_section_index = section_name_string_table_index;

    for (section_index = 0u; section_index < section_count; ++section_index) {
        size_t header_offset = section_header_offset + section_index * MACHINE_ELF_SECTION_HEADER_SIZE;
        uint32_t sh_name = machine_elf_read_u32_le(bytes, header_offset + 0u);
        uint32_t sh_type = machine_elf_read_u32_le(bytes, header_offset + 4u);
        MachineElfSection *section = &out_elf_file->sections[section_index];
        size_t name_length;

        if (sh_name >= shstrtab_size) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-136: section name offset exceeds shstrtab");
            return 0;
        }
        if (!machine_elf_string_length_within_limit(
                bytes + shstrtab_offset,
                sh_name,
                shstrtab_size,
                &name_length)) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-137: unterminated section name");
            return 0;
        }

        section->name = machine_elf_strdup((const char *)(bytes + shstrtab_offset + sh_name));
        if (!section->name) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-138: out of memory copying section name");
            return 0;
        }
        section->type = (MachineElfSectionType)sh_type;
        section->file_offset = machine_elf_read_u32_le(bytes, header_offset + 16u);
        section->byte_count = machine_elf_read_u32_le(bytes, header_offset + 20u);
        section->flags = machine_elf_read_u32_le(bytes, header_offset + 8u);
        section->link = machine_elf_read_u32_le(bytes, header_offset + 24u);
        section->info = machine_elf_read_u32_le(bytes, header_offset + 28u);
        section->align = machine_elf_read_u32_le(bytes, header_offset + 32u);
        section->entry_size = machine_elf_read_u32_le(bytes, header_offset + 36u);

        if ((section->type != MACHINE_ELF_SECTION_NULL || section->file_offset != 0u || section->byte_count != 0u) &&
            ((section->type != MACHINE_ELF_SECTION_NOBITS &&
                    (section->file_offset > byte_count || section->byte_count > byte_count - section->file_offset)) ||
                (section->type == MACHINE_ELF_SECTION_NOBITS && section->file_offset > byte_count))) {
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-139: parsed section exceeds file span");
            return 0;
        }

        if (section->type == MACHINE_ELF_SECTION_NULL && strcmp(section->name, "") == 0) {
            if (first_null_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed null section");
                return 0;
            }
            first_null_index = section_index;
        } else if (strcmp(section->name, ".text") == 0) {
            if (first_text_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .text section");
                return 0;
            }
            first_text_index = section_index;
        } else if (strcmp(section->name, ".sbss") == 0) {
            if (first_sbss_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .sbss section");
                return 0;
            }
            first_sbss_index = section_index;
        } else if (strcmp(section->name, ".sdata") == 0) {
            if (first_sdata_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .sdata section");
                return 0;
            }
            first_sdata_index = section_index;
        } else if (strcmp(section->name, ".strtab") == 0) {
            if (first_strtab_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .strtab section");
                return 0;
            }
            first_strtab_index = section_index;
        } else if (strcmp(section->name, ".symtab") == 0) {
            if (first_symtab_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .symtab section");
                return 0;
            }
            first_symtab_index = section_index;
        } else if (strcmp(section->name, ".rel.text") == 0) {
            if (first_rel_text_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .rel.text section");
                return 0;
            }
            first_rel_text_index = section_index;
        } else if (strcmp(section->name, ".shstrtab") == 0) {
            if (first_shstrtab_index != (size_t)-1) {
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: duplicate parsed .shstrtab section");
                return 0;
            }
            first_shstrtab_index = section_index;
        }
    }

    if (first_text_index == (size_t)-1 || first_strtab_index == (size_t)-1 ||
        first_symtab_index == (size_t)-1 ||
        first_shstrtab_index == (size_t)-1) {
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-140: missing required parsed sections");
        return 0;
    }
    if ((size_t)section_name_string_table_index != first_shstrtab_index ||
        out_elf_file->sections[first_symtab_index].link != first_strtab_index ||
        (first_rel_text_index != (size_t)-1 &&
            (out_elf_file->sections[first_rel_text_index].link != first_symtab_index ||
             out_elf_file->sections[first_rel_text_index].info != first_text_index))) {
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-141: parsed section references are inconsistent");
        return 0;
    }
    old_to_new_section_index = (size_t *)malloc((size_t)section_count * sizeof(size_t));
    if (!old_to_new_section_index) {
        machine_elf_file_free(out_elf_file);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-142: out of memory normalizing parsed sections");
        return 0;
    }
    for (section_index = 0u; section_index < section_count; ++section_index) {
        old_to_new_section_index[section_index] = (size_t)-1;
    }
    if (first_null_index != (size_t)-1) {
        old_to_new_section_index[first_null_index] = 0u;
    }
    old_to_new_section_index[first_text_index] = 1u;
    {
        size_t next_section_index = 2u;
        size_t canonical_section_count;
        size_t canonical_strtab_index;
        size_t canonical_symtab_index;
        size_t canonical_rel_text_index;
        size_t canonical_shstrtab_index;

        if (first_sbss_index != (size_t)-1) {
            old_to_new_section_index[first_sbss_index] = next_section_index++;
        }
        if (first_sdata_index != (size_t)-1) {
            old_to_new_section_index[first_sdata_index] = next_section_index++;
        }
        canonical_strtab_index = next_section_index++;
        canonical_symtab_index = next_section_index++;
        canonical_rel_text_index = next_section_index++;
        canonical_shstrtab_index = next_section_index++;
        old_to_new_section_index[first_strtab_index] = canonical_strtab_index;
        old_to_new_section_index[first_symtab_index] = canonical_symtab_index;
        if (first_rel_text_index != (size_t)-1) {
            old_to_new_section_index[first_rel_text_index] = canonical_rel_text_index;
        }
        old_to_new_section_index[first_shstrtab_index] = canonical_shstrtab_index;
        canonical_section_count = canonical_shstrtab_index + 1u;
    {
        MachineElfSection *canonical_sections =
            (MachineElfSection *)calloc(canonical_section_count, sizeof(MachineElfSection));
        if (!canonical_sections) {
            free(old_to_new_section_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-142: out of memory canonicalizing parsed sections");
            return 0;
        }
        if (first_null_index != (size_t)-1) {
            canonical_sections[0] = out_elf_file->sections[first_null_index];
        } else {
            canonical_sections[0].name = machine_elf_strdup("");
            if (!canonical_sections[0].name) {
                free(canonical_sections);
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-142: out of memory synthesizing null section");
                return 0;
            }
            canonical_sections[0].type = MACHINE_ELF_SECTION_NULL;
            canonical_sections[0].file_offset = 0u;
            canonical_sections[0].byte_count = 0u;
            canonical_sections[0].flags = 0u;
            canonical_sections[0].link = 0u;
            canonical_sections[0].info = 0u;
            canonical_sections[0].align = 0u;
            canonical_sections[0].entry_size = 0u;
        }
        canonical_sections[1] = out_elf_file->sections[first_text_index];
        if (first_sbss_index != (size_t)-1) {
            canonical_sections[old_to_new_section_index[first_sbss_index]] = out_elf_file->sections[first_sbss_index];
        }
        if (first_sdata_index != (size_t)-1) {
            canonical_sections[old_to_new_section_index[first_sdata_index]] = out_elf_file->sections[first_sdata_index];
        }
        canonical_sections[canonical_strtab_index] = out_elf_file->sections[first_strtab_index];
        canonical_sections[canonical_symtab_index] = out_elf_file->sections[first_symtab_index];
        if (first_rel_text_index != (size_t)-1) {
            canonical_sections[canonical_rel_text_index] = out_elf_file->sections[first_rel_text_index];
        } else {
            canonical_sections[canonical_rel_text_index].name = machine_elf_strdup(".rel.text");
            if (!canonical_sections[canonical_rel_text_index].name) {
                free(canonical_sections);
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-142: out of memory synthesizing empty .rel.text");
                return 0;
            }
        }
        canonical_sections[canonical_shstrtab_index] = out_elf_file->sections[first_shstrtab_index];
        for (section_index = 0u; section_index < section_count; ++section_index) {
            if (old_to_new_section_index[section_index] == (size_t)-1) {
                free(out_elf_file->sections[section_index].name);
            }
        }
        free(out_elf_file->sections);
        out_elf_file->sections = canonical_sections;
        out_elf_file->text_section_index = 1u;
        out_elf_file->strtab_section_index = canonical_strtab_index;
        out_elf_file->symtab_section_index = canonical_symtab_index;
        out_elf_file->rel_text_section_index = canonical_rel_text_index;
        out_elf_file->shstrtab_section_index = canonical_shstrtab_index;
        out_elf_file->section_count = canonical_section_count;
        out_elf_file->section_capacity = canonical_section_count;
    }
    }

    {
        const MachineElfSection *strtab_section = &out_elf_file->sections[old_to_new_section_index[first_strtab_index]];
        const MachineElfSection *symtab_section = &out_elf_file->sections[old_to_new_section_index[first_symtab_index]];
        const MachineElfSection *rel_text_section =
            first_rel_text_index != (size_t)-1
                ? &out_elf_file->sections[old_to_new_section_index[first_rel_text_index]]
                : NULL;

        if (symtab_section->entry_size != MACHINE_ELF_SYMBOL_ENTRY_SIZE ||
            (rel_text_section && rel_text_section->entry_size != MACHINE_ELF_RELOCATION_ENTRY_SIZE) ||
            symtab_section->byte_count % MACHINE_ELF_SYMBOL_ENTRY_SIZE != 0u ||
            (rel_text_section && rel_text_section->byte_count % MACHINE_ELF_RELOCATION_ENTRY_SIZE != 0u) ||
            strtab_section->file_offset > byte_count ||
            strtab_section->byte_count > byte_count - strtab_section->file_offset) {
            free(old_to_new_section_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-143: invalid parsed table layout");
            return 0;
        }

        out_elf_file->symbol_count = symtab_section->byte_count / MACHINE_ELF_SYMBOL_ENTRY_SIZE;
        out_elf_file->symbol_capacity = out_elf_file->symbol_count;
        out_elf_file->relocation_count =
            rel_text_section ? (rel_text_section->byte_count / MACHINE_ELF_RELOCATION_ENTRY_SIZE) : 0u;
        out_elf_file->relocation_capacity = out_elf_file->relocation_count;
        out_elf_file->symbols =
            (MachineElfSymbol *)calloc(out_elf_file->symbol_count == 0u ? 1u : out_elf_file->symbol_count,
                sizeof(MachineElfSymbol));
        out_elf_file->relocations = out_elf_file->relocation_count > 0u
            ? (MachineElfRelocation *)calloc(out_elf_file->relocation_count, sizeof(MachineElfRelocation))
            : NULL;
        if (!out_elf_file->symbols ||
            (out_elf_file->relocation_count > 0u && !out_elf_file->relocations)) {
            free(old_to_new_section_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-144: out of memory parsing symbol tables");
            return 0;
        }

        for (symbol_index = 0u; symbol_index < out_elf_file->symbol_count; ++symbol_index) {
            size_t entry_offset = symtab_section->file_offset + symbol_index * MACHINE_ELF_SYMBOL_ENTRY_SIZE;
            uint32_t st_name = machine_elf_read_u32_le(bytes, entry_offset + 0u);
            uint32_t st_value = machine_elf_read_u32_le(bytes, entry_offset + 4u);
            uint32_t st_size = machine_elf_read_u32_le(bytes, entry_offset + 8u);
            unsigned char st_info = bytes[entry_offset + 12u];
            uint16_t st_shndx = machine_elf_read_u16_le(bytes, entry_offset + 14u);
            MachineElfSymbol *symbol = &out_elf_file->symbols[symbol_index];
            size_t name_length;

            if (st_name >= strtab_section->byte_count) {
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-145: symbol name offset exceeds strtab");
                return 0;
            }
            if (!machine_elf_string_length_within_limit(
                    bytes + strtab_section->file_offset,
                    st_name,
                    strtab_section->byte_count,
                    &name_length)) {
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-146: unterminated symbol name");
                return 0;
            }

            symbol->name = machine_elf_strdup((const char *)(bytes + strtab_section->file_offset + st_name));
            if (!symbol->name) {
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-147: out of memory copying symbol name");
                return 0;
            }
            symbol->binding = (MachineElfSymbolBinding)(st_info >> 4u);
            symbol->type = (MachineElfSymbolType)(st_info & 0x0Fu);
            symbol->is_defined = st_shndx != MACHINE_ELF_SHN_UNDEF;
            if (!symbol->is_defined) {
                symbol->section_index = 0u;
            } else if ((size_t)st_shndx < section_count) {
                symbol->section_index = old_to_new_section_index[st_shndx];
            } else {
                symbol->section_index = (size_t)-1;
            }
            symbol->value = st_value;
            symbol->size = st_size;
        }

        for (relocation_index = 0u; relocation_index < out_elf_file->relocation_count; ++relocation_index) {
            size_t entry_offset =
                rel_text_section->file_offset + relocation_index * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
            uint32_t r_offset = machine_elf_read_u32_le(bytes, entry_offset + 0u);
            uint32_t r_info = machine_elf_read_u32_le(bytes, entry_offset + 4u);
            uint32_t symbol_ref = r_info >> 8u;
            uint32_t reloc_type = r_info & 0xFFu;
            MachineRelocKind source_kind = machine_elf_relocation_kind_from_type(profile, reloc_type);
            MachineElfRelocation *relocation = &out_elf_file->relocations[relocation_index];

            if (source_kind == (MachineRelocKind)-1 ||
                symbol_ref >= out_elf_file->symbol_count ||
                machine_elf_symbol_has_unsupported_defined_section(&out_elf_file->symbols[symbol_ref])) {
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-149: unsupported parsed relocation entry");
                return 0;
            }

            relocation->source_kind = source_kind;
            relocation->section_index = out_elf_file->rel_text_section_index;
            relocation->offset = r_offset;
            relocation->symbol_index = symbol_ref;
            relocation->type = reloc_type;
            relocation->symbol_name = machine_elf_strdup(out_elf_file->symbols[symbol_ref].name);
            if (!relocation->symbol_name) {
                free(old_to_new_section_index);
                machine_elf_file_free(out_elf_file);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-150: out of memory copying relocation name");
                return 0;
            }
        }
    }

    if (out_elf_file->symbol_count > 0u &&
        !machine_elf_symbol_is_canonical_null(&out_elf_file->symbols[0u])) {
        MachineElfSymbol *normalized_symbols =
            (MachineElfSymbol *)calloc(out_elf_file->symbol_count + 1u, sizeof(MachineElfSymbol));
        if (!normalized_symbols) {
            free(old_to_new_section_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-150: out of memory synthesizing null symbol");
            return 0;
        }
        normalized_symbols[0u].name = machine_elf_strdup("");
        if (!normalized_symbols[0u].name) {
            free(normalized_symbols);
            free(old_to_new_section_index);
            machine_elf_file_free(out_elf_file);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-150: out of memory copying null symbol");
            return 0;
        }
        normalized_symbols[0u].binding = MACHINE_ELF_SYMBOL_LOCAL;
        normalized_symbols[0u].type = MACHINE_ELF_SYMBOL_NOTYPE;
        normalized_symbols[0u].is_defined = 0;
        normalized_symbols[0u].section_index = 0u;
        normalized_symbols[0u].value = 0u;
        normalized_symbols[0u].size = 0u;
        for (symbol_index = 0u; symbol_index < out_elf_file->symbol_count; ++symbol_index) {
            normalized_symbols[symbol_index + 1u] = out_elf_file->symbols[symbol_index];
        }
        free(out_elf_file->symbols);
        out_elf_file->symbols = normalized_symbols;
        ++out_elf_file->symbol_count;
        ++out_elf_file->symbol_capacity;
        for (relocation_index = 0u; relocation_index < out_elf_file->relocation_count; ++relocation_index) {
            out_elf_file->relocations[relocation_index].symbol_index += 1u;
        }
    }

    free(old_to_new_section_index);
    return machine_elf_refresh_bytes(out_elf_file, error);
}

#endif

int machine_elf_refresh_bytes(MachineElfFile *elf_file, MachineElfError *error) {
    MachineElfTargetPolicy policy;
    unsigned char *old_bytes;
    unsigned char *text_bytes = NULL;
    unsigned char *sdata_bytes = NULL;
    unsigned char *new_bytes = NULL;
    size_t old_byte_count;
    size_t sbss_section_index = (size_t)-1;
    size_t sdata_section_index = (size_t)-1;
    size_t text_offset;
    size_t text_size;
    size_t sdata_offset = 0u;
    size_t sdata_size = 0u;
    size_t strtab_size = 1u;
    size_t shstrtab_size = 1u;
    size_t canonical_section_count;
    size_t symtab_offset;
    size_t rel_offset;
    size_t shstrtab_offset;
    size_t shoff;
    size_t byte_count;
    size_t string_cursor;
    size_t shstring_cursor;
    size_t local_symbol_count = 0u;
    size_t symbol_index;
    size_t relocation_index;
    size_t first_global_symbol_index;
    size_t next_local_symbol_index;
    size_t next_global_symbol_index;
    size_t source_symbol_count;
    size_t kept_symbol_count;
    unsigned char *symbol_referenced_by_relocation = NULL;
    size_t *old_to_new_symbol_index = NULL;
    MachineElfSymbol *normalized_symbols = NULL;
    uint32_t text_name_offset;
    uint32_t sbss_name_offset = 0u;
    uint32_t sdata_name_offset = 0u;
    uint32_t strtab_name_offset;
    uint32_t symtab_name_offset;
    uint32_t reltext_name_offset;
    uint32_t shstrtab_name_offset;

    if (!elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-148: invalid elf refresh contract");
        return 0;
    }
    if (!machine_elf_get_target_policy(elf_file->target_profile, &policy)) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-149: unknown elf refresh target profile");
        return 0;
    }
    if (elf_file->section_count < 6u || !elf_file->sections ||
        elf_file->symbol_count == 0u || !elf_file->symbols ||
        (elf_file->relocation_count > 0u && !elf_file->relocations) ||
        !elf_file->bytes ||
        elf_file->text_section_index != 1u ||
        elf_file->text_section_index >= elf_file->section_count ||
        elf_file->strtab_section_index >= elf_file->section_count ||
        elf_file->symtab_section_index >= elf_file->section_count ||
        elf_file->rel_text_section_index >= elf_file->section_count ||
        elf_file->shstrtab_section_index >= elf_file->section_count) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-150: invalid elf refresh metadata");
        return 0;
    }
    for (symbol_index = 0u; symbol_index < elf_file->section_count; ++symbol_index) {
        if (machine_elf_section_name_equals(&elf_file->sections[symbol_index], ".sbss")) {
            sbss_section_index = symbol_index;
        } else if (machine_elf_section_name_equals(&elf_file->sections[symbol_index], ".sdata")) {
            sdata_section_index = symbol_index;
        }
    }
    canonical_section_count = 6u + (sbss_section_index != (size_t)-1 ? 1u : 0u) + (sdata_section_index != (size_t)-1 ? 1u : 0u);

    text_offset = elf_file->sections[elf_file->text_section_index].file_offset;
    text_size = elf_file->sections[elf_file->text_section_index].byte_count;
    if (text_offset > elf_file->byte_count || text_size > elf_file->byte_count - text_offset) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-151: invalid text section source span");
        return 0;
    }
    if (sdata_section_index != (size_t)-1) {
        sdata_offset = elf_file->sections[sdata_section_index].file_offset;
        sdata_size = elf_file->sections[sdata_section_index].byte_count;
        if (sdata_offset > elf_file->byte_count || sdata_size > elf_file->byte_count - sdata_offset) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-151: invalid sdata section source span");
            return 0;
        }
    }

    if (!machine_elf_replace_owned_string(&elf_file->sections[0].name, "") ||
        !machine_elf_replace_owned_string(&elf_file->sections[1].name, ".text") ||
        (sbss_section_index != (size_t)-1 && !machine_elf_replace_owned_string(&elf_file->sections[sbss_section_index].name, ".sbss")) ||
        (sdata_section_index != (size_t)-1 && !machine_elf_replace_owned_string(&elf_file->sections[sdata_section_index].name, ".sdata")) ||
        !machine_elf_replace_owned_string(&elf_file->sections[elf_file->strtab_section_index].name, ".strtab") ||
        !machine_elf_replace_owned_string(&elf_file->sections[elf_file->symtab_section_index].name, ".symtab") ||
        !machine_elf_replace_owned_string(&elf_file->sections[elf_file->rel_text_section_index].name, ".rel.text") ||
        !machine_elf_replace_owned_string(&elf_file->sections[elf_file->shstrtab_section_index].name, ".shstrtab")) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-152: out of memory normalizing section names");
        return 0;
    }

    if (!machine_elf_replace_owned_string(&elf_file->symbols[0].name, "")) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-153: out of memory normalizing null symbol");
        return 0;
    }
    elf_file->symbols[0].binding = MACHINE_ELF_SYMBOL_LOCAL;
    elf_file->symbols[0].type = MACHINE_ELF_SYMBOL_NOTYPE;
    elf_file->symbols[0].is_defined = 0;
    elf_file->symbols[0].section_index = 0u;
    elf_file->symbols[0].value = 0u;
    elf_file->symbols[0].size = 0u;

    symbol_referenced_by_relocation =
        (unsigned char *)calloc(elf_file->symbol_count == 0u ? 1u : elf_file->symbol_count, sizeof(unsigned char));
    if (!symbol_referenced_by_relocation) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-158: out of memory tracking referenced symbols");
        return 0;
    }
    for (relocation_index = 0u; relocation_index < elf_file->relocation_count; ++relocation_index) {
        MachineElfRelocation *relocation = &elf_file->relocations[relocation_index];
        if (relocation->symbol_index >= elf_file->symbol_count ||
            machine_elf_map_relocation_type(elf_file->target_profile, relocation->source_kind) == 0u) {
            free(symbol_referenced_by_relocation);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-157: invalid relocation refresh metadata");
            return 0;
        }
        symbol_referenced_by_relocation[relocation->symbol_index] = 1u;
    }

    source_symbol_count = elf_file->symbol_count;
    kept_symbol_count = 1u;
    for (symbol_index = 1u; symbol_index < source_symbol_count; ++symbol_index) {
        MachineElfSymbol *symbol = &elf_file->symbols[symbol_index];

        if (machine_elf_symbol_is_droppable_on_refresh(
                symbol,
                symbol_referenced_by_relocation[symbol_index] != 0u)) {
            continue;
        }
        if (!symbol->name || symbol->name[0] == '\0') {
            free(symbol_referenced_by_relocation);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-154: refresh requires symbol names");
            return 0;
        }
        if (symbol->binding == MACHINE_ELF_SYMBOL_LOCAL) {
            ++local_symbol_count;
        } else if (symbol->binding != MACHINE_ELF_SYMBOL_GLOBAL) {
            free(symbol_referenced_by_relocation);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-155: refresh requires known symbol bindings");
            return 0;
        }
        if (symbol->is_defined) {
            if (symbol->section_index != elf_file->text_section_index &&
                symbol->section_index != sbss_section_index &&
                symbol->section_index != sdata_section_index) {
                free(symbol_referenced_by_relocation);
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-156: refresh only supports canonical direct-build sections");
                return 0;
            }
        } else {
            symbol->section_index = 0u;
        }
        strtab_size += strlen(symbol->name) + 1u;
        ++kept_symbol_count;
    }
    first_global_symbol_index = 1u + local_symbol_count;
    if (first_global_symbol_index > kept_symbol_count) {
        free(symbol_referenced_by_relocation);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-157: invalid local/global symbol partition");
        return 0;
    }
    old_to_new_symbol_index = (size_t *)malloc(source_symbol_count * sizeof(size_t));
    normalized_symbols = (MachineElfSymbol *)calloc(kept_symbol_count, sizeof(MachineElfSymbol));
    if (!old_to_new_symbol_index || !normalized_symbols) {
        free(symbol_referenced_by_relocation);
        free(old_to_new_symbol_index);
        free(normalized_symbols);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-158: out of memory canonicalizing symbol order");
        return 0;
    }
    for (symbol_index = 0u; symbol_index < source_symbol_count; ++symbol_index) {
        old_to_new_symbol_index[symbol_index] = (size_t)-1;
    }
    next_local_symbol_index = 1u;
    next_global_symbol_index = first_global_symbol_index;
    old_to_new_symbol_index[0u] = 0u;
    normalized_symbols[0u] = elf_file->symbols[0u];
    normalized_symbols[0u].name = machine_elf_strdup("");
    if (!normalized_symbols[0u].name) {
        free(symbol_referenced_by_relocation);
        free(old_to_new_symbol_index);
        free(normalized_symbols);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-159: out of memory copying canonical symbol names");
        return 0;
    }
    for (symbol_index = 1u; symbol_index < source_symbol_count; ++symbol_index) {
        if (machine_elf_symbol_is_droppable_on_refresh(
                &elf_file->symbols[symbol_index],
                symbol_referenced_by_relocation[symbol_index] != 0u)) {
            continue;
        }
        size_t new_index = elf_file->symbols[symbol_index].binding == MACHINE_ELF_SYMBOL_LOCAL
            ? next_local_symbol_index++
            : next_global_symbol_index++;
        normalized_symbols[new_index] = elf_file->symbols[symbol_index];
        normalized_symbols[new_index].name = machine_elf_strdup(elf_file->symbols[symbol_index].name);
        if (!normalized_symbols[new_index].name) {
            size_t cleanup_index;
            for (cleanup_index = 0u; cleanup_index < kept_symbol_count; ++cleanup_index) {
                free(normalized_symbols[cleanup_index].name);
            }
            free(symbol_referenced_by_relocation);
            free(old_to_new_symbol_index);
            free(normalized_symbols);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-159: out of memory copying canonical symbol names");
            return 0;
        }
        old_to_new_symbol_index[symbol_index] = new_index;
    }
    for (symbol_index = 0u; symbol_index < source_symbol_count; ++symbol_index) {
        free(elf_file->symbols[symbol_index].name);
    }
    free(elf_file->symbols);
    elf_file->symbols = normalized_symbols;
    elf_file->symbol_count = kept_symbol_count;
    elf_file->symbol_capacity = kept_symbol_count;
    if (first_global_symbol_index == kept_symbol_count && local_symbol_count == kept_symbol_count - 1u) {
        first_global_symbol_index = kept_symbol_count;
    }

    for (relocation_index = 0u; relocation_index < elf_file->relocation_count; ++relocation_index) {
        MachineElfRelocation *relocation = &elf_file->relocations[relocation_index];
        if (old_to_new_symbol_index[relocation->symbol_index] == (size_t)-1) {
            free(old_to_new_symbol_index);
            free(symbol_referenced_by_relocation);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-157: invalid relocation refresh metadata");
            return 0;
        }
        relocation->symbol_index = old_to_new_symbol_index[relocation->symbol_index];
        if (!machine_elf_replace_owned_string(&relocation->symbol_name, elf_file->symbols[relocation->symbol_index].name)) {
            free(old_to_new_symbol_index);
            free(symbol_referenced_by_relocation);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-160: out of memory normalizing relocation names");
            return 0;
        }
        relocation->section_index = elf_file->rel_text_section_index;
    }
    free(symbol_referenced_by_relocation);
    free(old_to_new_symbol_index);

    shstrtab_size += strlen(".text") + 1u;
    if (sbss_section_index != (size_t)-1) {
        shstrtab_size += strlen(".sbss") + 1u;
    }
    if (sdata_section_index != (size_t)-1) {
        shstrtab_size += strlen(".sdata") + 1u;
    }
    shstrtab_size += strlen(".strtab") + 1u;
    shstrtab_size += strlen(".symtab") + 1u;
    shstrtab_size += strlen(".rel.text") + 1u;
    shstrtab_size += strlen(".shstrtab") + 1u;

    text_bytes = (unsigned char *)malloc(text_size == 0u ? 1u : text_size);
    if (!text_bytes) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-158: out of memory copying text bytes");
        return 0;
    }
    if (text_size > 0u) {
        memcpy(text_bytes, elf_file->bytes + text_offset, text_size);
    }
    if (sdata_section_index != (size_t)-1) {
        sdata_bytes = (unsigned char *)malloc(sdata_size == 0u ? 1u : sdata_size);
        if (!sdata_bytes) {
            free(text_bytes);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-158: out of memory copying sdata bytes");
            return 0;
        }
        if (sdata_size > 0u) {
            memcpy(sdata_bytes, elf_file->bytes + sdata_offset, sdata_size);
        }
    }

    symtab_offset = machine_elf_align_up(MACHINE_ELF_HEADER_SIZE + text_size + sdata_size + strtab_size, 4u);
    rel_offset = machine_elf_align_up(symtab_offset + elf_file->symbol_count * MACHINE_ELF_SYMBOL_ENTRY_SIZE, 4u);
    shstrtab_offset = rel_offset + elf_file->relocation_count * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
    shoff = machine_elf_align_up(shstrtab_offset + shstrtab_size, 4u);
    byte_count = shoff + canonical_section_count * MACHINE_ELF_SECTION_HEADER_SIZE;

    new_bytes = (unsigned char *)calloc(byte_count == 0u ? 1u : byte_count, 1u);
    if (!new_bytes) {
        free(text_bytes);
        free(sdata_bytes);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-159: out of memory rebuilding elf bytes");
        return 0;
    }
    if (text_size > 0u) {
        memcpy(new_bytes + MACHINE_ELF_HEADER_SIZE, text_bytes, text_size);
    }
    if (sdata_size > 0u) {
        memcpy(new_bytes + MACHINE_ELF_HEADER_SIZE + text_size, sdata_bytes, sdata_size);
    }

    elf_file->sections[0].type = MACHINE_ELF_SECTION_NULL;
    elf_file->sections[0].file_offset = 0u;
    elf_file->sections[0].byte_count = 0u;
    elf_file->sections[0].flags = 0u;
    elf_file->sections[0].link = 0u;
    elf_file->sections[0].info = 0u;
    elf_file->sections[0].align = 0u;
    elf_file->sections[0].entry_size = 0u;

    elf_file->sections[1].type = MACHINE_ELF_SECTION_PROGBITS;
    elf_file->sections[1].file_offset = MACHINE_ELF_HEADER_SIZE;
    elf_file->sections[1].byte_count = text_size;
    elf_file->sections[1].flags = MACHINE_ELF_SHF_ALLOC | MACHINE_ELF_SHF_EXECINSTR;
    elf_file->sections[1].link = 0u;
    elf_file->sections[1].info = 0u;
    elf_file->sections[1].align = 1u;
    elf_file->sections[1].entry_size = 0u;

    {
        size_t next_section_index = 2u;

        if (sbss_section_index != (size_t)-1) {
            sbss_section_index = next_section_index++;
            elf_file->sections[sbss_section_index].type = MACHINE_ELF_SECTION_NOBITS;
            elf_file->sections[sbss_section_index].file_offset = MACHINE_ELF_HEADER_SIZE + text_size + sdata_size + strtab_size;
            elf_file->sections[sbss_section_index].byte_count = elf_file->sections[sbss_section_index].byte_count;
            elf_file->sections[sbss_section_index].flags = MACHINE_ELF_SHF_WRITE | MACHINE_ELF_SHF_ALLOC;
            elf_file->sections[sbss_section_index].link = 0u;
            elf_file->sections[sbss_section_index].info = 0u;
            elf_file->sections[sbss_section_index].align = 4u;
            elf_file->sections[sbss_section_index].entry_size = 0u;
        }
        if (sdata_section_index != (size_t)-1) {
            sdata_section_index = next_section_index++;
            elf_file->sections[sdata_section_index].type = MACHINE_ELF_SECTION_PROGBITS;
            elf_file->sections[sdata_section_index].file_offset = MACHINE_ELF_HEADER_SIZE + text_size;
            elf_file->sections[sdata_section_index].byte_count = sdata_size;
            elf_file->sections[sdata_section_index].flags = MACHINE_ELF_SHF_WRITE | MACHINE_ELF_SHF_ALLOC;
            elf_file->sections[sdata_section_index].link = 0u;
            elf_file->sections[sdata_section_index].info = 0u;
            elf_file->sections[sdata_section_index].align = 4u;
            elf_file->sections[sdata_section_index].entry_size = 0u;
        }
        elf_file->strtab_section_index = next_section_index++;
        elf_file->symtab_section_index = next_section_index++;
        elf_file->rel_text_section_index = next_section_index++;
        elf_file->shstrtab_section_index = next_section_index++;
        elf_file->section_count = canonical_section_count;
        elf_file->section_capacity = canonical_section_count;
    }

    elf_file->sections[elf_file->strtab_section_index].type = MACHINE_ELF_SECTION_STRTAB;
    elf_file->sections[elf_file->strtab_section_index].file_offset = MACHINE_ELF_HEADER_SIZE + text_size + sdata_size;
    elf_file->sections[elf_file->strtab_section_index].byte_count = strtab_size;
    elf_file->sections[elf_file->strtab_section_index].flags = 0u;
    elf_file->sections[elf_file->strtab_section_index].link = 0u;
    elf_file->sections[elf_file->strtab_section_index].info = 0u;
    elf_file->sections[elf_file->strtab_section_index].align = 1u;
    elf_file->sections[elf_file->strtab_section_index].entry_size = 0u;

    elf_file->sections[elf_file->symtab_section_index].type = MACHINE_ELF_SECTION_SYMTAB;
    elf_file->sections[elf_file->symtab_section_index].file_offset = symtab_offset;
    elf_file->sections[elf_file->symtab_section_index].byte_count = elf_file->symbol_count * MACHINE_ELF_SYMBOL_ENTRY_SIZE;
    elf_file->sections[elf_file->symtab_section_index].flags = 0u;
    elf_file->sections[elf_file->symtab_section_index].link = elf_file->strtab_section_index;
    elf_file->sections[elf_file->symtab_section_index].info = (uint32_t)first_global_symbol_index;
    elf_file->sections[elf_file->symtab_section_index].align = 4u;
    elf_file->sections[elf_file->symtab_section_index].entry_size = MACHINE_ELF_SYMBOL_ENTRY_SIZE;

    elf_file->sections[elf_file->rel_text_section_index].type = MACHINE_ELF_SECTION_REL;
    elf_file->sections[elf_file->rel_text_section_index].file_offset = rel_offset;
    elf_file->sections[elf_file->rel_text_section_index].byte_count = elf_file->relocation_count * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
    elf_file->sections[elf_file->rel_text_section_index].flags = 0u;
    elf_file->sections[elf_file->rel_text_section_index].link = elf_file->symtab_section_index;
    elf_file->sections[elf_file->rel_text_section_index].info = elf_file->text_section_index;
    elf_file->sections[elf_file->rel_text_section_index].align = 4u;
    elf_file->sections[elf_file->rel_text_section_index].entry_size = MACHINE_ELF_RELOCATION_ENTRY_SIZE;

    elf_file->sections[elf_file->shstrtab_section_index].type = MACHINE_ELF_SECTION_STRTAB;
    elf_file->sections[elf_file->shstrtab_section_index].file_offset = shstrtab_offset;
    elf_file->sections[elf_file->shstrtab_section_index].byte_count = shstrtab_size;
    elf_file->sections[elf_file->shstrtab_section_index].flags = 0u;
    elf_file->sections[elf_file->shstrtab_section_index].link = 0u;
    elf_file->sections[elf_file->shstrtab_section_index].info = 0u;
    elf_file->sections[elf_file->shstrtab_section_index].align = 1u;
    elf_file->sections[elf_file->shstrtab_section_index].entry_size = 0u;

    string_cursor = 1u;
    new_bytes[elf_file->sections[elf_file->strtab_section_index].file_offset] = '\0';
    for (symbol_index = 1u; symbol_index < elf_file->symbol_count; ++symbol_index) {
        MachineElfSymbol *symbol = &elf_file->symbols[symbol_index];
        size_t entry_offset = symtab_offset + symbol_index * MACHINE_ELF_SYMBOL_ENTRY_SIZE;
        uint32_t st_name = machine_elf_append_string(
            new_bytes,
            elf_file->sections[elf_file->strtab_section_index].file_offset,
            strtab_size,
            &string_cursor,
            symbol->name);
        uint8_t info = (uint8_t)(((uint32_t)symbol->binding << 4u) | (uint32_t)symbol->type);

        machine_elf_write_u32_le(new_bytes, entry_offset + 0u, st_name);
        machine_elf_write_u32_le(new_bytes, entry_offset + 4u, symbol->value);
        machine_elf_write_u32_le(new_bytes, entry_offset + 8u, symbol->size);
        new_bytes[entry_offset + 12u] = info;
        new_bytes[entry_offset + 13u] = 0u;
        machine_elf_write_u16_le(
            new_bytes,
            entry_offset + 14u,
            (uint16_t)(symbol->is_defined ? symbol->section_index : MACHINE_ELF_SHN_UNDEF));
    }

    for (relocation_index = 0u; relocation_index < elf_file->relocation_count; ++relocation_index) {
        MachineElfRelocation *relocation = &elf_file->relocations[relocation_index];
        uint32_t reloc_type = machine_elf_map_relocation_type(elf_file->target_profile, relocation->source_kind);
        size_t entry_offset = rel_offset + relocation_index * MACHINE_ELF_RELOCATION_ENTRY_SIZE;

        if (!machine_elf_replace_owned_string(&relocation->symbol_name, elf_file->symbols[relocation->symbol_index].name)) {
            free(text_bytes);
            free(new_bytes);
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-160: out of memory normalizing relocation names");
            return 0;
        }
        relocation->type = reloc_type;
        machine_elf_write_u32_le(new_bytes, entry_offset + 0u, relocation->offset);
        machine_elf_write_u32_le(new_bytes, entry_offset + 4u, (relocation->symbol_index << 8u) | (reloc_type & 0xFFu));
    }

    shstring_cursor = 1u;
    new_bytes[shstrtab_offset] = '\0';
    text_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".text");
    if (sbss_section_index != (size_t)-1) {
        sbss_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".sbss");
    }
    if (sdata_section_index != (size_t)-1) {
        sdata_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".sdata");
    }
    strtab_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".strtab");
    symtab_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".symtab");
    reltext_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".rel.text");
    shstrtab_name_offset = machine_elf_append_string(new_bytes, shstrtab_offset, shstrtab_size, &shstring_cursor, ".shstrtab");
    if (string_cursor != strtab_size || shstring_cursor != shstrtab_size) {
        free(text_bytes);
        free(new_bytes);
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-161: refresh string table layout mismatch");
        return 0;
    }

    new_bytes[0u] = 0x7Fu;
    new_bytes[1u] = 'E';
    new_bytes[2u] = 'L';
    new_bytes[3u] = 'F';
    new_bytes[4u] = 1u;
    new_bytes[5u] = 1u;
    new_bytes[6u] = 1u;
    new_bytes[7u] = 0u;
    memset(new_bytes + 8u, 0, 8u);
    machine_elf_write_u16_le(new_bytes, 16u, MACHINE_ELF_ET_REL);
    machine_elf_write_u16_le(new_bytes, 18u, policy.machine);
    machine_elf_write_u32_le(new_bytes, 20u, MACHINE_ELF_EV_CURRENT);
    machine_elf_write_u32_le(new_bytes, 24u, 0u);
    machine_elf_write_u32_le(new_bytes, 28u, 0u);
    machine_elf_write_u32_le(new_bytes, 32u, (uint32_t)shoff);
    machine_elf_write_u32_le(new_bytes, 36u, policy.flags);
    machine_elf_write_u16_le(new_bytes, 40u, MACHINE_ELF_HEADER_SIZE);
    machine_elf_write_u16_le(new_bytes, 42u, 0u);
    machine_elf_write_u16_le(new_bytes, 44u, 0u);
    machine_elf_write_u16_le(new_bytes, 46u, MACHINE_ELF_SECTION_HEADER_SIZE);
    machine_elf_write_u16_le(new_bytes, 48u, (uint16_t)canonical_section_count);
    machine_elf_write_u16_le(new_bytes, 50u, (uint16_t)elf_file->shstrtab_section_index);

    machine_elf_write_u32_le(new_bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, text_name_offset);
    machine_elf_write_u32_le(new_bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_PROGBITS);
    machine_elf_write_u32_le(new_bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 8u, MACHINE_ELF_SHF_ALLOC | MACHINE_ELF_SHF_EXECINSTR);
    machine_elf_write_u32_le(new_bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[1].file_offset);
    machine_elf_write_u32_le(new_bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[1].byte_count);
    machine_elf_write_u32_le(new_bytes, shoff + 1u * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 1u);

    if (sbss_section_index != (size_t)-1) {
        machine_elf_write_u32_le(new_bytes, shoff + sbss_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, sbss_name_offset);
        machine_elf_write_u32_le(new_bytes, shoff + sbss_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_NOBITS);
        machine_elf_write_u32_le(new_bytes, shoff + sbss_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 8u, MACHINE_ELF_SHF_WRITE | MACHINE_ELF_SHF_ALLOC);
        machine_elf_write_u32_le(new_bytes, shoff + sbss_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[sbss_section_index].file_offset);
        machine_elf_write_u32_le(new_bytes, shoff + sbss_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[sbss_section_index].byte_count);
        machine_elf_write_u32_le(new_bytes, shoff + sbss_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 4u);
    }

    if (sdata_section_index != (size_t)-1) {
        machine_elf_write_u32_le(new_bytes, shoff + sdata_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, sdata_name_offset);
        machine_elf_write_u32_le(new_bytes, shoff + sdata_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_PROGBITS);
        machine_elf_write_u32_le(new_bytes, shoff + sdata_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 8u, MACHINE_ELF_SHF_WRITE | MACHINE_ELF_SHF_ALLOC);
        machine_elf_write_u32_le(new_bytes, shoff + sdata_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[sdata_section_index].file_offset);
        machine_elf_write_u32_le(new_bytes, shoff + sdata_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[sdata_section_index].byte_count);
        machine_elf_write_u32_le(new_bytes, shoff + sdata_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 4u);
    }

    machine_elf_write_u32_le(new_bytes, shoff + elf_file->strtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, strtab_name_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->strtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_STRTAB);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->strtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[elf_file->strtab_section_index].file_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->strtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[elf_file->strtab_section_index].byte_count);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->strtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 1u);

    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, symtab_name_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_SYMTAB);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[elf_file->symtab_section_index].file_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[elf_file->symtab_section_index].byte_count);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 24u, (uint32_t)elf_file->sections[elf_file->symtab_section_index].link);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 28u, elf_file->sections[elf_file->symtab_section_index].info);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 4u);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->symtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 36u, MACHINE_ELF_SYMBOL_ENTRY_SIZE);

    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, reltext_name_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_REL);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[elf_file->rel_text_section_index].file_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[elf_file->rel_text_section_index].byte_count);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 24u, (uint32_t)elf_file->sections[elf_file->rel_text_section_index].link);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 28u, (uint32_t)elf_file->sections[elf_file->rel_text_section_index].info);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 4u);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->rel_text_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 36u, MACHINE_ELF_RELOCATION_ENTRY_SIZE);

    machine_elf_write_u32_le(new_bytes, shoff + elf_file->shstrtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 0u, shstrtab_name_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->shstrtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 4u, MACHINE_ELF_SECTION_STRTAB);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->shstrtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 16u, (uint32_t)elf_file->sections[elf_file->shstrtab_section_index].file_offset);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->shstrtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 20u, (uint32_t)elf_file->sections[elf_file->shstrtab_section_index].byte_count);
    machine_elf_write_u32_le(new_bytes, shoff + elf_file->shstrtab_section_index * MACHINE_ELF_SECTION_HEADER_SIZE + 32u, 1u);

    old_bytes = elf_file->bytes;
    old_byte_count = elf_file->byte_count;
    elf_file->bytes = new_bytes;
    elf_file->byte_count = byte_count;
    elf_file->section_header_offset = shoff;
    free(old_bytes);
    free(text_bytes);
    free(sdata_bytes);
    (void)old_byte_count;
    return machine_elf_verify_file(elf_file, error);
}

int machine_elf_normalize_bytes_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    return machine_elf_normalize_bytes_from_bytes_with_profile(
        bytes,
        byte_count,
        (MachineElfTargetProfile)-1,
        out_bytes,
        out_byte_count,
        error);
}

int machine_elf_normalize_bytes_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_parse_file_from_bytes(bytes, byte_count, &elf_file, error);
    if (ok && profile != (MachineElfTargetProfile)-1) {
        elf_file.target_profile = profile;
        ok = machine_elf_refresh_bytes(&elf_file, error);
    }
    ok = ok && machine_elf_file_copy_bytes(&elf_file, out_bytes, out_byte_count, error);
    machine_elf_file_free(&elf_file);
    return ok;
}
