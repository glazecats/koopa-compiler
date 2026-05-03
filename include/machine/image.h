#ifndef MACHINE_IMAGE_H
#define MACHINE_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "machine/elf.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineImageError;

typedef struct {
    const char *name;
    size_t image_offset;
    size_t virtual_address;
    size_t byte_count;
    size_t align;
} MachineImageSegmentSummary;

typedef struct {
    const char *name;
    MachineElfSymbolBinding binding;
    MachineElfSymbolType type;
    int is_defined;
    size_t segment_index;
    size_t value_offset;
    size_t virtual_address;
} MachineImageSymbolSummary;

typedef struct {
    MachineRelocKind source_kind;
    size_t segment_index;
    uint32_t segment_offset;
    size_t site_virtual_address;
    int is_resolved;
    size_t target_virtual_address;
    uint32_t type;
    size_t symbol_index;
    const char *symbol_name;
} MachineImageRelocationSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    int has_entry;
    size_t entry_symbol_index;
    size_t entry_virtual_address;
    size_t segment_count;
    size_t byte_count;
} MachineImageHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    size_t segment_alignment;
} MachineImageTargetPolicySummary;

typedef struct {
    char *name;
    size_t image_offset;
    size_t virtual_address;
    size_t byte_count;
    size_t align;
} MachineImageSegment;

typedef struct {
    char *name;
    MachineElfSymbolBinding binding;
    MachineElfSymbolType type;
    int is_defined;
    size_t segment_index;
    size_t value_offset;
    size_t virtual_address;
} MachineImageSymbol;

typedef struct {
    MachineRelocKind source_kind;
    size_t segment_index;
    uint32_t segment_offset;
    size_t site_virtual_address;
    int is_resolved;
    size_t target_virtual_address;
    uint32_t type;
    size_t symbol_index;
    char *symbol_name;
} MachineImageRelocation;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    int has_entry;
    size_t entry_symbol_index;
    size_t entry_virtual_address;
    MachineElfArtifactSummary source_elf_artifact_summary;
    MachineImageSegment *segments;
    size_t segment_count;
    size_t segment_capacity;
    MachineImageSymbol *symbols;
    size_t symbol_count;
    size_t symbol_capacity;
    MachineImageRelocation *relocations;
    size_t relocation_count;
    size_t relocation_capacity;
    unsigned char *bytes;
    size_t byte_count;
} MachineImageFile;

typedef struct {
    MachineImageFile file;
    MachineElfArtifactSummary source_elf_artifact_summary;
    MachineImageHeaderSummary header_summary;
    MachineImageTargetPolicySummary target_policy_summary;
    MachineImageSymbolSummary entry_symbol_summary;
    int has_entry_symbol_summary;
    MachineImageSegmentSummary *segment_summaries;
    size_t segment_summary_count;
    size_t *segment_symbol_start_indices;
    size_t *segment_symbol_counts;
    size_t *segment_symbol_indices;
    size_t segment_symbol_index_count;
    MachineImageSymbolSummary *symbol_summaries;
    size_t symbol_summary_count;
    size_t *defined_symbol_indices;
    size_t defined_symbol_count;
    size_t *undefined_symbol_indices;
    size_t undefined_symbol_count;
    size_t *global_symbol_indices;
    size_t global_symbol_count;
    size_t *local_symbol_indices;
    size_t local_symbol_count;
    size_t *defined_global_symbol_indices;
    size_t defined_global_symbol_count;
    size_t *undefined_global_symbol_indices;
    size_t undefined_global_symbol_count;
    size_t *segment_relocation_start_indices;
    size_t *segment_relocation_counts;
    size_t *segment_relocation_indices;
    size_t segment_relocation_index_count;
    size_t *segment_resolved_relocation_start_indices;
    size_t *segment_resolved_relocation_counts;
    size_t *segment_resolved_relocation_indices;
    size_t segment_resolved_relocation_index_count;
    size_t *segment_unresolved_relocation_start_indices;
    size_t *segment_unresolved_relocation_counts;
    size_t *segment_unresolved_relocation_indices;
    size_t segment_unresolved_relocation_index_count;
    size_t *symbol_relocation_start_indices;
    size_t *symbol_relocation_counts;
    size_t *symbol_relocation_indices;
    size_t symbol_relocation_index_count;
    size_t *symbol_resolved_relocation_start_indices;
    size_t *symbol_resolved_relocation_counts;
    size_t *symbol_resolved_relocation_indices;
    size_t symbol_resolved_relocation_index_count;
    size_t *symbol_unresolved_relocation_start_indices;
    size_t *symbol_unresolved_relocation_counts;
    size_t *symbol_unresolved_relocation_indices;
    size_t symbol_unresolved_relocation_index_count;
    MachineImageRelocationSummary *relocation_summaries;
    size_t relocation_summary_count;
    size_t *resolved_relocation_indices;
    size_t resolved_relocation_count;
    size_t *unresolved_relocation_indices;
    size_t unresolved_relocation_count;
    size_t *call_relocation_indices;
    size_t call_relocation_count;
    size_t *control_primary_relocation_indices;
    size_t control_primary_relocation_count;
    size_t *control_secondary_relocation_indices;
    size_t control_secondary_relocation_count;
} MachineImageReport;

typedef struct {
    const MachineImageReport *report;
    size_t segment_index;
    const MachineImageSegmentSummary *segment_summary;
    const size_t *symbol_indices;
    size_t symbol_count;
    const size_t *relocation_indices;
    size_t relocation_count;
    const size_t *resolved_relocation_indices;
    size_t resolved_relocation_count;
    const size_t *unresolved_relocation_indices;
    size_t unresolved_relocation_count;
} MachineImageReportSegmentArtifact;

typedef struct {
    const MachineImageReport *report;
    size_t symbol_index;
    const MachineImageSymbolSummary *symbol_summary;
    int is_entry_symbol;
    int has_segment_artifact;
    MachineImageReportSegmentArtifact segment_artifact;
    const size_t *incoming_relocation_indices;
    size_t incoming_relocation_count;
    const size_t *incoming_resolved_relocation_indices;
    size_t incoming_resolved_relocation_count;
    const size_t *incoming_unresolved_relocation_indices;
    size_t incoming_unresolved_relocation_count;
} MachineImageReportSymbolArtifact;

typedef struct {
    const MachineImageReport *report;
    size_t relocation_index;
    const MachineImageRelocationSummary *relocation_summary;
    MachineImageReportSegmentArtifact source_segment_artifact;
    int has_target_symbol_summary;
    size_t target_symbol_index;
    const MachineImageSymbolSummary *target_symbol_summary;
    int has_target_segment_artifact;
    MachineImageReportSegmentArtifact target_segment_artifact;
} MachineImageReportRelocationArtifact;

typedef enum {
    MACHINE_IMAGE_SYMBOL_FILTER_DEFINED = 0,
    MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED,
    MACHINE_IMAGE_SYMBOL_FILTER_GLOBAL,
    MACHINE_IMAGE_SYMBOL_FILTER_LOCAL,
    MACHINE_IMAGE_SYMBOL_FILTER_DEFINED_GLOBAL,
    MACHINE_IMAGE_SYMBOL_FILTER_UNDEFINED_GLOBAL,
} MachineImageReportSymbolFilterKind;

typedef enum {
    MACHINE_IMAGE_RELOCATION_FILTER_RESOLVED = 0,
    MACHINE_IMAGE_RELOCATION_FILTER_UNRESOLVED,
    MACHINE_IMAGE_RELOCATION_FILTER_CALL,
    MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_PRIMARY,
    MACHINE_IMAGE_RELOCATION_FILTER_CONTROL_SECONDARY,
} MachineImageReportRelocationFilterKind;

typedef struct {
    const MachineImageReport *report;
    const MachineImageHeaderSummary *header_summary;
    const MachineImageTargetPolicySummary *target_policy_summary;
    int has_entry_symbol_artifact;
    MachineImageReportSymbolArtifact entry_symbol_artifact;
    size_t segment_count;
    size_t symbol_count;
    size_t relocation_count;
    size_t byte_count;
    size_t resolved_relocation_count;
    size_t unresolved_relocation_count;
} MachineImageReportOverviewArtifact;

void machine_image_file_init(MachineImageFile *image_file);
void machine_image_file_free(MachineImageFile *image_file);
int machine_image_clone_file(const MachineImageFile *source,
    MachineImageFile *out_image_file,
    MachineImageError *error);
void machine_image_report_init(MachineImageReport *report);
void machine_image_report_free(MachineImageReport *report);

int machine_image_file_get_summary(const MachineImageFile *image_file,
    size_t *out_segment_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count);
int machine_image_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineImageTargetPolicySummary *out_summary);
int machine_image_file_get_target_profile(const MachineImageFile *image_file,
    MachineElfTargetProfile *out_profile);
int machine_image_file_get_source_elf_artifact_summary(const MachineImageFile *image_file,
    MachineElfArtifactSummary *out_summary);
int machine_image_file_get_target_policy_summary(const MachineImageFile *image_file,
    MachineImageTargetPolicySummary *out_summary);
int machine_image_file_get_header_summary(const MachineImageFile *image_file,
    MachineImageHeaderSummary *out_summary);
int machine_image_file_copy_bytes(const MachineImageFile *image_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineImageError *error);
int machine_image_file_get_segment(const MachineImageFile *image_file,
    size_t segment_index,
    const MachineImageSegment **out_segment);
int machine_image_file_find_segment_by_name(const MachineImageFile *image_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineImageSegment **out_segment);
int machine_image_file_find_segment_covering_virtual_address(const MachineImageFile *image_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineImageSegment **out_segment);
int machine_image_segment_copy_bytes(const MachineImageFile *image_file,
    size_t segment_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineImageError *error);
int machine_image_file_get_segment_symbol_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count);
int machine_image_file_get_symbol(const MachineImageFile *image_file,
    size_t symbol_index,
    const MachineImageSymbol **out_symbol);
int machine_image_file_get_symbol_relocation_count(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t *out_count);
int machine_image_file_get_symbol_resolved_relocation_count(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t *out_count);
int machine_image_file_get_symbol_unresolved_relocation_count(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t *out_count);
int machine_image_file_get_symbol_relocation_by_index(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t symbol_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_symbol_resolved_relocation_by_index(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t symbol_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_symbol_unresolved_relocation_by_index(const MachineImageFile *image_file,
    size_t symbol_index,
    size_t symbol_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_segment_symbol_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_symbol_index,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol);
int machine_image_file_find_symbol_by_name(const MachineImageFile *image_file,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol);
int machine_image_file_find_symbol_by_virtual_address(const MachineImageFile *image_file,
    size_t virtual_address,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol);
int machine_image_file_get_entry_symbol(const MachineImageFile *image_file,
    size_t *out_symbol_index,
    const MachineImageSymbol **out_symbol);
int machine_image_file_get_relocation(const MachineImageFile *image_file,
    size_t relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_segment_relocation_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count);
int machine_image_file_get_segment_relocation_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_segment_resolved_relocation_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count);
int machine_image_file_get_segment_unresolved_relocation_count(const MachineImageFile *image_file,
    size_t segment_index,
    size_t *out_count);
int machine_image_file_get_segment_resolved_relocation_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_segment_unresolved_relocation_by_index(const MachineImageFile *image_file,
    size_t segment_index,
    size_t segment_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_find_relocation_by_site_virtual_address(const MachineImageFile *image_file,
    size_t site_virtual_address,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_resolved_relocation_count(const MachineImageFile *image_file,
    size_t *out_count);
int machine_image_file_get_unresolved_relocation_count(const MachineImageFile *image_file,
    size_t *out_count);
int machine_image_file_get_resolved_relocation_by_index(const MachineImageFile *image_file,
    size_t resolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);
int machine_image_file_get_unresolved_relocation_by_index(const MachineImageFile *image_file,
    size_t unresolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocation **out_relocation);

int machine_image_segment_get_summary(const MachineImageSegment *segment,
    MachineImageSegmentSummary *out_summary);
int machine_image_symbol_get_summary(const MachineImageSymbol *symbol,
    MachineImageSymbolSummary *out_summary);
int machine_image_relocation_get_summary(const MachineImageRelocation *relocation,
    MachineImageRelocationSummary *out_summary);

int machine_image_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_elf_report(const MachineElfReport *report,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineImageFile *out_image_file,
    MachineImageError *error);
int machine_image_build_report_from_file(const MachineImageFile *source,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_file_with_profile(const MachineImageFile *source,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineImageReport *out_report,
    MachineImageError *error);
int machine_image_report_refresh(MachineImageReport *report,
    MachineImageError *error);
int machine_image_verify_file(const MachineImageFile *image_file,
    MachineImageError *error);
int machine_image_dump_file(const MachineImageFile *image_file,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_file(const MachineImageFile *source,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineImageError *error);
int machine_image_report_get_summary(const MachineImageReport *report,
    size_t *out_segment_count,
    size_t *out_symbol_count,
    size_t *out_relocation_count,
    size_t *out_byte_count);
int machine_image_report_get_overview_artifact(const MachineImageReport *report,
    MachineImageReportOverviewArtifact *out_artifact);
int machine_image_report_get_file(const MachineImageReport *report,
    const MachineImageFile **out_file);
int machine_image_report_get_source_elf_artifact_summary_artifact(const MachineImageReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_image_report_get_header_summary_artifact(const MachineImageReport *report,
    const MachineImageHeaderSummary **out_summary);
int machine_image_report_get_target_policy_summary_artifact(const MachineImageReport *report,
    const MachineImageTargetPolicySummary **out_summary);
int machine_image_report_get_entry_symbol_summary_artifact(const MachineImageReport *report,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_get_entry_symbol_artifact(const MachineImageReport *report,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact);
int machine_image_report_get_segment_summary(const MachineImageReport *report,
    size_t segment_index,
    const MachineImageSegmentSummary **out_summary);
int machine_image_report_get_segment_artifact(const MachineImageReport *report,
    size_t segment_index,
    MachineImageReportSegmentArtifact *out_artifact);
int machine_image_report_find_segment_summary_by_name(const MachineImageReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineImageSegmentSummary **out_summary);
int machine_image_report_find_segment_artifact_by_name(const MachineImageReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineImageReportSegmentArtifact *out_artifact);
int machine_image_report_find_segment_summary_covering_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineImageSegmentSummary **out_summary);
int machine_image_report_find_segment_artifact_covering_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineImageReportSegmentArtifact *out_artifact);
int machine_image_report_get_segment_symbol_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count);
int machine_image_report_segment_artifact_get_symbol_summary(const MachineImageReportSegmentArtifact *artifact,
    size_t segment_symbol_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_get_symbol_summary(const MachineImageReport *report,
    size_t symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_get_symbol_artifact(const MachineImageReport *report,
    size_t symbol_index,
    MachineImageReportSymbolArtifact *out_artifact);
int machine_image_report_get_symbol_filter_count(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t *out_count);
int machine_image_report_get_symbol_relocation_summary_count(const MachineImageReport *report,
    size_t symbol_index,
    size_t *out_count);
int machine_image_report_get_symbol_resolved_relocation_summary_count(const MachineImageReport *report,
    size_t symbol_index,
    size_t *out_count);
int machine_image_report_get_symbol_unresolved_relocation_summary_count(const MachineImageReport *report,
    size_t symbol_index,
    size_t *out_count);
int machine_image_report_symbol_artifact_get_relocation_summary(
    const MachineImageReportSymbolArtifact *artifact,
    size_t symbol_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_symbol_artifact_get_resolved_relocation_summary(
    const MachineImageReportSymbolArtifact *artifact,
    size_t symbol_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_symbol_artifact_get_unresolved_relocation_summary(
    const MachineImageReportSymbolArtifact *artifact,
    size_t symbol_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_symbol_relocation_summary_by_index(const MachineImageReport *report,
    size_t symbol_index,
    size_t symbol_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_symbol_resolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t symbol_index,
    size_t symbol_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_symbol_unresolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t symbol_index,
    size_t symbol_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_symbol_summary_by_filter_index(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_get_symbol_artifact_by_filter_index(const MachineImageReport *report,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact);
int machine_image_report_overview_artifact_get_symbol_summary(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_overview_artifact_get_symbol_artifact(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportSymbolFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact);
int machine_image_report_overview_artifact_get_relocation_summary(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_overview_artifact_get_relocation_artifact(
    const MachineImageReportOverviewArtifact *artifact,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact);
int machine_image_report_get_segment_symbol_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_symbol_index,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_find_symbol_summary_by_name(const MachineImageReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_find_symbol_artifact_by_name(const MachineImageReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact);
int machine_image_report_find_symbol_summary_by_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_symbol_index,
    const MachineImageSymbolSummary **out_summary);
int machine_image_report_find_symbol_artifact_by_virtual_address(const MachineImageReport *report,
    size_t virtual_address,
    size_t *out_symbol_index,
    MachineImageReportSymbolArtifact *out_artifact);
int machine_image_report_get_relocation_summary(const MachineImageReport *report,
    size_t relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_relocation_artifact(const MachineImageReport *report,
    size_t relocation_index,
    MachineImageReportRelocationArtifact *out_artifact);
int machine_image_report_get_relocation_filter_count(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t *out_count);
int machine_image_report_get_segment_relocation_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count);
int machine_image_report_segment_artifact_get_relocation_summary(const MachineImageReportSegmentArtifact *artifact,
    size_t segment_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_segment_relocation_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_segment_resolved_relocation_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count);
int machine_image_report_get_segment_unresolved_relocation_summary_count(const MachineImageReport *report,
    size_t segment_index,
    size_t *out_count);
int machine_image_report_segment_artifact_get_resolved_relocation_summary(
    const MachineImageReportSegmentArtifact *artifact,
    size_t segment_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_segment_artifact_get_unresolved_relocation_summary(
    const MachineImageReportSegmentArtifact *artifact,
    size_t segment_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_segment_resolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_resolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_segment_unresolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t segment_index,
    size_t segment_unresolved_relocation_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_find_relocation_summary_by_site_virtual_address(const MachineImageReport *report,
    size_t site_virtual_address,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_find_relocation_artifact_by_site_virtual_address(const MachineImageReport *report,
    size_t site_virtual_address,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact);
int machine_image_report_get_relocation_summary_by_filter_index(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_relocation_artifact_by_filter_index(const MachineImageReport *report,
    MachineImageReportRelocationFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact);
int machine_image_report_get_resolved_relocation_count(const MachineImageReport *report,
    size_t *out_count);
int machine_image_report_get_resolved_relocation_index(const MachineImageReport *report,
    size_t resolved_index,
    size_t *out_relocation_index);
int machine_image_report_get_resolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t resolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_resolved_relocation_artifact_by_index(const MachineImageReport *report,
    size_t resolved_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact);
int machine_image_report_get_unresolved_relocation_count(const MachineImageReport *report,
    size_t *out_count);
int machine_image_report_get_unresolved_relocation_index(const MachineImageReport *report,
    size_t unresolved_index,
    size_t *out_relocation_index);
int machine_image_report_get_unresolved_relocation_summary_by_index(const MachineImageReport *report,
    size_t unresolved_index,
    size_t *out_relocation_index,
    const MachineImageRelocationSummary **out_summary);
int machine_image_report_get_unresolved_relocation_artifact_by_index(const MachineImageReport *report,
    size_t unresolved_index,
    size_t *out_relocation_index,
    MachineImageReportRelocationArtifact *out_artifact);
int machine_image_dump_report(const MachineImageReport *report,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineImageError *error);
int machine_image_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_file(const MachineImageFile *source,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_file_with_profile(const MachineImageFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_elf_file_with_profile(
    const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_ir_report(
    const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineImageError *error);
int machine_image_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineImageError *error);

#endif
