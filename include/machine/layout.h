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

void machine_layout_program_init(MachineLayoutProgram *program);
void machine_layout_program_free(MachineLayoutProgram *program);

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
int machine_layout_function_compute_summary(const MachineLayoutFunction *function,
    MachineLayoutFunctionSummary *out_summary);

#endif
