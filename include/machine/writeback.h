#ifndef MACHINE_WRITEBACK_H
#define MACHINE_WRITEBACK_H

#include <stddef.h>

#include "machine/mutation.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineWritebackError;

typedef enum {
    MACHINE_WRITEBACK_RESOLUTION_NONE = 0,
    MACHINE_WRITEBACK_RESOLUTION_COMMITTED_NO_OP,
    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_REGISTER_WRITEBACK,
    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_LOCAL_WRITEBACK,
    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_GLOBAL_WRITEBACK,
    MACHINE_WRITEBACK_RESOLUTION_DEFERRED_CALL_WRITEBACK,
    MACHINE_WRITEBACK_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_WRITEBACK_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineWritebackResolutionKind;

typedef enum {
    MACHINE_WRITEBACK_COMMIT_KIND_NONE = 0,
    MACHINE_WRITEBACK_COMMIT_KIND_NO_OP,
    MACHINE_WRITEBACK_COMMIT_KIND_REGISTER,
    MACHINE_WRITEBACK_COMMIT_KIND_LOCAL_SLOT,
    MACHINE_WRITEBACK_COMMIT_KIND_GLOBAL_SLOT,
    MACHINE_WRITEBACK_COMMIT_KIND_CALL
} MachineWritebackCommitKind;

typedef struct {
    MachineWritebackResolutionKind resolution_kind;
    MachineWritebackCommitKind commit_kind;
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
} MachineWritebackSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineMutationResolutionKind mutation_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineWritebackHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int commits_no_op_halt;
    int defers_register_writeback;
    int defers_slot_writeback;
    int defers_call_writeback;
} MachineWritebackTargetPolicySummary;

typedef struct {
    MachineMutationFile mutation_file;
    MachineWritebackResolutionKind resolution_kind;
    MachineWritebackCommitKind commit_kind;
} MachineWritebackFile;

typedef struct {
    MachineWritebackFile file;
    MachineWritebackHeaderSummary header_summary;
    MachineWritebackTargetPolicySummary target_policy_summary;
    MachineMutationReport mutation_report;
    MachineWritebackSummary writeback_summary;
} MachineWritebackReport;

typedef struct {
    const MachineWritebackReport *report;
    const MachineWritebackHeaderSummary *header_summary;
    const MachineWritebackTargetPolicySummary *target_policy_summary;
    const MachineMutationReport *mutation_report;
    const MachineWritebackSummary *writeback_summary;
} MachineWritebackReportOverviewArtifact;

void machine_writeback_file_init(MachineWritebackFile *writeback_file);
void machine_writeback_file_free(MachineWritebackFile *writeback_file);
void machine_writeback_report_init(MachineWritebackReport *report);
void machine_writeback_report_free(MachineWritebackReport *report);
int machine_writeback_clone_file(const MachineWritebackFile *source,
    MachineWritebackFile *out_clone,
    MachineWritebackError *error);

const char *machine_writeback_resolution_kind_name(MachineWritebackResolutionKind resolution_kind);
const char *machine_writeback_commit_kind_name(MachineWritebackCommitKind commit_kind);
int machine_writeback_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineWritebackTargetPolicySummary *out_summary);
int machine_writeback_file_get_target_policy_summary(const MachineWritebackFile *writeback_file,
    MachineWritebackTargetPolicySummary *out_summary);
int machine_writeback_file_get_summary(const MachineWritebackFile *writeback_file,
    size_t *out_mapped_byte_count);
int machine_writeback_file_get_header_summary(const MachineWritebackFile *writeback_file,
    MachineWritebackHeaderSummary *out_summary);
int machine_writeback_file_get_mutation_summary(const MachineWritebackFile *writeback_file,
    MachineMutationSummary *out_summary);
int machine_writeback_file_get_writeback_summary(const MachineWritebackFile *writeback_file,
    MachineWritebackSummary *out_summary);

int machine_writeback_build_from_machine_mutation_file(const MachineMutationFile *mutation_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_mutation_report(const MachineMutationReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_state_file(const MachineStateFile *state_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_state_report(const MachineStateReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_transition_file(const MachineTransitionFile *transition_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_transition_report(const MachineTransitionReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_interp_file(const MachineInterpFile *interp_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_interp_report(const MachineInterpReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_payload_decode_file(const MachinePayloadDecodeFile *payload_decode_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_decode_file(const MachineDecodeFile *decode_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_decode_report(const MachineDecodeReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_step_report(const MachineStepReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);
int machine_writeback_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineWritebackFile *out_writeback_file,
    MachineWritebackError *error);

int machine_writeback_build_report_from_file(const MachineWritebackFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_mutation_file(const MachineMutationFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_mutation_report(const MachineMutationReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_state_file(const MachineStateFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_state_report(const MachineStateReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_transition_file(const MachineTransitionFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_transition_report(const MachineTransitionReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_interp_file(const MachineInterpFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_interp_report(const MachineInterpReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_decode_file(const MachineDecodeFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_decode_report(const MachineDecodeReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineWritebackReport *out_report,
    MachineWritebackError *error);
int machine_writeback_report_refresh(MachineWritebackReport *report,
    MachineWritebackError *error);

int machine_writeback_verify_file(const MachineWritebackFile *writeback_file,
    MachineWritebackError *error);
int machine_writeback_dump_file(const MachineWritebackFile *writeback_file,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_file(const MachineWritebackFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_mutation_file(const MachineMutationFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_mutation_report(const MachineMutationReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_state_file(const MachineStateFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_state_report(const MachineStateReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineWritebackError *error);

int machine_writeback_report_get_summary(const MachineWritebackReport *report,
    size_t *out_mapped_byte_count);
int machine_writeback_report_get_overview_artifact(const MachineWritebackReport *report,
    MachineWritebackReportOverviewArtifact *out_artifact);
int machine_writeback_report_get_file(const MachineWritebackReport *report,
    const MachineWritebackFile **out_file);
int machine_writeback_report_get_mutation_file(const MachineWritebackReport *report,
    const MachineMutationFile **out_mutation_file);
int machine_writeback_report_get_mutation_report(const MachineWritebackReport *report,
    const MachineMutationReport **out_mutation_report);
int machine_writeback_report_get_header_summary_artifact(const MachineWritebackReport *report,
    const MachineWritebackHeaderSummary **out_summary);
int machine_writeback_report_get_target_policy_summary_artifact(const MachineWritebackReport *report,
    const MachineWritebackTargetPolicySummary **out_summary);
int machine_writeback_report_get_writeback_summary_artifact(const MachineWritebackReport *report,
    const MachineWritebackSummary **out_summary);
int machine_writeback_report_overview_artifact_get_mutation_report(
    const MachineWritebackReportOverviewArtifact *artifact,
    const MachineMutationReport **out_mutation_report);
int machine_writeback_report_overview_artifact_get_writeback_summary_artifact(
    const MachineWritebackReportOverviewArtifact *artifact,
    const MachineWritebackSummary **out_summary);

int machine_writeback_dump_report(const MachineWritebackReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_file(const MachineWritebackFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_mutation_file(const MachineMutationFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_mutation_report(const MachineMutationReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_state_file(const MachineStateFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_state_report(const MachineStateReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_transition_file(const MachineTransitionFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_transition_report(const MachineTransitionReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_interp_file(const MachineInterpFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_interp_report(const MachineInterpReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_payload_decode_file(const MachinePayloadDecodeFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_payload_decode_report(const MachinePayloadDecodeReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_decode_file(const MachineDecodeFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_decode_report(const MachineDecodeReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineWritebackError *error);
int machine_writeback_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineWritebackError *error);

#endif
