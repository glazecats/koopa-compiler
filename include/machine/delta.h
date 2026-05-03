#ifndef MACHINE_DELTA_H
#define MACHINE_DELTA_H

#include <stddef.h>

#include "machine/observe.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineDeltaError;

typedef enum {
    MACHINE_DELTA_RESOLUTION_NONE = 0,
    MACHINE_DELTA_RESOLUTION_EXACT_STATE_DELTA,
    MACHINE_DELTA_RESOLUTION_PREVIEW_STATE_DELTA,
    MACHINE_DELTA_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_DELTA_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineDeltaResolutionKind;

typedef enum {
    MACHINE_DELTA_KIND_NONE = 0,
    MACHINE_DELTA_KIND_STATE
} MachineDeltaKind;

typedef struct {
    MachineDeltaResolutionKind resolution_kind;
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
    int is_exact_delta;
    MachineStepStatus origin_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    int has_origin_fetch;
    size_t origin_segment_index;
    unsigned char origin_byte;
    size_t origin_byte_memory_offset;
    int has_observed_state;
    MachineStepStatus observed_status;
    size_t observed_program_counter;
    size_t observed_stack_pointer;
    int has_observed_fetch;
    size_t observed_segment_index;
    unsigned char observed_byte;
    size_t observed_byte_memory_offset;
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
} MachineDeltaSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineObserveResolutionKind observe_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineDeltaHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_delta;
    int surfaces_preview_delta;
    int surfaces_change_flags;
} MachineDeltaTargetPolicySummary;

typedef struct {
    MachineObserveFile observe_file;
    MachineDeltaResolutionKind resolution_kind;
    MachineDeltaKind delta_kind;
    int has_observed_state;
    MachineStepStatus observed_status;
    size_t observed_program_counter;
    size_t observed_stack_pointer;
    int has_observed_fetch;
    size_t observed_segment_index;
    unsigned char observed_byte;
    size_t observed_byte_memory_offset;
} MachineDeltaFile;

typedef struct {
    MachineDeltaFile file;
    MachineDeltaHeaderSummary header_summary;
    MachineDeltaTargetPolicySummary target_policy_summary;
    MachineObserveReport observe_report;
    MachineDeltaSummary delta_summary;
} MachineDeltaReport;

typedef struct {
    const MachineDeltaReport *report;
    const MachineDeltaHeaderSummary *header_summary;
    const MachineDeltaTargetPolicySummary *target_policy_summary;
    const MachineObserveReport *observe_report;
    const MachineDeltaSummary *delta_summary;
} MachineDeltaReportOverviewArtifact;

void machine_delta_file_init(MachineDeltaFile *delta_file);
void machine_delta_file_free(MachineDeltaFile *delta_file);
void machine_delta_report_init(MachineDeltaReport *report);
void machine_delta_report_free(MachineDeltaReport *report);
int machine_delta_clone_file(const MachineDeltaFile *source,
    MachineDeltaFile *out_clone,
    MachineDeltaError *error);

const char *machine_delta_resolution_kind_name(MachineDeltaResolutionKind resolution_kind);
const char *machine_delta_kind_name(MachineDeltaKind delta_kind);
int machine_delta_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineDeltaTargetPolicySummary *out_summary);
int machine_delta_file_get_target_policy_summary(const MachineDeltaFile *delta_file,
    MachineDeltaTargetPolicySummary *out_summary);
int machine_delta_file_get_summary(const MachineDeltaFile *delta_file,
    size_t *out_mapped_byte_count);
int machine_delta_file_get_source_elf_artifact_summary(const MachineDeltaFile *delta_file,
    MachineElfArtifactSummary *out_summary);
int machine_delta_file_get_header_summary(const MachineDeltaFile *delta_file,
    MachineDeltaHeaderSummary *out_summary);
int machine_delta_file_get_observe_summary(const MachineDeltaFile *delta_file,
    MachineObserveSummary *out_summary);
int machine_delta_file_get_delta_summary(const MachineDeltaFile *delta_file,
    MachineDeltaSummary *out_summary);

int machine_delta_build_from_machine_observe_file(const MachineObserveFile *observe_file,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_observe_report(const MachineObserveReport *report,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_step_report(const MachineStepReport *report,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);
int machine_delta_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDeltaFile *out_delta_file,
    MachineDeltaError *error);

int machine_delta_build_report_from_file(const MachineDeltaFile *source,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_observe_file(const MachineObserveFile *source,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_observe_report(const MachineObserveReport *report,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineDeltaReport *out_report,
    MachineDeltaError *error);
int machine_delta_report_refresh(MachineDeltaReport *report,
    MachineDeltaError *error);

int machine_delta_verify_file(const MachineDeltaFile *delta_file,
    MachineDeltaError *error);
int machine_delta_dump_file(const MachineDeltaFile *delta_file,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_file(const MachineDeltaFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDeltaError *error);

int machine_delta_report_get_summary(const MachineDeltaReport *report,
    size_t *out_mapped_byte_count);
int machine_delta_report_get_overview_artifact(const MachineDeltaReport *report,
    MachineDeltaReportOverviewArtifact *out_artifact);
int machine_delta_report_get_file(const MachineDeltaReport *report,
    const MachineDeltaFile **out_file);
int machine_delta_report_get_observe_file(const MachineDeltaReport *report,
    const MachineObserveFile **out_observe_file);
int machine_delta_report_get_observe_report(const MachineDeltaReport *report,
    const MachineObserveReport **out_observe_report);
int machine_delta_report_get_source_elf_artifact_summary_artifact(
    const MachineDeltaReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_delta_report_get_header_summary_artifact(const MachineDeltaReport *report,
    const MachineDeltaHeaderSummary **out_summary);
int machine_delta_report_get_target_policy_summary_artifact(const MachineDeltaReport *report,
    const MachineDeltaTargetPolicySummary **out_summary);
int machine_delta_report_get_delta_summary_artifact(const MachineDeltaReport *report,
    const MachineDeltaSummary **out_summary);
int machine_delta_report_overview_artifact_get_observe_report(
    const MachineDeltaReportOverviewArtifact *artifact,
    const MachineObserveReport **out_observe_report);
int machine_delta_report_overview_artifact_get_delta_summary_artifact(
    const MachineDeltaReportOverviewArtifact *artifact,
    const MachineDeltaSummary **out_summary);

int machine_delta_dump_report(const MachineDeltaReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_file(const MachineDeltaFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_observe_file(const MachineObserveFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_observe_report(const MachineObserveReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineDeltaError *error);
int machine_delta_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineDeltaError *error);

#endif
