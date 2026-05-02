#ifndef MACHINE_COMMIT_H
#define MACHINE_COMMIT_H

#include <stddef.h>

#include "machine/writeback.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineCommitError;

typedef enum {
    MACHINE_COMMIT_RESOLUTION_NONE = 0,
    MACHINE_COMMIT_RESOLUTION_COMMITTED_STATE,
    MACHINE_COMMIT_RESOLUTION_DEFERRED_REGISTER_COMMIT,
    MACHINE_COMMIT_RESOLUTION_DEFERRED_LOCAL_COMMIT,
    MACHINE_COMMIT_RESOLUTION_DEFERRED_GLOBAL_COMMIT,
    MACHINE_COMMIT_RESOLUTION_DEFERRED_CALL_COMMIT,
    MACHINE_COMMIT_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_COMMIT_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineCommitResolutionKind;

typedef enum {
    MACHINE_COMMIT_KIND_NONE = 0,
    MACHINE_COMMIT_KIND_STATE,
    MACHINE_COMMIT_KIND_REGISTER,
    MACHINE_COMMIT_KIND_LOCAL_SLOT,
    MACHINE_COMMIT_KIND_GLOBAL_SLOT,
    MACHINE_COMMIT_KIND_CALL
} MachineCommitKind;

typedef struct {
    size_t byte_virtual_address;
    size_t byte_memory_offset;
    size_t segment_index;
    const char *segment_name;
    unsigned char byte_value;
} MachineCommitCurrentFetchSummary;

typedef struct {
    MachineCommitResolutionKind resolution_kind;
    MachineCommitKind commit_kind;
    MachineWritebackResolutionKind writeback_resolution_kind;
    MachineWritebackCommitKind writeback_commit_kind;
    MachineMutationResolutionKind mutation_resolution_kind;
    MachineMutationEffectKind mutation_effect_kind;
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
    int has_committed_state;
    MachineStepStatus committed_status;
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
} MachineCommitSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineWritebackResolutionKind writeback_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineCommitHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int commits_no_op_state;
    int defers_register_commit;
    int defers_slot_commit;
    int defers_call_commit;
} MachineCommitTargetPolicySummary;

typedef struct {
    MachineWritebackFile writeback_file;
    MachineCommitResolutionKind resolution_kind;
    MachineCommitKind commit_kind;
    int has_committed_state;
    MachineStepStatus committed_status;
    size_t program_counter;
    size_t stack_pointer;
    int has_current_fetch;
    size_t current_segment_index;
    unsigned char current_byte;
    size_t current_byte_memory_offset;
} MachineCommitFile;

typedef struct {
    MachineCommitFile file;
    MachineCommitHeaderSummary header_summary;
    MachineCommitTargetPolicySummary target_policy_summary;
    MachineWritebackReport writeback_report;
    MachineCommitSummary commit_summary;
    MachineCommitCurrentFetchSummary current_fetch_summary;
} MachineCommitReport;

typedef struct {
    const MachineCommitReport *report;
    const MachineCommitHeaderSummary *header_summary;
    const MachineCommitTargetPolicySummary *target_policy_summary;
    const MachineWritebackReport *writeback_report;
    const MachineCommitSummary *commit_summary;
    const MachineCommitCurrentFetchSummary *current_fetch_summary;
} MachineCommitReportOverviewArtifact;

void machine_commit_file_init(MachineCommitFile *commit_file);
void machine_commit_file_free(MachineCommitFile *commit_file);
void machine_commit_report_init(MachineCommitReport *report);
void machine_commit_report_free(MachineCommitReport *report);
int machine_commit_clone_file(const MachineCommitFile *source,
    MachineCommitFile *out_clone,
    MachineCommitError *error);

const char *machine_commit_resolution_kind_name(MachineCommitResolutionKind resolution_kind);
const char *machine_commit_kind_name(MachineCommitKind commit_kind);
int machine_commit_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineCommitTargetPolicySummary *out_summary);
int machine_commit_file_get_target_policy_summary(const MachineCommitFile *commit_file,
    MachineCommitTargetPolicySummary *out_summary);
int machine_commit_file_get_summary(const MachineCommitFile *commit_file,
    size_t *out_mapped_byte_count);
int machine_commit_file_get_header_summary(const MachineCommitFile *commit_file,
    MachineCommitHeaderSummary *out_summary);
int machine_commit_file_get_writeback_summary(const MachineCommitFile *commit_file,
    MachineWritebackSummary *out_summary);
int machine_commit_file_get_commit_summary(const MachineCommitFile *commit_file,
    MachineCommitSummary *out_summary);
int machine_commit_file_get_current_fetch_summary(const MachineCommitFile *commit_file,
    MachineCommitCurrentFetchSummary *out_summary);

int machine_commit_build_from_machine_writeback_file(const MachineWritebackFile *writeback_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_writeback_report(const MachineWritebackReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_mutation_file(const MachineMutationFile *mutation_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_mutation_report(const MachineMutationReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_state_file(const MachineStateFile *state_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_state_report(const MachineStateReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_transition_file(const MachineTransitionFile *transition_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_transition_report(const MachineTransitionReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_interp_file(const MachineInterpFile *interp_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_interp_report(const MachineInterpReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_step_report(const MachineStepReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);
int machine_commit_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineCommitFile *out_commit_file,
    MachineCommitError *error);

int machine_commit_build_report_from_file(const MachineCommitFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_writeback_file(const MachineWritebackFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_writeback_report(const MachineWritebackReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_mutation_file(const MachineMutationFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_mutation_report(const MachineMutationReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_state_file(const MachineStateFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_state_report(const MachineStateReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_transition_file(const MachineTransitionFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_transition_report(const MachineTransitionReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_interp_file(const MachineInterpFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_interp_report(const MachineInterpReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineCommitReport *out_report,
    MachineCommitError *error);
int machine_commit_report_refresh(MachineCommitReport *report,
    MachineCommitError *error);

int machine_commit_verify_file(const MachineCommitFile *commit_file,
    MachineCommitError *error);
int machine_commit_dump_file(const MachineCommitFile *commit_file,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_file(const MachineCommitFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_writeback_file(const MachineWritebackFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_writeback_report(const MachineWritebackReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_mutation_file(const MachineMutationFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_mutation_report(const MachineMutationReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_state_file(const MachineStateFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_state_report(const MachineStateReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineCommitError *error);

int machine_commit_report_get_summary(const MachineCommitReport *report,
    size_t *out_mapped_byte_count);
int machine_commit_report_get_overview_artifact(const MachineCommitReport *report,
    MachineCommitReportOverviewArtifact *out_artifact);
int machine_commit_report_get_file(const MachineCommitReport *report,
    const MachineCommitFile **out_file);
int machine_commit_report_get_writeback_file(const MachineCommitReport *report,
    const MachineWritebackFile **out_writeback_file);
int machine_commit_report_get_writeback_report(const MachineCommitReport *report,
    const MachineWritebackReport **out_writeback_report);
int machine_commit_report_get_header_summary_artifact(const MachineCommitReport *report,
    const MachineCommitHeaderSummary **out_summary);
int machine_commit_report_get_target_policy_summary_artifact(const MachineCommitReport *report,
    const MachineCommitTargetPolicySummary **out_summary);
int machine_commit_report_get_commit_summary_artifact(const MachineCommitReport *report,
    const MachineCommitSummary **out_summary);
int machine_commit_report_get_current_fetch_summary_artifact(const MachineCommitReport *report,
    const MachineCommitCurrentFetchSummary **out_summary);
int machine_commit_report_overview_artifact_get_writeback_report(
    const MachineCommitReportOverviewArtifact *artifact,
    const MachineWritebackReport **out_writeback_report);
int machine_commit_report_overview_artifact_get_commit_summary_artifact(
    const MachineCommitReportOverviewArtifact *artifact,
    const MachineCommitSummary **out_summary);
int machine_commit_report_overview_artifact_get_current_fetch_summary_artifact(
    const MachineCommitReportOverviewArtifact *artifact,
    const MachineCommitCurrentFetchSummary **out_summary);

int machine_commit_dump_report(const MachineCommitReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_file(const MachineCommitFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_writeback_file(const MachineWritebackFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_writeback_report(const MachineWritebackReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_mutation_file(const MachineMutationFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_mutation_report(const MachineMutationReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_state_file(const MachineStateFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_state_report(const MachineStateReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineCommitError *error);
int machine_commit_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineCommitError *error);

#endif
