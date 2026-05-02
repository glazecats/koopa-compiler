#ifndef MACHINE_CONTAINER_H
#define MACHINE_CONTAINER_H

#include <stddef.h>

#include "machine/reloc.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineContainerError;

typedef struct {
    size_t header_offset;
    size_t header_size;
    size_t section_table_offset;
    size_t symbol_table_offset;
    size_t relocation_table_offset;
    size_t string_table_offset;
    size_t string_table_size;
    size_t payload_offset;
    size_t payload_size;
    size_t total_byte_count;
} MachineContainerLayoutSummary;

typedef struct {
    size_t reloc_section_index;
    size_t object_section_index;
    const char *name;
    size_t logical_start_byte_offset;
    size_t byte_count;
    size_t file_offset;
    size_t relocation_count;
} MachineContainerSectionSummary;

typedef struct {
    size_t reloc_section_index;
    size_t object_section_index;
    char *name;
    size_t logical_start_byte_offset;
    size_t byte_count;
    size_t file_offset;
    size_t relocation_start_index;
    size_t relocation_count;
} MachineContainerSection;

typedef struct {
    MachineRelocFile reloc_file;
    MachineContainerSection *sections;
    size_t section_count;
    size_t section_capacity;
    unsigned char *bytes;
    size_t byte_count;
    size_t header_offset;
    size_t header_size;
    size_t section_table_offset;
    size_t symbol_table_offset;
    size_t relocation_table_offset;
    size_t string_table_offset;
    size_t string_table_size;
    size_t payload_offset;
    size_t payload_size;
} MachineContainerFile;

void machine_container_file_init(MachineContainerFile *container_file);
void machine_container_file_free(MachineContainerFile *container_file);

int machine_container_file_get_summary(const MachineContainerFile *container_file,
    size_t *out_object_byte_count,
    size_t *out_section_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_container_byte_count);
int machine_container_file_get_reloc_file(const MachineContainerFile *container_file,
    const MachineRelocFile **out_reloc_file);
int machine_container_file_get_layout_summary(const MachineContainerFile *container_file,
    MachineContainerLayoutSummary *out_summary);
int machine_container_file_copy_bytes(const MachineContainerFile *container_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineContainerError *error);
int machine_container_file_get_section(const MachineContainerFile *container_file,
    size_t section_index,
    const MachineContainerSection **out_section);
int machine_container_file_find_section_by_name(const MachineContainerFile *container_file,
    const char *section_name,
    size_t *out_section_index,
    const MachineContainerSection **out_section);

int machine_container_section_get_summary(const MachineContainerSection *section,
    MachineContainerSectionSummary *out_summary);

int machine_container_build_from_machine_reloc_file(const MachineRelocFile *reloc_file,
    MachineContainerFile *out_container_file,
    MachineContainerError *error);
int machine_container_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineContainerFile *out_container_file,
    MachineContainerError *error);
int machine_container_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineContainerFile *out_container_file,
    MachineContainerError *error);

int machine_container_verify_file(const MachineContainerFile *container_file, MachineContainerError *error);
int machine_container_dump_file(const MachineContainerFile *container_file,
    char **out_text,
    MachineContainerError *error);
int machine_container_build_dump_from_machine_reloc_file(const MachineRelocFile *reloc_file,
    char **out_text,
    MachineContainerError *error);
int machine_container_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineContainerError *error);

#endif
