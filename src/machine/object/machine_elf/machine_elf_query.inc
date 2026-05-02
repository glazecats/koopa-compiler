#if defined(__INTELLISENSE__) && !defined(MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(MACHINE_ELF_SPLIT_AGGREGATOR)
#define MACHINE_ELF_SPLIT_INTELLISENSE_CONTEXT
#include "machine_elf.c"
#else

int machine_elf_report_get_summary(const MachineElfReport *report,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count) {
    if (!report) {
        return 0;
    }
    if (out_section_count) {
        *out_section_count = report->section_summary_count;
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

#endif

int machine_elf_report_get_file(const MachineElfReport *report,
    const MachineElfFile **out_file) {
    if (!report || !out_file) {
        return 0;
    }
    *out_file = &report->file;
    return 1;
}

int machine_elf_report_get_header_summary_artifact(const MachineElfReport *report,
    const MachineElfHeaderSummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->header_summary;
    return 1;
}

int machine_elf_report_get_target_policy_summary_artifact(const MachineElfReport *report,
    const MachineElfTargetPolicySummary **out_summary) {
    if (!report || !out_summary) {
        return 0;
    }
    *out_summary = &report->target_policy_summary;
    return 1;
}

int machine_elf_report_get_section_summary(const MachineElfReport *report,
    size_t section_index,
    const MachineElfSectionSummary **out_summary) {
    if (!report || !out_summary || section_index >= report->section_summary_count || !report->section_summaries) {
        return 0;
    }
    *out_summary = &report->section_summaries[section_index];
    return 1;
}

int machine_elf_report_find_section_summary_by_name(const MachineElfReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary) {
    size_t section_index;

    if (!report || !section_name || !report->section_summaries) {
        return 0;
    }
    for (section_index = 0u; section_index < report->section_summary_count; ++section_index) {
        if (report->section_summaries[section_index].name &&
            strcmp(report->section_summaries[section_index].name, section_name) == 0) {
            if (out_section_index) {
                *out_section_index = section_index;
            }
            if (out_summary) {
                *out_summary = &report->section_summaries[section_index];
            }
            return 1;
        }
    }
    return 0;
}

int machine_elf_report_get_text_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary) {
    return machine_elf_report_find_section_summary_by_name(report, ".text", out_section_index, out_summary);
}

int machine_elf_report_get_strtab_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary) {
    return machine_elf_report_find_section_summary_by_name(report, ".strtab", out_section_index, out_summary);
}

int machine_elf_report_get_symtab_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary) {
    return machine_elf_report_find_section_summary_by_name(report, ".symtab", out_section_index, out_summary);
}

int machine_elf_report_get_rel_text_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary) {
    return machine_elf_report_find_section_summary_by_name(report, ".rel.text", out_section_index, out_summary);
}

int machine_elf_report_get_shstrtab_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary) {
    return machine_elf_report_find_section_summary_by_name(report, ".shstrtab", out_section_index, out_summary);
}

int machine_elf_report_get_symbol_summary(const MachineElfReport *report,
    size_t symbol_index,
    const MachineElfSymbolSummary **out_summary) {
    if (!report || !out_summary || symbol_index >= report->symbol_summary_count || !report->symbol_summaries) {
        return 0;
    }
    *out_summary = &report->symbol_summaries[symbol_index];
    return 1;
}

int machine_elf_report_find_symbol_summary_by_name(const MachineElfReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineElfSymbolSummary **out_summary) {
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

int machine_elf_report_get_first_global_symbol_index(const MachineElfReport *report,
    size_t *out_symbol_index) {
    if (!report) {
        return 0;
    }
    return machine_elf_file_get_first_global_symbol_index(&report->file, out_symbol_index);
}

int machine_elf_report_get_relocation_summary(const MachineElfReport *report,
    size_t relocation_index,
    const MachineElfRelocationSummary **out_summary) {
    if (!report || !out_summary || relocation_index >= report->relocation_summary_count ||
        !report->relocation_summaries) {
        return 0;
    }
    *out_summary = &report->relocation_summaries[relocation_index];
    return 1;
}
