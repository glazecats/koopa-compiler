#ifndef MACHINE_EMIT_H
#define MACHINE_EMIT_H

#include <stddef.h>

#include "machine/layout.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineEmitError;

typedef MachineLayoutOperand MachineEmitOperand;
typedef MachineLayoutSlotRef MachineEmitSlotRef;
typedef MachineLayoutGlobal MachineEmitGlobal;
typedef MachineLayoutRegisterDesc MachineEmitRegisterDesc;
typedef MachineLayoutRegisterBank MachineEmitRegisterBank;
typedef MachineLayoutTerminatorKind MachineEmitTerminatorKind;
typedef MachineLayoutBranchTerminator MachineEmitBranchTerminator;
typedef MachineLayoutBranchFallthroughTerminator MachineEmitBranchFallthroughTerminator;
typedef MachineLayoutCompareBranchTerminator MachineEmitCompareBranchTerminator;
typedef MachineLayoutCompareBranchFallthroughTerminator MachineEmitCompareBranchFallthroughTerminator;
typedef MachineLayoutTerminator MachineEmitTerminator;
typedef MachineLayoutOp MachineEmitOp;
typedef MachineLayoutFunctionSummary MachineEmitFunctionSummary;
typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    const char *label_name;
    size_t op_count;
    int has_terminator;
    MachineEmitTerminatorKind terminator_kind;
} MachineEmitBlockSummary;

typedef struct {
    size_t block_count;
    size_t op_count;
    size_t call_count;
    size_t blocks_with_calls_count;
    size_t jump_count;
    size_t fallthrough_count;
    size_t branch_count;
    size_t branch_fallthrough_count;
    size_t compare_branch_count;
    size_t compare_branch_imm_count;
    size_t compare_branch_fallthrough_count;
    size_t compare_branch_imm_fallthrough_count;
    size_t return_count;
    size_t return_imm_count;
    size_t return_spill_count;
} MachineEmitFunctionShapeSummary;

typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    const char *label_name;
    size_t op_count;
    size_t call_count;
    int has_terminator;
    MachineEmitTerminatorKind terminator_kind;
} MachineEmitBlockShapeSummary;

typedef struct {
    size_t emit_index;
    size_t original_layout_index;
    size_t original_block_id;
    char *label_name;
    MachineEmitOp *ops;
    size_t op_count;
    size_t op_capacity;
    int has_terminator;
    MachineEmitTerminator terminator;
} MachineEmitBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    size_t local_count;
    size_t spill_slot_count;
    MachineEmitBlock *blocks;
    size_t block_count;
    size_t block_capacity;
} MachineEmitFunction;

typedef struct {
    MachineEmitRegisterBank register_bank;
    MachineEmitGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    MachineEmitFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MachineEmitProgram;

typedef struct {
    MachineEmitProgram program;
    MachineEmitFunctionShapeSummary *function_shape_summaries;
    size_t *function_block_summary_offsets;
    MachineEmitBlockShapeSummary *block_shape_summaries;
    size_t total_block_summary_count;
    size_t functions_with_calls_count;
    size_t functions_with_fallthrough_count;
    size_t functions_with_branches_count;
    size_t *function_indices_with_calls;
    size_t *function_indices_with_fallthrough;
    size_t *function_indices_with_branches;
} MachineEmitLowerReport;

void machine_emit_program_init(MachineEmitProgram *program);
void machine_emit_program_free(MachineEmitProgram *program);
void machine_emit_lower_report_init(MachineEmitLowerReport *report);
void machine_emit_lower_report_free(MachineEmitLowerReport *report);
int machine_emit_clone_program(const MachineEmitProgram *source,
    MachineEmitProgram *out_program,
    MachineEmitError *error);

int machine_emit_lower_program_from_machine_layout(const MachineLayoutProgram *source,
    MachineEmitProgram *out_program,
    MachineEmitError *error);
int machine_emit_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineEmitProgram *out_program,
    MachineEmitError *error);
int machine_emit_build_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEmitProgram *out_program,
    MachineEmitError *error);
int machine_emit_verify_program(const MachineEmitProgram *program, MachineEmitError *error);
int machine_emit_dump_program(const MachineEmitProgram *program,
    char **out_text,
    MachineEmitError *error);
int machine_emit_dump_lower_report(const MachineEmitLowerReport *report,
    char **out_text,
    MachineEmitError *error);
int machine_emit_lower_dump_from_machine_layout(const MachineLayoutProgram *source,
    char **out_text,
    MachineEmitError *error);
int machine_emit_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineEmitError *error);
int machine_emit_program_get_summary(const MachineEmitProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_emit_program_get_function(const MachineEmitProgram *program,
    size_t function_index,
    const MachineEmitFunction **out_function);
int machine_emit_program_get_function_by_name(const MachineEmitProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineEmitFunction **out_function);
int machine_emit_program_find_block_by_label(const MachineEmitProgram *program,
    const char *label_name,
    size_t *out_function_index,
    const MachineEmitFunction **out_function,
    const MachineEmitBlock **out_block);
int machine_emit_lower_report_get_summary(const MachineEmitLowerReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count);
int machine_emit_lower_report_get_program(const MachineEmitLowerReport *report,
    const MachineEmitProgram **out_program);
int machine_emit_lower_report_get_function(const MachineEmitLowerReport *report,
    size_t function_index,
    const MachineEmitFunction **out_function);
int machine_emit_lower_report_get_function_by_name(const MachineEmitLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineEmitFunction **out_function);
int machine_emit_lower_report_find_block_shape_by_label(const MachineEmitLowerReport *report,
    const char *label_name,
    size_t *out_function_index,
    const MachineEmitBlockShapeSummary **out_summary);
int machine_emit_lower_report_get_function_shape_summary(const MachineEmitLowerReport *report,
    size_t function_index,
    const MachineEmitFunctionShapeSummary **out_summary);
int machine_emit_lower_report_get_block_shape_summary(const MachineEmitLowerReport *report,
    size_t function_index,
    size_t block_index,
    const MachineEmitBlockShapeSummary **out_summary);
int machine_emit_lower_report_get_functions_with_calls(const MachineEmitLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_emit_lower_report_get_functions_with_fallthrough(const MachineEmitLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_emit_lower_report_get_functions_with_branches(const MachineEmitLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_emit_function_get_summary(const MachineEmitFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count);
int machine_emit_function_get_block(const MachineEmitFunction *function,
    size_t emit_index,
    const MachineEmitBlock **out_block);
int machine_emit_function_find_block_by_label(const MachineEmitFunction *function,
    const char *label_name,
    size_t *out_emit_index,
    const MachineEmitBlock **out_block);
int machine_emit_block_get_summary(const MachineEmitBlock *block,
    MachineEmitBlockSummary *out_summary);
int machine_emit_function_compute_summary(const MachineEmitFunction *function,
    MachineEmitFunctionSummary *out_summary);
int machine_emit_build_report_from_machine_layout_program(const MachineLayoutProgram *source,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error);
int machine_emit_build_report_from_program(const MachineEmitProgram *source,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error);
int machine_emit_build_report_from_machine_layout_program_dump(const MachineLayoutProgram *source,
    char **out_text,
    MachineEmitError *error);
int machine_emit_build_report_from_program_dump(const MachineEmitProgram *source,
    char **out_text,
    MachineEmitError *error);
int machine_emit_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error);
int machine_emit_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineEmitError *error);
int machine_emit_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineEmitLowerReport *out_report,
    MachineEmitError *error);
int machine_emit_build_report_from_machine_ir_report_dump(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineEmitError *error);
int machine_emit_lower_report_refresh_shape(MachineEmitLowerReport *report, MachineEmitError *error);

#endif
