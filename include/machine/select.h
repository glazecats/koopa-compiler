#ifndef MACHINE_SELECT_H
#define MACHINE_SELECT_H

#include <stddef.h>

#include "machine/ir.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineSelectError;

typedef enum {
    MACHINE_SELECT_OPERAND_NONE = 0,
    MACHINE_SELECT_OPERAND_IMMEDIATE,
    MACHINE_SELECT_OPERAND_REGISTER,
    MACHINE_SELECT_OPERAND_SPILL_SLOT,
} MachineSelectOperandKind;

typedef struct {
    MachineSelectOperandKind kind;
    long long immediate;
    size_t machine_register_id;
    size_t spill_slot;
} MachineSelectOperand;

typedef enum {
    MACHINE_SELECT_SLOT_LOCAL = 0,
    MACHINE_SELECT_SLOT_GLOBAL,
} MachineSelectSlotKind;

typedef struct {
    MachineSelectSlotKind kind;
    size_t id;
} MachineSelectSlotRef;

typedef enum {
    MACHINE_SELECT_OP_COPY = 0,
    MACHINE_SELECT_OP_MATERIALIZE_IMM,
    MACHINE_SELECT_OP_ALU,
    MACHINE_SELECT_OP_ALU_IMM,
    MACHINE_SELECT_OP_CMP,
    MACHINE_SELECT_OP_CMP_IMM,
    MACHINE_SELECT_OP_CALL,
    MACHINE_SELECT_OP_CALL_IMM,
    MACHINE_SELECT_OP_CALL_SPILL,
    MACHINE_SELECT_OP_CALL_IMM_SPILL,
    MACHINE_SELECT_OP_CALL_VOID,
    MACHINE_SELECT_OP_CALL_VOID_IMM,
    MACHINE_SELECT_OP_ADDR_LOCAL,
    MACHINE_SELECT_OP_ADDR_GLOBAL,
    MACHINE_SELECT_OP_LOAD_LOCAL,
    MACHINE_SELECT_OP_STORE_LOCAL,
    MACHINE_SELECT_OP_STORE_LOCAL_IMM,
    MACHINE_SELECT_OP_LOAD_GLOBAL,
    MACHINE_SELECT_OP_STORE_GLOBAL,
    MACHINE_SELECT_OP_STORE_GLOBAL_IMM,
    MACHINE_SELECT_OP_LOAD_INDIRECT,
    MACHINE_SELECT_OP_STORE_INDIRECT,
} MachineSelectOpKind;

typedef struct {
    MachineIrBinaryOp op;
    MachineSelectOperand lhs;
    MachineSelectOperand rhs;
} MachineSelectBinaryOp;

typedef struct {
    MachineIrBinaryOp op;
    MachineSelectOperand lhs;
    MachineSelectOperand rhs;
    size_t then_target;
    size_t else_target;
} MachineSelectCompareBranch;

typedef struct {
    char *callee_name;
    MachineSelectOperand *args;
    size_t arg_count;
} MachineSelectCallOp;

typedef struct {
    MachineSelectSlotRef slot;
    MachineSelectOperand value;
} MachineSelectStoreOp;

typedef struct {
    MachineSelectOperand addr;
    MachineSelectOperand value;
} MachineSelectIndirectStoreOp;

typedef struct {
    MachineSelectOpKind kind;
    int has_result;
    MachineSelectOperand result;
    union {
        MachineSelectOperand copy_value;
        MachineSelectBinaryOp binary;
        MachineSelectCallOp call;
        MachineSelectSlotRef addr_slot;
        MachineSelectSlotRef load_slot;
        MachineSelectStoreOp store;
        MachineSelectOperand load_indirect_addr;
        MachineSelectIndirectStoreOp store_indirect;
    } as;
} MachineSelectOp;

typedef enum {
    MACHINE_SELECT_TERM_RETURN = 0,
    MACHINE_SELECT_TERM_RETURN_IMM,
    MACHINE_SELECT_TERM_RETURN_SPILL,
    MACHINE_SELECT_TERM_JUMP,
    MACHINE_SELECT_TERM_BRANCH,
    MACHINE_SELECT_TERM_COMPARE_BRANCH,
    MACHINE_SELECT_TERM_COMPARE_BRANCH_IMM,
} MachineSelectTerminatorKind;

typedef struct {
    MachineSelectTerminatorKind kind;
    union {
        MachineSelectOperand return_value;
        size_t jump_target;
        struct {
            MachineSelectOperand condition;
            size_t then_target;
            size_t else_target;
        } branch;
        MachineSelectCompareBranch compare_branch;
    } as;
} MachineSelectTerminator;

typedef struct {
    size_t id;
    MachineSelectOp *ops;
    size_t op_count;
    size_t op_capacity;
    int has_terminator;
    MachineSelectTerminator terminator;
} MachineSelectBasicBlock;

typedef struct {
    size_t id;
    char *source_name;
    int is_parameter;
    AstFunctionReturnType value_type;
} MachineSelectLocal;

typedef struct {
    size_t id;
    char *name;
    AstFunctionReturnType value_type;
    size_t byte_size;
    int has_initializer;
    long long initializer_value;
    int has_runtime_initializer;
} MachineSelectGlobal;

typedef struct {
    size_t register_id;
    char *name;
    ValueSsaMachineRegisterClass register_class;
    unsigned char allocatable;
    unsigned char caller_clobbered;
    unsigned char callee_preserved;
} MachineSelectRegisterDesc;

typedef struct {
    size_t register_count;
    MachineSelectRegisterDesc *registers;
} MachineSelectRegisterBank;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    MachineSelectLocal *locals;
    size_t local_count;
    size_t local_capacity;
    MachineSelectBasicBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t spill_slot_count;
} MachineSelectFunction;

typedef struct {
    MachineSelectRegisterBank register_bank;
    MachineSelectGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    MachineSelectFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MachineSelectProgram;

typedef struct {
    size_t current_riscv32_preview_logical_register_cap;
    int supports_early_immediate_legalization;
    int supports_compare_branch_fusion;
    int preserves_spill_operands_for_later_materialization;
    int preserves_global_slot_ops_for_later_address_formation;
} MachineSelectTargetPolicySummary;

typedef struct {
    size_t block_count;
    size_t op_count;
    size_t call_count;
    size_t call_imm_count;
    size_t call_spill_count;
    size_t call_imm_spill_count;
    size_t call_void_count;
    size_t call_void_imm_count;
    size_t materialize_imm_count;
    size_t alu_count;
    size_t alu_imm_count;
    size_t cmp_count;
    size_t cmp_imm_count;
    size_t load_local_count;
    size_t store_local_count;
    size_t store_local_imm_count;
    size_t load_global_count;
    size_t store_global_count;
    size_t store_global_imm_count;
    size_t return_count;
    size_t return_imm_count;
    size_t return_spill_count;
    size_t compare_branch_count;
    size_t compare_branch_imm_count;
    size_t plain_branch_count;
    size_t blocks_with_calls_count;
    size_t blocks_with_memory_ops_count;
    size_t branch_block_count;
    size_t jump_block_count;
    size_t return_block_count;
    int has_spills;
} MachineSelectFunctionSummary;

typedef struct {
    size_t block_id;
    size_t op_count;
    size_t call_count;
    size_t load_local_count;
    size_t store_local_count;
    size_t store_local_imm_count;
    size_t load_global_count;
    size_t store_global_count;
    size_t store_global_imm_count;
    int has_return_terminator;
    int has_jump_terminator;
    int has_branch_terminator;
    int has_compare_branch_terminator;
} MachineSelectBlockShapeSummary;

typedef struct {
    MachineSelectTerminatorKind kind;
    int has_return_value;
    MachineSelectOperand return_value;
    int has_primary_target;
    size_t primary_target_block_id;
    int has_secondary_target;
    size_t secondary_target_block_id;
    int has_condition;
    MachineSelectOperand condition;
    int has_compare;
    MachineIrBinaryOp compare_op;
    MachineSelectOperand compare_lhs;
    MachineSelectOperand compare_rhs;
} MachineSelectTerminatorSummary;

typedef enum {
    MACHINE_SELECT_BLOCK_FILTER_CALLS = 0,
    MACHINE_SELECT_BLOCK_FILTER_MEMORY_OPS,
    MACHINE_SELECT_BLOCK_FILTER_RETURNS,
    MACHINE_SELECT_BLOCK_FILTER_JUMPS,
    MACHINE_SELECT_BLOCK_FILTER_BRANCHES,
    MACHINE_SELECT_BLOCK_FILTER_COMPARE_BRANCHES,
} MachineSelectBlockFilterKind;

typedef struct {
    MachineSelectProgram program;
    MachineSelectTargetPolicySummary target_policy_summary;
    MachineSelectFunctionSummary *function_summaries;
    size_t function_summary_count;
    size_t *function_block_summary_offsets;
    MachineSelectBlockShapeSummary *block_summaries;
    size_t total_block_summary_count;
    size_t functions_with_calls_count;
    size_t functions_with_spills_count;
    size_t functions_with_memory_ops_count;
    size_t functions_with_branches_count;
    size_t *function_indices_with_calls;
    size_t *function_indices_with_spills;
    size_t *function_indices_with_memory_ops;
    size_t *function_indices_with_branches;
} MachineSelectLowerReport;

void machine_select_program_init(MachineSelectProgram *program);
void machine_select_program_free(MachineSelectProgram *program);
void machine_select_lower_report_init(MachineSelectLowerReport *report);
void machine_select_lower_report_free(MachineSelectLowerReport *report);
int machine_select_get_target_policy_summary(MachineSelectTargetPolicySummary *out_summary);
int machine_select_clone_program(const MachineSelectProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error);

MachineSelectOperand machine_select_operand_none(void);
MachineSelectOperand machine_select_operand_immediate(long long value);
MachineSelectOperand machine_select_operand_register(size_t machine_register_id);
MachineSelectOperand machine_select_operand_spill_slot(size_t spill_slot);
MachineSelectSlotRef machine_select_slot_local(size_t local_id);
MachineSelectSlotRef machine_select_slot_global(size_t global_id);

int machine_select_lower_program_from_machine_ir(const MachineIrProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_lower_program_from_machine_ir_conservative(const MachineIrProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_lower_canonicalized_program_from_machine_ir(const MachineIrProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_lower_canonicalized_program_from_machine_ir_conservative(const MachineIrProgram *source,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_verify_program(const MachineSelectProgram *program, MachineSelectError *error);
int machine_select_dump_program(const MachineSelectProgram *program,
    char **out_text,
    MachineSelectError *error);
int machine_select_lower_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error);
int machine_select_lower_canonicalized_dump_from_machine_ir(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error);
int machine_select_program_get_summary(const MachineSelectProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_select_program_get_target_policy_summary(const MachineSelectProgram *program,
    MachineSelectTargetPolicySummary *out_summary);
int machine_select_verify_current_riscv32_preview_compatibility(const MachineSelectProgram *program,
    MachineSelectError *error);
int machine_select_verify_current_riscv32_preview_bytes_compatibility(const MachineSelectProgram *program,
    MachineSelectError *error);
int machine_select_program_get_function_by_name(const MachineSelectProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function);
int machine_select_program_get_function(const MachineSelectProgram *program,
    size_t function_index,
    const MachineSelectFunction **out_function);
int machine_select_program_get_function_artifact(const MachineSelectProgram *program,
    size_t function_index,
    const MachineSelectFunction **out_function,
    MachineSelectFunctionSummary *out_summary);
int machine_select_program_get_function_summary(const MachineSelectProgram *program,
    size_t function_index,
    MachineSelectFunctionSummary *out_summary);
int machine_select_program_get_function_artifact_by_name(const MachineSelectProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    MachineSelectFunctionSummary *out_summary);
int machine_select_program_get_function_summary_by_name(const MachineSelectProgram *program,
    const char *function_name,
    size_t *out_function_index,
    MachineSelectFunctionSummary *out_summary);
int machine_select_program_get_block_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block);
int machine_select_program_get_block_artifact_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block,
    MachineSelectBlockShapeSummary *out_block_summary,
    MachineSelectTerminatorSummary *out_terminator_summary);
int machine_select_program_get_block_summary_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    MachineSelectBlockShapeSummary *out_summary);
int machine_select_basic_block_get_summary(const MachineSelectBasicBlock *block,
    size_t *out_block_id,
    size_t *out_op_count,
    int *out_has_terminator,
    MachineSelectTerminatorKind *out_terminator_kind);
int machine_select_basic_block_get_terminator_summary(const MachineSelectBasicBlock *block,
    MachineSelectTerminatorSummary *out_summary);
int machine_select_program_get_block_terminator_summary_by_function_name_and_block_id(const MachineSelectProgram *program,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    MachineSelectTerminatorSummary *out_summary);
int machine_select_function_get_summary(const MachineSelectFunction *function,
    const char **out_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count);
int machine_select_function_get_block(const MachineSelectFunction *function,
    size_t block_id,
    const MachineSelectBasicBlock **out_block);
int machine_select_function_compute_summary(const MachineSelectFunction *function,
    MachineSelectFunctionSummary *out_summary);
int machine_select_lower_report_get_summary(const MachineSelectLowerReport *report,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count,
    size_t *out_total_block_summary_count,
    size_t *out_functions_with_calls_count,
    size_t *out_functions_with_spills_count,
    size_t *out_functions_with_memory_ops_count,
    size_t *out_functions_with_branches_count);
int machine_select_lower_report_refresh(MachineSelectLowerReport *report, MachineSelectError *error);
int machine_select_lower_report_get_target_policy_summary_artifact(const MachineSelectLowerReport *report,
    const MachineSelectTargetPolicySummary **out_summary);
int machine_select_lower_report_verify_current_riscv32_preview_compatibility(const MachineSelectLowerReport *report,
    MachineSelectError *error);
int machine_select_lower_report_verify_current_riscv32_preview_bytes_compatibility(const MachineSelectLowerReport *report,
    MachineSelectError *error);
int machine_select_lower_report_get_program(const MachineSelectLowerReport *report,
    const MachineSelectProgram **out_program);
int machine_select_lower_report_get_function(const MachineSelectLowerReport *report,
    size_t function_index,
    const MachineSelectFunction **out_function);
int machine_select_lower_report_get_function_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function);
int machine_select_lower_report_get_function_artifact_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_summary_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_summary_artifact_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_block_summary_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_index,
    const MachineSelectBlockShapeSummary **out_summary);
int machine_select_lower_report_get_block_summary_artifact_by_id(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_id,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary);
int machine_select_lower_report_get_block_summary_artifact_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary);
int machine_select_lower_report_get_block_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block);
int machine_select_lower_report_get_block_artifact_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectFunction **out_function,
    const MachineSelectBasicBlock **out_block,
    const MachineSelectBlockShapeSummary **out_block_summary,
    MachineSelectTerminatorSummary *out_terminator_summary);
int machine_select_lower_report_get_function_block_filter_count(const MachineSelectLowerReport *report,
    size_t function_index,
    MachineSelectBlockFilterKind filter_kind,
    size_t *out_count);
int machine_select_lower_report_get_function_filtered_block_summary(const MachineSelectLowerReport *report,
    size_t function_index,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary);
int machine_select_lower_report_get_function_filtered_block_artifact(const MachineSelectLowerReport *report,
    size_t function_index,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_block_index,
    const MachineSelectBasicBlock **out_block,
    const MachineSelectBlockShapeSummary **out_summary,
    MachineSelectTerminatorSummary *out_terminator_summary);
int machine_select_lower_report_get_function_filtered_block_summary_by_name(
    const MachineSelectLowerReport *report,
    const char *function_name,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectBlockShapeSummary **out_summary);
int machine_select_lower_report_get_function_filtered_block_artifact_by_name(
    const MachineSelectLowerReport *report,
    const char *function_name,
    MachineSelectBlockFilterKind filter_kind,
    size_t filtered_index,
    size_t *out_function_index,
    size_t *out_block_index,
    const MachineSelectBasicBlock **out_block,
    const MachineSelectBlockShapeSummary **out_summary,
    MachineSelectTerminatorSummary *out_terminator_summary);
int machine_select_lower_report_get_block_terminator_summary(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_index,
    MachineSelectTerminatorSummary *out_summary);
int machine_select_lower_report_get_block_terminator_summary_by_id(const MachineSelectLowerReport *report,
    size_t function_index,
    size_t block_id,
    size_t *out_block_index,
    MachineSelectTerminatorSummary *out_summary);
int machine_select_lower_report_get_block_terminator_summary_by_function_name_and_block_id(
    const MachineSelectLowerReport *report,
    const char *function_name,
    size_t block_id,
    size_t *out_function_index,
    size_t *out_block_index,
    MachineSelectTerminatorSummary *out_summary);
int machine_select_lower_report_get_functions_with_calls(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_select_lower_report_get_function_with_calls(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_with_calls_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_functions_with_spills(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_select_lower_report_get_function_with_spills(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_with_spills_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_functions_with_memory_ops(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_select_lower_report_get_function_with_memory_ops(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_with_memory_ops_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_functions_with_branches(const MachineSelectLowerReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_select_lower_report_get_function_with_branches(const MachineSelectLowerReport *report,
    size_t filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_lower_report_get_function_with_branches_by_name(const MachineSelectLowerReport *report,
    const char *function_name,
    size_t *out_filtered_index,
    size_t *out_function_index,
    const MachineSelectFunction **out_function,
    const MachineSelectFunctionSummary **out_summary);
int machine_select_dump_lower_report_artifact(const MachineSelectLowerReport *report,
    char **out_text,
    MachineSelectError *error);
int machine_select_dump_report_from_machine_ir_program(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error);
int machine_select_build_report_from_canonicalized_machine_ir_program(const MachineIrProgram *source,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error);
int machine_select_dump_report_from_canonicalized_machine_ir_program(const MachineIrProgram *source,
    char **out_text,
    MachineSelectError *error);
int machine_select_dump_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineSelectError *error);
int machine_select_build_report_from_program(const MachineSelectProgram *source,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error);
int machine_select_build_report_from_program_dump(const MachineSelectProgram *source,
    char **out_text,
    MachineSelectError *error);
int machine_select_build_program_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_build_program_from_machine_ir_report_conservative(const MachineIrAllocateRewriteReport *report,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_build_program_from_machine_ir_report_conservative_no_phi(
    const MachineIrAllocateRewriteReport *report,
    MachineSelectProgram *out_program,
    MachineSelectError *error);
int machine_select_build_report_from_machine_ir_program(const MachineIrProgram *source,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error);
int machine_select_build_report_from_machine_ir_report(const MachineIrAllocateRewriteReport *report,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error);
int machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineSelectLowerReport *out_report,
    MachineSelectError *error);
int machine_select_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineSelectError *error);

#endif
