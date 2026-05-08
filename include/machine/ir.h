#ifndef MACHINE_IR_H
#define MACHINE_IR_H

#include <stddef.h>

#include "value_ssa.h"
#include "value_ssa_machine.h"

typedef struct {
    int line;
    int column;
    char message[512];
} MachineIrError;

typedef enum {
    MACHINE_IR_OPERAND_NONE = 0,
    MACHINE_IR_OPERAND_IMMEDIATE,
    MACHINE_IR_OPERAND_REGISTER,
    MACHINE_IR_OPERAND_SPILL_SLOT,
} MachineIrOperandKind;

typedef struct {
    MachineIrOperandKind kind;
    long long immediate;
    size_t machine_register_id;
    size_t spill_slot;
} MachineIrOperand;

typedef enum {
    MACHINE_IR_SLOT_LOCAL = 0,
    MACHINE_IR_SLOT_GLOBAL,
} MachineIrSlotKind;

typedef struct {
    MachineIrSlotKind kind;
    size_t id;
} MachineIrSlotRef;

typedef enum {
    MACHINE_IR_BINARY_ADD = 0,
    MACHINE_IR_BINARY_SUB,
    MACHINE_IR_BINARY_MUL,
    MACHINE_IR_BINARY_DIV,
    MACHINE_IR_BINARY_MOD,
    MACHINE_IR_BINARY_BIT_AND,
    MACHINE_IR_BINARY_BIT_XOR,
    MACHINE_IR_BINARY_BIT_OR,
    MACHINE_IR_BINARY_SHIFT_LEFT,
    MACHINE_IR_BINARY_SHIFT_RIGHT,
    MACHINE_IR_BINARY_EQ,
    MACHINE_IR_BINARY_NE,
    MACHINE_IR_BINARY_LT,
    MACHINE_IR_BINARY_LE,
    MACHINE_IR_BINARY_GT,
    MACHINE_IR_BINARY_GE,
} MachineIrBinaryOp;

typedef struct {
    size_t predecessor_block_id;
    MachineIrOperand value;
} MachineIrPhiInput;

typedef struct {
    MachineIrOperand result;
    MachineIrPhiInput *inputs;
    size_t input_count;
    size_t input_capacity;
} MachineIrPhi;

typedef enum {
    MACHINE_IR_INSTR_MOV = 0,
    MACHINE_IR_INSTR_BINARY,
    MACHINE_IR_INSTR_CALL,
    MACHINE_IR_INSTR_ADDR_LOCAL,
    MACHINE_IR_INSTR_ADDR_GLOBAL,
    MACHINE_IR_INSTR_LOAD_LOCAL,
    MACHINE_IR_INSTR_STORE_LOCAL,
    MACHINE_IR_INSTR_LOAD_GLOBAL,
    MACHINE_IR_INSTR_STORE_GLOBAL,
    MACHINE_IR_INSTR_LOAD_INDIRECT,
    MACHINE_IR_INSTR_STORE_INDIRECT,
} MachineIrInstructionKind;

typedef struct {
    MachineIrInstructionKind kind;
    int has_result;
    MachineIrOperand result;
    union {
        MachineIrOperand mov_value;
        struct {
            MachineIrBinaryOp op;
            MachineIrOperand lhs;
            MachineIrOperand rhs;
        } binary;
        struct {
            char *callee_name;
            MachineIrOperand *args;
            size_t arg_count;
        } call;
        MachineIrSlotRef addr_slot;
        MachineIrSlotRef load_slot;
        struct {
            MachineIrSlotRef slot;
            MachineIrOperand value;
        } store;
        MachineIrOperand load_indirect_addr;
        struct {
            MachineIrOperand addr;
            MachineIrOperand value;
        } store_indirect;
    } as;
} MachineIrInstruction;

typedef enum {
    MACHINE_IR_TERM_RETURN = 0,
    MACHINE_IR_TERM_JUMP,
    MACHINE_IR_TERM_BRANCH,
} MachineIrTerminatorKind;

typedef struct {
    MachineIrTerminatorKind kind;
    union {
        MachineIrOperand return_value;
        size_t jump_target;
        struct {
            MachineIrOperand condition;
            size_t then_target;
            size_t else_target;
        } branch;
    } as;
} MachineIrTerminator;

typedef struct {
    size_t id;
    char *source_name;
    int is_parameter;
} MachineIrLocal;

typedef struct {
    size_t id;
    char *name;
    size_t byte_size;
    int has_initializer;
    long long initializer_value;
    int has_runtime_initializer;
} MachineIrGlobal;

typedef struct {
    size_t register_id;
    char *name;
    ValueSsaMachineRegisterClass register_class;
    unsigned char allocatable;
    unsigned char caller_clobbered;
    unsigned char callee_preserved;
} MachineIrRegisterDesc;

typedef struct {
    size_t register_count;
    MachineIrRegisterDesc *registers;
} MachineIrRegisterBank;

typedef struct {
    size_t id;
    MachineIrPhi *phis;
    size_t phi_count;
    size_t phi_capacity;
    MachineIrInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    int has_terminator;
    MachineIrTerminator terminator;
} MachineIrBasicBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    MachineIrLocal *locals;
    size_t local_count;
    size_t local_capacity;
    MachineIrBasicBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t spill_slot_count;
} MachineIrFunction;

typedef struct {
    MachineIrRegisterBank register_bank;
    MachineIrGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    MachineIrFunction *functions;
    size_t function_count;
    size_t function_capacity;
} MachineIrProgram;

typedef struct {
    size_t block_count;
    size_t phi_count;
    size_t instruction_count;
    size_t call_count;
    size_t spill_slot_count;
    size_t load_local_count;
    size_t store_local_count;
    size_t load_global_count;
    size_t store_global_count;
    size_t memory_op_count;
    size_t blocks_with_phi_count;
    size_t blocks_with_call_count;
    size_t blocks_with_memory_ops_count;
    size_t branch_block_count;
    size_t jump_block_count;
    size_t return_block_count;
} MachineIrFunctionShapeSummary;

typedef struct {
    size_t block_id;
    size_t phi_count;
    size_t instruction_count;
    size_t call_count;
    size_t load_local_count;
    size_t store_local_count;
    size_t load_global_count;
    size_t store_global_count;
    size_t memory_op_count;
    int has_return_terminator;
    int has_jump_terminator;
    int has_branch_terminator;
} MachineIrBlockShapeSummary;

typedef struct {
    size_t allocation_rounds;
    size_t rewrite_rounds;
    MachineIrProgram program;
    MachineIrFunctionShapeSummary *function_shape_summaries;
    size_t *function_block_summary_offsets;
    MachineIrBlockShapeSummary *block_shape_summaries;
    size_t total_block_summary_count;
    size_t *function_phi_counts;
    size_t *function_call_counts;
    size_t *function_spill_slot_counts;
    size_t *function_load_local_counts;
    size_t *function_store_local_counts;
    size_t *function_load_global_counts;
    size_t *function_store_global_counts;
    size_t *function_memory_op_counts;
    size_t functions_with_phi_count;
    size_t functions_with_call_count;
    size_t functions_with_spills_count;
    size_t functions_with_memory_ops_count;
    size_t functions_register_only_count;
    size_t *function_indices_with_phi;
    size_t *function_indices_with_call;
    size_t *function_indices_with_spills;
    size_t *function_indices_with_memory_ops;
    size_t *function_indices_register_only;
} MachineIrAllocateRewriteReport;

void machine_ir_program_init(MachineIrProgram *program);
void machine_ir_program_free(MachineIrProgram *program);
void machine_ir_register_bank_init(MachineIrRegisterBank *bank);
void machine_ir_register_bank_free(MachineIrRegisterBank *bank);
void machine_ir_allocate_rewrite_report_init(MachineIrAllocateRewriteReport *report);
void machine_ir_allocate_rewrite_report_free(MachineIrAllocateRewriteReport *report);

MachineIrOperand machine_ir_operand_none(void);
MachineIrOperand machine_ir_operand_immediate(long long value);
MachineIrOperand machine_ir_operand_register(size_t machine_register_id);
MachineIrOperand machine_ir_operand_spill_slot(size_t spill_slot);
MachineIrSlotRef machine_ir_slot_local(size_t local_id);
MachineIrSlotRef machine_ir_slot_global(size_t global_id);

int machine_ir_program_append_global(MachineIrProgram *program,
    const char *name,
    MachineIrGlobal **out_global,
    MachineIrError *error);
int machine_ir_program_append_function(MachineIrProgram *program,
    const char *name,
    int has_body,
    MachineIrFunction **out_function,
    MachineIrError *error);
int machine_ir_function_append_local(MachineIrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    MachineIrError *error);
int machine_ir_function_append_block(MachineIrFunction *function,
    size_t *out_block_id,
    MachineIrBasicBlock **out_block,
    MachineIrError *error);
int machine_ir_block_append_phi(MachineIrBasicBlock *block,
    MachineIrOperand result,
    size_t *out_phi_index,
    MachineIrPhi **out_phi,
    MachineIrError *error);
int machine_ir_phi_append_input(MachineIrPhi *phi,
    size_t predecessor_block_id,
    MachineIrOperand value,
    MachineIrError *error);
int machine_ir_block_append_instruction(MachineIrBasicBlock *block,
    const MachineIrInstruction *instruction,
    MachineIrError *error);
int machine_ir_block_set_return(MachineIrBasicBlock *block,
    MachineIrOperand value,
    MachineIrError *error);
int machine_ir_block_set_void_return(MachineIrBasicBlock *block, MachineIrError *error);
int machine_ir_block_set_jump(MachineIrBasicBlock *block,
    size_t target_block_id,
    MachineIrError *error);
int machine_ir_block_set_branch(MachineIrBasicBlock *block,
    MachineIrOperand condition,
    size_t then_target,
    size_t else_target,
    MachineIrError *error);

int machine_ir_copy_register_bank_from_value_ssa(const ValueSsaMachineRegisterBank *source_bank,
    MachineIrRegisterBank *out_bank,
    MachineIrError *error);
int machine_ir_lower_function_from_value_ssa(const ValueSsaFunction *function,
    const ValueSsaFunctionMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrFunction *out_function,
    MachineIrError *error);
int machine_ir_lower_program_from_value_ssa(const ValueSsaProgram *program,
    const ValueSsaProgramMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error);
int machine_ir_allocate_lower_program_from_value_ssa(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error);
int machine_ir_allocate_lower_program_from_value_ssa_flat(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrProgram *out_program,
    MachineIrError *error);

int machine_ir_register_bank_get_summary(const MachineIrRegisterBank *bank,
    size_t *out_register_count,
    size_t *out_allocatable_count,
    size_t *out_caller_clobbered_count,
    size_t *out_callee_preserved_count);
int machine_ir_register_bank_get_register(const MachineIrRegisterBank *bank,
    size_t register_index,
    const MachineIrRegisterDesc **out_desc);
int machine_ir_register_bank_find_register_by_name(const MachineIrRegisterBank *bank,
    const char *register_name,
    size_t *out_register_index,
    const MachineIrRegisterDesc **out_desc);
int machine_ir_basic_block_get_summary(const MachineIrBasicBlock *block,
    size_t *out_block_id,
    size_t *out_phi_count,
    size_t *out_instruction_count,
    int *out_has_terminator,
    MachineIrTerminatorKind *out_terminator_kind);
int machine_ir_function_get_summary(const MachineIrFunction *function,
    const char **out_function_name,
    int *out_has_body,
    size_t *out_parameter_count,
    size_t *out_local_count,
    size_t *out_block_count,
    size_t *out_spill_slot_count);
int machine_ir_function_get_block(const MachineIrFunction *function,
    size_t block_id,
    const MachineIrBasicBlock **out_block);
int machine_ir_program_get_summary(const MachineIrProgram *program,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_ir_program_get_function(const MachineIrProgram *program,
    size_t function_index,
    const MachineIrFunction **out_function);
int machine_ir_program_get_function_by_name(const MachineIrProgram *program,
    const char *function_name,
    size_t *out_function_index,
    const MachineIrFunction **out_function);
int machine_ir_allocate_rewrite_report_get_summary(const MachineIrAllocateRewriteReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds,
    size_t *out_register_count,
    size_t *out_global_count,
    size_t *out_function_count);
int machine_ir_allocate_rewrite_report_get_program(const MachineIrAllocateRewriteReport *report,
    const MachineIrProgram **out_program);
int machine_ir_allocate_rewrite_report_get_function(const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    const MachineIrFunction **out_function);
int machine_ir_allocate_rewrite_report_get_function_by_name(const MachineIrAllocateRewriteReport *report,
    const char *function_name,
    size_t *out_function_index,
    const MachineIrFunction **out_function);
int machine_ir_allocate_rewrite_report_get_function_shape_summary(const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    size_t *out_phi_count,
    size_t *out_call_count,
    size_t *out_spill_slot_count,
    size_t *out_load_local_count,
    size_t *out_store_local_count,
    size_t *out_load_global_count,
    size_t *out_store_global_count,
    size_t *out_memory_op_count);
int machine_ir_allocate_rewrite_report_get_function_shape_summary_artifact(
    const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    const MachineIrFunctionShapeSummary **out_summary);
int machine_ir_allocate_rewrite_report_get_block_shape_summary(const MachineIrAllocateRewriteReport *report,
    size_t function_index,
    size_t block_index,
    const MachineIrBlockShapeSummary **out_summary);
int machine_ir_allocate_rewrite_report_get_functions_with_phi(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_ir_allocate_rewrite_report_get_functions_with_call(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_ir_allocate_rewrite_report_get_functions_with_spills(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_ir_allocate_rewrite_report_get_functions_with_memory_ops(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_ir_allocate_rewrite_report_get_functions_register_only(const MachineIrAllocateRewriteReport *report,
    size_t *out_count,
    const size_t **out_function_indices);
int machine_ir_allocate_rewrite_report_refresh_shape(MachineIrAllocateRewriteReport *report,
    MachineIrError *error);

int machine_ir_verify_program(const MachineIrProgram *program, MachineIrError *error);
int machine_ir_dump_program(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error);
int machine_ir_dump_allocate_rewrite_report(const MachineIrAllocateRewriteReport *report,
    char **out_text,
    MachineIrError *error);
int machine_ir_lower_program_from_value_ssa_dump(const ValueSsaProgram *program,
    const ValueSsaProgramMachineAllocationView *machine_view,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error);
int machine_ir_allocate_lower_program_from_value_ssa_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error);
int machine_ir_allocate_lower_program_from_value_ssa_flat_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error);
int machine_ir_allocate_and_rewrite_program_single_block_spills(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrProgram *out_program,
    MachineIrError *error);
int machine_ir_allocate_and_rewrite_program_single_block_spills_flat(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrProgram *out_program,
    MachineIrError *error);
int machine_ir_allocate_and_rewrite_program_single_block_spills_dump(const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error);
int machine_ir_allocate_and_rewrite_program_single_block_spills_flat_dump(const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_program_only_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    const ValueSsaMachineRegisterBank *bank,
    char **out_text,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error);
int machine_ir_eliminate_phi_nodes(MachineIrProgram *program,
    MachineIrError *error);
int machine_ir_eliminate_phi_nodes_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error);
int machine_ir_eliminate_phi_nodes_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error);
int machine_ir_eliminate_trivial_moves(MachineIrProgram *program,
    MachineIrError *error);
int machine_ir_eliminate_trivial_moves_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error);
int machine_ir_eliminate_trivial_moves_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error);
int machine_ir_canonicalize_program(MachineIrProgram *program,
    MachineIrError *error);
int machine_ir_build_canonicalized_program(const MachineIrProgram *source,
    MachineIrProgram *out_program,
    MachineIrError *error);
int machine_ir_canonicalize_program_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error);
int machine_ir_canonicalize_program_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error);
int machine_ir_cleanup_after_phi_elimination(MachineIrProgram *program,
    MachineIrError *error);
int machine_ir_cleanup_after_phi_elimination_in_report(MachineIrAllocateRewriteReport *report,
    MachineIrError *error);
int machine_ir_cleanup_after_phi_elimination_dump(const MachineIrProgram *program,
    char **out_text,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_phi_eliminated_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_cleaned_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    MachineIrAllocateRewriteReport *out_report,
    MachineIrError *error);
int machine_ir_build_allocate_and_rewrite_program_single_block_spills_canonicalized_flat_report_dump(
    const ValueSsaProgram *program,
    size_t color_budget,
    size_t machine_register_count,
    char **out_text,
    MachineIrError *error);

#endif
