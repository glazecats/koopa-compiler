#ifndef IR_H
#define IR_H

#include <stddef.h>

#include "ast.h"

typedef struct {
    int line;
    int column;
    char message[512];
} IrError;

typedef enum {
    IR_VALUE_IMMEDIATE = 0,
    IR_VALUE_LOCAL,
    IR_VALUE_GLOBAL,
    IR_VALUE_TEMP,
} IrValueKind;

typedef struct {
    IrValueKind kind;
    long long immediate;
    size_t id;
} IrValueRef;

typedef enum {
    IR_BINARY_ADD = 0,
    IR_BINARY_SUB,
    IR_BINARY_MUL,
    IR_BINARY_DIV,
    IR_BINARY_MOD,
    IR_BINARY_BIT_AND,
    IR_BINARY_BIT_XOR,
    IR_BINARY_BIT_OR,
    IR_BINARY_SHIFT_LEFT,
    IR_BINARY_SHIFT_RIGHT,
    IR_BINARY_EQ,
    IR_BINARY_NE,
    IR_BINARY_LT,
    IR_BINARY_LE,
    IR_BINARY_GT,
    IR_BINARY_GE,
} IrBinaryOp;

typedef enum {
    IR_INSTR_MOV = 0,
    IR_INSTR_BINARY,
    IR_INSTR_CALL,
} IrInstructionKind;

typedef struct {
    size_t id;
    char *source_name;
    int is_parameter;
} IrLocal;

typedef struct {
    size_t id;
    char *name;
    int has_initializer;
    long long initializer_value;
    int has_runtime_initializer;
} IrGlobal;

typedef struct {
    IrInstructionKind kind;
    IrValueRef result;
    union {
        IrValueRef mov_value;
        struct {
            IrBinaryOp op;
            IrValueRef lhs;
            IrValueRef rhs;
        } binary;
        struct {
            char *callee_name;
            IrValueRef *args;
            size_t arg_count;
        } call;
    } as;
} IrInstruction;

typedef enum {
    IR_TERM_RETURN = 0,
    IR_TERM_JUMP,
    IR_TERM_BRANCH,
} IrTerminatorKind;

typedef struct {
    IrTerminatorKind kind;
    union {
        IrValueRef return_value;
        size_t jump_target;
        struct {
            IrValueRef condition;
            size_t then_target;
            size_t else_target;
        } branch;
    } as;
} IrTerminator;

typedef struct {
    size_t id;
    IrInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    int has_terminator;
    IrTerminator terminator;
} IrBasicBlock;

typedef struct {
    char *name;
    int has_body;
    size_t parameter_count;
    IrLocal *locals;
    size_t local_count;
    size_t local_capacity;
    IrBasicBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t next_temp_id;
} IrFunction;

typedef struct {
    IrGlobal *globals;
    size_t global_count;
    size_t global_capacity;
    IrFunction *functions;
    size_t function_count;
    size_t function_capacity;
} IrProgram;

void ir_program_init(IrProgram *program);
void ir_program_free(IrProgram *program);

/*
 * Lowers a semantically valid AST into the canonical IR.
 * out_program should be initialized via ir_program_init or zeroed.
 * Returns 1 on success, 0 on allocation/lowering error.
 */
int ir_lower_program(const AstProgram *ast_program, IrProgram *out_program, IrError *error);

/*
 * Verifies structural IR invariants for a fully built program.
 * Returns 1 when the IR is structurally valid, 0 and fills error otherwise.
 */
int ir_verify_program(const IrProgram *program, IrError *error);

/*
 * Dumps the IR into a newly allocated string owned by the caller.
 * Returns 1 on success, 0 on allocation/contract error.
 */
int ir_dump_program(const IrProgram *program, char **out_text);

#endif