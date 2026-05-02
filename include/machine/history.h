#ifndef MACHINE_HISTORY_H
#define MACHINE_HISTORY_H

#include <stddef.h>

#include "machine/outcome.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineHistoryError;

typedef enum {
    MACHINE_HISTORY_RESOLUTION_NONE = 0,
    MACHINE_HISTORY_RESOLUTION_EXACT_HISTORY,
    MACHINE_HISTORY_RESOLUTION_PREVIEW_HISTORY,
    MACHINE_HISTORY_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_HISTORY_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineHistoryResolutionKind;

typedef enum {
    MACHINE_HISTORY_KIND_NONE = 0,
    MACHINE_HISTORY_KIND_PROGRAM_STOP_HISTORY,
    MACHINE_HISTORY_KIND_VALUE_HISTORY,
    MACHINE_HISTORY_KIND_LOCAL_UPDATE_HISTORY,
    MACHINE_HISTORY_KIND_GLOBAL_UPDATE_HISTORY,
    MACHINE_HISTORY_KIND_CALL_HISTORY,
    MACHINE_HISTORY_KIND_BLOCKED_CONTROL_HISTORY,
    MACHINE_HISTORY_KIND_BLOCKED_UNSUPPORTED_HISTORY
} MachineHistoryKind;

typedef struct {
    MachineHistoryResolutionKind resolution_kind;
    MachineHistoryKind history_kind;
    MachineOutcomeResolutionKind outcome_resolution_kind;
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
    int is_exact_history;
    int is_single_entry_history;
    size_t history_entry_count;
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
} MachineHistorySummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineOutcomeResolutionKind outcome_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
    size_t history_entry_count;
} MachineHistoryHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_history;
    int surfaces_preview_history;
    int surfaces_single_entry_history;
} MachineHistoryTargetPolicySummary;

typedef struct {
    MachineOutcomeFile outcome_file;
    MachineHistoryResolutionKind resolution_kind;
    MachineHistoryKind history_kind;
    size_t history_entry_count;
} MachineHistoryFile;

typedef struct {
    MachineHistoryFile file;
    MachineHistoryHeaderSummary header_summary;
    MachineHistoryTargetPolicySummary target_policy_summary;
    MachineOutcomeReport outcome_report;
    MachineHistorySummary history_summary;
} MachineHistoryReport;

typedef struct {
    const MachineHistoryReport *report;
    const MachineHistoryHeaderSummary *header_summary;
    const MachineHistoryTargetPolicySummary *target_policy_summary;
    const MachineOutcomeReport *outcome_report;
    const MachineHistorySummary *history_summary;
} MachineHistoryReportOverviewArtifact;

void machine_history_file_init(MachineHistoryFile *history_file);
void machine_history_file_free(MachineHistoryFile *history_file);
void machine_history_report_init(MachineHistoryReport *report);
void machine_history_report_free(MachineHistoryReport *report);
int machine_history_clone_file(const MachineHistoryFile *source,
    MachineHistoryFile *out_clone,
    MachineHistoryError *error);

const char *machine_history_resolution_kind_name(MachineHistoryResolutionKind resolution_kind);
const char *machine_history_kind_name(MachineHistoryKind history_kind);
int machine_history_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineHistoryTargetPolicySummary *out_summary);
int machine_history_file_get_target_policy_summary(const MachineHistoryFile *history_file,
    MachineHistoryTargetPolicySummary *out_summary);
int machine_history_file_get_summary(const MachineHistoryFile *history_file,
    size_t *out_mapped_byte_count);
int machine_history_file_get_header_summary(const MachineHistoryFile *history_file,
    MachineHistoryHeaderSummary *out_summary);
int machine_history_file_get_outcome_summary(const MachineHistoryFile *history_file,
    MachineOutcomeSummary *out_summary);
int machine_history_file_get_history_summary(const MachineHistoryFile *history_file,
    MachineHistorySummary *out_summary);

int machine_history_build_from_machine_outcome_file(const MachineOutcomeFile *outcome_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_event_file(const MachineEventFile *event_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_event_report(const MachineEventReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_step_report(const MachineStepReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);
int machine_history_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineHistoryFile *out_history_file,
    MachineHistoryError *error);

int machine_history_build_report_from_file(const MachineHistoryFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_outcome_file(const MachineOutcomeFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_event_file(const MachineEventFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_event_report(const MachineEventReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_trace_file(const MachineTraceFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_trace_report(const MachineTraceReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineHistoryReport *out_report,
    MachineHistoryError *error);
int machine_history_report_refresh(MachineHistoryReport *report,
    MachineHistoryError *error);

int machine_history_verify_file(const MachineHistoryFile *history_file,
    MachineHistoryError *error);
int machine_history_dump_file(const MachineHistoryFile *history_file,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_file(const MachineHistoryFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineHistoryError *error);

int machine_history_report_get_summary(const MachineHistoryReport *report,
    size_t *out_mapped_byte_count);
int machine_history_report_get_overview_artifact(const MachineHistoryReport *report,
    MachineHistoryReportOverviewArtifact *out_artifact);
int machine_history_report_get_file(const MachineHistoryReport *report,
    const MachineHistoryFile **out_file);
int machine_history_report_get_outcome_file(const MachineHistoryReport *report,
    const MachineOutcomeFile **out_outcome_file);
int machine_history_report_get_outcome_report(const MachineHistoryReport *report,
    const MachineOutcomeReport **out_outcome_report);
int machine_history_report_get_header_summary_artifact(const MachineHistoryReport *report,
    const MachineHistoryHeaderSummary **out_summary);
int machine_history_report_get_target_policy_summary_artifact(const MachineHistoryReport *report,
    const MachineHistoryTargetPolicySummary **out_summary);
int machine_history_report_get_history_summary_artifact(const MachineHistoryReport *report,
    const MachineHistorySummary **out_summary);
int machine_history_report_overview_artifact_get_outcome_report(
    const MachineHistoryReportOverviewArtifact *artifact,
    const MachineOutcomeReport **out_outcome_report);
int machine_history_report_overview_artifact_get_history_summary_artifact(
    const MachineHistoryReportOverviewArtifact *artifact,
    const MachineHistorySummary **out_summary);

int machine_history_dump_report(const MachineHistoryReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_file(const MachineHistoryFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineHistoryError *error);
int machine_history_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineHistoryError *error);

#endif
