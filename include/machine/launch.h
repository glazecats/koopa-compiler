#ifndef MACHINE_LAUNCH_H
#define MACHINE_LAUNCH_H

#include <stddef.h>

#include "machine/runtime.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineLaunchError;

typedef struct {
    const char *name;
    size_t value;
} MachineLaunchRegisterSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t program_counter;
    size_t stack_pointer;
    size_t register_count;
    size_t runtime_segment_count;
    size_t mapped_byte_count;
} MachineLaunchHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    const char *program_counter_register_name;
    const char *stack_pointer_register_name;
} MachineLaunchTargetPolicySummary;

typedef struct {
    char *name;
    size_t value;
} MachineLaunchRegister;

typedef struct {
    MachineRuntimeFile runtime_file;
    size_t program_counter;
    size_t stack_pointer;
    MachineLaunchRegister *registers;
    size_t register_count;
    size_t register_capacity;
    size_t program_counter_register_index;
    size_t stack_pointer_register_index;
} MachineLaunchFile;

typedef struct {
    MachineLaunchFile file;
    MachineLaunchHeaderSummary header_summary;
    MachineLaunchTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineLaunchRegisterSummary *register_summaries;
    size_t register_summary_count;
    size_t program_counter_register_index;
    size_t stack_pointer_register_index;
} MachineLaunchReport;

typedef struct {
    const MachineLaunchReport *report;
    const MachineLaunchHeaderSummary *header_summary;
    const MachineLaunchTargetPolicySummary *target_policy_summary;
    const MachineRuntimeLaunchSummary *runtime_launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    const MachineRuntimeMemorySummary *runtime_memory_summary;
    size_t program_counter_register_index;
    const MachineLaunchRegisterSummary *program_counter_register_summary;
    size_t stack_pointer_register_index;
    const MachineLaunchRegisterSummary *stack_pointer_register_summary;
} MachineLaunchReportOverviewArtifact;

typedef struct {
    const MachineLaunchReport *report;
    size_t register_index;
    const MachineLaunchRegisterSummary *register_summary;
    int is_program_counter_register;
    int is_stack_pointer_register;
} MachineLaunchReportRegisterArtifact;

void machine_launch_file_init(MachineLaunchFile *launch_file);
void machine_launch_file_free(MachineLaunchFile *launch_file);
void machine_launch_report_init(MachineLaunchReport *report);
void machine_launch_report_free(MachineLaunchReport *report);
int machine_launch_clone_file(const MachineLaunchFile *source,
    MachineLaunchFile *out_clone,
    MachineLaunchError *error);

int machine_launch_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineLaunchTargetPolicySummary *out_summary);
int machine_launch_file_get_target_policy_summary(const MachineLaunchFile *launch_file,
    MachineLaunchTargetPolicySummary *out_summary);
int machine_launch_file_get_summary(const MachineLaunchFile *launch_file,
    size_t *out_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count);
int machine_launch_file_get_header_summary(const MachineLaunchFile *launch_file,
    MachineLaunchHeaderSummary *out_summary);
int machine_launch_file_get_runtime_launch_summary(const MachineLaunchFile *launch_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_launch_file_get_initial_stack_summary(const MachineLaunchFile *launch_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_launch_file_get_runtime_memory_summary(const MachineLaunchFile *launch_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_launch_file_get_register(const MachineLaunchFile *launch_file,
    size_t register_index,
    const MachineLaunchRegister **out_register);
int machine_launch_file_find_register_by_name(const MachineLaunchFile *launch_file,
    const char *register_name,
    size_t *out_register_index,
    const MachineLaunchRegister **out_register);
int machine_launch_file_get_program_counter_register(const MachineLaunchFile *launch_file,
    size_t *out_register_index,
    const MachineLaunchRegister **out_register);
int machine_launch_file_get_stack_pointer_register(const MachineLaunchFile *launch_file,
    size_t *out_register_index,
    const MachineLaunchRegister **out_register);

int machine_launch_register_get_summary(const MachineLaunchRegister *reg,
    MachineLaunchRegisterSummary *out_summary);

int machine_launch_build_from_machine_runtime_file(const MachineRuntimeFile *runtime_file,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error);
int machine_launch_build_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error);
int machine_launch_build_from_machine_load_file(const MachineLoadFile *load_file,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error);
int machine_launch_build_from_machine_load_report(const MachineLoadReport *report,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error);
int machine_launch_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error);
int machine_launch_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLaunchFile *out_launch_file,
    MachineLaunchError *error);

int machine_launch_build_report_from_file(const MachineLaunchFile *source,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_build_report_from_machine_runtime_file(const MachineRuntimeFile *source,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_build_report_from_machine_runtime_report(const MachineRuntimeReport *report,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_build_report_from_machine_load_file(const MachineLoadFile *source,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_build_report_from_machine_load_report(const MachineLoadReport *report,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLaunchReport *out_report,
    MachineLaunchError *error);
int machine_launch_report_refresh(MachineLaunchReport *report,
    MachineLaunchError *error);

int machine_launch_verify_file(const MachineLaunchFile *launch_file,
    MachineLaunchError *error);
int machine_launch_dump_file(const MachineLaunchFile *launch_file,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_file(const MachineLaunchFile *source,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLaunchError *error);

int machine_launch_report_get_summary(const MachineLaunchReport *report,
    size_t *out_register_count,
    size_t *out_runtime_segment_count,
    size_t *out_mapped_byte_count);
int machine_launch_report_get_overview_artifact(const MachineLaunchReport *report,
    MachineLaunchReportOverviewArtifact *out_artifact);
int machine_launch_report_get_file(const MachineLaunchReport *report,
    const MachineLaunchFile **out_file);
int machine_launch_report_get_runtime_file(const MachineLaunchReport *report,
    const MachineRuntimeFile **out_runtime_file);
int machine_launch_report_get_header_summary_artifact(const MachineLaunchReport *report,
    const MachineLaunchHeaderSummary **out_summary);
int machine_launch_report_get_target_policy_summary_artifact(const MachineLaunchReport *report,
    const MachineLaunchTargetPolicySummary **out_summary);
int machine_launch_report_get_runtime_launch_summary_artifact(const MachineLaunchReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_launch_report_get_initial_stack_summary_artifact(const MachineLaunchReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_launch_report_get_runtime_memory_summary_artifact(const MachineLaunchReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_launch_report_get_register_summary(const MachineLaunchReport *report,
    size_t register_index,
    const MachineLaunchRegisterSummary **out_summary);
int machine_launch_report_get_register_artifact(const MachineLaunchReport *report,
    size_t register_index,
    MachineLaunchReportRegisterArtifact *out_artifact);
int machine_launch_report_find_register_summary_by_name(const MachineLaunchReport *report,
    const char *register_name,
    size_t *out_register_index,
    const MachineLaunchRegisterSummary **out_summary);
int machine_launch_report_find_register_artifact_by_name(const MachineLaunchReport *report,
    const char *register_name,
    size_t *out_register_index,
    MachineLaunchReportRegisterArtifact *out_artifact);
int machine_launch_report_get_program_counter_register_summary_artifact(const MachineLaunchReport *report,
    size_t *out_register_index,
    const MachineLaunchRegisterSummary **out_summary);
int machine_launch_report_get_stack_pointer_register_summary_artifact(const MachineLaunchReport *report,
    size_t *out_register_index,
    const MachineLaunchRegisterSummary **out_summary);
int machine_launch_report_get_program_counter_register_artifact(const MachineLaunchReport *report,
    MachineLaunchReportRegisterArtifact *out_artifact);
int machine_launch_report_get_stack_pointer_register_artifact(const MachineLaunchReport *report,
    MachineLaunchReportRegisterArtifact *out_artifact);
int machine_launch_report_overview_artifact_get_program_counter_register_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineLaunchRegisterSummary **out_summary);
int machine_launch_report_overview_artifact_get_stack_pointer_register_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineLaunchRegisterSummary **out_summary);
int machine_launch_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_launch_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_launch_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary);
int machine_launch_report_overview_artifact_get_program_counter_register_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    MachineLaunchReportRegisterArtifact *out_artifact);
int machine_launch_report_overview_artifact_get_stack_pointer_register_artifact(
    const MachineLaunchReportOverviewArtifact *artifact,
    MachineLaunchReportRegisterArtifact *out_artifact);

int machine_launch_dump_report(const MachineLaunchReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_file(const MachineLaunchFile *source,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_machine_runtime_file(const MachineRuntimeFile *source,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_machine_runtime_report(const MachineRuntimeReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_machine_load_file(const MachineLoadFile *source,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_machine_load_report(const MachineLoadReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLaunchError *error);
int machine_launch_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLaunchError *error);

#endif
