#ifndef MACHINE_EVENT_H
#define MACHINE_EVENT_H

#include <stddef.h>

#include "machine/trace.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineEventError;

typedef enum {
    MACHINE_EVENT_RESOLUTION_NONE = 0,
    MACHINE_EVENT_RESOLUTION_EXACT_EVENT,
    MACHINE_EVENT_RESOLUTION_PREVIEW_EVENT,
    MACHINE_EVENT_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_EVENT_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineEventResolutionKind;

typedef enum {
    MACHINE_EVENT_KIND_NONE = 0,
    MACHINE_EVENT_KIND_CONTROL_HALT,
    MACHINE_EVENT_KIND_REGISTER_RESULT,
    MACHINE_EVENT_KIND_LOCAL_STORE,
    MACHINE_EVENT_KIND_GLOBAL_STORE,
    MACHINE_EVENT_KIND_CALL_EFFECT,
    MACHINE_EVENT_KIND_BLOCKED_CONTROL,
    MACHINE_EVENT_KIND_BLOCKED_UNSUPPORTED
} MachineEventKind;

typedef struct {
    MachineEventResolutionKind resolution_kind;
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
    int is_exact_event;
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
} MachineEventSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineTraceResolutionKind trace_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineEventHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_event;
    int surfaces_preview_event;
    int classifies_event_family;
} MachineEventTargetPolicySummary;

typedef struct {
    MachineTraceFile trace_file;
    MachineEventResolutionKind resolution_kind;
    MachineEventKind event_kind;
} MachineEventFile;

typedef struct {
    MachineEventFile file;
    MachineEventHeaderSummary header_summary;
    MachineEventTargetPolicySummary target_policy_summary;
    MachineTraceReport trace_report;
    MachineEventSummary event_summary;
} MachineEventReport;

typedef struct {
    const MachineEventReport *report;
    const MachineEventHeaderSummary *header_summary;
    const MachineEventTargetPolicySummary *target_policy_summary;
    const MachineTraceReport *trace_report;
    const MachineEventSummary *event_summary;
} MachineEventReportOverviewArtifact;

void machine_event_file_init(MachineEventFile *event_file);
void machine_event_file_free(MachineEventFile *event_file);
void machine_event_report_init(MachineEventReport *report);
void machine_event_report_free(MachineEventReport *report);
int machine_event_clone_file(const MachineEventFile *source,
    MachineEventFile *out_clone,
    MachineEventError *error);

const char *machine_event_resolution_kind_name(MachineEventResolutionKind resolution_kind);
const char *machine_event_kind_name(MachineEventKind event_kind);
int machine_event_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineEventTargetPolicySummary *out_summary);
int machine_event_file_get_target_policy_summary(const MachineEventFile *event_file,
    MachineEventTargetPolicySummary *out_summary);
int machine_event_file_get_summary(const MachineEventFile *event_file,
    size_t *out_mapped_byte_count);
int machine_event_file_get_source_elf_artifact_summary(const MachineEventFile *event_file,
    MachineElfArtifactSummary *out_summary);
int machine_event_file_get_header_summary(const MachineEventFile *event_file,
    MachineEventHeaderSummary *out_summary);
int machine_event_file_get_trace_summary(const MachineEventFile *event_file,
    MachineTraceSummary *out_summary);
int machine_event_file_get_event_summary(const MachineEventFile *event_file,
    MachineEventSummary *out_summary);

int machine_event_build_from_machine_trace_file(const MachineTraceFile *trace_file,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_trace_report(const MachineTraceReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_step_report(const MachineStepReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEventFile *out_event_file,
    MachineEventError *error);
int machine_event_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineEventFile *out_event_file,
    MachineEventError *error);

int machine_event_build_report_from_file(const MachineEventFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_trace_file(const MachineTraceFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_trace_report(const MachineTraceReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineEventReport *out_report,
    MachineEventError *error);
int machine_event_report_refresh(MachineEventReport *report,
    MachineEventError *error);

int machine_event_verify_file(const MachineEventFile *event_file,
    MachineEventError *error);
int machine_event_dump_file(const MachineEventFile *event_file,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_file(const MachineEventFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineEventError *error);

int machine_event_report_get_summary(const MachineEventReport *report,
    size_t *out_mapped_byte_count);
int machine_event_report_get_overview_artifact(const MachineEventReport *report,
    MachineEventReportOverviewArtifact *out_artifact);
int machine_event_report_get_file(const MachineEventReport *report,
    const MachineEventFile **out_file);
int machine_event_report_get_trace_file(const MachineEventReport *report,
    const MachineTraceFile **out_trace_file);
int machine_event_report_get_trace_report(const MachineEventReport *report,
    const MachineTraceReport **out_trace_report);
int machine_event_report_get_source_elf_artifact_summary_artifact(
    const MachineEventReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_event_report_get_header_summary_artifact(const MachineEventReport *report,
    const MachineEventHeaderSummary **out_summary);
int machine_event_report_get_target_policy_summary_artifact(const MachineEventReport *report,
    const MachineEventTargetPolicySummary **out_summary);
int machine_event_report_get_event_summary_artifact(const MachineEventReport *report,
    const MachineEventSummary **out_summary);
int machine_event_report_overview_artifact_get_trace_report(
    const MachineEventReportOverviewArtifact *artifact,
    const MachineTraceReport **out_trace_report);
int machine_event_report_overview_artifact_get_event_summary_artifact(
    const MachineEventReportOverviewArtifact *artifact,
    const MachineEventSummary **out_summary);

int machine_event_dump_report(const MachineEventReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_file(const MachineEventFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_trace_file(const MachineTraceFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_trace_report(const MachineTraceReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEventError *error);
int machine_event_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineEventError *error);

#endif
