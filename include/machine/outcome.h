#ifndef MACHINE_OUTCOME_H
#define MACHINE_OUTCOME_H

#include <stddef.h>

#include "machine/event.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineOutcomeError;

typedef enum {
    MACHINE_OUTCOME_RESOLUTION_NONE = 0,
    MACHINE_OUTCOME_RESOLUTION_EXACT_OUTCOME,
    MACHINE_OUTCOME_RESOLUTION_PREVIEW_OUTCOME,
    MACHINE_OUTCOME_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_OUTCOME_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineOutcomeResolutionKind;

typedef enum {
    MACHINE_OUTCOME_KIND_NONE = 0,
    MACHINE_OUTCOME_KIND_PROGRAM_STOPPED,
    MACHINE_OUTCOME_KIND_VALUE_AVAILABLE,
    MACHINE_OUTCOME_KIND_LOCAL_UPDATED,
    MACHINE_OUTCOME_KIND_GLOBAL_UPDATED,
    MACHINE_OUTCOME_KIND_CALL_ISSUED,
    MACHINE_OUTCOME_KIND_BLOCKED_CONTROL,
    MACHINE_OUTCOME_KIND_BLOCKED_UNSUPPORTED
} MachineOutcomeKind;

typedef struct {
    MachineOutcomeResolutionKind resolution_kind;
    MachineOutcomeKind outcome_kind;
    MachineEventResolutionKind event_resolution_kind;
    MachineEventKind event_kind;
    MachineTraceResolutionKind trace_resolution_kind;
    MachineTraceKind trace_kind;
    MachineTraceChangeClass trace_change_class;
    MachineDeltaResolutionKind delta_resolution_kind;
    MachineDeltaKind delta_kind;
    MachineObserveResolutionKind observe_resolution_kind;
    MachineObserveKind observe_kind;
    MachineApplyResolutionKind apply_resolution_kind;
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
    int is_exact_outcome;
    MachineStepStatus origin_status;
    MachineStepStatus observed_status;
    int has_observed_state;
    int has_status_change;
    int status_changed;
    int has_program_counter_change;
    int program_counter_changed;
    int has_stack_pointer_change;
    int stack_pointer_changed;
    int has_fetch_change;
    int fetch_changed;
    int has_primary_target_block;
    size_t primary_target_block_index;
    int has_secondary_target_block;
    size_t secondary_target_block_index;
    int has_return_immediate;
    size_t return_immediate;
} MachineOutcomeSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineEventResolutionKind event_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineOutcomeHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_outcome;
    int surfaces_preview_outcome;
    int classifies_outcome_family;
} MachineOutcomeTargetPolicySummary;

typedef struct {
    MachineEventFile event_file;
    MachineOutcomeResolutionKind resolution_kind;
    MachineOutcomeKind outcome_kind;
} MachineOutcomeFile;

typedef struct {
    MachineOutcomeFile file;
    MachineOutcomeHeaderSummary header_summary;
    MachineOutcomeTargetPolicySummary target_policy_summary;
    MachineEventReport event_report;
    MachineOutcomeSummary outcome_summary;
} MachineOutcomeReport;

typedef struct {
    const MachineOutcomeReport *report;
    const MachineOutcomeHeaderSummary *header_summary;
    const MachineOutcomeTargetPolicySummary *target_policy_summary;
    const MachineEventReport *event_report;
    const MachineOutcomeSummary *outcome_summary;
} MachineOutcomeReportOverviewArtifact;

void machine_outcome_file_init(MachineOutcomeFile *outcome_file);
void machine_outcome_file_free(MachineOutcomeFile *outcome_file);
void machine_outcome_report_init(MachineOutcomeReport *report);
void machine_outcome_report_free(MachineOutcomeReport *report);
int machine_outcome_clone_file(const MachineOutcomeFile *source,
    MachineOutcomeFile *out_clone,
    MachineOutcomeError *error);

const char *machine_outcome_resolution_kind_name(MachineOutcomeResolutionKind resolution_kind);
const char *machine_outcome_kind_name(MachineOutcomeKind outcome_kind);
int machine_outcome_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineOutcomeTargetPolicySummary *out_summary);
int machine_outcome_file_get_target_policy_summary(const MachineOutcomeFile *outcome_file,
    MachineOutcomeTargetPolicySummary *out_summary);
int machine_outcome_file_get_summary(const MachineOutcomeFile *outcome_file,
    size_t *out_mapped_byte_count);
int machine_outcome_file_get_source_elf_artifact_summary(const MachineOutcomeFile *outcome_file,
    MachineElfArtifactSummary *out_summary);
int machine_outcome_file_get_header_summary(const MachineOutcomeFile *outcome_file,
    MachineOutcomeHeaderSummary *out_summary);
int machine_outcome_file_get_event_summary(const MachineOutcomeFile *outcome_file,
    MachineEventSummary *out_summary);
int machine_outcome_file_get_outcome_summary(const MachineOutcomeFile *outcome_file,
    MachineOutcomeSummary *out_summary);

int machine_outcome_build_from_machine_event_file(const MachineEventFile *event_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_event_report(const MachineEventReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_step_report(const MachineStepReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);
int machine_outcome_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineOutcomeFile *out_outcome_file,
    MachineOutcomeError *error);

int machine_outcome_build_report_from_file(const MachineOutcomeFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_event_file(const MachineEventFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_event_report(const MachineEventReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_trace_file(const MachineTraceFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_trace_report(const MachineTraceReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineOutcomeReport *out_report,
    MachineOutcomeError *error);
int machine_outcome_report_refresh(MachineOutcomeReport *report,
    MachineOutcomeError *error);

int machine_outcome_verify_file(const MachineOutcomeFile *outcome_file,
    MachineOutcomeError *error);
int machine_outcome_dump_file(const MachineOutcomeFile *outcome_file,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineOutcomeError *error);

int machine_outcome_report_get_summary(const MachineOutcomeReport *report,
    size_t *out_mapped_byte_count);
int machine_outcome_report_get_overview_artifact(const MachineOutcomeReport *report,
    MachineOutcomeReportOverviewArtifact *out_artifact);
int machine_outcome_report_get_file(const MachineOutcomeReport *report,
    const MachineOutcomeFile **out_file);
int machine_outcome_report_get_event_file(const MachineOutcomeReport *report,
    const MachineEventFile **out_event_file);
int machine_outcome_report_get_event_report(const MachineOutcomeReport *report,
    const MachineEventReport **out_event_report);
int machine_outcome_report_get_source_elf_artifact_summary_artifact(
    const MachineOutcomeReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_outcome_report_get_header_summary_artifact(const MachineOutcomeReport *report,
    const MachineOutcomeHeaderSummary **out_summary);
int machine_outcome_report_get_target_policy_summary_artifact(const MachineOutcomeReport *report,
    const MachineOutcomeTargetPolicySummary **out_summary);
int machine_outcome_report_get_outcome_summary_artifact(const MachineOutcomeReport *report,
    const MachineOutcomeSummary **out_summary);
int machine_outcome_report_overview_artifact_get_event_report(
    const MachineOutcomeReportOverviewArtifact *artifact,
    const MachineEventReport **out_event_report);
int machine_outcome_report_overview_artifact_get_outcome_summary_artifact(
    const MachineOutcomeReportOverviewArtifact *artifact,
    const MachineOutcomeSummary **out_summary);

int machine_outcome_dump_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineOutcomeError *error);
int machine_outcome_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineOutcomeError *error);

#endif
