#if defined(__INTELLISENSE__) && !defined(MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ELF_SPLIT_AGGREGATOR)
#define MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT
#include "machine_elf.c"
#else

int machine_elf_dump_file(const MachineElfFile *elf_file,
    char **out_text,
    MachineElfError *error) {
    MachineElfStringBuilder builder;
    size_t section_index;
    int ok = 0;

    if (out_text) {
        *out_text = NULL;
    }
    if (!elf_file || !out_text) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-118: invalid elf dump contract");
        return 0;
    }
    if (!machine_elf_verify_file(elf_file, error)) {
        return 0;
    }

    memset(&builder, 0, sizeof(builder));
    if (!machine_elf_append_format(
            &builder,
            "machine_elf profile=%s origin=%s semantics=%s bytes=%zu sections=%zu symbols=%zu relocations=%zu shoff=%zu\nsections:\n",
            machine_elf_target_profile_name(elf_file->target_profile),
            machine_elf_target_profile_name(elf_file->origin_profile),
            machine_elf_relocation_semantics_name(elf_file->relocation_semantics),
            elf_file->byte_count,
            elf_file->section_count,
            elf_file->symbol_count,
            elf_file->relocation_count,
            elf_file->section_header_offset)) {
        goto cleanup;
    }
    for (section_index = 0; section_index < elf_file->section_count; ++section_index) {
        const MachineElfSection *section = &elf_file->sections[section_index];

        if (!machine_elf_append_format(
                &builder,
                "  esec.%zu %s type=%u file@%zu bytes=%zu link=%u info=%u align=%u entsize=%u\n",
                section_index,
                section->name ? section->name : "<null>",
                (unsigned)section->type,
                section->file_offset,
                section->byte_count,
                section->link,
                section->info,
                section->align,
                section->entry_size)) {
            goto cleanup;
        }
    }
    if (!machine_elf_append_format(&builder, "symbols:\n")) {
        goto cleanup;
    }
    for (section_index = 0; section_index < elf_file->symbol_count; ++section_index) {
        const MachineElfSymbol *symbol = &elf_file->symbols[section_index];
        if (!machine_elf_append_format(
                &builder,
                "  esym.%zu %s bind=%s type=%u defined=%d sec=%zu value=%u size=%u\n",
                section_index,
                symbol->name ? symbol->name : "<null>",
                symbol->binding == MACHINE_ELF_SYMBOL_LOCAL ? "local" : "global",
                (unsigned)symbol->type,
                symbol->is_defined,
                symbol->section_index,
                symbol->value,
                symbol->size)) {
            goto cleanup;
        }
    }
    if (!machine_elf_append_format(&builder, "relocations:\n")) {
        goto cleanup;
    }
    for (section_index = 0; section_index < elf_file->relocation_count; ++section_index) {
        const MachineElfRelocation *relocation = &elf_file->relocations[section_index];
        if (!machine_elf_append_format(
                &builder,
                "  erel.%zu kind=%s sec=%zu off=%u sym=%u type=%u name=%s\n",
                section_index,
                machine_elf_relocation_kind_name(relocation->source_kind),
                relocation->section_index,
                relocation->offset,
                relocation->symbol_index,
                relocation->type,
                relocation->symbol_name ? relocation->symbol_name : "<null>")) {
            goto cleanup;
        }
    }

    *out_text = builder.data;
    builder.data = NULL;
    ok = 1;

cleanup:
    free(builder.data);
    return ok;
}

#endif

int machine_elf_dump_report(const MachineElfReport *report,
    char **out_text,
    MachineElfError *error) {
    if (!report) {
        machine_elf_set_error(error, 0, 0, "MACHINE-ELF-151: invalid elf report dump contract");
        return 0;
    }
    return machine_elf_dump_file(&report->file, out_text, error);
}

int machine_elf_build_report_dump_from_file(const MachineElfFile *source,
    char **out_text,
    MachineElfError *error) {
    MachineElfReport report;
    int ok;

    machine_elf_report_init(&report);
    ok = machine_elf_build_report_from_file(source, &report, error) &&
        machine_elf_dump_report(&report, out_text, error);
    machine_elf_report_free(&report);
    return ok;
}

int machine_elf_build_report_dump_from_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfReport report;
    int ok;

    machine_elf_report_init(&report);
    ok = machine_elf_build_report_from_file_with_profile(source, profile, &report, error) &&
        machine_elf_dump_report(&report, out_text, error);
    machine_elf_report_free(&report);
    return ok;
}

int machine_elf_build_dump_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_container_file_with_profile(container_file, profile, &elf_file, error) &&
        machine_elf_dump_file(&elf_file, out_text, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_dump_from_machine_container_file(const MachineContainerFile *container_file,
    char **out_text,
    MachineElfError *error) {
    return machine_elf_build_dump_from_machine_container_file_with_profile(
        container_file,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_text,
        error);
}

int machine_elf_build_report_dump_from_machine_container_file(const MachineContainerFile *container_file,
    char **out_text,
    MachineElfError *error) {
    return machine_elf_build_report_dump_from_machine_container_file_with_profile(
        container_file,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_text,
        error);
}

int machine_elf_build_report_dump_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfReport report;
    int ok;

    machine_elf_report_init(&report);
    ok = machine_elf_build_report_from_machine_container_file_with_profile(
            container_file,
            profile,
            &report,
            error) &&
        machine_elf_dump_report(&report, out_text, error);
    machine_elf_report_free(&report);
    return ok;
}

int machine_elf_build_dump_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_build_from_machine_ir_report_with_profile(report, profile, &elf_file, error) &&
        machine_elf_dump_file(&elf_file, out_text, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineElfError *error) {
    return machine_elf_build_dump_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_text,
        error);
}

int machine_elf_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineElfError *error) {
    return machine_elf_build_report_dump_from_machine_ir_report_with_profile(
        report,
        MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32,
        out_text,
        error);
}

int machine_elf_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfReport report_artifact;
    int ok;

    machine_elf_report_init(&report_artifact);
    ok = machine_elf_build_report_from_machine_ir_report_with_profile(
            report,
            profile,
            &report_artifact,
            error) &&
        machine_elf_dump_report(&report_artifact, out_text, error);
    machine_elf_report_free(&report_artifact);
    return ok;
}

int machine_elf_build_dump_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineElfError *error) {
    return machine_elf_build_dump_from_bytes_with_profile(
        bytes,
        byte_count,
        (MachineElfTargetProfile)-1,
        out_text,
        error);
}

int machine_elf_build_dump_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfFile elf_file;
    int ok;

    machine_elf_file_init(&elf_file);
    ok = machine_elf_parse_file_from_bytes(bytes, byte_count, &elf_file, error);
    if (ok && profile != (MachineElfTargetProfile)-1) {
        elf_file.target_profile = profile;
        ok = machine_elf_refresh_bytes(&elf_file, error);
    }
    ok = ok &&
        machine_elf_dump_file(&elf_file, out_text, error);
    machine_elf_file_free(&elf_file);
    return ok;
}

int machine_elf_build_report_dump_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineElfError *error) {
    return machine_elf_build_report_dump_from_bytes_with_profile(
        bytes,
        byte_count,
        (MachineElfTargetProfile)-1,
        out_text,
        error);
}

int machine_elf_build_report_dump_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error) {
    MachineElfReport report;
    int ok;

    machine_elf_report_init(&report);
    ok = machine_elf_build_report_from_bytes_with_profile(bytes, byte_count, profile, &report, error) &&
        machine_elf_dump_report(&report, out_text, error);
    machine_elf_report_free(&report);
    return ok;
}
