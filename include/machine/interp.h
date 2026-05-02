#ifndef MACHINE_INTERP_H
#define MACHINE_INTERP_H

#include <stddef.h>

#include "machine/payload_decode.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineInterpError;

typedef enum {
    MACHINE_INTERP_ACTION_NONE = 0,
    MACHINE_INTERP_ACTION_ADVANCE,
    MACHINE_INTERP_ACTION_HALT,
    MACHINE_INTERP_ACTION_CONTROL_TRANSFER,
    MACHINE_INTERP_ACTION_UNSUPPORTED
} MachineInterpActionKind;

typedef struct {
    MachineInterpActionKind action_kind;
    unsigned char raw_byte;
    unsigned char tag_value;
    int is_known;
    const char *tag_name;
    size_t instruction_byte_count;
    MachineStepStatus next_status;
    int has_next_program_counter;
    size_t next_program_counter;
    int has_primary_target_block;
    size_t primary_target_block_index;
    int has_secondary_target_block;
    size_t secondary_target_block_index;
    int has_return_immediate;
    size_t return_immediate;
} MachineInterpSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineStepStatus step_status;
    size_t program_counter;
    size_t stack_pointer;
    size_t current_segment_index;
    size_t mapped_byte_count;
} MachineInterpHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    size_t max_control_target_count;
    int resolves_linear_next_program_counter;
    int resolves_control_targets_as_block_indices;
} MachineInterpTargetPolicySummary;

typedef struct {
    MachinePayloadDecodeFile payload_decode_file;
    MachineInterpActionKind action_kind;
    MachineStepStatus next_status;
    int has_next_program_counter;
    size_t next_program_counter;
    int has_primary_target_block;
    size_t primary_target_block_index;
    int has_secondary_target_block;
    size_t secondary_target_block_index;
    int has_return_immediate;
    size_t return_immediate;
} MachineInterpFile;

typedef struct {
    MachineInterpFile file;
    MachineInterpHeaderSummary header_summary;
    MachineInterpTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineInterpSummary interp_summary;
} MachineInterpReport;

typedef struct {
    const MachineInterpReport *report;
    const MachineInterpHeaderSummary *header_summary;
    const MachineInterpTargetPolicySummary *target_policy_summary;
    const MachineRuntimeLaunchSummary *runtime_launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    const MachineRuntimeMemorySummary *runtime_memory_summary;
    const MachineRuntimeSegmentSummary *current_segment_summary;
    const MachineStepFetchSummary *fetch_summary;
    const MachineDecodeTagSummary *decode_tag_summary;
    const MachinePayloadDecodeSummary *payload_summary;
    const MachineInterpSummary *interp_summary;
} MachineInterpReportOverviewArtifact;

void machine_interp_file_init(MachineInterpFile *interp_file);
void machine_interp_file_free(MachineInterpFile *interp_file);
void machine_interp_report_init(MachineInterpReport *report);
void machine_interp_report_free(MachineInterpReport *report);
int machine_interp_clone_file(const MachineInterpFile *source,
    MachineInterpFile *out_clone,
    MachineInterpError *error);

const char *machine_interp_action_kind_name(MachineInterpActionKind action_kind);
int machine_interp_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineInterpTargetPolicySummary *out_summary);
int machine_interp_file_get_target_policy_summary(const MachineInterpFile *interp_file,
    MachineInterpTargetPolicySummary *out_summary);
int machine_interp_file_get_summary(const MachineInterpFile *interp_file,
    size_t *out_mapped_byte_count);
int machine_interp_file_get_header_summary(const MachineInterpFile *interp_file,
    MachineInterpHeaderSummary *out_summary);
int machine_interp_file_get_runtime_launch_summary(const MachineInterpFile *interp_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_interp_file_get_initial_stack_summary(const MachineInterpFile *interp_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_interp_file_get_runtime_memory_summary(const MachineInterpFile *interp_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_interp_file_get_current_segment_summary(const MachineInterpFile *interp_file,
    MachineRuntimeSegmentSummary *out_summary);
int machine_interp_file_get_fetch_summary(const MachineInterpFile *interp_file,
    MachineStepFetchSummary *out_summary);
int machine_interp_file_get_decode_tag_summary(const MachineInterpFile *interp_file,
    MachineDecodeTagSummary *out_summary);
int machine_interp_file_get_payload_summary(const MachineInterpFile *interp_file,
    MachinePayloadDecodeSummary *out_summary);
int machine_interp_file_get_interp_summary(const MachineInterpFile *interp_file,
    MachineInterpSummary *out_summary);

int machine_interp_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_step_report(const MachineStepReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);
int machine_interp_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineInterpFile *out_interp_file,
    MachineInterpError *error);

int machine_interp_build_report_from_file(const MachineInterpFile *source,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineInterpReport *out_report,
    MachineInterpError *error);
int machine_interp_report_refresh(MachineInterpReport *report,
    MachineInterpError *error);

int machine_interp_verify_file(const MachineInterpFile *interp_file,
    MachineInterpError *error);
int machine_interp_dump_file(const MachineInterpFile *interp_file,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_file(const MachineInterpFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineInterpError *error);

int machine_interp_report_get_summary(const MachineInterpReport *report,
    size_t *out_mapped_byte_count);
int machine_interp_report_get_overview_artifact(const MachineInterpReport *report,
    MachineInterpReportOverviewArtifact *out_artifact);
int machine_interp_report_get_file(const MachineInterpReport *report,
    const MachineInterpFile **out_file);
int machine_interp_report_get_payload_decode_file(const MachineInterpReport *report,
    const MachinePayloadDecodeFile **out_payload_decode_file);
int machine_interp_report_get_header_summary_artifact(const MachineInterpReport *report,
    const MachineInterpHeaderSummary **out_summary);
int machine_interp_report_get_target_policy_summary_artifact(const MachineInterpReport *report,
    const MachineInterpTargetPolicySummary **out_summary);
int machine_interp_report_get_runtime_launch_summary_artifact(const MachineInterpReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_interp_report_get_initial_stack_summary_artifact(const MachineInterpReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_interp_report_get_runtime_memory_summary_artifact(const MachineInterpReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_interp_report_get_current_segment_summary_artifact(const MachineInterpReport *report,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_interp_report_get_fetch_summary_artifact(const MachineInterpReport *report,
    const MachineStepFetchSummary **out_summary);
int machine_interp_report_get_decode_tag_summary_artifact(const MachineInterpReport *report,
    const MachineDecodeTagSummary **out_summary);
int machine_interp_report_get_payload_summary_artifact(const MachineInterpReport *report,
    const MachinePayloadDecodeSummary **out_summary);
int machine_interp_report_get_interp_summary_artifact(const MachineInterpReport *report,
    const MachineInterpSummary **out_summary);
int machine_interp_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_interp_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_interp_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary);
int machine_interp_report_overview_artifact_get_current_segment_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_interp_report_overview_artifact_get_fetch_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary);
int machine_interp_report_overview_artifact_get_decode_tag_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineDecodeTagSummary **out_summary);
int machine_interp_report_overview_artifact_get_payload_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachinePayloadDecodeSummary **out_summary);
int machine_interp_report_overview_artifact_get_interp_summary_artifact(
    const MachineInterpReportOverviewArtifact *artifact,
    const MachineInterpSummary **out_summary);

int machine_interp_dump_report(const MachineInterpReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_file(const MachineInterpFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineInterpError *error);
int machine_interp_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineInterpError *error);

#endif
