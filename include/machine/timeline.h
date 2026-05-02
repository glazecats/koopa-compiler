#ifndef MACHINE_TIMELINE_H
#define MACHINE_TIMELINE_H

#include <stddef.h>

#include "machine/history.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineTimelineError;

typedef enum {
    MACHINE_TIMELINE_RESOLUTION_NONE = 0,
    MACHINE_TIMELINE_RESOLUTION_EXACT_TIMELINE,
    MACHINE_TIMELINE_RESOLUTION_PREVIEW_TIMELINE,
    MACHINE_TIMELINE_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_TIMELINE_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineTimelineResolutionKind;

typedef enum {
    MACHINE_TIMELINE_KIND_NONE = 0,
    MACHINE_TIMELINE_KIND_PROGRAM_STOP_TIMELINE,
    MACHINE_TIMELINE_KIND_VALUE_TIMELINE,
    MACHINE_TIMELINE_KIND_LOCAL_UPDATE_TIMELINE,
    MACHINE_TIMELINE_KIND_GLOBAL_UPDATE_TIMELINE,
    MACHINE_TIMELINE_KIND_CALL_TIMELINE,
    MACHINE_TIMELINE_KIND_BLOCKED_CONTROL_TIMELINE,
    MACHINE_TIMELINE_KIND_BLOCKED_UNSUPPORTED_TIMELINE
} MachineTimelineKind;

typedef struct {
    MachineTimelineResolutionKind resolution_kind;
    MachineTimelineKind timeline_kind;
    MachineHistoryResolutionKind history_resolution_kind;
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
    int is_exact_timeline;
    int is_single_tick_timeline;
    size_t timeline_entry_count;
    size_t timeline_entry_index;
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
} MachineTimelineSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineHistoryResolutionKind history_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
    size_t timeline_entry_count;
    size_t timeline_entry_index;
} MachineTimelineHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_timeline;
    int surfaces_preview_timeline;
    int surfaces_single_tick_timeline;
} MachineTimelineTargetPolicySummary;

typedef struct {
    MachineHistoryFile history_file;
    MachineTimelineResolutionKind resolution_kind;
    MachineTimelineKind timeline_kind;
    size_t timeline_entry_count;
    size_t timeline_entry_index;
} MachineTimelineFile;

typedef struct {
    MachineTimelineFile file;
    MachineTimelineHeaderSummary header_summary;
    MachineTimelineTargetPolicySummary target_policy_summary;
    MachineHistoryReport history_report;
    MachineTimelineSummary timeline_summary;
} MachineTimelineReport;

typedef struct {
    const MachineTimelineReport *report;
    const MachineTimelineHeaderSummary *header_summary;
    const MachineTimelineTargetPolicySummary *target_policy_summary;
    const MachineHistoryReport *history_report;
    const MachineTimelineSummary *timeline_summary;
} MachineTimelineReportOverviewArtifact;

void machine_timeline_file_init(MachineTimelineFile *timeline_file);
void machine_timeline_file_free(MachineTimelineFile *timeline_file);
void machine_timeline_report_init(MachineTimelineReport *report);
void machine_timeline_report_free(MachineTimelineReport *report);
int machine_timeline_clone_file(const MachineTimelineFile *source,
    MachineTimelineFile *out_clone,
    MachineTimelineError *error);

const char *machine_timeline_resolution_kind_name(MachineTimelineResolutionKind resolution_kind);
const char *machine_timeline_kind_name(MachineTimelineKind timeline_kind);
int machine_timeline_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineTimelineTargetPolicySummary *out_summary);
int machine_timeline_file_get_target_policy_summary(const MachineTimelineFile *timeline_file,
    MachineTimelineTargetPolicySummary *out_summary);
int machine_timeline_file_get_summary(const MachineTimelineFile *timeline_file,
    size_t *out_mapped_byte_count);
int machine_timeline_file_get_header_summary(const MachineTimelineFile *timeline_file,
    MachineTimelineHeaderSummary *out_summary);
int machine_timeline_file_get_history_summary(const MachineTimelineFile *timeline_file,
    MachineHistorySummary *out_summary);
int machine_timeline_file_get_timeline_summary(const MachineTimelineFile *timeline_file,
    MachineTimelineSummary *out_summary);

int machine_timeline_build_from_machine_history_file(const MachineHistoryFile *history_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_history_report(const MachineHistoryReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_outcome_file(const MachineOutcomeFile *outcome_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_event_file(const MachineEventFile *event_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_event_report(const MachineEventReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_step_report(const MachineStepReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);
int machine_timeline_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTimelineFile *out_timeline_file,
    MachineTimelineError *error);

int machine_timeline_build_report_from_file(const MachineTimelineFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_history_file(const MachineHistoryFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_history_report(const MachineHistoryReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_outcome_file(const MachineOutcomeFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_event_file(const MachineEventFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_event_report(const MachineEventReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_trace_file(const MachineTraceFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_trace_report(const MachineTraceReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTimelineReport *out_report,
    MachineTimelineError *error);
int machine_timeline_report_refresh(MachineTimelineReport *report,
    MachineTimelineError *error);

int machine_timeline_verify_file(const MachineTimelineFile *timeline_file,
    MachineTimelineError *error);
int machine_timeline_dump_file(const MachineTimelineFile *timeline_file,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_file(const MachineTimelineFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_history_file(const MachineHistoryFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_history_report(const MachineHistoryReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTimelineError *error);

int machine_timeline_report_get_summary(const MachineTimelineReport *report,
    size_t *out_mapped_byte_count);
int machine_timeline_report_get_overview_artifact(const MachineTimelineReport *report,
    MachineTimelineReportOverviewArtifact *out_artifact);
int machine_timeline_report_get_file(const MachineTimelineReport *report,
    const MachineTimelineFile **out_file);
int machine_timeline_report_get_history_file(const MachineTimelineReport *report,
    const MachineHistoryFile **out_history_file);
int machine_timeline_report_get_history_report(const MachineTimelineReport *report,
    const MachineHistoryReport **out_history_report);
int machine_timeline_report_get_header_summary_artifact(const MachineTimelineReport *report,
    const MachineTimelineHeaderSummary **out_summary);
int machine_timeline_report_get_target_policy_summary_artifact(const MachineTimelineReport *report,
    const MachineTimelineTargetPolicySummary **out_summary);
int machine_timeline_report_get_timeline_summary_artifact(const MachineTimelineReport *report,
    const MachineTimelineSummary **out_summary);
int machine_timeline_report_overview_artifact_get_history_report(
    const MachineTimelineReportOverviewArtifact *artifact,
    const MachineHistoryReport **out_history_report);
int machine_timeline_report_overview_artifact_get_timeline_summary_artifact(
    const MachineTimelineReportOverviewArtifact *artifact,
    const MachineTimelineSummary **out_summary);

int machine_timeline_dump_report(const MachineTimelineReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_file(const MachineTimelineFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_history_file(const MachineHistoryFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_history_report(const MachineHistoryReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineTimelineError *error);
int machine_timeline_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTimelineError *error);

#endif
