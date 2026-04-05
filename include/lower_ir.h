#ifndef LOWER_IR_H
#define LOWER_IR_H

#include <stddef.h>

#include "ir.h"

typedef struct {
    int line;
    int column;
    char message[512];
} LowerIrError;

typedef enum {
    LOWER_IR_VALUE_IMMEDIATE = 0,
    LOWER_IR_VALUE_TEMP,
} LowerIrValueKind;

typedef struct {
    LowerIrValueKind kind;
    long long immediate;
    size_t temp_id;
} LowerIrValueRef;

typedef enum {
    LOWER_IR_SLOT_LOCAL = 0,
    LOWER_IR_SLOT_GLOBAL,
} LowerIrSlotKind;

typedef struct {
    LowerIrSlotKind kind;
    size_t id;
} LowerIrSlotRef;

typedef enum {
    LOWER_IR_BINARY_ADD = 0,
    LOWER_IR_BINARY_SUB,
    LOWER_IR_BINARY_MUL,
    LOWER_IR_BINARY_DIV,
    LOWER_IR_BINARY_MOD,
    LOWER_IR_BINARY_BIT_AND,
    LOWER_IR_BINARY_BIT_XOR,
    LOWER_IR_BINARY_BIT_OR,
    LOWER_IR_BINARY_SHIFT_LEFT,
    LOWER_IR_BINARY_SHIFT_RIGHT,
    LOWER_IR_BINARY_EQ,
    LOWER_IR_BINARY_NE,
    LOWER_IR_BINARY_LT,
    LOWER_IR_BINARY_LE,
    LOWER_IR_BINARY_GT,
    LOWER_IR_BINARY_GE,
} LowerIrBinaryOp;

typedef enum {
    LOWER_IR_INSTR_MOV = 0,
    LOWER_IR_INSTR_BINARY,
    LOWER_IR_INSTR_CALL,
    LOWER_IR_INSTR_LOAD_LOCAL,
    LOWER_IR_INSTR_STORE_LOCAL,
    LOWER_IR_INSTR_LOAD_GLOBAL,
    LOWER_IR_INSTR_STORE_GLOBAL,
} LowerIrInstructionKind;

typedef struct {
    size_t id;
    char *source_name;
    int is_parameter;
} LowerIrLocal;

typedef struct {
    size_t id;
    char *name;
    int has_initializer;
    long long initializer_value;
    int has_runtime_initializer;
} LowerIrGlobal;

typedef struct {
    LowerIrInstructionKind kind;
    int has_result;
    LowerIrValueRef result;
    union {
        LowerIrValueRef mov_value;
        struct {
            LowerIrBinaryOp op;
            LowerIrValueRef lhs;
            LowerIrValueRef rhs;
        } binary;
        struct {
            char *callee_name;
            LowerIrValueRef *args;
            size_t arg_count;
        } call;
        LowerIrSlotRef load_slot;
        struct {
            LowerIrSlotRef slot;
            LowerIrValueRef value;
        } store;
    } as;
} LowerIrInstruction;

typedef enum {
    LOWER_IR_TERM_RETURN = 0,
    LOWER_IR_TERM_JUMP,
    LOWER_IR_TERM_BRANCH,
} LowerIrTerminatorKind;

typedef struct {
    LowerIrTerminatorKind kind;
    union {
        LowerIrValueRef return_value;
        size_t jump_target;
        struct {
            LowerIrValueRef condition;
            size_t then_target;
            size_t else_target;
        } branch;
    } as;
} LowerIrTerminator;

typedef struct {
    size_t id;
    LowerIrInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    int has_terminator;
    LowerIrTerminator terminator;
} LowerIrBasicBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    LowerIrLocal *locals;
    size_t local_count;
    size_t local_capacity;
    LowerIrBasicBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t next_temp_id;
} LowerIrFunction;

typedef struct {
    LowerIrGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    LowerIrFunction *functions;
    size_t function_count;
    size_t function_capacity;
} LowerIrProgram;

void lower_ir_program_init(LowerIrProgram *program);
void lower_ir_program_free(LowerIrProgram *program);

LowerIrValueRef lower_ir_value_immediate(long long value);
LowerIrValueRef lower_ir_value_temp(size_t temp_id);
LowerIrSlotRef lower_ir_slot_local(size_t local_id);
LowerIrSlotRef lower_ir_slot_global(size_t global_id);

int lower_ir_program_append_global(LowerIrProgram *program,
    const char *name,
    LowerIrGlobal **out_global,
    LowerIrError *error);
int lower_ir_program_append_function(LowerIrProgram *program,
    const char *name,
    int has_body,
    LowerIrFunction **out_function,
    LowerIrError *error);
int lower_ir_function_append_local(LowerIrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    LowerIrError *error);
int lower_ir_function_append_block(LowerIrFunction *function,
    size_t *out_block_id,
    LowerIrBasicBlock **out_block,
    LowerIrError *error);
size_t lower_ir_function_allocate_temp(LowerIrFunction *function);
int lower_ir_block_append_instruction(LowerIrBasicBlock *block,
    const LowerIrInstruction *instruction,
    LowerIrError *error);
int lower_ir_block_set_return(LowerIrBasicBlock *block,
    LowerIrValueRef value,
    LowerIrError *error);
int lower_ir_block_set_jump(LowerIrBasicBlock *block,
    size_t target_block_id,
    LowerIrError *error);
int lower_ir_block_set_branch(LowerIrBasicBlock *block,
    LowerIrValueRef condition,
    size_t then_target,
    size_t else_target,
    LowerIrError *error);

int lower_ir_lower_from_ir(const IrProgram *program,
    LowerIrProgram *out_program,
    LowerIrError *error);

int lower_ir_verify_program(const LowerIrProgram *program, LowerIrError *error);
int lower_ir_dump_program(const LowerIrProgram *program, char **out_text);

#endif
