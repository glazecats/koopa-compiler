#ifndef MACHINE_STEP_H
#define MACHINE_STEP_H

#include <stddef.h>

#include "machine/launch.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineStepError;

typedef enum {
    MACHINE_STEP_STATUS_READY = 0,
    MACHINE_STEP_STATUS_HALTED
} MachineStepStatus;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineStepStatus status;
    size_t program_counter;
    size_t stack_pointer;
    size_t launch_register_count;
    size_t runtime_segment_count;
    size_t mapped_byte_count;
    size_t current_segment_index;
} MachineStepHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    const char *program_counter_register_name;
    const char *stack_pointer_register_name;
    size_t fetch_byte_count;
} MachineStepTargetPolicySummary;

typedef struct {
    size_t byte_virtual_address;
    size_t byte_memory_offset;
    size_t segment_index;
    const char *segment_name;
    unsigned char byte_value;
} MachineStepFetchSummary;

typedef struct {
    MachineLaunchFile launch_file;
    MachineStepStatus status;
    size_t program_counter;
    size_t stack_pointer;
    size_t current_segment_index;
    unsigned char current_byte;
    size_t current_byte_memory_offset;
} MachineStepFile;

typedef struct {
    MachineStepFile file;
    MachineStepHeaderSummary header_summary;
    MachineStepTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
} MachineStepReport;

typedef struct {
    const MachineStepReport *report;
    const MachineStepHeaderSummary *header_summary;
    const MachineStepTargetPolicySummary *target_policy_summary;
    const MachineRuntimeLaunchSummary *runtime_launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    const MachineRuntimeMemorySummary *runtime_memory_summary;
    const MachineRuntimeSegmentSummary *current_segment_summary;
    const MachineStepFetchSummary *fetch_summary;
} MachineStepReportOverviewArtifact;

void machine_step_file_init(MachineStepFile *step_file);
void machine_step_file_free(MachineStepFile *step_file);
void machine_step_report_init(MachineStepReport *report);
void machine_step_report_free(MachineStepReport *report);
int machine_step_clone_file(const MachineStepFile *source,
    MachineStepFile *out_clone,
    MachineStepError *error);

const char *machine_step_status_name(MachineStepStatus status);
int machine_step_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineStepTargetPolicySummary *out_summary);
int machine_step_file_get_target_policy_summary(const MachineStepFile *step_file,
    MachineStepTargetPolicySummary *out_summary);
int machine_step_file_get_summary(const MachineStepFile *step_file,
    size_t *out_launch_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count);
int machine_step_file_get_header_summary(const MachineStepFile *step_file,
    MachineStepHeaderSummary *out_summary);
int machine_step_file_get_runtime_launch_summary(const MachineStepFile *step_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_step_file_get_initial_stack_summary(const MachineStepFile *step_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_step_file_get_runtime_memory_summary(const MachineStepFile *step_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_step_file_get_current_segment_summary(const MachineStepFile *step_file,
    size_t *out_segment_index,
    MachineRuntimeSegmentSummary *out_summary);
int machine_step_file_get_fetch_summary(const MachineStepFile *step_file,
    MachineStepFetchSummary *out_summary);

int machine_step_build_from_machine_launch_file(const MachineLaunchFile *launch_file,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_launch_report(const MachineLaunchReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_runtime_file(const MachineRuntimeFile *runtime_file,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_load_file(const MachineLoadFile *load_file,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_load_report(const MachineLoadReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineStepFile *out_step_file,
    MachineStepError *error);
int machine_step_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStepFile *out_step_file,
    MachineStepError *error);

int machine_step_build_report_from_file(const MachineStepFile *source,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_launch_file(const MachineLaunchFile *source,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_launch_report(const MachineLaunchReport *report,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_runtime_file(const MachineRuntimeFile *source,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_load_file(const MachineLoadFile *source,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_load_report(const MachineLoadReport *report,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStepReport *out_report,
    MachineStepError *error);
int machine_step_report_refresh(MachineStepReport *report,
    MachineStepError *error);

int machine_step_verify_file(const MachineStepFile *step_file,
    MachineStepError *error);
int machine_step_dump_file(const MachineStepFile *step_file,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_file(const MachineStepFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStepError *error);

int machine_step_report_get_summary(const MachineStepReport *report,
    size_t *out_launch_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count);
int machine_step_report_get_overview_artifact(const MachineStepReport *report,
    MachineStepReportOverviewArtifact *out_artifact);
int machine_step_report_get_file(const MachineStepReport *report,
    const MachineStepFile **out_file);
int machine_step_report_get_launch_file(const MachineStepReport *report,
    const MachineLaunchFile **out_launch_file);
int machine_step_report_get_header_summary_artifact(const MachineStepReport *report,
    const MachineStepHeaderSummary **out_summary);
int machine_step_report_get_target_policy_summary_artifact(const MachineStepReport *report,
    const MachineStepTargetPolicySummary **out_summary);
int machine_step_report_get_runtime_launch_summary_artifact(const MachineStepReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_step_report_get_initial_stack_summary_artifact(const MachineStepReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_step_report_get_runtime_memory_summary_artifact(const MachineStepReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_step_report_get_current_segment_summary_artifact(const MachineStepReport *report,
    size_t *out_segment_index,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_step_report_get_fetch_summary_artifact(const MachineStepReport *report,
    const MachineStepFetchSummary **out_summary);
int machine_step_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_step_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_step_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary);
int machine_step_report_overview_artifact_get_current_segment_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_step_report_overview_artifact_get_fetch_summary_artifact(
    const MachineStepReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary);

int machine_step_dump_report(const MachineStepReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_file(const MachineStepFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_launch_file(const MachineLaunchFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_launch_report(const MachineLaunchReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineStepError *error);
int machine_step_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStepError *error);

#endif
