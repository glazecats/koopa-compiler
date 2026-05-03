#ifndef MACHINE_OBSERVE_H
#define MACHINE_OBSERVE_H

#include <stddef.h>

#include "machine/apply.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineObserveError;

typedef enum {
    MACHINE_OBSERVE_RESOLUTION_NONE = 0,
    MACHINE_OBSERVE_RESOLUTION_EXACT_STATE,
    MACHINE_OBSERVE_RESOLUTION_PREVIEW_STATE,
    MACHINE_OBSERVE_RESOLUTION_BLOCKED_ON_CONTROL,
    MACHINE_OBSERVE_RESOLUTION_BLOCKED_UNSUPPORTED
} MachineObserveResolutionKind;

typedef enum {
    MACHINE_OBSERVE_KIND_NONE = 0,
    MACHINE_OBSERVE_KIND_STATE
} MachineObserveKind;

typedef struct {
    size_t byte_virtual_address;
    size_t byte_memory_offset;
    size_t segment_index;
    const char *segment_name;
    unsigned char byte_value;
} MachineObserveCurrentFetchSummary;

typedef struct {
    MachineObserveResolutionKind resolution_kind;
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
    int is_exact_state;
    int has_observed_state;
    MachineStepStatus observed_status;
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
} MachineObserveSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    MachineApplyResolutionKind apply_resolution_kind;
    MachineStepStatus origin_step_status;
    size_t origin_program_counter;
    size_t origin_stack_pointer;
    size_t origin_current_segment_index;
    size_t mapped_byte_count;
} MachineObserveHeaderSummary;

typedef struct {
    MachineElfTargetProfile target_profile;
    int surfaces_exact_state;
    int surfaces_preview_state;
} MachineObserveTargetPolicySummary;

typedef struct {
    MachineApplyFile apply_file;
    MachineObserveResolutionKind resolution_kind;
    MachineObserveKind observe_kind;
    int has_observed_state;
    MachineStepStatus observed_status;
    size_t program_counter;
    size_t stack_pointer;
    int has_current_fetch;
    size_t current_segment_index;
    unsigned char current_byte;
    size_t current_byte_memory_offset;
} MachineObserveFile;

typedef struct {
    MachineObserveFile file;
    MachineObserveHeaderSummary header_summary;
    MachineObserveTargetPolicySummary target_policy_summary;
    MachineApplyReport apply_report;
    MachineObserveSummary observe_summary;
    MachineObserveCurrentFetchSummary current_fetch_summary;
} MachineObserveReport;

typedef struct {
    const MachineObserveReport *report;
    const MachineObserveHeaderSummary *header_summary;
    const MachineObserveTargetPolicySummary *target_policy_summary;
    const MachineApplyReport *apply_report;
    const MachineObserveSummary *observe_summary;
    const MachineObserveCurrentFetchSummary *current_fetch_summary;
} MachineObserveReportOverviewArtifact;

void machine_observe_file_init(MachineObserveFile *observe_file);
void machine_observe_file_free(MachineObserveFile *observe_file);
void machine_observe_report_init(MachineObserveReport *report);
void machine_observe_report_free(MachineObserveReport *report);
int machine_observe_clone_file(const MachineObserveFile *source,
    MachineObserveFile *out_clone,
    MachineObserveError *error);

const char *machine_observe_resolution_kind_name(MachineObserveResolutionKind resolution_kind);
const char *machine_observe_kind_name(MachineObserveKind observe_kind);
int machine_observe_get_target_policy_summary(MachineElfTargetProfile profile,
    MachineObserveTargetPolicySummary *out_summary);
int machine_observe_file_get_target_policy_summary(const MachineObserveFile *observe_file,
    MachineObserveTargetPolicySummary *out_summary);
int machine_observe_file_get_summary(const MachineObserveFile *observe_file,
    size_t *out_mapped_byte_count);
int machine_observe_file_get_source_elf_artifact_summary(const MachineObserveFile *observe_file,
    MachineElfArtifactSummary *out_summary);
int machine_observe_file_get_header_summary(const MachineObserveFile *observe_file,
    MachineObserveHeaderSummary *out_summary);
int machine_observe_file_get_apply_summary(const MachineObserveFile *observe_file,
    MachineApplySummary *out_summary);
int machine_observe_file_get_observe_summary(const MachineObserveFile *observe_file,
    MachineObserveSummary *out_summary);
int machine_observe_file_get_current_fetch_summary(const MachineObserveFile *observe_file,
    MachineObserveCurrentFetchSummary *out_summary);

int machine_observe_build_from_machine_apply_file(const MachineApplyFile *apply_file,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_apply_report(const MachineApplyReport *report,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_commit_file(const MachineCommitFile *commit_file,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_commit_report(const MachineCommitReport *report,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_step_file(const MachineStepFile *step_file,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_step_report(const MachineStepReport *report,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);
int machine_observe_build_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineObserveFile *out_observe_file,
    MachineObserveError *error);

int machine_observe_build_report_from_file(const MachineObserveFile *source,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_apply_file(const MachineApplyFile *source,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_apply_report(const MachineApplyReport *report,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_commit_file(const MachineCommitFile *source,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_commit_report(const MachineCommitReport *report,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_step_file(const MachineStepFile *source,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_step_report(const MachineStepReport *report,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_build_report_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    MachineObserveReport *out_report,
    MachineObserveError *error);
int machine_observe_report_refresh(MachineObserveReport *report,
    MachineObserveError *error);

int machine_observe_verify_file(const MachineObserveFile *observe_file,
    MachineObserveError *error);
int machine_observe_dump_file(const MachineObserveFile *observe_file,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_file(const MachineObserveFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineObserveError *error);

int machine_observe_report_get_summary(const MachineObserveReport *report,
    size_t *out_mapped_byte_count);
int machine_observe_report_get_overview_artifact(const MachineObserveReport *report,
    MachineObserveReportOverviewArtifact *out_artifact);
int machine_observe_report_get_file(const MachineObserveReport *report,
    const MachineObserveFile **out_file);
int machine_observe_report_get_apply_file(const MachineObserveReport *report,
    const MachineApplyFile **out_apply_file);
int machine_observe_report_get_apply_report(const MachineObserveReport *report,
    const MachineApplyReport **out_apply_report);
int machine_observe_report_get_source_elf_artifact_summary_artifact(
    const MachineObserveReport *report,
    const MachineElfArtifactSummary **out_summary);
int machine_observe_report_get_header_summary_artifact(const MachineObserveReport *report,
    const MachineObserveHeaderSummary **out_summary);
int machine_observe_report_get_target_policy_summary_artifact(const MachineObserveReport *report,
    const MachineObserveTargetPolicySummary **out_summary);
int machine_observe_report_get_observe_summary_artifact(const MachineObserveReport *report,
    const MachineObserveSummary **out_summary);
int machine_observe_report_get_current_fetch_summary_artifact(const MachineObserveReport *report,
    const MachineObserveCurrentFetchSummary **out_summary);
int machine_observe_report_overview_artifact_get_apply_report(
    const MachineObserveReportOverviewArtifact *artifact,
    const MachineApplyReport **out_apply_report);
int machine_observe_report_overview_artifact_get_observe_summary_artifact(
    const MachineObserveReportOverviewArtifact *artifact,
    const MachineObserveSummary **out_summary);
int machine_observe_report_overview_artifact_get_current_fetch_summary_artifact(
    const MachineObserveReportOverviewArtifact *artifact,
    const MachineObserveCurrentFetchSummary **out_summary);

int machine_observe_dump_report(const MachineObserveReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_file(const MachineObserveFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_apply_file(const MachineApplyFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_apply_report(const MachineApplyReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_commit_file(const MachineCommitFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_commit_report(const MachineCommitReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_step_file(const MachineStepFile *source,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_step_report(const MachineStepReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineObserveError *error);
int machine_observe_build_report_dump_from_machine_ir_report_with_profile(
    const MachineIrAllocateRewriteReport *report,
    MachineElfTargetProfile profile,
    char **out_text,
    MachineObserveError *error);

#endif
