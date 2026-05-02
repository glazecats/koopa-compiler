#ifndef MACHINE_APPLY_H
#define MACHINE_APPLY_H

#include <stddef.h>

#include "machine/commit.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineApplyError;

typedef enum {
    MACHINE_APPLY_RESOLUTION_NONE = 0,
    MACHINE_APPLY_RESOLUTION_APPLIED_STATE,
    MACHINE_APPLY_RESOLUTION_PENDING_REGISTER_APPLICATION,
    MACHINE_APPLY_RESOLUTION_PENDING_LOCAL_APPLICATION,
    MACHINE_APPLY_RESOLUTION_PENDING_GLOBAL_APPLICATION,
    MACHINE_APPLY_RESOLUTION_PENDING_CALL_APPLICATION,
    MACHINE_APPLY_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_APPLY_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineApplyResolutionKind;

typedef enum {
    MACHINE_APPLY_KIND_NONE = 0,
    MACHINE_APPLY_KIND_STATE,
    MACHINE_APPLY_KIND_REGISTER,
    MACHINE_APPLY_KIND_LOCAL_SLOT,
    MACHINE_APPLY_KIND_GLOBAL_SLOT,
    MACHINE_APPLY_KIND_CALL
} MachineApplyKind;

typedef struct {
    MachineApplyResolutionKind resolution_kind;
    MachineApplyKind apply_kind;
    MachineCommitResolutionKind commit_resolution_kind;
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
    size_t payload_byte_count;
    unsigned char payload_bytes[3];
    int has_immediate_hint;
    size_t immediate_hint;
    int has_applied_state;
    MachineStepStatus applied_status;
    size_t applied_program_counter;
    size_t applied_stack_pointer;
    int has_applied_fetch;
    size_t applied_segment_index;
    unsigned char applied_byte;
    size_t applied_byte_memory_offset;
    int has_preview_state;
    MachineStepStatus preview_status;
    size_t preview_program_counter;
    size_t preview_stack_pointer;
    int has_preview_fetch;
    size_t preview_segment_index;
    unsigned char preview_byte;
    size_t preview_byte_memory_offset;
    int has_primary_target_block;
    size_t primary_target_block_index;
    int has_secondary_target_block;
    size_t secondary_target_block_index;
    int has_return_immediate;
    size_t return_immediate;
} MachineApplySummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineCommitResolutionKind commit_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineApplyHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int applies_no_op_state;
    int surfaces_register_application;
    int surfaces_slot_application;
    int surfaces_call_application;
    int surfaces_state_preview;
} MachineApplyTargetPolicySummary;

typedef struct {
    MachineCommitFile commit_file;
    MachineApplyResolutionKind resolution_kind;
    MachineApplyKind apply_kind;
    size_t payload_byte_count;
    unsigned char payload_bytes[3];
    int has_immediate_hint;
    size_t immediate_hint;
    int has_applied_state;
    MachineStepStatus applied_status;
    size_t applied_program_counter;
    size_t applied_stack_pointer;
    int has_applied_fetch;
    size_t applied_segment_index;
    unsigned char applied_byte;
    size_t applied_byte_memory_offset;
    int has_preview_state;
    MachineStepStatus preview_status;
    size_t preview_program_counter;
    size_t preview_stack_pointer;
    int has_preview_fetch;
    size_t preview_segment_index;
    unsigned char preview_byte;
    size_t preview_byte_memory_offset;
} MachineApplyFile;

typedef struct {
    MachineApplyFile file;
    MachineApplyHeaderSummary header_summary;
    MachineApplyTargetPolicySummary target_policy_summary;
    MachineCommitReport commit_report;
    MachineApplySummary apply_summary;
} MachineApplyReport;

typedef struct {
    const MachineApplyReport *report;
    const MachineApplyHeaderSummary *header_summary;
    const MachineApplyTargetPolicySummary *target_policy_summary;
    const MachineCommitReport *commit_report;
    const MachineApplySummary *apply_summary;
} MachineApplyReportOverviewArtifact;

void machine_apply_file_init(MachineApplyFile *apply_file);
void machine_apply_file_free(MachineApplyFile *apply_file);
void machine_apply_report_init(MachineApplyReport *report);
void machine_apply_report_free(MachineApplyReport *report);
int machine_apply_clone_file(const MachineApplyFile *source,
    MachineApplyFile *out_clone,
    MachineApplyError *error);

const char *machine_apply_resolution_kind_name(MachineApplyResolutionKind resolution_kind);
const char *machine_apply_kind_name(MachineApplyKind apply_kind);
int machine_apply_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineApplyTargetPolicySummary *out_summary);
int machine_apply_file_get_target_policy_summary(const MachineApplyFile *apply_file,
    MachineApplyTargetPolicySummary *out_summary);
int machine_apply_file_get_summary(const MachineApplyFile *apply_file,
    size_t *out_mapped_byte_count);
int machine_apply_file_get_header_summary(const MachineApplyFile *apply_file,
    MachineApplyHeaderSummary *out_summary);
int machine_apply_file_get_commit_summary(const MachineApplyFile *apply_file,
    MachineCommitSummary *out_summary);
int machine_apply_file_get_apply_summary(const MachineApplyFile *apply_file,
    MachineApplySummary *out_summary);

int machine_apply_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error);
int machine_apply_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error);
int machine_apply_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error);
int machine_apply_build_from_machine_step_report(const MachineStepReport *report,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error);
int machine_apply_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error);
int machine_apply_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineApplyFile *out_apply_file,
    MachineApplyError *error);

int machine_apply_build_report_from_file(const MachineApplyFile *source,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineApplyReport *out_report,
    MachineApplyError *error);
int machine_apply_report_refresh(MachineApplyReport *report,
    MachineApplyError *error);

int machine_apply_verify_file(const MachineApplyFile *apply_file,
    MachineApplyError *error);
int machine_apply_dump_file(const MachineApplyFile *apply_file,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_file(const MachineApplyFile *source,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineApplyError *error);

int machine_apply_report_get_summary(const MachineApplyReport *report,
    size_t *out_mapped_byte_count);
int machine_apply_report_get_overview_artifact(const MachineApplyReport *report,
    MachineApplyReportOverviewArtifact *out_artifact);
int machine_apply_report_get_file(const MachineApplyReport *report,
    const MachineApplyFile **out_file);
int machine_apply_report_get_commit_file(const MachineApplyReport *report,
    const MachineCommitFile **out_commit_file);
int machine_apply_report_get_commit_report(const MachineApplyReport *report,
    const MachineCommitReport **out_commit_report);
int machine_apply_report_get_header_summary_artifact(const MachineApplyReport *report,
    const MachineApplyHeaderSummary **out_summary);
int machine_apply_report_get_target_policy_summary_artifact(const MachineApplyReport *report,
    const MachineApplyTargetPolicySummary **out_summary);
int machine_apply_report_get_apply_summary_artifact(const MachineApplyReport *report,
    const MachineApplySummary **out_summary);
int machine_apply_report_overview_artifact_get_commit_report(
    const MachineApplyReportOverviewArtifact *artifact,
    const MachineCommitReport **out_commit_report);
int machine_apply_report_overview_artifact_get_apply_summary_artifact(
    const MachineApplyReportOverviewArtifact *artifact,
    const MachineApplySummary **out_summary);

int machine_apply_dump_report(const MachineApplyReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_file(const MachineApplyFile *source,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineApplyError *error);
int machine_apply_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineApplyError *error);

#endif
