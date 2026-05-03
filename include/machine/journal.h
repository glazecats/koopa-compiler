#ifndef MACHINE_JOURNAL_H
#define MACHINE_JOURNAL_H

#include <stddef.h>

#include "machine/log.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineJournalError;

typedef enum {
    MACHINE_JOURNAL_RESOLUTION_NONE = 0,
    MACHINE_JOURNAL_RESOLUTION_EXACT_JOURNAL,
    MACHINE_JOURNAL_RESOLUTION_PREVIEW_JOURNAL,
    MACHINE_JOURNAL_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_JOURNAL_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineJournalResolutionKind;

typedef enum {
    MACHINE_JOURNAL_KIND_NONE = 0,
    MACHINE_JOURNAL_KIND_PROGRAM_STOP_JOURNAL,
    MACHINE_JOURNAL_KIND_VALUE_JOURNAL,
    MACHINE_JOURNAL_KIND_LOCAL_UPDATE_JOURNAL,
    MACHINE_JOURNAL_KIND_GLOBAL_UPDATE_JOURNAL,
    MACHINE_JOURNAL_KIND_CALL_JOURNAL,
    MACHINE_JOURNAL_KIND_BLOCKED_CONTROL_JOURNAL,
    MACHINE_JOURNAL_KIND_BLOCKED_UNSUPPORTED_JOURNAL
} MachineJournalKind;

typedef struct {
    MachineJournalResolutionKind resolution_kind;
    MachineJournalKind journal_kind;
    MachineLogResolutionKind log_resolution_kind;
    MachineLogKind log_kind;
    int is_exact_journal;
    int is_single_record_journal;
    size_t journal_record_count;
    size_t journal_record_index;
    MachineLogSummary log_summary;
} MachineJournalSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineLogResolutionKind log_resolution_kind;
    MachineTimelineResolutionKind timeline_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
    size_t journal_record_count;
    size_t journal_record_index;
} MachineJournalHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_journal;
    int surfaces_preview_journal;
    int surfaces_single_record_journal;
} MachineJournalTargetPolicySummary;

typedef struct {
    MachineLogFile log_file;
    MachineJournalResolutionKind resolution_kind;
    MachineJournalKind journal_kind;
    size_t journal_record_count;
    size_t journal_record_index;
} MachineJournalFile;

typedef struct {
    MachineJournalFile file;
    MachineJournalHeaderSummary header_summary;
    MachineJournalTargetPolicySummary target_policy_summary;
    MachineLogReport log_report;
    MachineJournalSummary journal_summary;
} MachineJournalReport;

typedef struct {
    const MachineJournalReport *report;
    const MachineJournalHeaderSummary *header_summary;
    const MachineJournalTargetPolicySummary *target_policy_summary;
    const MachineLogReport *log_report;
    const MachineJournalSummary *journal_summary;
} MachineJournalReportOverviewArtifact;

void machine_journal_file_init(MachineJournalFile *journal_file);
void machine_journal_file_free(MachineJournalFile *journal_file);
void machine_journal_report_init(MachineJournalReport *report);
void machine_journal_report_free(MachineJournalReport *report);
int machine_journal_clone_file(const MachineJournalFile *source,
    MachineJournalFile *out_clone,
    MachineJournalError *error);

const char *machine_journal_resolution_kind_name(
    MachineJournalResolutionKind resolution_kind);
const char *machine_journal_kind_name(MachineJournalKind journal_kind);
int machine_journal_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineJournalTargetPolicySummary *out_summary);
int machine_journal_file_get_target_policy_summary(
    const MachineJournalFile *journal_file,
    MachineJournalTargetPolicySummary *out_summary);
int machine_journal_file_get_summary(const MachineJournalFile *journal_file,
    size_t *out_mapped_byte_count);
int machine_journal_file_get_source_elf_artifact_summary(
    const MachineJournalFile *journal_file,
    MachineElfArtifactSummary *out_summary);
int machine_journal_file_get_header_summary(const MachineJournalFile *journal_file,
    MachineJournalHeaderSummary *out_summary);
int machine_journal_file_get_log_summary(const MachineJournalFile *journal_file,
    MachineLogSummary *out_summary);
int machine_journal_file_get_journal_summary(const MachineJournalFile *journal_file,
    MachineJournalSummary *out_summary);

int machine_journal_build_from_machine_log_file(const MachineLogFile *log_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_log_report(const MachineLogReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_timeline_file(const MachineTimelineFile *timeline_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_timeline_report(const MachineTimelineReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_history_file(const MachineHistoryFile *history_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_history_report(const MachineHistoryReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_outcome_file(const MachineOutcomeFile *outcome_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_event_file(const MachineEventFile *event_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_event_report(const MachineEventReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_step_report(const MachineStepReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);
int machine_journal_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineJournalFile *out_journal_file,
    MachineJournalError *error);

int machine_journal_build_report_from_file(const MachineJournalFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_log_file(const MachineLogFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_log_report(const MachineLogReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_timeline_file(const MachineTimelineFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_timeline_report(const MachineTimelineReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_history_file(const MachineHistoryFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_history_report(const MachineHistoryReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_outcome_file(const MachineOutcomeFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_outcome_report(const MachineOutcomeReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_event_file(const MachineEventFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_event_report(const MachineEventReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_trace_file(const MachineTraceFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_trace_report(const MachineTraceReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineJournalReport *out_report,
    MachineJournalError *error);
int machine_journal_report_refresh(MachineJournalReport *report,
    MachineJournalError *error);

int machine_journal_verify_file(const MachineJournalFile *journal_file,
    MachineJournalError *error);
int machine_journal_dump_file(const MachineJournalFile *journal_file,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_file(const MachineJournalFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_log_file(const MachineLogFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_log_report(const MachineLogReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_timeline_file(const MachineTimelineFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_timeline_report(const MachineTimelineReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_history_file(const MachineHistoryFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_history_report(const MachineHistoryReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineJournalError *error);

int machine_journal_report_get_summary(const MachineJournalReport *report,
    size_t *out_mapped_byte_count);
int machine_journal_report_get_overview_artifact(const MachineJournalReport *report,
    MachineJournalReportOverviewArtifact *out_artifact);
int machine_journal_report_get_file(const MachineJournalReport *report,
    const MachineJournalFile **out_file);
int machine_journal_report_get_log_file(const MachineJournalReport *report,
    const MachineLogFile **out_log_file);
int machine_journal_report_get_log_report(const MachineJournalReport *report,
    const MachineLogReport **out_log_report);
int machine_journal_report_get_source_elf_artifact_summary_artifact(
    const MachineJournalReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_journal_report_get_header_summary_artifact(const MachineJournalReport *report,
    const MachineJournalHeaderSummary **out_summary);
int machine_journal_report_get_target_policy_summary_artifact(
    const MachineJournalReport *report,
    const MachineJournalTargetPolicySummary **out_summary);
int machine_journal_report_get_journal_summary_artifact(const MachineJournalReport *report,
    const MachineJournalSummary **out_summary);
int machine_journal_report_overview_artifact_get_log_report(
    const MachineJournalReportOverviewArtifact *artifact,
    const MachineLogReport **out_log_report);
int machine_journal_report_overview_artifact_get_journal_summary_artifact(
    const MachineJournalReportOverviewArtifact *artifact,
    const MachineJournalSummary **out_summary);

int machine_journal_dump_report(const MachineJournalReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_file(const MachineJournalFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_log_file(const MachineLogFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_log_report(const MachineLogReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_timeline_file(const MachineTimelineFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_timeline_report(const MachineTimelineReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_history_file(const MachineHistoryFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_history_report(const MachineHistoryReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_outcome_file(const MachineOutcomeFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_outcome_report(const MachineOutcomeReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_event_file(const MachineEventFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_event_report(const MachineEventReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_ir_report(
    const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineJournalError *error);
int machine_journal_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineJournalError *error);

#endif
