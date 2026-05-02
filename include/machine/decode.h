#ifndef MACHINE_DECODE_H
#define MACHINE_DECODE_H

#include <stddef.h>

#include "machine/step.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineDecodeError;

typedef enum {
    MACHINE_DECODE_TAG_NONE = 0,
    MACHINE_DECODE_TAG_OP,
    MACHINE_DECODE_TAG_TERMINATOR
} MachineDecodeTagClass;

typedef struct {
    MachineDecodeTagClass tag_class;
    unsigned char raw_byte;
    unsigned char tag_value;
    int is_known;
    const char *tag_name;
} MachineDecodeTagSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineStepStatus step_status;
    size_t program_counter;
    size_t stack_pointer;
    size_t current_segment_index;
    size_t mapped_byte_count;
} MachineDecodeHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t opcode_tag_base;
    size_t terminator_tag_base;
} MachineDecodeTargetPolicySummary;

typedef struct {
    MachineStepFile step_file;
    MachineDecodeTagClass tag_class;
    unsigned char raw_byte;
    unsigned char tag_value;
    int is_known;
} MachineDecodeFile;

typedef struct {
    MachineDecodeFile file;
    MachineDecodeHeaderSummary header_summary;
    MachineDecodeTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary tag_summary;
} MachineDecodeReport;

typedef struct {
    const MachineDecodeReport *report;
    const MachineDecodeHeaderSummary *header_summary;
    const MachineDecodeTargetPolicySummary *target_policy_summary;
    const MachineRuntimeLaunchSummary *runtime_launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    const MachineRuntimeMemorySummary *runtime_memory_summary;
    const MachineRuntimeSegmentSummary *current_segment_summary;
    const MachineStepFetchSummary *fetch_summary;
    const MachineDecodeTagSummary *tag_summary;
} MachineDecodeReportOverviewArtifact;

void machine_decode_file_init(MachineDecodeFile *decode_file);
void machine_decode_file_free(MachineDecodeFile *decode_file);
void machine_decode_report_init(MachineDecodeReport *report);
void machine_decode_report_free(MachineDecodeReport *report);
int machine_decode_clone_file(const MachineDecodeFile *source,
    MachineDecodeFile *out_clone,
    MachineDecodeError *error);

const char *machine_decode_tag_class_name(MachineDecodeTagClass tag_class);
int machine_decode_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineDecodeTargetPolicySummary *out_summary);
int machine_decode_file_get_target_policy_summary(const MachineDecodeFile *decode_file,
    MachineDecodeTargetPolicySummary *out_summary);
int machine_decode_file_get_summary(const MachineDecodeFile *decode_file,
    size_t *out_mapped_byte_count);
int machine_decode_file_get_header_summary(const MachineDecodeFile *decode_file,
    MachineDecodeHeaderSummary *out_summary);
int machine_decode_file_get_runtime_launch_summary(const MachineDecodeFile *decode_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_decode_file_get_initial_stack_summary(const MachineDecodeFile *decode_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_decode_file_get_runtime_memory_summary(const MachineDecodeFile *decode_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_decode_file_get_current_segment_summary(const MachineDecodeFile *decode_file,
    size_t *out_segment_index,
    MachineRuntimeSegmentSummary *out_summary);
int machine_decode_file_get_fetch_summary(const MachineDecodeFile *decode_file,
    MachineStepFetchSummary *out_summary);
int machine_decode_file_get_tag_summary(const MachineDecodeFile *decode_file,
    MachineDecodeTagSummary *out_summary);

int machine_decode_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error);
int machine_decode_build_from_machine_step_report(const MachineStepReport *report,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error);
int machine_decode_build_from_machine_launch_file(const MachineLaunchFile *launch_file,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error);
int machine_decode_build_from_machine_launch_report(const MachineLaunchReport *report,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error);
int machine_decode_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error);
int machine_decode_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDecodeFile *out_decode_file,
    MachineDecodeError *error);

int machine_decode_build_report_from_file(const MachineDecodeFile *source,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_build_report_from_machine_launch_file(const MachineLaunchFile *source,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_build_report_from_machine_launch_report(const MachineLaunchReport *report,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDecodeReport *out_report,
    MachineDecodeError *error);
int machine_decode_report_refresh(MachineDecodeReport *report,
    MachineDecodeError *error);

int machine_decode_verify_file(const MachineDecodeFile *decode_file,
    MachineDecodeError *error);
int machine_decode_dump_file(const MachineDecodeFile *decode_file,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_file(const MachineDecodeFile *source,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDecodeError *error);

int machine_decode_report_get_summary(const MachineDecodeReport *report,
    size_t *out_mapped_byte_count);
int machine_decode_report_get_overview_artifact(const MachineDecodeReport *report,
    MachineDecodeReportOverviewArtifact *out_artifact);
int machine_decode_report_get_file(const MachineDecodeReport *report,
    const MachineDecodeFile **out_file);
int machine_decode_report_get_step_file(const MachineDecodeReport *report,
    const MachineStepFile **out_step_file);
int machine_decode_report_get_header_summary_artifact(const MachineDecodeReport *report,
    const MachineDecodeHeaderSummary **out_summary);
int machine_decode_report_get_target_policy_summary_artifact(const MachineDecodeReport *report,
    const MachineDecodeTargetPolicySummary **out_summary);
int machine_decode_report_get_runtime_launch_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_decode_report_get_initial_stack_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_decode_report_get_runtime_memory_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_decode_report_get_current_segment_summary_artifact(const MachineDecodeReport *report,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_decode_report_get_fetch_summary_artifact(const MachineDecodeReport *report,
    const MachineStepFetchSummary **out_summary);
int machine_decode_report_get_tag_summary_artifact(const MachineDecodeReport *report,
    const MachineDecodeTagSummary **out_summary);
int machine_decode_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_decode_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_decode_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary);
int machine_decode_report_overview_artifact_get_current_segment_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_decode_report_overview_artifact_get_fetch_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary);
int machine_decode_report_overview_artifact_get_tag_summary_artifact(
    const MachineDecodeReportOverviewArtifact *artifact,
    const MachineDecodeTagSummary **out_summary);

int machine_decode_dump_report(const MachineDecodeReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_file(const MachineDecodeFile *source,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineDecodeError *error);
int machine_decode_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDecodeError *error);

#endif
