#ifndef MACHINE_PAYLOAD_DECODE_H
#define MACHINE_PAYLOAD_DECODE_H

#include <stddef.h>

#include "machine/decode.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachinePayloadDecodeError;

typedef enum {
    MACHINE_PAYLOAD_DECODE_KIND_NONE = 0,
    MACHINE_PAYLOAD_DECODE_KIND_OP,
    MACHINE_PAYLOAD_DECODE_KIND_TERMINATOR
} MachinePayloadDecodeKind;

typedef struct {
    MachinePayloadDecodeKind kind;
    unsigned char raw_byte;
    unsigned char tag_value;
    int is_known;
    const char *tag_name;
    size_t total_byte_count;
    size_t payload_byte_count;
    unsigned char payload_bytes[3];
    size_t immediate_value;
} MachinePayloadDecodeSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineStepStatus step_status;
    size_t program_counter;
    size_t stack_pointer;
    size_t current_segment_index;
    size_t mapped_byte_count;
} MachinePayloadDecodeHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t max_payload_byte_count;
} MachinePayloadDecodeTargetPolicySummary;

typedef struct {
    MachineDecodeFile decode_file;
    MachinePayloadDecodeKind kind;
    int is_known;
    size_t total_byte_count;
    size_t payload_byte_count;
    unsigned char payload_bytes[3];
    size_t immediate_value;
} MachinePayloadDecodeFile;

typedef struct {
    MachinePayloadDecodeFile file;
    MachinePayloadDecodeHeaderSummary header_summary;
    MachinePayloadDecodeTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachinePayloadDecodeSummary payload_summary;
} MachinePayloadDecodeReport;

typedef struct {
    const MachinePayloadDecodeReport *report;
    const MachinePayloadDecodeHeaderSummary *header_summary;
    const MachinePayloadDecodeTargetPolicySummary *target_policy_summary;
    const MachineRuntimeLaunchSummary *runtime_launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    const MachineRuntimeMemorySummary *runtime_memory_summary;
    const MachineRuntimeSegmentSummary *current_segment_summary;
    const MachineStepFetchSummary *fetch_summary;
    const MachineDecodeTagSummary *decode_tag_summary;
    const MachinePayloadDecodeSummary *payload_summary;
} MachinePayloadDecodeReportOverviewArtifact;

void machine_payload_decode_file_init(MachinePayloadDecodeFile *decode_file);
void machine_payload_decode_file_free(MachinePayloadDecodeFile *decode_file);
void machine_payload_decode_report_init(MachinePayloadDecodeReport *report);
void machine_payload_decode_report_free(MachinePayloadDecodeReport *report);
int machine_payload_decode_clone_file(const MachinePayloadDecodeFile *source,
    MachinePayloadDecodeFile *out_clone,
    MachinePayloadDecodeError *error);

const char *machine_payload_decode_kind_name(MachinePayloadDecodeKind kind);
int machine_payload_decode_get_target_policy_summary(MachineElfTargetProfile profile,
    MachinePayloadDecodeTargetPolicySummary *out_summary);
int machine_payload_decode_file_get_target_policy_summary(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeTargetPolicySummary *out_summary);
int machine_payload_decode_file_get_summary(const MachinePayloadDecodeFile *decode_file,
    size_t *out_mapped_byte_count);
int machine_payload_decode_file_get_source_elf_artifact_summary(const MachinePayloadDecodeFile *decode_file,
    MachineElfArtifactSummary *out_summary);
int machine_payload_decode_file_get_header_summary(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeHeaderSummary *out_summary);
int machine_payload_decode_file_get_runtime_launch_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_payload_decode_file_get_initial_stack_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_payload_decode_file_get_runtime_memory_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_payload_decode_file_get_current_segment_summary(const MachinePayloadDecodeFile *decode_file,
    MachineRuntimeSegmentSummary *out_summary);
int machine_payload_decode_file_get_fetch_summary(const MachinePayloadDecodeFile *decode_file,
    MachineStepFetchSummary *out_summary);
int machine_payload_decode_file_get_decode_tag_summary(const MachinePayloadDecodeFile *decode_file,
    MachineDecodeTagSummary *out_summary);
int machine_payload_decode_file_get_payload_summary(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeSummary *out_summary);

int machine_payload_decode_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_from_machine_step_file(const MachineStepFile *step_file,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_from_machine_step_report(const MachineStepReport *report,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachinePayloadDecodeFile *out_payload_decode_file,
    MachinePayloadDecodeError *error);

int machine_payload_decode_build_report_from_file(const MachinePayloadDecodeFile *source,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_from_machine_step_file(const MachineStepFile *source,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_from_machine_step_report(const MachineStepReport *report,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachinePayloadDecodeReport *out_report,
    MachinePayloadDecodeError *error);
int machine_payload_decode_report_refresh(MachinePayloadDecodeReport *report,
    MachinePayloadDecodeError *error);

int machine_payload_decode_verify_file(const MachinePayloadDecodeFile *decode_file,
    MachinePayloadDecodeError *error);
int machine_payload_decode_dump_file(const MachinePayloadDecodeFile *decode_file,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachinePayloadDecodeError *error);

int machine_payload_decode_report_get_summary(const MachinePayloadDecodeReport *report,
    size_t *out_mapped_byte_count);
int machine_payload_decode_report_get_overview_artifact(const MachinePayloadDecodeReport *report,
    MachinePayloadDecodeReportOverviewArtifact *out_artifact);
int machine_payload_decode_report_get_file(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeFile **out_file);
int machine_payload_decode_report_get_decode_file(const MachinePayloadDecodeReport *report,
    const MachineDecodeFile **out_decode_file);
int machine_payload_decode_report_get_source_elf_artifact_summary_artifact(
    const MachinePayloadDecodeReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_payload_decode_report_get_header_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeHeaderSummary **out_summary);
int machine_payload_decode_report_get_target_policy_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeTargetPolicySummary **out_summary);
int machine_payload_decode_report_get_runtime_launch_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_payload_decode_report_get_initial_stack_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_payload_decode_report_get_runtime_memory_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_payload_decode_report_get_current_segment_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_payload_decode_report_get_fetch_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineStepFetchSummary **out_summary);
int machine_payload_decode_report_get_decode_tag_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachineDecodeTagSummary **out_summary);
int machine_payload_decode_report_get_payload_summary_artifact(const MachinePayloadDecodeReport *report,
    const MachinePayloadDecodeSummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_current_segment_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_fetch_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_decode_tag_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachineDecodeTagSummary **out_summary);
int machine_payload_decode_report_overview_artifact_get_payload_summary_artifact(
    const MachinePayloadDecodeReportOverviewArtifact *artifact,
    const MachinePayloadDecodeSummary **out_summary);

int machine_payload_decode_dump_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachinePayloadDecodeError *error);
int machine_payload_decode_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachinePayloadDecodeError *error);

#endif
