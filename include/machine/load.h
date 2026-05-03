#ifndef MACHINE_LOAD_H
#define MACHINE_LOAD_H

#include <stddef.h>

#include "machine/exec.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineLoadError;

typedef struct {
    const char *name;
    size_t exec_segment_index;
    size_t virtual_address;
    size_t file_byte_count;
    size_t memory_byte_count;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineLoadSegmentSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    size_t entry_virtual_address;
    size_t segment_count;
    size_t file_byte_count;
    size_t memory_byte_count;
} MachineLoadHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    size_t segment_alignment;
} MachineLoadTargetPolicySummary;

typedef struct {
    size_t base_virtual_address;
    size_t end_virtual_address;
    size_t memory_byte_count;
    size_t entry_offset;
} MachineLoadMemorySummary;

typedef struct {
    char *name;
    size_t exec_segment_index;
    size_t virtual_address;
    size_t file_byte_count;
    size_t memory_byte_count;
    unsigned char *bytes;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineLoadSegment;

typedef struct {
    MachineExecFile exec_file;
    size_t entry_virtual_address;
    MachineLoadSegment *segments;
    size_t segment_count;
    size_t segment_capacity;
    size_t total_memory_byte_count;
} MachineLoadFile;

typedef struct {
    MachineLoadFile file;
    MachineLoadHeaderSummary header_summary;
    MachineLoadTargetPolicySummary target_policy_summary;
    MachineLoadMemorySummary memory_summary;
    MachineLoadSegmentSummary *segment_summaries;
    size_t segment_summary_count;
    size_t *executable_segment_indices;
    size_t executable_segment_count;
    size_t *non_executable_segment_indices;
    size_t non_executable_segment_count;
} MachineLoadReport;

typedef struct {
    const MachineLoadReport *report;
    const MachineLoadHeaderSummary *header_summary;
    const MachineLoadTargetPolicySummary *target_policy_summary;
    const MachineLoadMemorySummary *memory_summary;
    size_t entry_segment_index;
    const MachineLoadSegmentSummary *entry_segment_summary;
    const size_t *executable_segment_indices;
    size_t executable_segment_count;
    const size_t *non_executable_segment_indices;
    size_t non_executable_segment_count;
} MachineLoadReportOverviewArtifact;

typedef struct {
    const MachineLoadReport *report;
    size_t segment_index;
    const MachineLoadSegmentSummary *segment_summary;
    int is_entry_segment;
    int is_executable_segment;
} MachineLoadReportSegmentArtifact;

typedef enum {
    MACHINE_LOAD_SEGMENT_FILTER_EXECUTABLE = 0,
    MACHINE_LOAD_SEGMENT_FILTER_NON_EXECUTABLE,
    MACHINE_LOAD_SEGMENT_FILTER_ENTRY
} MachineLoadReportSegmentFilterKind;

void machine_load_file_init(MachineLoadFile *load_file);
void machine_load_file_free(MachineLoadFile *load_file);
void machine_load_report_init(MachineLoadReport *report);
void machine_load_report_free(MachineLoadReport *report);
int machine_load_clone_file(const MachineLoadFile *source,
    MachineLoadFile *out_clone,
    MachineLoadError *error);
int machine_load_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineLoadTargetPolicySummary *out_summary);
int machine_load_file_get_target_policy_summary(const MachineLoadFile *load_file,
    MachineLoadTargetPolicySummary *out_summary);

int machine_load_file_get_summary(const MachineLoadFile *load_file,
    size_t *out_segment_count,
    size_t *out_file_byte_count,
    size_t *out_memory_byte_count,
    size_t *out_executable_segment_count);
int machine_load_file_get_source_elf_artifact_summary(const MachineLoadFile *load_file,
    MachineElfArtifactSummary *out_summary);
int machine_load_file_get_header_summary(const MachineLoadFile *load_file,
    MachineLoadHeaderSummary *out_summary);
int machine_load_file_get_memory_summary(const MachineLoadFile *load_file,
    MachineLoadMemorySummary *out_summary);
int machine_load_file_get_segment(const MachineLoadFile *load_file,
    size_t segment_index,
    const MachineLoadSegment **out_segment);
int machine_load_file_find_segment_by_name(const MachineLoadFile *load_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment);
int machine_load_file_find_segment_covering_virtual_address(const MachineLoadFile *load_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment);
int machine_load_file_get_memory_byte_at_virtual_address(const MachineLoadFile *load_file,
    size_t virtual_address,
    unsigned char *out_byte);
int machine_load_file_copy_flat_memory_image(const MachineLoadFile *load_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineLoadError *error);
int machine_load_file_get_entry_segment(const MachineLoadFile *load_file,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment);
int machine_load_file_get_executable_segment_count(const MachineLoadFile *load_file,
    size_t *out_count);
int machine_load_file_get_executable_segment_by_index(const MachineLoadFile *load_file,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineLoadSegment **out_segment);

int machine_load_segment_get_summary(const MachineLoadSegment *segment,
    MachineLoadSegmentSummary *out_summary);

int machine_load_build_from_machine_exec_file(const MachineExecFile *exec_file,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_exec_report(const MachineExecReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_image_file(const MachineImageFile *image_file,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_image_report(const MachineImageReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_elf_report(const MachineElfReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);
int machine_load_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLoadFile *out_load_file,
    MachineLoadError *error);

int machine_load_build_report_from_file(const MachineLoadFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_exec_file(const MachineExecFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_exec_report(const MachineExecReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_image_file(const MachineImageFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_image_report(const MachineImageReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLoadReport *out_report,
    MachineLoadError *error);
int machine_load_report_refresh(MachineLoadReport *report,
    MachineLoadError *error);

int machine_load_verify_file(const MachineLoadFile *load_file,
    MachineLoadError *error);
int machine_load_dump_file(const MachineLoadFile *load_file,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_file(const MachineLoadFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);

int machine_load_report_get_summary(const MachineLoadReport *report,
    size_t *out_segment_count,
    size_t *out_file_byte_count,
    size_t *out_memory_byte_count,
    size_t *out_executable_segment_count);
int machine_load_report_get_overview_artifact(const MachineLoadReport *report,
    MachineLoadReportOverviewArtifact *out_artifact);
int machine_load_report_get_file(const MachineLoadReport *report,
    const MachineLoadFile **out_file);
int machine_load_report_get_source_elf_artifact_summary_artifact(const MachineLoadReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_load_report_get_header_summary_artifact(const MachineLoadReport *report,
    const MachineLoadHeaderSummary **out_summary);
int machine_load_report_get_target_policy_summary_artifact(const MachineLoadReport *report,
    const MachineLoadTargetPolicySummary **out_summary);
int machine_load_report_get_memory_summary_artifact(const MachineLoadReport *report,
    const MachineLoadMemorySummary **out_summary);
int machine_load_report_get_entry_segment_summary_artifact(const MachineLoadReport *report,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary);
int machine_load_report_get_segment_summary(const MachineLoadReport *report,
    size_t segment_index,
    const MachineLoadSegmentSummary **out_summary);
int machine_load_report_get_segment_artifact(const MachineLoadReport *report,
    size_t segment_index,
    MachineLoadReportSegmentArtifact *out_artifact);
int machine_load_report_find_segment_summary_by_name(const MachineLoadReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary);
int machine_load_report_find_segment_artifact_by_name(const MachineLoadReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact);
int machine_load_report_find_segment_summary_covering_virtual_address(const MachineLoadReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary);
int machine_load_report_find_segment_artifact_covering_virtual_address(const MachineLoadReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact);
int machine_load_report_get_segment_filter_count(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t *out_count);
int machine_load_report_get_segment_summary_by_filter_index(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary);
int machine_load_report_get_segment_artifact_by_filter_index(const MachineLoadReport *report,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact);
int machine_load_report_overview_artifact_get_segment_summary(
    const MachineLoadReportOverviewArtifact *artifact,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary);
int machine_load_report_overview_artifact_get_segment_artifact(
    const MachineLoadReportOverviewArtifact *artifact,
    MachineLoadReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineLoadReportSegmentArtifact *out_artifact);
int machine_load_report_get_executable_segment_count(const MachineLoadReport *report,
    size_t *out_count);
int machine_load_report_get_executable_segment_summary_by_index(const MachineLoadReport *report,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineLoadSegmentSummary **out_summary);

int machine_load_dump_report(const MachineLoadReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_file(const MachineLoadFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLoadError *error);
int machine_load_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLoadError *error);

#endif
