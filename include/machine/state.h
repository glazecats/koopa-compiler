#ifndef MACHINE_STATE_H
#define MACHINE_STATE_H

#include <stddef.h>

#include "machine/transition.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineStateError;

typedef enum {
    MACHINE_STATE_RESOLUTION_NONE = 0,
    MACHINE_STATE_RESOLUTION_READY,
    MACHINE_STATE_RESOLUTION_HALTED,
    MACHINE_STATE_RESOLUTION_DEFERRED_CONTROL_TRANSFER,
    MACHINE_STATE_RESOLUTION_UNSUPPORTED
} MachineStateResolutionKind;

typedef struct {
    size_t byte_virtual_address;
    size_t byte_memory_offset;
    size_t segment_index;
    const char *segment_name;
    unsigned char byte_value;
} MachineStateCurrentFetchSummary;

typedef struct {
    MachineStateResolutionKind resolution_kind;
    MachineTransitionResolutionKind transition_resolution_kind;
    MachineInterpActionKind action_kind;
    unsigned char raw_byte;
    unsigned char tag_value;
    int is_known;
    const char *tag_name;
    size_t instruction_byte_count;
    int has_state;
    MachineStepStatus state_status;
    size_t program_counter;
    size_t stack_pointer;
    int has_current_fetch;
    size_t current_segment_index;
    unsigned char current_byte;
    size_t current_byte_memory_offset;
    int has_primary_target_block;
    size_t primary_target_block_index;
    int has_secondary_target_block;
    size_t secondary_target_block_index;
    int has_return_immediate;
    size_t return_immediate;
} MachineStateSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineTransitionResolutionKind transition_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineStateHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int resolves_ready_state;
    int resolves_halt_state;
    int defers_control_transfer_state;
} MachineStateTargetPolicySummary;

typedef struct {
    MachineTransitionFile transition_file;
    MachineStateResolutionKind resolution_kind;
    int has_state;
    MachineStepStatus state_status;
    size_t program_counter;
    size_t stack_pointer;
    int has_current_fetch;
    size_t current_segment_index;
    unsigned char current_byte;
    size_t current_byte_memory_offset;
} MachineStateFile;

typedef struct {
    MachineStateFile file;
    MachineStateHeaderSummary header_summary;
    MachineStateTargetPolicySummary target_policy_summary;
    MachineTransitionReport transition_report;
    MachineStateSummary state_summary;
    MachineStateCurrentFetchSummary current_fetch_summary;
} MachineStateReport;

typedef struct {
    const MachineStateReport *report;
    const MachineStateHeaderSummary *header_summary;
    const MachineStateTargetPolicySummary *target_policy_summary;
    const MachineTransitionReport *transition_report;
    const MachineStateSummary *state_summary;
    const MachineStateCurrentFetchSummary *current_fetch_summary;
} MachineStateReportOverviewArtifact;

void machine_state_file_init(MachineStateFile *state_file);
void machine_state_file_free(MachineStateFile *state_file);
void machine_state_report_init(MachineStateReport *report);
void machine_state_report_free(MachineStateReport *report);
int machine_state_clone_file(const MachineStateFile *source,
    MachineStateFile *out_clone,
    MachineStateError *error);

const char *machine_state_resolution_kind_name(MachineStateResolutionKind resolution_kind);
int machine_state_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineStateTargetPolicySummary *out_summary);
int machine_state_file_get_target_policy_summary(const MachineStateFile *state_file,
    MachineStateTargetPolicySummary *out_summary);
int machine_state_file_get_summary(const MachineStateFile *state_file,
    size_t *out_mapped_byte_count);
int machine_state_file_get_header_summary(const MachineStateFile *state_file,
    MachineStateHeaderSummary *out_summary);
int machine_state_file_get_runtime_launch_summary(const MachineStateFile *state_file,
    MachineRuntimeLaunchSummary *out_summary);
int machine_state_file_get_initial_stack_summary(const MachineStateFile *state_file,
    MachineRuntimeInitialStackSummary *out_summary);
int machine_state_file_get_runtime_memory_summary(const MachineStateFile *state_file,
    MachineRuntimeMemorySummary *out_summary);
int machine_state_file_get_decode_tag_summary(const MachineStateFile *state_file,
    MachineDecodeTagSummary *out_summary);
int machine_state_file_get_payload_summary(const MachineStateFile *state_file,
    MachinePayloadDecodeSummary *out_summary);
int machine_state_file_get_transition_summary(const MachineStateFile *state_file,
    MachineTransitionSummary *out_summary);
int machine_state_file_get_state_summary(const MachineStateFile *state_file,
    MachineStateSummary *out_summary);
int machine_state_file_get_current_fetch_summary(const MachineStateFile *state_file,
    MachineStateCurrentFetchSummary *out_summary);

int machine_state_build_from_machine_transition_file(const MachineTransitionFile *transition_file,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_transition_report(const MachineTransitionReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_interp_file(const MachineInterpFile *interp_file,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_interp_report(const MachineInterpReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_step_report(const MachineStepReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineStateFile *out_state_file,
    MachineStateError *error);
int machine_state_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStateFile *out_state_file,
    MachineStateError *error);

int machine_state_build_report_from_file(const MachineStateFile *source,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_transition_file(const MachineTransitionFile *source,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_transition_report(const MachineTransitionReport *report,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_interp_file(const MachineInterpFile *source,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_interp_report(const MachineInterpReport *report,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineStateReport *out_report,
    MachineStateError *error);
int machine_state_report_refresh(MachineStateReport *report,
    MachineStateError *error);

int machine_state_verify_file(const MachineStateFile *state_file,
    MachineStateError *error);
int machine_state_dump_file(const MachineStateFile *state_file,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_file(const MachineStateFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStateError *error);

int machine_state_report_get_summary(const MachineStateReport *report,
    size_t *out_mapped_byte_count);
int machine_state_report_get_overview_artifact(const MachineStateReport *report,
    MachineStateReportOverviewArtifact *out_artifact);
int machine_state_report_get_file(const MachineStateReport *report,
    const MachineStateFile **out_file);
int machine_state_report_get_transition_file(const MachineStateReport *report,
    const MachineTransitionFile **out_transition_file);
int machine_state_report_get_transition_report(const MachineStateReport *report,
    const MachineTransitionReport **out_transition_report);
int machine_state_report_get_header_summary_artifact(const MachineStateReport *report,
    const MachineStateHeaderSummary **out_summary);
int machine_state_report_get_target_policy_summary_artifact(const MachineStateReport *report,
    const MachineStateTargetPolicySummary **out_summary);
int machine_state_report_get_state_summary_artifact(const MachineStateReport *report,
    const MachineStateSummary **out_summary);
int machine_state_report_get_current_fetch_summary_artifact(const MachineStateReport *report,
    const MachineStateCurrentFetchSummary **out_summary);
int machine_state_report_overview_artifact_get_transition_report(
    const MachineStateReportOverviewArtifact *artifact,
    const MachineTransitionReport **out_transition_report);
int machine_state_report_overview_artifact_get_state_summary_artifact(
    const MachineStateReportOverviewArtifact *artifact,
    const MachineStateSummary **out_summary);
int machine_state_report_overview_artifact_get_current_fetch_summary_artifact(
    const MachineStateReportOverviewArtifact *artifact,
    const MachineStateCurrentFetchSummary **out_summary);

int machine_state_dump_report(const MachineStateReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_file(const MachineStateFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineStateError *error);
int machine_state_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineStateError *error);

#endif
