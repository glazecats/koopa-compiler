#ifndef MACHINE_LOG_H
#define MACHINE_LOG_H

#include <stddef.h>

#include "machine/timeline.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineLogError;

typedef enum {
    MACHINE_LOG_RESOLUTION_NONE = 0,
    MACHINE_LOG_RESOLUTION_EXACT_LOG,
    MACHINE_LOG_RESOLUTION_PREVIEW_LOG,
    MACHINE_LOG_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_LOG_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineLogResolutionKind;

typedef enum {
    MACHINE_LOG_KIND_NONE = 0,
    MACHINE_LOG_KIND_PROGRAM_STOP_LOG,
    MACHINE_LOG_KIND_VALUE_LOG,
    MACHINE_LOG_KIND_LOCAL_UPDATE_LOG,
    MACHINE_LOG_KIND_GLOBAL_UPDATE_LOG,
    MACHINE_LOG_KIND_CALL_LOG,
    MACHINE_LOG_KIND_BLOCKED_CONTROL_LOG,
    MACHINE_LOG_KIND_BLOCKED_UNSUPPORTED_LOG
} MachineLogKind;

typedef struct {
    MachineLogResolutionKind resolution_kind;
    MachineLogKind log_kind;
    MachineTimelineResolutionKind timeline_resolution_kind;
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
    int is_exact_log;
    int is_single_line_log;
    size_t log_line_count;
    size_t log_line_index;
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
} MachineLogSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineTimelineResolutionKind timeline_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
    size_t log_line_count;
    size_t log_line_index;
} MachineLogHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_log;
    int surfaces_preview_log;
    int surfaces_single_line_log;
} MachineLogTargetPolicySummary;

typedef struct {
    MachineTimelineFile timeline_file;
    MachineLogResolutionKind resolution_kind;
    MachineLogKind log_kind;
    size_t log_line_count;
    size_t log_line_index;
} MachineLogFile;

typedef struct {
    MachineLogFile file;
    MachineLogHeaderSummary header_summary;
    MachineLogTargetPolicySummary target_policy_summary;
    MachineTimelineReport timeline_report;
    MachineLogSummary log_summary;
} MachineLogReport;

typedef struct {
    const MachineLogReport *report;
    const MachineLogHeaderSummary *header_summary;
    const MachineLogTargetPolicySummary *target_policy_summary;
    const MachineTimelineReport *timeline_report;
    const MachineLogSummary *log_summary;
} MachineLogReportOverviewArtifact;

void machine_log_file_init(MachineLogFile *log_file);
void machine_log_file_free(MachineLogFile *log_file);
void machine_log_report_init(MachineLogReport *report);
void machine_log_report_free(MachineLogReport *report);
int machine_log_clone_file(const MachineLogFile *source,
    MachineLogFile *out_clone,
    MachineLogError *error);

const char *machine_log_resolution_kind_name(MachineLogResolutionKind resolution_kind);
const char *machine_log_kind_name(MachineLogKind log_kind);
int machine_log_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineLogTargetPolicySummary *out_summary);
int machine_log_file_get_target_policy_summary(const MachineLogFile *log_file,
    MachineLogTargetPolicySummary *out_summary);
int machine_log_file_get_summary(const MachineLogFile *log_file,
    size_t *out_mapped_byte_count);
int machine_log_file_get_source_elf_artifact_summary(const MachineLogFile *log_file,
    MachineElfArtifactSummary *out_summary);
int machine_log_file_get_header_summary(const MachineLogFile *log_file,
    MachineLogHeaderSummary *out_summary);
int machine_log_file_get_timeline_summary(const MachineLogFile *log_file,
    MachineTimelineSummary *out_summary);
int machine_log_file_get_log_summary(const MachineLogFile *log_file,
    MachineLogSummary *out_summary);

int machine_log_build_from_machine_timeline_file(const MachineTimelineFile *timeline_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_timeline_report(const MachineTimelineReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_history_file(const MachineHistoryFile *history_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_history_report(const MachineHistoryReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_outcome_file(const MachineOutcomeFile *outcome_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_event_file(const MachineEventFile *event_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_event_report(const MachineEventReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_step_report(const MachineStepReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLogFile *out_log_file,
    MachineLogError *error);
int machine_log_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLogFile *out_log_file,
    MachineLogError *error);

int machine_log_build_report_from_file(const MachineLogFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_timeline_file(const MachineTimelineFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_timeline_report(const MachineTimelineReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_history_file(const MachineHistoryFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_history_report(const MachineHistoryReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_outcome_file(const MachineOutcomeFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_event_file(const MachineEventFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_event_report(const MachineEventReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_trace_file(const MachineTraceFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_trace_report(const MachineTraceReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineLogReport *out_report,
    MachineLogError *error);
int machine_log_report_refresh(MachineLogReport *report,
    MachineLogError *error);

int machine_log_verify_file(const MachineLogFile *log_file,
    MachineLogError *error);
int machine_log_dump_file(const MachineLogFile *log_file,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_file(const MachineLogFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_timeline_file(const MachineTimelineFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_timeline_report(const MachineTimelineReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_history_file(const MachineHistoryFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_history_report(const MachineHistoryReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLogError *error);

int machine_log_report_get_summary(const MachineLogReport *report,
    size_t *out_mapped_byte_count);
int machine_log_report_get_overview_artifact(const MachineLogReport *report,
    MachineLogReportOverviewArtifact *out_artifact);
int machine_log_report_get_file(const MachineLogReport *report,
    const MachineLogFile **out_file);
int machine_log_report_get_timeline_file(const MachineLogReport *report,
    const MachineTimelineFile **out_timeline_file);
int machine_log_report_get_timeline_report(const MachineLogReport *report,
    const MachineTimelineReport **out_timeline_report);
int machine_log_report_get_source_elf_artifact_summary_artifact(
    const MachineLogReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_log_report_get_header_summary_artifact(const MachineLogReport *report,
    const MachineLogHeaderSummary **out_summary);
int machine_log_report_get_target_policy_summary_artifact(const MachineLogReport *report,
    const MachineLogTargetPolicySummary **out_summary);
int machine_log_report_get_log_summary_artifact(const MachineLogReport *report,
    const MachineLogSummary **out_summary);
int machine_log_report_overview_artifact_get_timeline_report(
    const MachineLogReportOverviewArtifact *artifact,
    const MachineTimelineReport **out_timeline_report);
int machine_log_report_overview_artifact_get_log_summary_artifact(
    const MachineLogReportOverviewArtifact *artifact,
    const MachineLogSummary **out_summary);

int machine_log_dump_report(const MachineLogReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_file(const MachineLogFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_timeline_file(const MachineTimelineFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_timeline_report(const MachineTimelineReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_history_file(const MachineHistoryFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_history_report(const MachineHistoryReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineLogError *error);
int machine_log_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineLogError *error);

#endif
