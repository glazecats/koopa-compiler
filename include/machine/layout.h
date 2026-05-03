#ifndef MACHINE_LAYOUT_H
#define MACHINE_LAYOUT_H

#include <stddef.h>

#include "machine/select.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineLayoutError;

typedef MachineSelectOperand MachineLayoutOperand;
typedef MachineSelectSlotRef MachineLayoutSlotRef;
typedef MachineSelectGlobal MachineLayoutGlobal;
typedef MachineSelectRegisterDesc MachineLayoutRegisterDesc;
typedef MachineSelectRegisterBank MachineLayoutRegisterBank;

typedef enum {
    MACHINE_LAYOUT_TERM_RETURN = 0,
    MACHINE_LAYOUT_TERM_RETURN_IMM,
    MACHINE_LAYOUT_TERM_RETURN_SPILL,
    MACHINE_LAYOUT_TERM_FALLTHROUGH,
    MACHINE_LAYOUT_TERM_JUMP,
    MACHINE_LAYOUT_TERM_BRANCH,
    MACHINE_LAYOUT_TERM_BRANCH_FALLTHROUGH,
    MACHINE_LAYOUT_TERM_COMPARE_BRANCH,
    MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM,
    MACHINE_LAYOUT_TERM_COMPARE_BRANCH_FALLTHROUGH,
    MACHINE_LAYOUT_TERM_COMPARE_BRANCH_IMM_FALLTHROUGH,
} MachineLayoutTerminatorKind;

typedef struct {
    MachineLayoutOperand condition;
    size_t then_target;
    size_t else_target;
} MachineLayoutBranchTerminator;

typedef struct {
    MachineLayoutOperand condition;
    size_t taken_target;
    size_t fallthrough_target;
    unsigned char branch_on_true;
} MachineLayoutBranchFallthroughTerminator;

typedef struct {
    MachineIrBinaryOp op;
    MachineLayoutOperand lhs;
    MachineLayoutOperand rhs;
    size_t then_target;
    size_t else_target;
} MachineLayoutCompareBranchTerminator;

typedef struct {
    MachineIrBinaryOp op;
    MachineLayoutOperand lhs;
    MachineLayoutOperand rhs;
    size_t taken_target;
    size_t fallthrough_target;
    unsigned char branch_on_true;
} MachineLayoutCompareBranchFallthroughTerminator;

typedef struct {
    MachineLayoutTerminatorKind kind;
    union {
        MachineLayoutOperand return_value;
        size_t jump_target;
        size_t fallthrough_target;
        MachineLayoutBranchTerminator branch;
        MachineLayoutBranchFallthroughTerminator branch_fallthrough;
        MachineLayoutCompareBranchTerminator compare_branch;
        MachineLayoutCompareBranchFallthroughTerminator compare_branch_fallthrough;
    } as;
} MachineLayoutTerminator;

typedef struct {
    size_t layout_index;
    size_t original_block_id;
    size_t op_count;
    int has_terminator;
    MachineLayoutTerminatorKind terminator_kind;
} MachineLayoutBlockSummary;

typedef MachineSelectOp MachineLayoutOp;

typedef struct {
    size_t layout_index;
    size_t original_block_id;
    MachineLayoutOp *ops;
    size_t op_count;
    size_t op_capacity;
    int has_terminator;
    MachineLayoutTerminator terminator;
} MachineLayoutBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    size_t local_count;
    size_t spill_slot_count;
    MachineLayoutBlock *blocks;
    size_t block_count;
    size_t block_capacity;
} MachineLayoutFunction;

typedef struct {
    MachineLayoutRegisterBank register_bank;
    MachineLayoutGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    MachineLayoutFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MachineLayoutProgram;

typedef struct {
    MachineSelectTargetPolicySummary select_policy;
    int preserves_spill_operands_for_later_materialization;
    int preserves_global_slot_ops_for_later_address_formation;
    int preserves_fallthrough_terminator_shapes;
} MachineLayoutTargetPolicySummary;

typedef struct {
    size_t block_count;
    size_t op_count;
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
} MachineLayoutFunctionSummary;

typedef struct {
    MachineLayoutProgram program;
    MachineLayoutTargetPolicySummary target_policy_summary;
    MachineLayoutFunctionSummary *function_summaries;
    size_t function_summary_count;
    size_t *function_block_summary_offsets;
    MachineLayoutBlockSummary *block_summaries;
    size_t total_block_summary_count;
    size_t functions_with_fallthrough_count;
    size_t functions_with_branches_count;
    size_t *function_indices_with_fallthrough;
    size_t *function_indices_with_branches;
} MachineLayoutReport;

void machine_layout_program_init(MachineLayoutProgram *program);
void machine_layout_program_free(MachineLayoutProgram *program);
void machine_layout_report_init(MachineLayoutReport *report);
void machine_layout_report_free(MachineLayoutReport *report);
int machine_layout_get_target_policy_summary(MachineLayoutTargetPolicySummary *out_summary);
int machine_layout_clone_program(const MachineLayoutProgram *source,
    MachineLayoutProgram *out_program,
    MachineLayoutError *error);
int machine_layout_program_get_target_policy_summary(const MachineLayoutProgram *program,
    MachineLayoutTargetPolicySummary *out_summary);
int machine_layout_verify_current_riscv32_preview_compatibility(const MachineLayoutProgram *program,
    MachineLayoutError *error);
int machine_layout_verify_current_riscv32_preview_bytes_compatibility(const MachineLayoutProgram *program,
    MachineLayoutError *error);
int machine_layout_program_get_summary(const MachineLayoutProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_layout_program_get_function(const MachineLayoutProgram *program,
    size_t function_index,
    const MachineLayoutFunction **out_function);
int machine_layout_program_get_function_by_name(const MachineLayoutProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineLayoutFunction **out_function);
int machine_layout_function_get_summary(const MachineLayoutFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count);
int machine_layout_function_get_block(const MachineLayoutFunction *function,
    size_t layout_index,
    const MachineLayoutBlock **out_block);
int machine_layout_block_get_summary(const MachineLayoutBlock *block,
    MachineLayoutBlockSummary *out_summary);
int machine_layout_report_get_summary(const MachineLayoutReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count,
    size_t *out_functions_with_fallthrough_count,
    size_t *out_functions_with_branches_count);
int machine_layout_report_get_target_policy_summary_artifact(const MachineLayoutReport *report,
    const MachineLayoutTargetPolicySummary **out_summary);
int machine_layout_report_verify_current_riscv32_preview_compatibility(const MachineLayoutReport *report,
    MachineLayoutError *error);
int machine_layout_report_verify_current_riscv32_preview_bytes_compatibility(const MachineLayoutReport *report,
    MachineLayoutError *error);
int machine_layout_report_get_program(const MachineLayoutReport *report,
    const MachineLayoutProgram **out_program);
int machine_layout_report_get_function(const MachineLayoutReport *report,
    size_t function_index,
    const MachineLayoutFunction **out_function);
int machine_layout_report_get_function_by_name(const MachineLayoutReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineLayoutFunction **out_function);
int machine_layout_report_get_function_summary_artifact(const MachineLayoutReport *report,
    size_t function_index,
    const MachineLayoutFunctionSummary **out_summary);
int machine_layout_report_get_block_summary(const MachineLayoutReport *report,
    size_t function_index,
    size_t block_index,
    const MachineLayoutBlockSummary **out_summary);
int machine_layout_report_get_functions_with_fallthrough(const MachineLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_layout_report_get_functions_with_branches(const MachineLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices);

int machine_layout_lower_program_from_machine_select(const MachineSelectProgram *source,
    MachineLayoutProgram *out_program,
    MachineLayoutError *error);
int machine_layout_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineLayoutProgram *out_program,
    MachineLayoutError *error);
int machine_layout_verify_program(const MachineLayoutProgram *program, MachineLayoutError *error);
int machine_layout_dump_program(const MachineLayoutProgram *program,
    char **out_text,
    MachineLayoutError *error);
int machine_layout_report_refresh(MachineLayoutReport *report, MachineLayoutError *error);
int machine_layout_build_report_from_program(const MachineLayoutProgram *source,
    MachineLayoutReport *out_report,
    MachineLayoutError *error);
int machine_layout_build_report_from_machine_select_program(const MachineSelectProgram *source,
    MachineLayoutReport *out_report,
    MachineLayoutError *error);
int machine_layout_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineLayoutReport *out_report,
    MachineLayoutError *error);
int machine_layout_dump_report(const MachineLayoutReport *report,
    char **out_text,
    MachineLayoutError *error);
int machine_layout_build_report_from_program_dump(const MachineLayoutProgram *source,
    char **out_text,
    MachineLayoutError *error);
int machine_layout_build_report_from_machine_select_program_dump(const MachineSelectProgram *source,
    char **out_text,
    MachineLayoutError *error);
int machine_layout_build_report_from_machine_ir_program_dump(const MachineIrProgram *source,
    char **out_text,
    MachineLayoutError *error);
int machine_layout_function_compute_summary(const MachineLayoutFunction *function,
    MachineLayoutFunctionSummary *out_summary);

#endif
