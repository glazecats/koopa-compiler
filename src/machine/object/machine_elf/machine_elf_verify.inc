#if defined(__INTELLISENSE__) && !defined(MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ELF_SPLIT_AGGREGATOR)
#define MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT
#include "machine_elf.c"
#else

int machine_elf_verify_file(const MachineElfFile *elf_file, MachineElfError *error) {
    size_t section_index;
    const MachineElfSection *shstrtab_section;
    const MachineElfSection *strtab_section;
    const MachineElfSection *symtab_section;
    MachineElfHeaderSummary header_summary;
    uint16_t expected_machine;
    uint32_t expected_flags;

    if (!elf_file) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-113: invalid elf verifier contract");
        return 0;
    }
    if (!machine_elf_get_target_policy(elf_file->target_profile, NULL)) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-114: unknown elf target profile");
        return 0;
    }
    expected_machine = machine_elf_expected_machine_for_profile(elf_file->target_profile);
    expected_flags = machine_elf_expected_flags_for_profile(elf_file->target_profile);
    if ((elf_file->section_count > 0u && !elf_file->sections) ||
        (elf_file->symbol_count > 0u && !elf_file->symbols) ||
        (elf_file->relocation_count > 0u && !elf_file->relocations) ||
        !elf_file->bytes || elf_file->byte_count < MACHINE_ELF_HEADER_SIZE) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-115: invalid elf table metadata");
        return 0;
    }
    if (elf_file->bytes[0u] != 0x7Fu || elf_file->bytes[1u] != 'E' ||
        elf_file->bytes[2u] != 'L' || elf_file->bytes[3u] != 'F') {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-116: invalid elf magic");
        return 0;
    }
    if (!machine_elf_file_get_header_summary(elf_file, &header_summary) ||
        header_summary.elf_class != 1u ||
        header_summary.data_encoding != 1u ||
        header_summary.ident_version != MACHINE_ELF_EV_CURRENT ||
        header_summary.object_type != MACHINE_ELF_ET_REL ||
        header_summary.target_profile != elf_file->target_profile ||
        header_summary.machine != expected_machine ||
        header_summary.elf_version != MACHINE_ELF_EV_CURRENT ||
        header_summary.flags != expected_flags ||
        header_summary.section_header_offset != elf_file->section_header_offset ||
        header_summary.section_header_entry_size != MACHINE_ELF_SECTION_HEADER_SIZE ||
        header_summary.section_header_count != elf_file->section_count ||
        header_summary.section_name_string_table_index != elf_file->shstrtab_section_index ||
        machine_elf_read_u32_le(elf_file->bytes, 24u) != 0u ||
        machine_elf_read_u32_le(elf_file->bytes, 28u) != 0u ||
        machine_elf_read_u16_le(elf_file->bytes, 40u) != MACHINE_ELF_HEADER_SIZE ||
        machine_elf_read_u16_le(elf_file->bytes, 42u) != 0u ||
        machine_elf_read_u16_le(elf_file->bytes, 44u) != 0u) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-117: invalid elf header fields");
        return 0;
    }
    if (elf_file->section_count != 6u ||
        elf_file->text_section_index != 1u ||
        elf_file->strtab_section_index != 2u ||
        elf_file->symtab_section_index != 3u ||
        elf_file->rel_text_section_index != 4u ||
        elf_file->shstrtab_section_index != 5u) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-118: invalid elf section layout");
        return 0;
    }
    if (!machine_elf_section_name_equals(&elf_file->sections[0], "") ||
        elf_file->sections[0].type != MACHINE_ELF_SECTION_NULL ||
        elf_file->sections[0].file_offset != 0u ||
        elf_file->sections[0].byte_count != 0u ||
        elf_file->sections[0].flags != 0u ||
        elf_file->sections[0].link != 0u ||
        elf_file->sections[0].info != 0u ||
        elf_file->sections[0].align != 0u ||
        elf_file->sections[0].entry_size != 0u ||
        !machine_elf_section_name_equals(&elf_file->sections[elf_file->text_section_index], ".text") ||
        elf_file->sections[elf_file->text_section_index].type != MACHINE_ELF_SECTION_PROGBITS ||
        elf_file->sections[elf_file->text_section_index].flags != (MACHINE_ELF_SHF_ALLOC | MACHINE_ELF_SHF_EXECINSTR) ||
        elf_file->sections[elf_file->text_section_index].link != 0u ||
        elf_file->sections[elf_file->text_section_index].info != 0u ||
        elf_file->sections[elf_file->text_section_index].align != 1u ||
        elf_file->sections[elf_file->text_section_index].entry_size != 0u ||
        !machine_elf_section_name_equals(&elf_file->sections[elf_file->strtab_section_index], ".strtab") ||
        elf_file->sections[elf_file->strtab_section_index].type != MACHINE_ELF_SECTION_STRTAB ||
        elf_file->sections[elf_file->strtab_section_index].flags != 0u ||
        elf_file->sections[elf_file->strtab_section_index].link != 0u ||
        elf_file->sections[elf_file->strtab_section_index].info != 0u ||
        elf_file->sections[elf_file->strtab_section_index].align != 1u ||
        elf_file->sections[elf_file->strtab_section_index].entry_size != 0u ||
        !machine_elf_section_name_equals(&elf_file->sections[elf_file->symtab_section_index], ".symtab") ||
        elf_file->sections[elf_file->symtab_section_index].type != MACHINE_ELF_SECTION_SYMTAB ||
        elf_file->sections[elf_file->symtab_section_index].flags != 0u ||
        elf_file->sections[elf_file->symtab_section_index].link != elf_file->strtab_section_index ||
        elf_file->sections[elf_file->symtab_section_index].align != 4u ||
        elf_file->sections[elf_file->symtab_section_index].entry_size != MACHINE_ELF_SYMBOL_ENTRY_SIZE ||
        !machine_elf_section_name_equals(&elf_file->sections[elf_file->rel_text_section_index], ".rel.text") ||
        elf_file->sections[elf_file->rel_text_section_index].type != MACHINE_ELF_SECTION_REL ||
        elf_file->sections[elf_file->rel_text_section_index].flags != 0u ||
        elf_file->sections[elf_file->rel_text_section_index].link != elf_file->symtab_section_index ||
        elf_file->sections[elf_file->rel_text_section_index].info != elf_file->text_section_index ||
        elf_file->sections[elf_file->rel_text_section_index].align != 4u ||
        elf_file->sections[elf_file->rel_text_section_index].entry_size != MACHINE_ELF_RELOCATION_ENTRY_SIZE ||
        !machine_elf_section_name_equals(&elf_file->sections[elf_file->shstrtab_section_index], ".shstrtab") ||
        elf_file->sections[elf_file->shstrtab_section_index].type != MACHINE_ELF_SECTION_STRTAB ||
        elf_file->sections[elf_file->shstrtab_section_index].flags != 0u ||
        elf_file->sections[elf_file->shstrtab_section_index].link != 0u ||
        elf_file->sections[elf_file->shstrtab_section_index].info != 0u ||
        elf_file->sections[elf_file->shstrtab_section_index].align != 1u ||
        elf_file->sections[elf_file->shstrtab_section_index].entry_size != 0u) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-131: invalid canonical elf section semantics");
        return 0;
    }
    for (section_index = 0; section_index < elf_file->section_count; ++section_index) {
        const MachineElfSection *section = &elf_file->sections[section_index];

        if (!section->name) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-119: elf section requires name");
            return 0;
        }
        if (section_index > 0u &&
            (section->file_offset + section->byte_count > elf_file->byte_count ||
             (section->byte_count > 0u && section->file_offset < MACHINE_ELF_HEADER_SIZE))) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-120: elf section exceeds file span");
            return 0;
        }
    }
    if (elf_file->section_header_offset + elf_file->section_count * MACHINE_ELF_SECTION_HEADER_SIZE > elf_file->byte_count) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-121: invalid section-header span");
        return 0;
    }
    shstrtab_section = &elf_file->sections[elf_file->shstrtab_section_index];
    strtab_section = &elf_file->sections[elf_file->strtab_section_index];
    symtab_section = &elf_file->sections[elf_file->symtab_section_index];
    if (elf_file->symbol_count == 0u || !elf_file->symbols[0].name ||
        elf_file->symbols[0].binding != MACHINE_ELF_SYMBOL_LOCAL ||
        elf_file->symbols[0].type != MACHINE_ELF_SYMBOL_NOTYPE ||
        elf_file->symbols[0].is_defined) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-122: invalid null symbol");
        return 0;
    }
    {
        size_t first_global_index = elf_file->symbol_count;
        int seen_global = 0;
        for (section_index = 1u; section_index < elf_file->symbol_count; ++section_index) {
            const MachineElfSymbol *symbol = &elf_file->symbols[section_index];
            if (!symbol->name || symbol->name[0] == '\0') {
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-123: elf symbol requires name");
                return 0;
            }
            if (symbol->binding == MACHINE_ELF_SYMBOL_GLOBAL) {
                if (!seen_global) {
                    first_global_index = section_index;
                    seen_global = 1;
                }
            } else if (seen_global) {
                machine_elf_set_error(error, 0, 0, "MACHINE-ELF-124: local symbols must precede globals in symtab");
                return 0;
            }
        }
        if (elf_file->sections[elf_file->symtab_section_index].info != (uint32_t)first_global_index) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-125: symtab sh_info does not match first global symbol");
            return 0;
        }
    }
    for (section_index = 0u; section_index < elf_file->section_count; ++section_index) {
        const MachineElfSection *section = &elf_file->sections[section_index];
        size_t header_offset = elf_file->section_header_offset + section_index * MACHINE_ELF_SECTION_HEADER_SIZE;
        uint32_t sh_name = machine_elf_read_u32_le(elf_file->bytes, header_offset + 0u);
        uint32_t sh_type = machine_elf_read_u32_le(elf_file->bytes, header_offset + 4u);
        uint32_t sh_flags = machine_elf_read_u32_le(elf_file->bytes, header_offset + 8u);
        uint32_t sh_addr = machine_elf_read_u32_le(elf_file->bytes, header_offset + 12u);
        uint32_t sh_offset = machine_elf_read_u32_le(elf_file->bytes, header_offset + 16u);
        uint32_t sh_size = machine_elf_read_u32_le(elf_file->bytes, header_offset + 20u);
        uint32_t sh_link = machine_elf_read_u32_le(elf_file->bytes, header_offset + 24u);
        uint32_t sh_info = machine_elf_read_u32_le(elf_file->bytes, header_offset + 28u);
        uint32_t sh_addralign = machine_elf_read_u32_le(elf_file->bytes, header_offset + 32u);
        uint32_t sh_entsize = machine_elf_read_u32_le(elf_file->bytes, header_offset + 36u);
        const char *header_name;

        if (sh_name >= shstrtab_section->byte_count) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-126: section name offset exceeds shstrtab");
            return 0;
        }
        header_name = (const char *)(elf_file->bytes + shstrtab_section->file_offset + sh_name);
        if (strcmp(header_name, section->name ? section->name : "") != 0 ||
            sh_type != (uint32_t)section->type ||
            sh_flags != section->flags ||
            sh_addr != 0u ||
            sh_offset != (uint32_t)section->file_offset ||
            sh_size != (uint32_t)section->byte_count ||
            sh_link != section->link ||
            sh_info != section->info ||
            sh_addralign != section->align ||
            sh_entsize != section->entry_size) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-127: section header bytes disagree with section metadata");
            return 0;
        }
    }
    for (section_index = 0u; section_index < elf_file->symbol_count; ++section_index) {
        const MachineElfSymbol *symbol = &elf_file->symbols[section_index];
        size_t entry_offset = symtab_section->file_offset + section_index * MACHINE_ELF_SYMBOL_ENTRY_SIZE;
        uint32_t st_name = machine_elf_read_u32_le(elf_file->bytes, entry_offset + 0u);
        uint32_t st_value = machine_elf_read_u32_le(elf_file->bytes, entry_offset + 4u);
        uint32_t st_size = machine_elf_read_u32_le(elf_file->bytes, entry_offset + 8u);
        unsigned char st_info = elf_file->bytes[entry_offset + 12u];
        unsigned char st_other = elf_file->bytes[entry_offset + 13u];
        uint16_t st_shndx = machine_elf_read_u16_le(elf_file->bytes, entry_offset + 14u);
        const char *header_name;

        if (st_name >= strtab_section->byte_count) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-128: symbol name offset exceeds strtab");
            return 0;
        }
        header_name = (const char *)(elf_file->bytes + strtab_section->file_offset + st_name);
        if (strcmp(header_name, symbol->name ? symbol->name : "") != 0 ||
            st_value != symbol->value ||
            st_size != symbol->size ||
            st_info != (unsigned char)(((uint32_t)symbol->binding << 4u) | (uint32_t)symbol->type) ||
            st_other != 0u ||
            st_shndx != (uint16_t)(symbol->is_defined ? symbol->section_index : MACHINE_ELF_SHN_UNDEF)) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-129: symbol table bytes disagree with symbol metadata");
            return 0;
        }
    }
    for (section_index = 0u; section_index < elf_file->relocation_count; ++section_index) {
        const MachineElfRelocation *relocation = &elf_file->relocations[section_index];
        size_t entry_offset = elf_file->sections[elf_file->rel_text_section_index].file_offset +
            section_index * MACHINE_ELF_RELOCATION_ENTRY_SIZE;
        uint32_t r_offset = machine_elf_read_u32_le(elf_file->bytes, entry_offset + 0u);
        uint32_t r_info = machine_elf_read_u32_le(elf_file->bytes, entry_offset + 4u);
        if (!relocation->symbol_name || relocation->symbol_name[0] == '\0' ||
            relocation->symbol_index >= elf_file->symbol_count ||
            strcmp(relocation->symbol_name, elf_file->symbols[relocation->symbol_index].name) != 0 ||
            relocation->section_index != elf_file->rel_text_section_index ||
            r_offset != relocation->offset ||
            (r_info >> 8u) != relocation->symbol_index ||
            (r_info & 0xFFu) != relocation->type ||
            relocation->type != machine_elf_map_relocation_type(elf_file->target_profile, relocation->source_kind)) {
            machine_elf_set_error(error, 0, 0, "MACHINE-ELF-130: invalid elf relocation symbol mapping");
            return 0;
        }
    }
    return 1;
}

#endif
