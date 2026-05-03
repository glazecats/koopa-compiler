#ifndef MACHINE_RUNTIME_H
#define MACHINE_RUNTIME_H

#include <stddef.h>

#include "machine/load.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineRuntimeError;

typedef enum {
    MACHINE_RUNTIME_SEGMENT_KIND_LOAD = 0,
    MACHINE_RUNTIME_SEGMENT_KIND_STACK
} MachineRuntimeSegmentKind;

typedef struct {
    const char *name;
    MachineRuntimeSegmentKind kind;
    size_t load_segment_index;
    size_t virtual_address;
    size_t byte_count;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineRuntimeSegmentSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    size_t entry_virtual_address;
    size_t initial_stack_pointer;
    size_t segment_count;
    size_t mapped_byte_count;
} MachineRuntimeHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t base_virtual_address;
    size_t stack_alignment;
    size_t stack_byte_count;
    size_t stack_gap_byte_count;
} MachineRuntimeTargetPolicySummary;

typedef struct {
    size_t base_virtual_address;
    size_t end_virtual_address;
    size_t span_byte_count;
    size_t mapped_byte_count;
    size_t entry_offset;
    size_t initial_stack_pointer_offset;
} MachineRuntimeMemorySummary;

typedef struct {
    size_t stack_segment_index;
    size_t base_virtual_address;
    size_t end_virtual_address;
    size_t byte_count;
    size_t initial_stack_pointer;
    size_t initial_stack_pointer_offset;
} MachineRuntimeStackSummary;

typedef struct {
    size_t base_virtual_address;
    size_t end_virtual_address;
    size_t byte_count;
} MachineRuntimeGapSummary;

typedef struct {
    size_t entry_virtual_address;
    size_t initial_stack_pointer;
    size_t stack_segment_index;
    size_t stack_base_virtual_address;
    size_t stack_end_virtual_address;
    size_t stack_byte_count;
} MachineRuntimeLaunchSummary;

typedef struct {
    size_t word_byte_count;
    size_t image_base_virtual_address;
    size_t image_end_virtual_address;
    size_t image_byte_count;
    size_t argc;
    size_t argc_virtual_address;
    size_t argv_terminator_virtual_address;
    size_t envp_terminator_virtual_address;
    size_t auxv_terminator_virtual_address;
} MachineRuntimeInitialStackSummary;

typedef struct {
    char *name;
    MachineRuntimeSegmentKind kind;
    size_t load_segment_index;
    size_t virtual_address;
    size_t byte_count;
    unsigned char *bytes;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineRuntimeSegment;

typedef struct {
    MachineLoadFile load_file;
    size_t entry_virtual_address;
    size_t initial_stack_pointer;
    MachineRuntimeSegment *segments;
    size_t segment_count;
    size_t segment_capacity;
    size_t total_mapped_byte_count;
    size_t stack_segment_index;
    int has_stack_segment;
} MachineRuntimeFile;

typedef struct {
    MachineRuntimeFile file;
    MachineRuntimeHeaderSummary header_summary;
    MachineRuntimeTargetPolicySummary target_policy_summary;
    MachineRuntimeMemorySummary memory_summary;
    MachineRuntimeStackSummary stack_summary;
    MachineRuntimeGapSummary gap_summary;
    MachineRuntimeLaunchSummary launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeSegmentSummary *segment_summaries;
    size_t segment_summary_count;
    size_t *executable_segment_indices;
    size_t executable_segment_count;
    size_t *non_executable_segment_indices;
    size_t non_executable_segment_count;
    size_t stack_segment_index;
    int has_stack_segment;
} MachineRuntimeReport;

typedef struct {
    const MachineRuntimeReport *report;
    const MachineRuntimeHeaderSummary *header_summary;
    const MachineRuntimeTargetPolicySummary *target_policy_summary;
    const MachineRuntimeMemorySummary *memory_summary;
    const MachineRuntimeStackSummary *stack_summary;
    const MachineRuntimeGapSummary *gap_summary;
    const MachineRuntimeLaunchSummary *launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    size_t entry_segment_index;
    const MachineRuntimeSegmentSummary *entry_segment_summary;
    int has_stack_segment;
    size_t stack_segment_index;
    const MachineRuntimeSegmentSummary *stack_segment_summary;
    const size_t *executable_segment_indices;
    size_t executable_segment_count;
    const size_t *non_executable_segment_indices;
    size_t non_executable_segment_count;
} MachineRuntimeReportOverviewArtifact;

typedef struct {
    const MachineRuntimeReport *report;
    size_t segment_index;
    const MachineRuntimeSegmentSummary *segment_summary;
    int is_entry_segment;
    int is_executable_segment;
    int is_stack_segment;
} MachineRuntimeReportSegmentArtifact;

typedef struct {
    const MachineRuntimeReport *report;
    const MachineRuntimeStackSummary *stack_summary;
    MachineRuntimeReportSegmentArtifact stack_segment_artifact;
} MachineRuntimeReportStackArtifact;

typedef struct {
    const MachineRuntimeReport *report;
    const MachineRuntimeGapSummary *gap_summary;
} MachineRuntimeReportGapArtifact;

typedef struct {
    const MachineRuntimeReport *report;
    const MachineRuntimeLaunchSummary *launch_summary;
    MachineRuntimeReportSegmentArtifact entry_segment_artifact;
    MachineRuntimeReportStackArtifact stack_artifact;
} MachineRuntimeReportLaunchArtifact;

typedef struct {
    const MachineRuntimeReport *report;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    MachineRuntimeReportStackArtifact stack_artifact;
    MachineRuntimeReportLaunchArtifact launch_artifact;
} MachineRuntimeReportInitialStackArtifact;

typedef enum {
    MACHINE_RUNTIME_REGION_LOAD = 0,
    MACHINE_RUNTIME_REGION_GAP,
    MACHINE_RUNTIME_REGION_STACK
} MachineRuntimeRegionKind;

typedef struct {
    MachineRuntimeRegionKind kind;
    size_t region_index;
    size_t segment_index;
    const char *name;
    size_t base_virtual_address;
    size_t end_virtual_address;
    size_t byte_count;
    unsigned char readable;
    unsigned char writable;
    unsigned char executable;
} MachineRuntimeRegionSummary;

typedef struct {
    const MachineRuntimeReport *report;
    MachineRuntimeRegionSummary summary;
    MachineRuntimeReportSegmentArtifact segment_artifact;
    MachineRuntimeReportGapArtifact gap_artifact;
    MachineRuntimeReportStackArtifact stack_artifact;
    int has_segment_artifact;
    int has_gap_artifact;
    int has_stack_artifact;
} MachineRuntimeReportRegionArtifact;

typedef enum {
    MACHINE_RUNTIME_REGION_FILTER_LOAD = 0,
    MACHINE_RUNTIME_REGION_FILTER_GAP,
    MACHINE_RUNTIME_REGION_FILTER_STACK
} MachineRuntimeRegionFilterKind;

typedef enum {
    MACHINE_RUNTIME_SEGMENT_FILTER_EXECUTABLE = 0,
    MACHINE_RUNTIME_SEGMENT_FILTER_NON_EXECUTABLE,
    MACHINE_RUNTIME_SEGMENT_FILTER_ENTRY,
    MACHINE_RUNTIME_SEGMENT_FILTER_STACK
} MachineRuntimeReportSegmentFilterKind;

void machine_runtime_file_init(MachineRuntimeFile *runtime_file);
void machine_runtime_file_free(MachineRuntimeFile *runtime_file);
void machine_runtime_report_init(MachineRuntimeReport *report);
void machine_runtime_report_free(MachineRuntimeReport *report);
int machine_runtime_clone_file(const MachineRuntimeFile *source,
    MachineRuntimeFile *out_clone,
    MachineRuntimeError *error);
int machine_runtime_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineRuntimeTargetPolicySummary *out_summary);
int machine_runtime_file_get_target_policy_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeTargetPolicySummary *out_summary);

int machine_runtime_file_get_summary(const MachineRuntimeFile *runtime_file,
    size_t *out_segment_count,
    size_t *out_mapped_byte_count,
    size_t *out_executable_segment_count);
int machine_runtime_file_get_source_elf_artifact_summary(const MachineRuntimeFile *runtime_file,
    MachineElfArtifactSummary *out_summary);
int machine_runtime_file_get_header_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeHeaderSummary *out_summary);
int machine_runtime_file_get_memory_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_runtime_file_get_stack_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeStackSummary *out_summary);
int machine_runtime_file_get_gap_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeGapSummary *out_summary);
int machine_runtime_file_get_launch_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_runtime_file_get_initial_stack_summary(const MachineRuntimeFile *runtime_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_runtime_file_get_segment(const MachineRuntimeFile *runtime_file,
    size_t segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_find_segment_by_name(const MachineRuntimeFile *runtime_file,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_find_segment_covering_virtual_address(const MachineRuntimeFile *runtime_file,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_find_segment_covering_memory_offset(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_get_memory_byte_at_virtual_address(const MachineRuntimeFile *runtime_file,
    size_t virtual_address,
    unsigned char *out_byte);
int machine_runtime_file_get_memory_byte_at_offset(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    unsigned char *out_byte);
int machine_runtime_file_copy_flat_memory_image(const MachineRuntimeFile *runtime_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error);
int machine_runtime_file_copy_segment_bytes(const MachineRuntimeFile *runtime_file,
    size_t segment_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineRuntimeError *error);
int machine_runtime_file_copy_memory_window(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error);
int machine_runtime_file_get_stack_byte_at_offset(const MachineRuntimeFile *runtime_file,
    size_t stack_offset,
    unsigned char *out_byte);
int machine_runtime_file_copy_stack_bytes(const MachineRuntimeFile *runtime_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineRuntimeError *error);
int machine_runtime_file_copy_stack_window(const MachineRuntimeFile *runtime_file,
    size_t stack_offset,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error);
int machine_runtime_file_copy_initial_stack_window(const MachineRuntimeFile *runtime_file,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error);
int machine_runtime_file_copy_initial_stack_image(const MachineRuntimeFile *runtime_file,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error);
int machine_runtime_file_copy_gap_window(const MachineRuntimeFile *runtime_file,
    size_t gap_offset,
    size_t requested_byte_count,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    size_t *out_base_virtual_address,
    MachineRuntimeError *error);
int machine_runtime_file_get_entry_segment(const MachineRuntimeFile *runtime_file,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_get_stack_segment(const MachineRuntimeFile *runtime_file,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_get_executable_segment_count(const MachineRuntimeFile *runtime_file,
    size_t *out_count);
int machine_runtime_file_get_executable_segment_by_index(const MachineRuntimeFile *runtime_file,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineRuntimeSegment **out_segment);
int machine_runtime_file_get_region_count(const MachineRuntimeFile *runtime_file,
    size_t *out_count);
int machine_runtime_file_get_region_summary(const MachineRuntimeFile *runtime_file,
    size_t region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_file_find_region_covering_virtual_address(const MachineRuntimeFile *runtime_file,
    size_t virtual_address,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_file_find_region_covering_memory_offset(const MachineRuntimeFile *runtime_file,
    size_t memory_offset,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary);

int machine_runtime_segment_get_summary(const MachineRuntimeSegment *segment,
    MachineRuntimeSegmentSummary *out_summary);

int machine_runtime_build_from_machine_load_file(const MachineLoadFile *load_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_load_report(const MachineLoadReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_exec_file(const MachineExecFile *exec_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_exec_report(const MachineExecReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_image_file(const MachineImageFile *image_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_image_report(const MachineImageReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_elf_file(const MachineElfFile *elf_file,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_elf_file_with_profile(const MachineElfFile *elf_file,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_elf_report(const MachineElfReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);
int machine_runtime_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeFile *out_runtime_file,
    MachineRuntimeError *error);

int machine_runtime_build_report_from_file(const MachineRuntimeFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_load_file(const MachineLoadFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_load_report(const MachineLoadReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_exec_file(const MachineExecFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_exec_report(const MachineExecReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_image_file(const MachineImageFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_image_report(const MachineImageReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_elf_file(const MachineElfFile *source,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_elf_report(const MachineElfReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineRuntimeReport *out_report,
    MachineRuntimeError *error);
int machine_runtime_report_refresh(MachineRuntimeReport *report,
    MachineRuntimeError *error);

int machine_runtime_verify_file(const MachineRuntimeFile *runtime_file,
    MachineRuntimeError *error);
int machine_runtime_dump_file(const MachineRuntimeFile *runtime_file,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_elf_report_with_profile(const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_elf_bytes_with_profile(const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);

int machine_runtime_report_get_summary(const MachineRuntimeReport *report,
    size_t *out_segment_count,
    size_t *out_mapped_byte_count,
    size_t *out_executable_segment_count);
int machine_runtime_report_get_overview_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportOverviewArtifact *out_artifact);
int machine_runtime_report_get_file(const MachineRuntimeReport *report,
    const MachineRuntimeFile **out_file);
int machine_runtime_report_get_source_elf_artifact_summary_artifact(const MachineRuntimeReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_runtime_report_get_header_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeHeaderSummary **out_summary);
int machine_runtime_report_get_target_policy_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeTargetPolicySummary **out_summary);
int machine_runtime_report_get_memory_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_runtime_report_get_stack_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeStackSummary **out_summary);
int machine_runtime_report_get_gap_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeGapSummary **out_summary);
int machine_runtime_report_get_launch_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_runtime_report_get_initial_stack_summary_artifact(const MachineRuntimeReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_runtime_report_get_stack_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportStackArtifact *out_artifact);
int machine_runtime_report_get_gap_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportGapArtifact *out_artifact);
int machine_runtime_report_get_launch_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportLaunchArtifact *out_artifact);
int machine_runtime_report_get_initial_stack_artifact(const MachineRuntimeReport *report,
    MachineRuntimeReportInitialStackArtifact *out_artifact);
int machine_runtime_report_get_region_count(const MachineRuntimeReport *report,
    size_t *out_count);
int machine_runtime_report_get_region_summary(const MachineRuntimeReport *report,
    size_t region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_report_get_region_artifact(const MachineRuntimeReport *report,
    size_t region_index,
    MachineRuntimeReportRegionArtifact *out_artifact);
int machine_runtime_report_find_region_summary_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_report_find_region_artifact_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact);
int machine_runtime_report_find_region_summary_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_report_find_region_artifact_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact);
int machine_runtime_report_get_region_filter_count(const MachineRuntimeReport *report,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t *out_count);
int machine_runtime_report_get_region_summary_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_report_get_region_artifact_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact);
int machine_runtime_report_get_entry_segment_summary_artifact(const MachineRuntimeReport *report,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_get_stack_segment_summary_artifact(const MachineRuntimeReport *report,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_get_segment_summary(const MachineRuntimeReport *report,
    size_t segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_get_segment_artifact(const MachineRuntimeReport *report,
    size_t segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact);
int machine_runtime_report_find_segment_summary_by_name(const MachineRuntimeReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_find_segment_artifact_by_name(const MachineRuntimeReport *report,
    const char *segment_name,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact);
int machine_runtime_report_find_segment_summary_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_find_segment_artifact_covering_virtual_address(const MachineRuntimeReport *report,
    size_t virtual_address,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact);
int machine_runtime_report_find_segment_summary_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_find_segment_artifact_covering_memory_offset(const MachineRuntimeReport *report,
    size_t memory_offset,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact);
int machine_runtime_report_get_segment_filter_count(const MachineRuntimeReport *report,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t *out_count);
int machine_runtime_report_get_segment_summary_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_get_segment_artifact_by_filter_index(const MachineRuntimeReport *report,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact);
int machine_runtime_report_overview_artifact_get_segment_summary(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_runtime_report_overview_artifact_get_segment_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportSegmentFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_segment_index,
    MachineRuntimeReportSegmentArtifact *out_artifact);
int machine_runtime_report_overview_artifact_get_stack_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeStackSummary **out_summary);
int machine_runtime_report_overview_artifact_get_gap_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeGapSummary **out_summary);
int machine_runtime_report_overview_artifact_get_launch_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_runtime_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_runtime_report_overview_artifact_get_stack_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportStackArtifact *out_artifact);
int machine_runtime_report_overview_artifact_get_gap_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportGapArtifact *out_artifact);
int machine_runtime_report_overview_artifact_get_launch_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportLaunchArtifact *out_artifact);
int machine_runtime_report_overview_artifact_get_initial_stack_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeReportInitialStackArtifact *out_artifact);
int machine_runtime_report_overview_artifact_get_region_summary(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeRegionSummary *out_summary);
int machine_runtime_report_overview_artifact_get_region_artifact(
    const MachineRuntimeReportOverviewArtifact *artifact,
    MachineRuntimeRegionFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_region_index,
    MachineRuntimeReportRegionArtifact *out_artifact);
int machine_runtime_report_get_executable_segment_count(const MachineRuntimeReport *report,
    size_t *out_count);
int machine_runtime_report_get_executable_segment_summary_by_index(const MachineRuntimeReport *report,
    size_t executable_index,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);

int machine_runtime_dump_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_exec_file(const MachineExecFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_exec_report(const MachineExecReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_image_file(const MachineImageFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_image_report(const MachineImageReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_elf_file(const MachineElfFile *source,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_elf_file_with_profile(const MachineElfFile *source,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_elf_report(const MachineElfReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_elf_report_with_profile(
    const MachineElfReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_elf_bytes(const unsigned char *bytes,
    size_t byte_count,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_elf_bytes_with_profile(
    const unsigned char *bytes,
    size_t byte_count,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineRuntimeError *error);
int machine_runtime_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineRuntimeError *error);

#endif
