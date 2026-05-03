#ifndef MACHINE_OBJECT_H
#define MACHINE_OBJECT_H

#include <stddef.h>

#include "machine/bytes.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineObjectError;

typedef MachineBytesFixupKind MachineObjectFixupKind;
typedef MachineBytesFixupTargetKind MachineObjectFixupTargetKind;
typedef MachineBytesSymbolKind MachineObjectSymbolKind;
typedef MachineBytesSectionKind MachineObjectSectionKind;

typedef struct {
    MachineBytesTargetProfile target_profile;
    MachineBytesTargetPolicySummary bytes_policy;
    int preserves_preview_target_byte_offsets;
    int preserves_direct_fallthrough_honesty;
} MachineObjectTargetPolicySummary;

typedef struct {
    size_t call_fixup_count;
    size_t primary_control_fixup_count;
    size_t secondary_control_fixup_count;
    size_t data_address_fixup_count;
    size_t data_load_fixup_count;
    size_t data_store_fixup_count;
} MachineObjectFixupFamilySummary;

typedef struct {
    MachineObjectSectionKind kind;
    const char *name;
    size_t start_byte_offset;
    size_t end_byte_offset;
    size_t byte_count;
    size_t function_count;
    size_t block_count;
    size_t symbol_count;
    size_t fixup_count;
} MachineObjectSectionSummary;

typedef struct {
    MachineObjectSymbolKind kind;
    const char *name;
    int is_defined;
    int has_section_index;
    size_t section_index;
    size_t byte_offset_in_section;
    size_t byte_count;
    size_t incoming_fixup_count;
    int has_function_index;
    size_t function_index;
    int has_emit_index;
    size_t emit_index;
} MachineObjectSymbolSummary;

typedef struct {
    MachineObjectFixupKind kind;
    MachineObjectFixupTargetKind target_kind;
    size_t section_index;
    const char *source_label_name;
    size_t owner_byte_offset;
    size_t owner_byte_count;
    size_t patch_byte_offset;
    size_t patch_byte_count;
    const char *target_name;
    int has_target_function_index;
    size_t target_function_index;
    int has_target_emit_index;
    size_t target_emit_index;
    int has_target_byte_offset;
    size_t target_byte_offset;
    int has_target_symbol_index;
    size_t target_symbol_index;
} MachineObjectFixupSummary;

typedef struct {
    MachineObjectSectionKind kind;
    char *name;
    size_t start_byte_offset;
    size_t end_byte_offset;
    unsigned char *bytes;
    size_t byte_count;
    size_t function_count;
    size_t block_count;
    size_t symbol_start_index;
    size_t symbol_count;
    size_t fixup_start_index;
    size_t fixup_count;
} MachineObjectSection;

typedef struct {
    MachineObjectSymbolKind kind;
    char *name;
    int is_defined;
    int has_section_index;
    size_t section_index;
    size_t byte_offset_in_section;
    size_t byte_count;
    size_t incoming_fixup_count;
    int has_function_index;
    size_t function_index;
    int has_emit_index;
    size_t emit_index;
} MachineObjectSymbol;

typedef struct {
    MachineObjectFixupKind kind;
    MachineObjectFixupTargetKind target_kind;
    size_t section_index;
    char *source_label_name;
    size_t owner_byte_offset;
    size_t owner_byte_count;
    size_t patch_byte_offset;
    size_t patch_byte_count;
    char *target_name;
    int has_target_function_index;
    size_t target_function_index;
    int has_target_emit_index;
    size_t target_emit_index;
    int has_target_byte_offset;
    size_t target_byte_offset;
    int has_target_symbol_index;
    size_t target_symbol_index;
} MachineObjectFixup;

typedef struct {
    MachineBytesTargetProfile target_profile;
    MachineObjectSection *sections;
    size_t section_count;
    size_t section_capacity;
    MachineObjectSymbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    MachineObjectFixup *fixups;
    size_t fixup_count;
    size_t fixup_capacity;
    size_t total_byte_count;
} MachineObjectFile;

typedef struct {
    MachineObjectFile file;
    MachineObjectTargetPolicySummary target_policy_summary;
    MachineObjectFixupFamilySummary fixup_family_summary;
    MachineObjectSectionSummary *section_summaries;
    size_t section_summary_count;
    MachineObjectSymbolSummary *symbol_summaries;
    size_t symbol_summary_count;
    MachineObjectFixupSummary *fixup_summaries;
    size_t fixup_summary_count;
} MachineObjectReport;

void machine_object_file_init(MachineObjectFile *object_file);
void machine_object_file_free(MachineObjectFile *object_file);
void machine_object_report_init(MachineObjectReport *report);
void machine_object_report_free(MachineObjectReport *report);

int machine_object_file_get_summary(const MachineObjectFile *object_file,
    size_t *out_total_byte_count,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_fixup_count);
int machine_object_get_target_policy_summary(MachineBytesTargetProfile profile,
    MachineObjectTargetPolicySummary *out_summary);
int machine_object_file_get_target_policy_summary(const MachineObjectFile *object_file,
    MachineObjectTargetPolicySummary *out_summary);
int machine_object_file_get_fixup_family_summary(const MachineObjectFile *object_file,
    MachineObjectFixupFamilySummary *out_summary);
int machine_object_file_get_section(const MachineObjectFile *object_file,
    size_t section_index,
    const MachineObjectSection **out_section);
int machine_object_file_find_section_by_name(const MachineObjectFile *object_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineObjectSection **out_section);
int machine_object_file_copy_section_bytes(const MachineObjectFile *object_file,
    size_t section_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineObjectError *error);
int machine_object_file_get_section_symbols(const MachineObjectFile *object_file,
    size_t section_index,
    size_t *out_count,
    const MachineObjectSymbol **out_symbols);
int machine_object_file_get_section_fixups(const MachineObjectFile *object_file,
    size_t section_index,
    size_t *out_count,
    const MachineObjectFixup **out_fixups);
int machine_object_file_get_symbol(const MachineObjectFile *object_file,
    size_t symbol_index,
    const MachineObjectSymbol **out_symbol);
int machine_object_file_find_symbol_by_name(const MachineObjectFile *object_file,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineObjectSymbol **out_symbol);
int machine_object_file_get_fixup(const MachineObjectFile *object_file,
    size_t fixup_index,
    const MachineObjectFixup **out_fixup);

int machine_object_section_get_summary(const MachineObjectSection *section,
    MachineObjectSectionSummary *out_summary);
int machine_object_symbol_get_summary(const MachineObjectSymbol *symbol,
    MachineObjectSymbolSummary *out_summary);
int machine_object_fixup_get_summary(const MachineObjectFixup *fixup,
    MachineObjectFixupSummary *out_summary);

int machine_object_build_from_machine_bytes_report(const MachineBytesReport *report,
    MachineObjectFile *out_object_file,
    MachineObjectError *error);
int machine_object_build_from_machine_bytes_program(const MachineBytesProgram *program,
    MachineObjectFile *out_object_file,
    MachineObjectError *error);
int machine_object_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineObjectFile *out_object_file,
    MachineObjectError *error);
int machine_object_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineObjectFile *out_object_file,
    MachineObjectError *error);
int machine_object_build_report_from_file(const MachineObjectFile *source,
    MachineObjectReport *out_report,
    MachineObjectError *error);
int machine_object_build_report_from_machine_bytes_report(const MachineBytesReport *report,
    MachineObjectReport *out_report,
    MachineObjectError *error);
int machine_object_build_report_from_machine_bytes_program(const MachineBytesProgram *program,
    MachineObjectReport *out_report,
    MachineObjectError *error);
int machine_object_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineObjectReport *out_report,
    MachineObjectError *error);

int machine_object_verify_file(const MachineObjectFile *object_file, MachineObjectError *error);
int machine_object_report_get_summary(const MachineObjectReport *report,
    size_t *out_total_byte_count,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_fixup_count);
int machine_object_report_get_file(const MachineObjectReport *report,
    const MachineObjectFile **out_file);
int machine_object_report_get_target_policy_summary_artifact(const MachineObjectReport *report,
    const MachineObjectTargetPolicySummary **out_summary);
int machine_object_report_get_fixup_family_summary_artifact(const MachineObjectReport *report,
    const MachineObjectFixupFamilySummary **out_summary);
int machine_object_report_get_section_summary(const MachineObjectReport *report,
    size_t section_index,
    const MachineObjectSectionSummary **out_summary);
int machine_object_report_find_section_summary_by_name(const MachineObjectReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineObjectSectionSummary **out_summary);
int machine_object_report_get_symbol_summary(const MachineObjectReport *report,
    size_t symbol_index,
    const MachineObjectSymbolSummary **out_summary);
int machine_object_report_find_symbol_summary_by_name(const MachineObjectReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineObjectSymbolSummary **out_summary);
int machine_object_report_get_fixup_summary(const MachineObjectReport *report,
    size_t fixup_index,
    const MachineObjectFixupSummary **out_summary);
int machine_object_dump_file(const MachineObjectFile *object_file,
    char **out_text,
    MachineObjectError *error);
int machine_object_dump_report(const MachineObjectReport *report,
    char **out_text,
    MachineObjectError *error);
int machine_object_build_dump_from_machine_bytes_report(const MachineBytesReport *report,
    char **out_text,
    MachineObjectError *error);
int machine_object_build_dump_from_machine_bytes_program(const MachineBytesProgram *program,
    char **out_text,
    MachineObjectError *error);
int machine_object_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineObjectError *error);
int machine_object_build_report_dump_from_machine_bytes_report(const MachineBytesReport *report,
    char **out_text,
    MachineObjectError *error);
int machine_object_build_report_dump_from_machine_bytes_program(const MachineBytesProgram *program,
    char **out_text,
    MachineObjectError *error);
int machine_object_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineObjectError *error);

#endif
