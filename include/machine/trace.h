#ifndef MACHINE_TRACE_H
#define MACHINE_TRACE_H

#include <stddef.h>

#include "machine/delta.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineTraceError;

typedef enum {
    MACHINE_TRACE_RESOLUTION_NONE = 0,
    MACHINE_TRACE_RESOLUTION_EXACT_TRACE,
    MACHINE_TRACE_RESOLUTION_PREVIEW_TRACE,
    MACHINE_TRACE_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_TRACE_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineTraceResolutionKind;

typedef enum {
    MACHINE_TRACE_KIND_NONE = 0,
    MACHINE_TRACE_KIND_STATE_RECORD
} MachineTraceKind;

typedef enum {
    MACHINE_TRACE_CHANGE_CLASS_NONE = 0,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_ONLY,
    MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_ONLY,
    MACHINE_TRACE_CHANGE_CLASS_FETCH_ONLY,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_PROGRAM_COUNTER,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_FETCH,
    MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_AND_FETCH,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_AND_FETCH,
    MACHINE_TRACE_CHANGE_CLASS_STACK_ONLY,
    MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_AND_STACK,
    MACHINE_TRACE_CHANGE_CLASS_STACK_AND_FETCH,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_AND_STACK,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_AND_STACK,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_STACK_AND_FETCH,
    MACHINE_TRACE_CHANGE_CLASS_PROGRAM_COUNTER_STACK_AND_FETCH,
    MACHINE_TRACE_CHANGE_CLASS_STATUS_PROGRAM_COUNTER_STACK_AND_FETCH
} MachineTraceChangeClass;

typedef struct {
    MachineTraceResolutionKind resolution_kind;
    MachineTraceKind trace_kind;
    MachineTraceChangeClass change_class;
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
    int is_exact_trace;
    MachineStepStatus origin_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    int has_origin_fetch;
    size_t origin_segment_index;
    unsigned char origin_byte;
    int has_observed_state;
    MachineStepStatus observed_status;
    size_t observed_program_counter;
    size_t observed_stack_pointer;
    int has_observed_fetch;
    size_t observed_segment_index;
    unsigned char observed_byte;
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
} MachineTraceSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineDeltaResolutionKind delta_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineTraceHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_trace;
    int surfaces_preview_trace;
    int surfaces_change_class;
} MachineTraceTargetPolicySummary;

typedef struct {
    MachineDeltaFile delta_file;
    MachineTraceResolutionKind resolution_kind;
    MachineTraceKind trace_kind;
    MachineTraceChangeClass change_class;
} MachineTraceFile;

typedef struct {
    MachineTraceFile file;
    MachineTraceHeaderSummary header_summary;
    MachineTraceTargetPolicySummary target_policy_summary;
    MachineDeltaReport delta_report;
    MachineTraceSummary trace_summary;
} MachineTraceReport;

typedef struct {
    const MachineTraceReport *report;
    const MachineTraceHeaderSummary *header_summary;
    const MachineTraceTargetPolicySummary *target_policy_summary;
    const MachineDeltaReport *delta_report;
    const MachineTraceSummary *trace_summary;
} MachineTraceReportOverviewArtifact;

void machine_trace_file_init(MachineTraceFile *trace_file);
void machine_trace_file_free(MachineTraceFile *trace_file);
void machine_trace_report_init(MachineTraceReport *report);
void machine_trace_report_free(MachineTraceReport *report);
int machine_trace_clone_file(const MachineTraceFile *source,
    MachineTraceFile *out_clone,
    MachineTraceError *error);

const char *machine_trace_resolution_kind_name(MachineTraceResolutionKind resolution_kind);
const char *machine_trace_kind_name(MachineTraceKind trace_kind);
const char *machine_trace_change_class_name(MachineTraceChangeClass change_class);
int machine_trace_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineTraceTargetPolicySummary *out_summary);
int machine_trace_file_get_target_policy_summary(const MachineTraceFile *trace_file,
    MachineTraceTargetPolicySummary *out_summary);
int machine_trace_file_get_summary(const MachineTraceFile *trace_file,
    size_t *out_mapped_byte_count);
int machine_trace_file_get_header_summary(const MachineTraceFile *trace_file,
    MachineTraceHeaderSummary *out_summary);
int machine_trace_file_get_delta_summary(const MachineTraceFile *trace_file,
    MachineDeltaSummary *out_summary);
int machine_trace_file_get_trace_summary(const MachineTraceFile *trace_file,
    MachineTraceSummary *out_summary);

int machine_trace_build_from_machine_delta_file(const MachineDeltaFile *delta_file,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_delta_report(const MachineDeltaReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_step_report(const MachineStepReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);
int machine_trace_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTraceFile *out_trace_file,
    MachineTraceError *error);

int machine_trace_build_report_from_file(const MachineTraceFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_delta_file(const MachineDeltaFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_delta_report(const MachineDeltaReport *report,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineTraceReport *out_report,
    MachineTraceError *error);
int machine_trace_report_refresh(MachineTraceReport *report,
    MachineTraceError *error);

int machine_trace_verify_file(const MachineTraceFile *trace_file,
    MachineTraceError *error);
int machine_trace_dump_file(const MachineTraceFile *trace_file,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_file(const MachineTraceFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTraceError *error);

int machine_trace_report_get_summary(const MachineTraceReport *report,
    size_t *out_mapped_byte_count);
int machine_trace_report_get_overview_artifact(const MachineTraceReport *report,
    MachineTraceReportOverviewArtifact *out_artifact);
int machine_trace_report_get_file(const MachineTraceReport *report,
    const MachineTraceFile **out_file);
int machine_trace_report_get_delta_file(const MachineTraceReport *report,
    const MachineDeltaFile **out_delta_file);
int machine_trace_report_get_delta_report(const MachineTraceReport *report,
    const MachineDeltaReport **out_delta_report);
int machine_trace_report_get_header_summary_artifact(const MachineTraceReport *report,
    const MachineTraceHeaderSummary **out_summary);
int machine_trace_report_get_target_policy_summary_artifact(const MachineTraceReport *report,
    const MachineTraceTargetPolicySummary **out_summary);
int machine_trace_report_get_trace_summary_artifact(const MachineTraceReport *report,
    const MachineTraceSummary **out_summary);
int machine_trace_report_overview_artifact_get_delta_report(
    const MachineTraceReportOverviewArtifact *artifact,
    const MachineDeltaReport **out_delta_report);
int machine_trace_report_overview_artifact_get_trace_summary_artifact(
    const MachineTraceReportOverviewArtifact *artifact,
    const MachineTraceSummary **out_summary);

int machine_trace_dump_report(const MachineTraceReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_file(const MachineTraceFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_delta_file(const MachineDeltaFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_delta_report(const MachineDeltaReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineTraceError *error);
int machine_trace_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineTraceError *error);

#endif
