#ifndef MACHINE_MUTATION_H
#define MACHINE_MUTATION_H

#include <stddef.h>

#include "machine/state.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineMutationError;

typedef enum {
    MACHINE_MUTATION_RESOLUTION_NONE = 0,
    MACHINE_MUTATION_RESOLUTION_NO_MUTATION,
    MACHINE_MUTATION_RESOLUTION_DEFERRED_REGISTER_RESULT,
    MACHINE_MUTATION_RESOLUTION_DEFERRED_LOCAL_SLOT,
    MACHINE_MUTATION_RESOLUTION_DEFERRED_GLOBAL_SLOT,
    MACHINE_MUTATION_RESOLUTION_DEFERRED_CALL_EFFECT,
    MACHINE_MUTATION_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_MUTATION_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineMutationResolutionKind;

typedef enum {
    MACHINE_MUTATION_EFFECT_NONE = 0,
    MACHINE_MUTATION_EFFECT_CONTROL_ONLY,
    MACHINE_MUTATION_EFFECT_VALUE_RESULT,
    MACHINE_MUTATION_EFFECT_LOCAL_SLOT,
    MACHINE_MUTATION_EFFECT_GLOBAL_SLOT,
    MACHINE_MUTATION_EFFECT_CALL
} MachineMutationEffectKind;

typedef struct {
    MachineMutationResolutionKind resolution_kind;
    MachineMutationEffectKind effect_kind;
    MachineStateResolutionKind state_resolution_kind;
    MachineTransitionResolutionKind transition_resolution_kind;
    MachineInterpActionKind action_kind;
    MachinePayloadDecodeKind payload_kind;
    MachineDecodeTagClass tag_class;
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
} MachineMutationSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineStateResolutionKind state_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineMutationHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int classifies_control_only;
    int defers_register_result_mutation;
    int defers_slot_mutation;
    int defers_call_effect;
} MachineMutationTargetPolicySummary;

typedef struct {
    MachineStateFile state_file;
    MachineMutationResolutionKind resolution_kind;
    MachineMutationEffectKind effect_kind;
} MachineMutationFile;

typedef struct {
    MachineMutationFile file;
    MachineMutationHeaderSummary header_summary;
    MachineMutationTargetPolicySummary target_policy_summary;
    MachineStateReport state_report;
    MachineMutationSummary mutation_summary;
} MachineMutationReport;

typedef struct {
    const MachineMutationReport *report;
    const MachineMutationHeaderSummary *header_summary;
    const MachineMutationTargetPolicySummary *target_policy_summary;
    const MachineStateReport *state_report;
    const MachineMutationSummary *mutation_summary;
} MachineMutationReportOverviewArtifact;

void machine_mutation_file_init(MachineMutationFile *mutation_file);
void machine_mutation_file_free(MachineMutationFile *mutation_file);
void machine_mutation_report_init(MachineMutationReport *report);
void machine_mutation_report_free(MachineMutationReport *report);
int machine_mutation_clone_file(const MachineMutationFile *source,
    MachineMutationFile *out_clone,
    MachineMutationError *error);

const char *machine_mutation_resolution_kind_name(MachineMutationResolutionKind resolution_kind);
const char *machine_mutation_effect_kind_name(MachineMutationEffectKind effect_kind);
int machine_mutation_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineMutationTargetPolicySummary *out_summary);
int machine_mutation_file_get_target_policy_summary(const MachineMutationFile *mutation_file,
    MachineMutationTargetPolicySummary *out_summary);
int machine_mutation_file_get_summary(const MachineMutationFile *mutation_file,
    size_t *out_mapped_byte_count);
int machine_mutation_file_get_header_summary(const MachineMutationFile *mutation_file,
    MachineMutationHeaderSummary *out_summary);
int machine_mutation_file_get_state_summary(const MachineMutationFile *mutation_file,
    MachineStateSummary *out_summary);
int machine_mutation_file_get_payload_summary(const MachineMutationFile *mutation_file,
    MachinePayloadDecodeSummary *out_summary);
int machine_mutation_file_get_decode_tag_summary(const MachineMutationFile *mutation_file,
    MachineDecodeTagSummary *out_summary);
int machine_mutation_file_get_mutation_summary(const MachineMutationFile *mutation_file,
    MachineMutationSummary *out_summary);

int machine_mutation_build_from_machine_state_file(const MachineStateFile *state_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_state_report(const MachineStateReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_transition_file(const MachineTransitionFile *transition_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_transition_report(const MachineTransitionReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_interp_file(const MachineInterpFile *interp_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_interp_report(const MachineInterpReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_step_report(const MachineStepReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);
int machine_mutation_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineMutationFile *out_mutation_file,
    MachineMutationError *error);

int machine_mutation_build_report_from_file(const MachineMutationFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_state_file(const MachineStateFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_state_report(const MachineStateReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_transition_file(const MachineTransitionFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_transition_report(const MachineTransitionReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_interp_file(const MachineInterpFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_interp_report(const MachineInterpReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineMutationReport *out_report,
    MachineMutationError *error);
int machine_mutation_report_refresh(MachineMutationReport *report,
    MachineMutationError *error);

int machine_mutation_verify_file(const MachineMutationFile *mutation_file,
    MachineMutationError *error);
int machine_mutation_dump_file(const MachineMutationFile *mutation_file,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_file(const MachineMutationFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_state_file(const MachineStateFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_state_report(const MachineStateReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineMutationError *error);

int machine_mutation_report_get_summary(const MachineMutationReport *report,
    size_t *out_mapped_byte_count);
int machine_mutation_report_get_overview_artifact(const MachineMutationReport *report,
    MachineMutationReportOverviewArtifact *out_artifact);
int machine_mutation_report_get_file(const MachineMutationReport *report,
    const MachineMutationFile **out_file);
int machine_mutation_report_get_state_file(const MachineMutationReport *report,
    const MachineStateFile **out_state_file);
int machine_mutation_report_get_state_report(const MachineMutationReport *report,
    const MachineStateReport **out_state_report);
int machine_mutation_report_get_header_summary_artifact(const MachineMutationReport *report,
    const MachineMutationHeaderSummary **out_summary);
int machine_mutation_report_get_target_policy_summary_artifact(const MachineMutationReport *report,
    const MachineMutationTargetPolicySummary **out_summary);
int machine_mutation_report_get_mutation_summary_artifact(const MachineMutationReport *report,
    const MachineMutationSummary **out_summary);
int machine_mutation_report_overview_artifact_get_state_report(
    const MachineMutationReportOverviewArtifact *artifact,
    const MachineStateReport **out_state_report);
int machine_mutation_report_overview_artifact_get_mutation_summary_artifact(
    const MachineMutationReportOverviewArtifact *artifact,
    const MachineMutationSummary **out_summary);

int machine_mutation_dump_report(const MachineMutationReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_file(const MachineMutationFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_state_file(const MachineStateFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_state_report(const MachineStateReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineMutationError *error);
int machine_mutation_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineMutationError *error);

#endif
