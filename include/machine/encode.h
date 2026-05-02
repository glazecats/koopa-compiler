#ifndef MACHINE_ENCODE_H
#define MACHINE_ENCODE_H

#include <stddef.h>

#include "machine/emit.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineEncodeError;

typedef MachineEmitTerminatorKind MachineEncodeTerminatorKind;

typedef struct {
    size_t start_offset;
    size_t terminator_offset;
    size_t end_offset;
} MachineEncodeBlockSpan;

typedef struct {
    size_t block_count;
    size_t unit_count;
    size_t op_unit_count;
    size_t call_count;
    size_t blocks_with_calls_count;
    size_t terminator_unit_count;
    size_t jump_count;
    size_t fallthrough_count;
    size_t branch_count;
    size_t compare_branch_count;
    size_t return_count;
} MachineEncodeFunctionSummary;

typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    const char *label_name;
    size_t start_offset;
    size_t terminator_offset;
    size_t end_offset;
} MachineEncodeBlockSummary;

typedef struct {
    MachineEncodeTerminatorKind kind;
    int has_primary_target;
    const char *primary_target_label_name;
    size_t primary_target_offset;
    int has_secondary_target;
    const char *secondary_target_label_name;
    size_t secondary_target_offset;
    unsigned char branch_on_true;
    MachineIrBinaryOp compare_op;
} MachineEncodeTerminatorSummary;

typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    char *label_name;
    size_t start_offset;
    size_t terminator_offset;
    size_t end_offset;
    MachineEncodeBlockSpan span;
    MachineEmitBlock block;
} MachineEncodeBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    size_t local_count;
    size_t spill_slot_count;
    MachineEncodeBlock *blocks;
    size_t block_count;
    size_t block_capacity;
} MachineEncodeFunction;

typedef struct {
    MachineEmitRegisterBank register_bank;
    MachineEmitGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    MachineEncodeFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MachineEncodeProgram;

typedef struct {
    MachineEncodeProgram program;
    MachineEncodeFunctionSummary *function_summaries;
    size_t *function_block_summary_offsets;
    MachineEncodeBlockSummary *block_summaries;
    size_t total_block_summary_count;
    size_t functions_with_calls_count;
    size_t functions_with_fallthrough_count;
    size_t functions_with_branches_count;
    size_t *function_indices_with_calls;
    size_t *function_indices_with_fallthrough;
    size_t *function_indices_with_branches;
} MachineEncodeReport;

void machine_encode_program_init(MachineEncodeProgram *program);
void machine_encode_program_free(MachineEncodeProgram *program);
void machine_encode_report_init(MachineEncodeReport *report);
void machine_encode_report_free(MachineEncodeReport *report);

int machine_encode_clone_program(const MachineEncodeProgram *source,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error);
int machine_encode_program_get_summary(const MachineEncodeProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_encode_program_get_function(const MachineEncodeProgram *program,
    size_t function_index,
    const MachineEncodeFunction **out_function);
int machine_encode_program_get_function_by_name(const MachineEncodeProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineEncodeFunction **out_function);
int machine_encode_program_find_block_by_label(const MachineEncodeProgram *program,
    const char *label_name,
    size_t *out_function_index,
    const MachineEncodeFunction **out_function,
    const MachineEncodeBlock **out_block);
int machine_encode_program_find_block_by_function_name_and_offset(const MachineEncodeProgram *program,
    const char *function_name,
    size_t offset,
    size_t *out_function_index,
    size_t *out_emit_index,
    const MachineEncodeFunction **out_function,
    const MachineEncodeBlock **out_block);
int machine_encode_function_get_summary(const MachineEncodeFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count);
int machine_encode_function_get_block(const MachineEncodeFunction *function,
    size_t emit_index,
    const MachineEncodeBlock **out_block);
int machine_encode_function_find_block_by_label(const MachineEncodeFunction *function,
    const char *label_name,
    size_t *out_emit_index,
    const MachineEncodeBlock **out_block);
int machine_encode_function_find_block_covering_offset(const MachineEncodeFunction *function,
    size_t offset,
    size_t *out_emit_index,
    const MachineEncodeBlock **out_block);
int machine_encode_block_get_summary(const MachineEncodeBlock *block,
    MachineEncodeBlockSummary *out_summary);
int machine_encode_function_get_block_terminator_summary(const MachineEncodeFunction *function,
    size_t emit_index,
    MachineEncodeTerminatorSummary *out_summary);
int machine_encode_function_compute_summary(const MachineEncodeFunction *function,
    MachineEncodeFunctionSummary *out_summary);
int machine_encode_report_get_summary(const MachineEncodeReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count);
int machine_encode_report_get_program(const MachineEncodeReport *report,
    const MachineEncodeProgram **out_program);
int machine_encode_report_get_function(const MachineEncodeReport *report,
    size_t function_index,
    const MachineEncodeFunction **out_function);
int machine_encode_report_get_function_by_name(const MachineEncodeReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineEncodeFunction **out_function);
int machine_encode_report_get_function_summary_artifact(const MachineEncodeReport *report,
    size_t function_index,
    const MachineEncodeFunctionSummary **out_summary);
int machine_encode_report_find_block_summary_by_label(const MachineEncodeReport *report,
    const char *label_name,
    size_t *out_function_index,
    const MachineEncodeBlockSummary **out_summary);
int machine_encode_report_find_block_summary_covering_offset(const MachineEncodeReport *report,
    size_t function_index,
    size_t offset,
    const MachineEncodeBlockSummary **out_summary);
int machine_encode_report_find_block_summary_by_function_name_and_offset(const MachineEncodeReport *report,
    const char *function_name,
    size_t offset,
    size_t *out_function_index,
    const MachineEncodeBlockSummary **out_summary);
int machine_encode_report_get_block_summary(const MachineEncodeReport *report,
    size_t function_index,
    size_t block_index,
    const MachineEncodeBlockSummary **out_summary);
int machine_encode_report_get_block_terminator_summary(const MachineEncodeReport *report,
    size_t function_index,
    size_t block_index,
    MachineEncodeTerminatorSummary *out_summary);
int machine_encode_report_get_functions_with_calls(const MachineEncodeReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_encode_report_get_functions_with_fallthrough(const MachineEncodeReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_encode_report_get_functions_with_branches(const MachineEncodeReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_encode_lower_program_from_machine_emit(const MachineEmitProgram *source,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error);
int machine_encode_lower_program_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error);
int machine_encode_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error);
int machine_encode_lower_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEncodeProgram *out_program,
    MachineEncodeError *error);
int machine_encode_verify_program(const MachineEncodeProgram *program, MachineEncodeError *error);
int machine_encode_dump_program(const MachineEncodeProgram *program,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_dump_report(const MachineEncodeReport *report,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_lower_dump_from_machine_emit(const MachineEmitProgram *source,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_lower_dump_from_machine_emit_report(const MachineEmitLowerReport *report,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_lower_dump_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_emit_program(const MachineEmitProgram *source,
    MachineEncodeReport *out_report,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_emit_program_dump(const MachineEmitProgram *source,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_emit_report(const MachineEmitLowerReport *report,
    MachineEncodeReport *out_report,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_emit_report_dump(const MachineEmitLowerReport *report,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_build_report_from_program(const MachineEncodeProgram *source,
    MachineEncodeReport *out_report,
    MachineEncodeError *error);
int machine_encode_build_report_from_program_dump(const MachineEncodeProgram *source,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineEncodeReport *out_report,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEncodeReport *out_report,
    MachineEncodeError *error);
int machine_encode_build_report_from_machine_ir_report_dump(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEncodeError *error);
int machine_encode_report_refresh(MachineEncodeReport *report, MachineEncodeError *error);

#endif
