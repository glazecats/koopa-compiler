#ifndef MACHINE_TRANSITION_H
#define MACHINE_TRANSITION_H

#include <stddef.h>

#include "machine/interp.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineTransitionError;

typedef enum {
    MACHINE_TRANSITION_RESOLUTION_NONE = 0,
    MACHINE_TRANSITION_RESOLUTION_NEXT_FETCH,
    MACHINE_TRANSITION_RESOLUTION_HALT,
    MACHINE_TRANSITION_RESOLUTION_DEFERRED_CONTROL_TRANSFER,
    MACHINE_TRANSITION_RESOLUTION_UNSUPPORTED
} MachineTransitionResolutionKind;

typedef struct {
    size_t byte_virtual_address;
    size_t byte_memory_offset;
    size_t segment_index;
    const char *segment_name;
    unsigned char byte_value;
} MachineTransitionNextFetchSummary;

typedef struct {
    MachineTransitionResolutionKind resolution_kind;
    MachineInterpActionKind action_kind;
    unsigned char raw_byte;
    unsigned char tag_value;
    int is_known;
    const char *tag_name;
    size_t instruction_byte_count;
    MachineStepStatus next_status;
    int has_next_fetch;
    size_t next_program_counter;
    size_t next_current_segment_index;
    unsigned char next_current_byte;
    size_t next_current_byte_memory_offset;
    int has_primary_target_block;
    size_t primary_target_block_index;
    int has_secondary_target_block;
    size_t secondary_target_block_index;
    int has_return_immediate;
    size_t return_immediate;
} MachineTransitionSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineStepStatus step_status;
    size_t program_counter;
    size_t stack_pointer;
    size_t current_segment_index;
    size_t mapped_byte_count;
} MachineTransitionHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int resolves_linear_next_fetch;
    int resolves_halt_transition;
    int defers_control_transfer_targets;
} MachineTransitionTargetPolicySummary;

typedef struct {
    MachineInterpFile interp_file;
    MachineTransitionResolutionKind resolution_kind;
    MachineStepStatus next_status;
    int has_next_fetch;
    size_t next_program_counter;
    size_t next_current_segment_index;
    unsigned char next_current_byte;
    size_t next_current_byte_memory_offset;
} MachineTransitionFile;

typedef struct {
    MachineTransitionFile file;
    MachineTransitionHeaderSummary header_summary;
    MachineTransitionTargetPolicySummary target_policy_summary;
    MachineRuntimeLaunchSummary runtime_launch_summary;
    MachineRuntimeInitialStackSummary initial_stack_summary;
    MachineRuntimeMemorySummary runtime_memory_summary;
    MachineRuntimeSegmentSummary current_segment_summary;
    MachineStepFetchSummary fetch_summary;
    MachineDecodeTagSummary decode_tag_summary;
    MachinePayloadDecodeSummary payload_summary;
    MachineInterpSummary interp_summary;
    MachineTransitionSummary transition_summary;
    MachineTransitionNextFetchSummary next_fetch_summary;
} MachineTransitionReport;

typedef struct {
    const MachineTransitionReport *report;
    const MachineTransitionHeaderSummary *header_summary;
    const MachineTransitionTargetPolicySummary *target_policy_summary;
    const MachineRuntimeLaunchSummary *runtime_launch_summary;
    const MachineRuntimeInitialStackSummary *initial_stack_summary;
    const MachineRuntimeMemorySummary *runtime_memory_summary;
    const MachineRuntimeSegmentSummary *current_segment_summary;
    const MachineStepFetchSummary *fetch_summary;
    const MachineDecodeTagSummary *decode_tag_summary;
    const MachinePayloadDecodeSummary *payload_summary;
    const MachineInterpSummary *interp_summary;
    const MachineTransitionSummary *transition_summary;
    const MachineTransitionNextFetchSummary *next_fetch_summary;
} MachineTransitionReportOverviewArtifact;

void machine_transition_file_init(MachineTransitionFile *transition_file);
void machine_transition_file_free(MachineTransitionFile *transition_file);
void machine_transition_report_init(MachineTransitionReport *report);
void machine_transition_report_free(MachineTransitionReport *report);
int machine_transition_clone_file(const MachineTransitionFile *source,
    MachineTransitionFile *out_clone,
    MachineTransitionError *error);

const char *machine_transition_resolution_kind_name(MachineTransitionResolutionKind resolution_kind);
int machine_transition_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineTransitionTargetPolicySummary *out_summary);
int machine_transition_file_get_target_policy_summary(const MachineTransitionFile *transition_file,
    MachineTransitionTargetPolicySummary *out_summary);
int machine_transition_file_get_summary(const MachineTransitionFile *transition_file,
    size_t *out_mapped_byte_count);
int machine_transition_file_get_header_summary(const MachineTransitionFile *transition_file,
    MachineTransitionHeaderSummary *out_summary);
int machine_transition_file_get_runtime_launch_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_transition_file_get_initial_stack_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_transition_file_get_runtime_memory_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_transition_file_get_current_segment_summary(const MachineTransitionFile *transition_file,
    MachineRuntimeSegmentSummary *out_summary);
int machine_transition_file_get_fetch_summary(const MachineTransitionFile *transition_file,
    MachineStepFetchSummary *out_summary);
int machine_transition_file_get_decode_tag_summary(const MachineTransitionFile *transition_file,
    MachineDecodeTagSummary *out_summary);
int machine_transition_file_get_payload_summary(const MachineTransitionFile *transition_file,
    MachinePayloadDecodeSummary *out_summary);
int machine_transition_file_get_interp_summary(const MachineTransitionFile *transition_file,
    MachineInterpSummary *out_summary);
int machine_transition_file_get_transition_summary(const MachineTransitionFile *transition_file,
    MachineTransitionSummary *out_summary);
int machine_transition_file_get_next_fetch_summary(const MachineTransitionFile *transition_file,
    MachineTransitionNextFetchSummary *out_summary);

int machine_transition_build_from_machine_interp_file(const MachineInterpFile *interp_file,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_interp_report(const MachineInterpReport *report,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_step_report(const MachineStepReport *report,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);
int machine_transition_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTransitionFile *out_transition_file,
    MachineTransitionError *error);

int machine_transition_build_report_from_file(const MachineTransitionFile *source,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_interp_file(const MachineInterpFile *source,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_interp_report(const MachineInterpReport *report,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTransitionReport *out_report,
    MachineTransitionError *error);
int machine_transition_report_refresh(MachineTransitionReport *report,
    MachineTransitionError *error);

int machine_transition_verify_file(const MachineTransitionFile *transition_file,
    MachineTransitionError *error);
int machine_transition_dump_file(const MachineTransitionFile *transition_file,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_file(const MachineTransitionFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTransitionError *error);

int machine_transition_report_get_summary(const MachineTransitionReport *report,
    size_t *out_mapped_byte_count);
int machine_transition_report_get_overview_artifact(const MachineTransitionReport *report,
    MachineTransitionReportOverviewArtifact *out_artifact);
int machine_transition_report_get_file(const MachineTransitionReport *report,
    const MachineTransitionFile **out_file);
int machine_transition_report_get_interp_file(const MachineTransitionReport *report,
    const MachineInterpFile **out_interp_file);
int machine_transition_report_get_header_summary_artifact(const MachineTransitionReport *report,
    const MachineTransitionHeaderSummary **out_summary);
int machine_transition_report_get_target_policy_summary_artifact(const MachineTransitionReport *report,
    const MachineTransitionTargetPolicySummary **out_summary);
int machine_transition_report_get_runtime_launch_summary_artifact(const MachineTransitionReport *report,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_transition_report_get_initial_stack_summary_artifact(const MachineTransitionReport *report,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_transition_report_get_runtime_memory_summary_artifact(const MachineTransitionReport *report,
    const MachineRuntimeMemorySummary **out_summary);
int machine_transition_report_get_current_segment_summary_artifact(const MachineTransitionReport *report,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_transition_report_get_fetch_summary_artifact(const MachineTransitionReport *report,
    const MachineStepFetchSummary **out_summary);
int machine_transition_report_get_decode_tag_summary_artifact(const MachineTransitionReport *report,
    const MachineDecodeTagSummary **out_summary);
int machine_transition_report_get_payload_summary_artifact(const MachineTransitionReport *report,
    const MachinePayloadDecodeSummary **out_summary);
int machine_transition_report_get_interp_summary_artifact(const MachineTransitionReport *report,
    const MachineInterpSummary **out_summary);
int machine_transition_report_get_transition_summary_artifact(const MachineTransitionReport *report,
    const MachineTransitionSummary **out_summary);
int machine_transition_report_get_next_fetch_summary_artifact(const MachineTransitionReport *report,
    const MachineTransitionNextFetchSummary **out_summary);
int machine_transition_report_overview_artifact_get_runtime_launch_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineRuntimeLaunchSummary **out_summary);
int machine_transition_report_overview_artifact_get_initial_stack_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineRuntimeInitialStackSummary **out_summary);
int machine_transition_report_overview_artifact_get_runtime_memory_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineRuntimeMemorySummary **out_summary);
int machine_transition_report_overview_artifact_get_current_segment_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineRuntimeSegmentSummary **out_summary);
int machine_transition_report_overview_artifact_get_fetch_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineStepFetchSummary **out_summary);
int machine_transition_report_overview_artifact_get_decode_tag_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineDecodeTagSummary **out_summary);
int machine_transition_report_overview_artifact_get_payload_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachinePayloadDecodeSummary **out_summary);
int machine_transition_report_overview_artifact_get_interp_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineInterpSummary **out_summary);
int machine_transition_report_overview_artifact_get_transition_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineTransitionSummary **out_summary);
int machine_transition_report_overview_artifact_get_next_fetch_summary_artifact(
    const MachineTransitionReportOverviewArtifact *artifact,
    const MachineTransitionNextFetchSummary **out_summary);

int machine_transition_dump_report(const MachineTransitionReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_file(const MachineTransitionFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineTransitionError *error);
int machine_transition_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTransitionError *error);

#endif
