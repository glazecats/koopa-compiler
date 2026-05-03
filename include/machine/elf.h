#ifndef MACHINE_ELF_H
#define MACHINE_ELF_H

#include <stddef.h>
#include <stdint.h>

#include "machine/container.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineElfError;

typedef enum {
    MACHINE_ELF_TARGET_PROFILE_GENERIC_ELF32 = 0,
    MACHINE_ELF_TARGET_PROFILE_RISCV32_PREVIEW,
    MACHINE_ELF_TARGET_PROFILE_I386_PREVIEW,
} MachineElfTargetProfile;

typedef enum {
    MACHINE_ELF_RELOCATION_SEMANTICS_UNKNOWN = 0,
    MACHINE_ELF_RELOCATION_SEMANTICS_DIRECT_PATCH_SPANS,
    MACHINE_ELF_RELOCATION_SEMANTICS_IMPORTED_TABLE,
} MachineElfRelocationSemantics;

typedef enum {
    MACHINE_ELF_SECTION_NULL = 0,
    MACHINE_ELF_SECTION_PROGBITS = 1,
    MACHINE_ELF_SECTION_SYMTAB = 2,
    MACHINE_ELF_SECTION_STRTAB = 3,
    MACHINE_ELF_SECTION_NOBITS = 8,
    MACHINE_ELF_SECTION_REL = 9,
} MachineElfSectionType;

typedef enum {
    MACHINE_ELF_SYMBOL_LOCAL = 0,
    MACHINE_ELF_SYMBOL_GLOBAL = 1,
} MachineElfSymbolBinding;

typedef enum {
    MACHINE_ELF_SYMBOL_NOTYPE = 0,
    MACHINE_ELF_SYMBOL_OBJECT = 1,
    MACHINE_ELF_SYMBOL_FUNC = 2,
    MACHINE_ELF_SYMBOL_SECTION = 3,
    MACHINE_ELF_SYMBOL_FILE = 4,
} MachineElfSymbolType;

typedef struct {
    const char *name;
    MachineElfSectionType type;
    size_t file_offset;
    size_t byte_count;
    uint32_t flags;
    uint32_t link;
    uint32_t info;
    uint32_t align;
    uint32_t entry_size;
} MachineElfSectionSummary;

typedef struct {
    const char *name;
    MachineElfSymbolBinding binding;
    MachineElfSymbolType type;
    int is_defined;
    size_t section_index;
    uint32_t value;
    uint32_t size;
} MachineElfSymbolSummary;

typedef struct {
    MachineRelocKind source_kind;
    size_t section_index;
    uint32_t offset;
    uint32_t symbol_index;
    uint32_t type;
    const char *symbol_name;
} MachineElfRelocationSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineElfTargetProfile origin_profile;
    MachineElfRelocationSemantics relocation_semantics;
} MachineElfArtifactSummary;

typedef struct {
    size_t call_relocation_count;
    size_t primary_control_relocation_count;
    size_t secondary_control_relocation_count;
    size_t data_address_relocation_count;
    size_t data_load_relocation_count;
    size_t data_store_relocation_count;
} MachineElfRelocationFamilySummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    unsigned char elf_class;
    unsigned char data_encoding;
    unsigned char ident_version;
    uint16_t object_type;
    uint16_t machine;
    uint32_t elf_version;
    uint32_t flags;
    size_t section_header_offset;
    uint16_t section_header_entry_size;
    uint16_t section_header_count;
    uint16_t section_name_string_table_index;
} MachineElfHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineBytesTargetPolicySummary bytes_policy;
    uint16_t machine;
    uint32_t flags;
    uint32_t call_relocation_type;
    uint32_t primary_control_relocation_type;
    uint32_t secondary_control_relocation_type;
    uint32_t data_address_relocation_type;
    uint32_t data_load_relocation_type;
    uint32_t data_store_relocation_type;
} MachineElfTargetPolicySummary;

typedef struct {
    char *name;
    MachineElfSectionType type;
    size_t file_offset;
    size_t byte_count;
    uint32_t flags;
    uint32_t link;
    uint32_t info;
    uint32_t align;
    uint32_t entry_size;
} MachineElfSection;

typedef struct {
    char *name;
    MachineElfSymbolBinding binding;
    MachineElfSymbolType type;
    int is_defined;
    size_t section_index;
    uint32_t value;
    uint32_t size;
} MachineElfSymbol;

typedef struct {
    MachineRelocKind source_kind;
    size_t section_index;
    uint32_t offset;
    uint32_t symbol_index;
    uint32_t type;
    char *symbol_name;
} MachineElfRelocation;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineElfTargetProfile origin_profile;
    MachineElfRelocationSemantics relocation_semantics;
    MachineElfSection *sections;
    size_t section_count;
    size_t section_capacity;
    MachineElfSymbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    MachineElfRelocation *relocations;
    size_t relocation_count;
    size_t relocation_capacity;
    unsigned char *bytes;
    size_t byte_count;
    size_t section_header_offset;
    size_t text_section_index;
    size_t strtab_section_index;
    size_t symtab_section_index;
    size_t rel_text_section_index;
    size_t shstrtab_section_index;
} MachineElfFile;

typedef struct {
    MachineElfFile file;
    MachineElfArtifactSummary artifact_summary;
    MachineElfRelocationFamilySummary relocation_family_summary;
    MachineElfHeaderSummary header_summary;
    MachineElfTargetPolicySummary target_policy_summary;
    MachineElfSectionSummary *section_summaries;
    size_t section_summary_count;
    MachineElfSymbolSummary *symbol_summaries;
    size_t symbol_summary_count;
    MachineElfRelocationSummary *relocation_summaries;
    size_t relocation_summary_count;
} MachineElfReport;

void machine_elf_file_init(MachineElfFile *elf_file);
void machine_elf_file_free(MachineElfFile *elf_file);
int machine_elf_clone_file(const MachineElfFile *source,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_reprofile_file(MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineElfError *error);
void machine_elf_report_init(MachineElfReport *report);
void machine_elf_report_free(MachineElfReport *report);

const char *machine_elf_target_profile_name(MachineElfTargetProfile profile);
const char *machine_elf_relocation_semantics_name(MachineElfRelocationSemantics semantics);
int machine_elf_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineElfTargetPolicySummary *out_summary);

int machine_elf_file_get_summary(const MachineElfFile *elf_file,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count);
int machine_elf_file_get_target_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile *out_profile);
int machine_elf_file_get_artifact_summary(const MachineElfFile *elf_file,
    MachineElfArtifactSummary *out_summary);
int machine_elf_file_get_relocation_family_summary(const MachineElfFile *elf_file,
    MachineElfRelocationFamilySummary *out_summary);
int machine_elf_file_get_target_policy_summary(const MachineElfFile *elf_file,
    MachineElfTargetPolicySummary *out_summary);
int machine_elf_file_get_header_summary(const MachineElfFile *elf_file,
    MachineElfHeaderSummary *out_summary);
int machine_elf_file_copy_bytes(const MachineElfFile *elf_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_file_get_section(const MachineElfFile *elf_file,
    size_t section_index,
    const MachineElfSection **out_section);
int machine_elf_file_find_section_by_name(const MachineElfFile *elf_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineElfSection **out_section);
int machine_elf_file_get_text_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section);
int machine_elf_file_get_strtab_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section);
int machine_elf_file_get_symtab_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section);
int machine_elf_file_get_rel_text_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section);
int machine_elf_file_get_shstrtab_section(const MachineElfFile *elf_file,
    size_t *out_section_index,
    const MachineElfSection **out_section);
int machine_elf_file_get_symbol(const MachineElfFile *elf_file,
    size_t symbol_index,
    const MachineElfSymbol **out_symbol);
int machine_elf_file_find_symbol_by_name(const MachineElfFile *elf_file,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineElfSymbol **out_symbol);
int machine_elf_file_get_first_global_symbol_index(const MachineElfFile *elf_file,
    size_t *out_symbol_index);
int machine_elf_file_get_relocation(const MachineElfFile *elf_file,
    size_t relocation_index,
    const MachineElfRelocation **out_relocation);

int machine_elf_section_get_summary(const MachineElfSection *section,
    MachineElfSectionSummary *out_summary);
int machine_elf_symbol_get_summary(const MachineElfSymbol *symbol,
    MachineElfSymbolSummary *out_summary);
int machine_elf_relocation_get_summary(const MachineElfRelocation *relocation,
    MachineElfRelocationSummary *out_summary);

int machine_elf_build_report_from_file(const MachineElfFile *source,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_report_from_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_bytes_from_file(const MachineElfFile *source,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_build_bytes_from_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_build_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_build_from_machine_container_file(const MachineContainerFile *container_file,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_build_bytes_from_machine_container_file(const MachineContainerFile *container_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_build_bytes_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_build_bytes_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_build_bytes_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_build_file_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_build_file_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_build_report_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_report_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_report_from_machine_container_file(const MachineContainerFile *container_file,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_report_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineElfReport *out_report,
    MachineElfError *error);
int machine_elf_parse_file_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineElfFile *out_elf_file,
    MachineElfError *error);
int machine_elf_refresh_bytes(MachineElfFile *elf_file, MachineElfError *error);
int machine_elf_normalize_bytes_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);
int machine_elf_normalize_bytes_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineElfError *error);

int machine_elf_verify_file(const MachineElfFile *elf_file, MachineElfError *error);
int machine_elf_report_get_summary(const MachineElfReport *report,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count);
int machine_elf_report_get_file(const MachineElfReport *report,
    const MachineElfFile **out_file);
int machine_elf_report_get_artifact_summary_artifact(const MachineElfReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_elf_report_get_relocation_family_summary_artifact(const MachineElfReport *report,
    const MachineElfRelocationFamilySummary **out_summary);
int machine_elf_report_get_header_summary_artifact(const MachineElfReport *report,
    const MachineElfHeaderSummary **out_summary);
int machine_elf_report_get_target_policy_summary_artifact(const MachineElfReport *report,
    const MachineElfTargetPolicySummary **out_summary);
int machine_elf_report_get_section_summary(const MachineElfReport *report,
    size_t section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_find_section_summary_by_name(const MachineElfReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_get_text_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_get_strtab_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_get_symtab_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_get_rel_text_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_get_shstrtab_section_summary(const MachineElfReport *report,
    size_t *out_section_index,
    const MachineElfSectionSummary **out_summary);
int machine_elf_report_get_symbol_summary(const MachineElfReport *report,
    size_t symbol_index,
    const MachineElfSymbolSummary **out_summary);
int machine_elf_report_find_symbol_summary_by_name(const MachineElfReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineElfSymbolSummary **out_summary);
int machine_elf_report_get_first_global_symbol_index(const MachineElfReport *report,
    size_t *out_symbol_index);
int machine_elf_report_get_relocation_summary(const MachineElfReport *report,
    size_t relocation_index,
    const MachineElfRelocationSummary **out_summary);
int machine_elf_dump_file(const MachineElfFile *elf_file,
    char **out_text,
    MachineElfError *error);
int machine_elf_dump_report(const MachineElfReport *report,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_file(const MachineElfFile *source,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_dump_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_dump_from_machine_container_file(const MachineContainerFile *container_file,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_machine_container_file(const MachineContainerFile *container_file,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_machine_container_file_with_profile(
    const MachineContainerFile *container_file,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_dump_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_dump_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineElfError *error);
int machine_elf_build_report_dump_from_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineElfError *error);

#endif
