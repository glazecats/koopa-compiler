#ifndef MACHINE_BYTES_H
#define MACHINE_BYTES_H

#include <stddef.h>

#include "machine/encode.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineBytesError;

typedef enum {
    MACHINE_BYTES_TARGET_PROFILE_GENERIC = 0,
    MACHINE_BYTES_TARGET_PROFILE_RISCV32_PREVIEW,
    MACHINE_BYTES_TARGET_PROFILE_I386_PREVIEW,
} MachineBytesTargetProfile;

typedef struct {
    MachineBytesTargetProfile target_profile;
    size_t max_logical_machine_register_count;
    int preserves_known_internal_pc_relative_targets;
    int preserves_direct_fallthrough_honesty;
    int uses_paired_global_data_addressing;
    int supports_rv32m_alu_ops;
} MachineBytesTargetPolicySummary;

typedef MachineEncodeTerminatorKind MachineBytesTerminatorKind;

typedef struct {
    size_t block_count;
    size_t byte_count;
    size_t op_byte_count;
    size_t call_count;
    size_t blocks_with_calls_count;
    size_t terminator_byte_count;
    size_t jump_count;
    size_t fallthrough_count;
    size_t branch_count;
    size_t compare_branch_count;
    size_t return_count;
} MachineBytesFunctionSummary;

typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    const char *label_name;
    size_t start_byte_offset;
    size_t terminator_byte_offset;
    size_t end_byte_offset;
    size_t byte_count;
} MachineBytesBlockSummary;

typedef struct {
    MachineBytesTerminatorKind kind;
    int has_return_value;
    MachineEmitOperand return_value;
    int has_primary_target;
    const char *primary_target_label_name;
    size_t primary_target_byte_offset;
    int has_secondary_target;
    const char *secondary_target_label_name;
    size_t secondary_target_byte_offset;
    unsigned char branch_on_true;
    MachineIrBinaryOp compare_op;
} MachineBytesTerminatorSummary;

typedef enum {
    MACHINE_BYTES_REFERENCE_CALL = 0,
    MACHINE_BYTES_REFERENCE_CONTROL_PRIMARY,
    MACHINE_BYTES_REFERENCE_CONTROL_SECONDARY,
    MACHINE_BYTES_REFERENCE_DATA_ADDR,
    MACHINE_BYTES_REFERENCE_DATA_LOAD,
    MACHINE_BYTES_REFERENCE_DATA_STORE,
} MachineBytesReferenceKind;

typedef struct {
    MachineBytesReferenceKind kind;
    size_t function_index;
    size_t emit_index;
    const char *source_label_name;
    size_t owner_byte_offset;
    size_t owner_byte_count;
    size_t patch_byte_offset;
    size_t patch_byte_count;
    MachineSelectOpKind op_kind;
    MachineBytesTerminatorKind terminator_kind;
    const char *target_name;
    int has_target_function_index;
    size_t target_function_index;
    int has_target_emit_index;
    size_t target_emit_index;
    int has_target_byte_offset;
    size_t target_byte_offset;
} MachineBytesReferenceSummary;

typedef enum {
    MACHINE_BYTES_FIXUP_CALL_TARGET = 0,
    MACHINE_BYTES_FIXUP_CONTROL_PRIMARY,
    MACHINE_BYTES_FIXUP_CONTROL_SECONDARY,
    MACHINE_BYTES_FIXUP_DATA_ADDR_TARGET,
    MACHINE_BYTES_FIXUP_DATA_LOAD_TARGET,
    MACHINE_BYTES_FIXUP_DATA_STORE_TARGET,
} MachineBytesFixupKind;

typedef enum {
    MACHINE_BYTES_FIXUP_TARGET_SYMBOL = 0,
    MACHINE_BYTES_FIXUP_TARGET_BLOCK,
} MachineBytesFixupTargetKind;

typedef struct {
    MachineBytesFixupKind kind;
    MachineBytesFixupTargetKind target_kind;
    size_t function_index;
    size_t emit_index;
    const char *source_label_name;
    size_t owner_byte_offset;
    size_t owner_byte_count;
    size_t patch_byte_offset;
    size_t patch_byte_count;
    const char *target_name;
    int has_target_function_index;
    size_t target_function_index;
    int has_target_emit_index;
    size_t target_emit_index;
    int has_target_byte_offset;
    size_t target_byte_offset;
    int has_target_symbol_index;
    size_t target_symbol_index;
} MachineBytesFixupSummary;

typedef enum {
    MACHINE_BYTES_SYMBOL_FUNCTION = 0,
    MACHINE_BYTES_SYMBOL_BLOCK,
    MACHINE_BYTES_SYMBOL_GLOBAL_OBJECT,
    MACHINE_BYTES_SYMBOL_EXTERNAL,
} MachineBytesSymbolKind;

typedef struct {
    MachineBytesSymbolKind kind;
    const char *name;
    int is_defined;
    int has_section_index;
    size_t section_index;
    int has_function_index;
    size_t function_index;
    int has_emit_index;
    size_t emit_index;
    int has_byte_offset;
    size_t byte_offset;
    size_t byte_count;
    size_t incoming_fixup_count;
} MachineBytesSymbolSummary;

typedef enum {
    MACHINE_BYTES_SECTION_TEXT = 0,
    MACHINE_BYTES_SECTION_SBSS,
    MACHINE_BYTES_SECTION_SDATA,
} MachineBytesSectionKind;

typedef struct {
    MachineBytesSectionKind kind;
    const char *name;
    size_t start_byte_offset;
    size_t end_byte_offset;
    size_t byte_count;
    size_t function_count;
    size_t block_count;
    size_t symbol_start_index;
    size_t symbol_count;
    size_t fixup_start_index;
    size_t fixup_count;
} MachineBytesSectionSummary;

typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    char *label_name;
    size_t start_byte_offset;
    size_t terminator_byte_offset;
    size_t end_byte_offset;
    unsigned char *bytes;
    size_t byte_count;
    MachineEmitBlock block;
} MachineBytesBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    size_t local_count;
    size_t spill_slot_count;
    int uses_biased_frame_pointer;
    MachineBytesBlock *blocks;
    size_t block_count;
    size_t block_capacity;
} MachineBytesFunction;

typedef struct {
    MachineBytesTargetProfile target_profile;
    MachineEmitRegisterBank register_bank;
    MachineEmitGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    MachineBytesFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MachineBytesProgram;

typedef struct {
    MachineBytesProgram program;
    MachineBytesTargetPolicySummary target_policy_summary;
    MachineBytesFunctionSummary *function_summaries;
    size_t *function_byte_offsets;
    size_t *function_block_summary_offsets;
    MachineBytesBlockSummary *block_summaries;
    size_t total_block_summary_count;
    size_t total_program_byte_count;
    size_t *function_reference_offsets;
    MachineBytesReferenceSummary *reference_summaries;
    size_t total_reference_summary_count;
    size_t *function_fixup_offsets;
    MachineBytesFixupSummary *fixup_summaries;
    size_t total_fixup_summary_count;
    MachineBytesSymbolSummary *symbol_summaries;
    size_t total_symbol_summary_count;
    MachineBytesSymbolSummary **symbol_summaries_by_name;
    MachineBytesSectionSummary *section_summaries;
    size_t total_section_summary_count;
    size_t functions_with_calls_count;
    size_t functions_with_fallthrough_count;
    size_t functions_with_branches_count;
    size_t *function_indices_with_calls;
    size_t *function_indices_with_fallthrough;
    size_t *function_indices_with_branches;
} MachineBytesReport;

void machine_bytes_program_init(MachineBytesProgram *program);
void machine_bytes_program_free(MachineBytesProgram *program);
void machine_bytes_report_init(MachineBytesReport *report);
void machine_bytes_report_free(MachineBytesReport *report);

int machine_bytes_get_target_policy_summary(MachineBytesTargetProfile profile,
    MachineBytesTargetPolicySummary *out_summary);
int machine_bytes_clone_program(const MachineBytesProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_program_get_target_policy_summary(const MachineBytesProgram *program,
    MachineBytesTargetPolicySummary *out_summary);
int machine_bytes_verify_current_riscv32_preview_compatibility(const MachineBytesProgram *program,
    MachineBytesError *error);
int machine_bytes_program_get_summary(const MachineBytesProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_bytes_program_get_function(const MachineBytesProgram *program,
    size_t function_index,
    const MachineBytesFunction **out_function);
int machine_bytes_program_get_function_by_name(const MachineBytesProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineBytesFunction **out_function);
int machine_bytes_program_find_block_by_label(const MachineBytesProgram *program,
    const char *label_name,
    size_t *out_function_index,
    const MachineBytesFunction **out_function,
    const MachineBytesBlock **out_block);
int machine_bytes_program_find_block_by_function_name_and_offset(const MachineBytesProgram *program,
    const char *function_name,
    size_t byte_offset,
    size_t *out_function_index,
    size_t *out_emit_index,
    const MachineBytesFunction **out_function,
    const MachineBytesBlock **out_block);
int machine_bytes_program_find_block_by_program_byte_offset(const MachineBytesProgram *program,
    size_t program_byte_offset,
    size_t *out_function_index,
    size_t *out_emit_index,
    const MachineBytesFunction **out_function,
    const MachineBytesBlock **out_block);
int machine_bytes_program_get_total_byte_count(const MachineBytesProgram *program,
    size_t *out_total_byte_count);
int machine_bytes_program_get_function_byte_span(const MachineBytesProgram *program,
    size_t function_index,
    size_t *out_start_program_byte_offset,
    size_t *out_end_program_byte_offset);
int machine_bytes_program_copy_bytes(const MachineBytesProgram *program,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_program_copy_function_bytes(const MachineBytesProgram *program,
    size_t function_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_program_copy_block_bytes(const MachineBytesProgram *program,
    size_t function_index,
    size_t emit_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_function_get_summary(const MachineBytesFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count);
int machine_bytes_function_get_block(const MachineBytesFunction *function,
    size_t emit_index,
    const MachineBytesBlock **out_block);
int machine_bytes_function_find_block_by_label(const MachineBytesFunction *function,
    const char *label_name,
    size_t *out_emit_index,
    const MachineBytesBlock **out_block);
int machine_bytes_function_find_block_covering_offset(const MachineBytesFunction *function,
    size_t byte_offset,
    size_t *out_emit_index,
    const MachineBytesBlock **out_block);
int machine_bytes_function_copy_bytes(const MachineBytesFunction *function,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_block_get_summary(const MachineBytesBlock *block,
    MachineBytesBlockSummary *out_summary);
int machine_bytes_block_get_bytes(const MachineBytesBlock *block,
    const unsigned char **out_bytes,
    size_t *out_byte_count);
int machine_bytes_block_copy_bytes(const MachineBytesBlock *block,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_function_get_block_terminator_summary(const MachineBytesFunction *function,
    size_t emit_index,
    MachineBytesTerminatorSummary *out_summary);
int machine_bytes_function_compute_summary(const MachineBytesFunction *function,
    MachineBytesFunctionSummary *out_summary);

int machine_bytes_report_get_summary(const MachineBytesReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count);
int machine_bytes_report_get_target_policy_summary_artifact(const MachineBytesReport *report,
    const MachineBytesTargetPolicySummary **out_summary);
int machine_bytes_report_verify_current_riscv32_preview_compatibility(const MachineBytesReport *report,
    MachineBytesError *error);
int machine_bytes_report_get_program(const MachineBytesReport *report,
    const MachineBytesProgram **out_program);
int machine_bytes_report_get_function(const MachineBytesReport *report,
    size_t function_index,
    const MachineBytesFunction **out_function);
int machine_bytes_report_get_function_by_name(const MachineBytesReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineBytesFunction **out_function);
int machine_bytes_report_get_function_summary_artifact(const MachineBytesReport *report,
    size_t function_index,
    const MachineBytesFunctionSummary **out_summary);
int machine_bytes_report_get_total_program_byte_count(const MachineBytesReport *report,
    size_t *out_total_program_byte_count);
int machine_bytes_report_get_reference_summary_count(const MachineBytesReport *report,
    size_t *out_total_reference_summary_count);
int machine_bytes_report_get_reference_summary(const MachineBytesReport *report,
    size_t reference_index,
    const MachineBytesReferenceSummary **out_summary);
int machine_bytes_report_get_function_reference_summaries(const MachineBytesReport *report,
    size_t function_index,
    size_t *out_count,
    const MachineBytesReferenceSummary **out_summaries);
int machine_bytes_report_get_fixup_summary_count(const MachineBytesReport *report,
    size_t *out_total_fixup_summary_count);
int machine_bytes_report_get_fixup_summary(const MachineBytesReport *report,
    size_t fixup_index,
    const MachineBytesFixupSummary **out_summary);
int machine_bytes_report_get_function_fixup_summaries(const MachineBytesReport *report,
    size_t function_index,
    size_t *out_count,
    const MachineBytesFixupSummary **out_summaries);
int machine_bytes_report_get_symbol_summary_count(const MachineBytesReport *report,
    size_t *out_total_symbol_summary_count);
int machine_bytes_report_get_symbol_summary(const MachineBytesReport *report,
    size_t symbol_index,
    const MachineBytesSymbolSummary **out_summary);
int machine_bytes_report_find_symbol_summary_by_name(const MachineBytesReport *report,
    const char *symbol_name,
    size_t *out_symbol_index,
    const MachineBytesSymbolSummary **out_summary);
int machine_bytes_report_get_section_summary_count(const MachineBytesReport *report,
    size_t *out_total_section_summary_count);
int machine_bytes_report_get_section_summary(const MachineBytesReport *report,
    size_t section_index,
    const MachineBytesSectionSummary **out_summary);
int machine_bytes_report_find_section_summary_by_name(const MachineBytesReport *report,
    const char *section_name,
    size_t *out_section_index,
    const MachineBytesSectionSummary **out_summary);
int machine_bytes_report_get_section_symbol_summaries(const MachineBytesReport *report,
    size_t section_index,
    size_t *out_count,
    const MachineBytesSymbolSummary **out_summaries);
int machine_bytes_report_get_section_fixup_summaries(const MachineBytesReport *report,
    size_t section_index,
    size_t *out_count,
    const MachineBytesFixupSummary **out_summaries);
int machine_bytes_report_copy_bytes(const MachineBytesReport *report,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_report_copy_function_bytes(const MachineBytesReport *report,
    size_t function_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_report_copy_block_bytes(const MachineBytesReport *report,
    size_t function_index,
    size_t block_index,
    unsigned char **out_bytes,
    size_t *out_byte_count,
    MachineBytesError *error);
int machine_bytes_report_get_function_byte_span(const MachineBytesReport *report,
    size_t function_index,
    size_t *out_start_byte_offset,
    size_t *out_end_byte_offset);
int machine_bytes_report_find_block_summary_by_label(const MachineBytesReport *report,
    const char *label_name,
    size_t *out_function_index,
    const MachineBytesBlockSummary **out_summary);
int machine_bytes_report_find_block_summary_covering_offset(const MachineBytesReport *report,
    size_t function_index,
    size_t byte_offset,
    const MachineBytesBlockSummary **out_summary);
int machine_bytes_report_find_block_summary_by_function_name_and_offset(const MachineBytesReport *report,
    const char *function_name,
    size_t byte_offset,
    size_t *out_function_index,
    const MachineBytesBlockSummary **out_summary);
int machine_bytes_report_find_block_summary_by_program_byte_offset(const MachineBytesReport *report,
    size_t program_byte_offset,
    size_t *out_function_index,
    const MachineBytesBlockSummary **out_summary);
int machine_bytes_report_get_block_summary(const MachineBytesReport *report,
    size_t function_index,
    size_t block_index,
    const MachineBytesBlockSummary **out_summary);
int machine_bytes_report_get_block_terminator_summary(const MachineBytesReport *report,
    size_t function_index,
    size_t block_index,
    MachineBytesTerminatorSummary *out_summary);
int machine_bytes_report_get_functions_with_calls(const MachineBytesReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_bytes_report_get_functions_with_fallthrough(const MachineBytesReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_bytes_report_get_functions_with_branches(const MachineBytesReport *report,
    size_t *out_count,
    const size_t **out_function_indices);

int machine_bytes_lower_program_from_machine_encode_with_profile(const MachineEncodeProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_encode(const MachineEncodeProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_encode_report_with_profile(const MachineEncodeReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_encode_report(const MachineEncodeReport *report,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_emit_with_profile(const MachineEmitProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_emit(const MachineEmitProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_emit_report_with_profile(const MachineEmitLowerReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_ir_with_profile(const MachineIrProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesProgram *out_program,
    MachineBytesError *error);
int machine_bytes_lower_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineBytesProgram *out_program,
    MachineBytesError *error);

int machine_bytes_verify_program(const MachineBytesProgram *program, MachineBytesError *error);
int machine_bytes_dump_program(const MachineBytesProgram *program,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_dump_report(const MachineBytesReport *report,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_lower_dump_from_machine_encode(const MachineEncodeProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_lower_dump_from_machine_encode_report(const MachineEncodeReport *report,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_lower_dump_from_machine_emit(const MachineEmitProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_lower_dump_from_machine_emit_report(const MachineEmitLowerReport *report,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_lower_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineBytesError *error);

int machine_bytes_build_report_from_machine_encode_program(const MachineEncodeProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_encode_program_with_profile(const MachineEncodeProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_encode_report(const MachineEncodeReport *report,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_encode_report_with_profile(const MachineEncodeReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_emit_program(const MachineEmitProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_emit_program_with_profile(const MachineEmitProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_emit_report_with_profile(const MachineEmitLowerReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_program(const MachineBytesProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_ir_program_with_profile(const MachineIrProgram *source,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_ir_report_with_profile(const MachineIrAllocateRewriteReport *report,
    MachineBytesTargetProfile profile,
    MachineBytesReport *out_report,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_encode_program_dump(const MachineEncodeProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_encode_report_dump(const MachineEncodeReport *report,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_emit_program_dump(const MachineEmitProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_emit_report_dump(const MachineEmitLowerReport *report,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_build_report_from_program_dump(const MachineBytesProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_build_report_from_machine_ir_report_dump(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineBytesError *error);
int machine_bytes_report_refresh(MachineBytesReport *report, MachineBytesError *error);

#endif
