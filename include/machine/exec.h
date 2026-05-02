#ifndef MACHINE_EXEC_H
#define MACHINE_EXEC_H

#include <stddef.h>

#include "machine/image.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineExecError;

typedef struct {
    const char *name;
    size_t image_segment_index;
    size_t virtual_address;
    size_t byte_count;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineExecSegmentSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    size_t entry_virtual_address;
    size_t segment_count;
    size_t byte_count;
} MachineExecHeaderSummary;

typedef struct {
    char *name;
    size_t image_segment_index;
    size_t virtual_address;
    size_t byte_count;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineExecSegment;

typedef struct {
    MachineImageFile image_file;
    size_t entry_virtual_address;
    MachineExecSegment *segments;
    size_t segment_count;
    size_t segment_capacity;
} MachineExecFile;

typedef struct {
    MachineExecFile file;
    MachineExecHeaderSummary header_summary;
    MachineExecSegmentSummary *segment_summaries;
    size_t segment_summary_count;
    size_t *executable_segment_indices;
    size_t executable_segment_count;
    size_t *non_executable_segment_indices;
    size_t non_executable_segment_count;
} MachineExecReport;

typedef struct {
    const MachineExecReport *report;
    const MachineExecHeaderSummary *header_summary;
    size_t entry_segment_index;
    const MachineExecSegmentSummary *entry_segment_summary;
    const size_t *executable_segment_indices;
    size_t executable_segment_count;
    const size_t *non_executable_segment_indices;
    size_t non_executable_segment_count;
} MachineExecReportOverviewArtifact;

typedef struct {
    const MachineExecReport *report;
    size_t segment_index;
    const MachineExecSegmentSummary *segment_summary;
    int is_entry_segment;
    int is_executable_segment;
} MachineExecReportSegmentArtifact;

typedef enum {
    MACHINE_EXEC_SEGMENT_FILTER_EXECUTABLE = 0,
    MACHINE_EXEC_SEGMENT_FILTER_NON_EXECUTABLE,
    MACHINE_EXEC_SEGMENT_FILTER_ENTRY
} MachineExecReportSegmentFilterKind;

void machine_exec_file_init(MachineExecFile *exec_file);
void machine_exec_file_free(MachineExecFile *exec_file);
void machine_exec_report_init(MachineExecReport *report);
void machine_exec_report_free(MachineExecReport *report);
int machine_exec_clone_file(const MachineExecFile *source,
    MachineExecFile *out_clone,
    MachineExecError *error);

int machine_exec_file_get_summary(const MachineExecFile *exec_file,
    size_t *out_segment_count,
    size_t *out_byte_count,
    size_t *out_executable_segment_count);
int machine_exec_file_get_header_summary(const MachineExecFile *exec_file,
    MachineExecHeaderSummary *out_summary);
int machine_exec_file_get_segment(const MachineExecFile *exec_file,
    size_t segment_index,
    const MachineExecSegment **out_segment);
int machine_exec_file_find_segment_by_name(const MachineExecFile *exec_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment);
int machine_exec_file_find_segment_covering_virtual_address(const MachineExecFile *exec_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment);
int machine_exec_file_get_entry_segment(const MachineExecFile *exec_file,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment);
int machine_exec_file_get_executable_segment_count(const MachineExecFile *exec_file,
    size_t *out_count);
int machine_exec_file_get_executable_segment_by_index(const MachineExecFile *exec_file,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineExecSegment **out_segment);

int machine_exec_segment_get_summary(const MachineExecSegment *segment,
    MachineExecSegmentSummary *out_summary);

int machine_exec_build_from_machine_image_file(const MachineImageFile *image_file,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_image_report(const MachineImageReport *report,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_elf_report(const MachineElfReport *report,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineExecFile *out_exec_file,
    MachineExecError *error);
int machine_exec_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineExecFile *out_exec_file,
    MachineExecError *error);

int machine_exec_build_report_from_file(const MachineExecFile *source,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_image_file(const MachineImageFile *source,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_image_report(const MachineImageReport *report,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineExecReport *out_report,
    MachineExecError *error);
int machine_exec_report_refresh(MachineExecReport *report,
    MachineExecError *error);

int machine_exec_verify_file(const MachineExecFile *exec_file,
    MachineExecError *error);
int machine_exec_dump_file(const MachineExecFile *exec_file,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_file(const MachineExecFile *source,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);

int machine_exec_report_get_summary(const MachineExecReport *report,
    size_t *out_segment_count,
    size_t *out_byte_count,
    size_t *out_executable_segment_count);
int machine_exec_report_get_overview_artifact(const MachineExecReport *report,
    MachineExecReportOverviewArtifact *out_artifact);
int machine_exec_report_get_file(const MachineExecReport *report,
    const MachineExecFile **out_file);
int machine_exec_report_get_header_summary_artifact(const MachineExecReport *report,
    const MachineExecHeaderSummary **out_summary);
int machine_exec_report_get_entry_segment_summary_artifact(const MachineExecReport *report,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary);
int machine_exec_report_get_segment_summary(const MachineExecReport *report,
    size_t segment_index,
    const MachineExecSegmentSummary **out_summary);
int machine_exec_report_get_segment_artifact(const MachineExecReport *report,
    size_t segment_index,
    MachineExecReportSegmentArtifact *out_artifact);
int machine_exec_report_find_segment_summary_by_name(const MachineExecReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary);
int machine_exec_report_find_segment_artifact_by_name(const MachineExecReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact);
int machine_exec_report_find_segment_summary_covering_virtual_address(const MachineExecReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary);
int machine_exec_report_find_segment_artifact_covering_virtual_address(const MachineExecReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact);
int machine_exec_report_get_segment_filter_count(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t *out_count);
int machine_exec_report_get_segment_summary_by_filter_index(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary);
int machine_exec_report_get_segment_artifact_by_filter_index(const MachineExecReport *report,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact);
int machine_exec_report_overview_artifact_get_segment_summary(
    const MachineExecReportOverviewArtifact *artifact,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary);
int machine_exec_report_overview_artifact_get_segment_artifact(
    const MachineExecReportOverviewArtifact *artifact,
    MachineExecReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineExecReportSegmentArtifact *out_artifact);
int machine_exec_report_get_executable_segment_count(const MachineExecReport *report,
    size_t *out_count);
int machine_exec_report_get_executable_segment_summary_by_index(const MachineExecReport *report,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineExecSegmentSummary **out_summary);

int machine_exec_dump_report(const MachineExecReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineExecError *error);
int machine_exec_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineExecError *error);

#endif
