#ifndef MACHINE_RELOC_H
#define MACHINE_RELOC_H

#include <stddef.h>

#include "machine/object.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineRelocError;

typedef MachineObjectFixupKind MachineRelocKind;
typedef MachineObjectFixupTargetKind MachineRelocTargetKind;

typedef struct {
    size_t object_section_index;
    const char *name;
    size_t start_byte_offset;
    size_t end_byte_offset;
    size_t byte_count;
    size_t relocation_count;
} MachineRelocSectionSummary;

typedef struct {
    MachineRelocKind kind;
    MachineRelocTargetKind target_kind;
    size_t object_section_index;
    const char *source_label_name;
    size_t owner_byte_offset;
    size_t owner_byte_count;
    size_t patch_byte_offset;
    size_t patch_byte_count;
    const char *target_name;
    int has_target_symbol_index;
    size_t target_symbol_index;
    int has_target_section_index;
    size_t target_section_index;
    int has_target_byte_offset;
    size_t target_byte_offset;
    ptrdiff_t addend;
} MachineRelocationSummary;

typedef struct {
    size_t object_section_index;
    char *name;
    size_t start_byte_offset;
    size_t end_byte_offset;
    size_t byte_count;
    size_t relocation_start_index;
    size_t relocation_count;
} MachineRelocSection;

typedef struct {
    MachineRelocKind kind;
    MachineRelocTargetKind target_kind;
    size_t object_section_index;
    char *source_label_name;
    size_t owner_byte_offset;
    size_t owner_byte_count;
    size_t patch_byte_offset;
    size_t patch_byte_count;
    char *target_name;
    int has_target_symbol_index;
    size_t target_symbol_index;
    int has_target_section_index;
    size_t target_section_index;
    int has_target_byte_offset;
    size_t target_byte_offset;
    ptrdiff_t addend;
} MachineRelocation;

typedef struct {
    MachineObjectFile object_file;
    MachineRelocSection *sections;
    size_t section_count;
    size_t section_capacity;
    MachineRelocation *relocations;
    size_t relocation_count;
    size_t relocation_capacity;
} MachineRelocFile;

void machine_reloc_file_init(MachineRelocFile *reloc_file);
void machine_reloc_file_free(MachineRelocFile *reloc_file);

int machine_reloc_file_get_summary(const MachineRelocFile *reloc_file,
    size_t *out_total_byte_count,
    size_t *out_object_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_section_count,
    size_t *out_relocation_count);
int machine_reloc_file_get_object_file(const MachineRelocFile *reloc_file,
    const MachineObjectFile **out_object_file);
int machine_reloc_file_get_section(const MachineRelocFile *reloc_file,
    size_t section_index,
    const MachineRelocSection **out_section);
int machine_reloc_file_find_section_by_name(const MachineRelocFile *reloc_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineRelocSection **out_section);
int machine_reloc_file_get_section_relocations(const MachineRelocFile *reloc_file,
    size_t section_index,
    size_t *out_count,
    const MachineRelocation **out_relocations);
int machine_reloc_file_get_relocation(const MachineRelocFile *reloc_file,
    size_t relocation_index,
    const MachineRelocation **out_relocation);

int machine_reloc_section_get_summary(const MachineRelocSection *section,
    MachineRelocSectionSummary *out_summary);
int machine_relocation_get_summary(const MachineRelocation *relocation,
    MachineRelocationSummary *out_summary);

int machine_reloc_build_from_machine_object_file(const MachineObjectFile *object_file,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error);
int machine_reloc_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error);
int machine_reloc_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRelocFile *out_reloc_file,
    MachineRelocError *error);

int machine_reloc_verify_file(const MachineRelocFile *reloc_file, MachineRelocError *error);
int machine_reloc_dump_file(const MachineRelocFile *reloc_file,
    char **out_text,
    MachineRelocError *error);
int machine_reloc_build_dump_from_machine_object_file(const MachineObjectFile *object_file,
    char **out_text,
    MachineRelocError *error);
int machine_reloc_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRelocError *error);

#endif
