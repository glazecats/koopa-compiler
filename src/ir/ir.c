#include "ir.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char **names;
    size_t *local_ids;
    size_t count;
    size_t capacity;
} IrLowerScope;

typedef struct {
    IrLowerScope *frames;
    size_t count;
    size_t capacity;
} IrLowerScopeStack;

typedef struct {
    size_t break_target;
    size_t continue_target;
} IrLoopTarget;

typedef struct {
    IrLoopTarget *items;
    size_t count;
    size_t capacity;
} IrLoopTargetStack;

typedef struct {
    IrProgram *program;
    IrFunction *function;
    size_t block_id;
    int has_block;
    IrLowerScopeStack scopes;
    IrLoopTargetStack loop_targets;
    IrError *error;
} IrLowerContext;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} IrStringBuilder;

static void ir_lower_scope_stack_free(IrLowerScopeStack *stack);
static int ir_lower_scope_stack_push(IrLowerScopeStack *stack);
static void ir_lower_scope_stack_pop(IrLowerScopeStack *stack);
static int ir_lower_scope_add_binding(IrLowerScopeStack *stack,
    const char *name,
    size_t local_id);
static int ir_lower_scope_lookup(const IrLowerScopeStack *stack,
    const char *name,
    size_t *out_local_id);

static void ir_basic_block_free(IrBasicBlock *block);
static void ir_function_free(IrFunction *function);
static void ir_global_free(IrGlobal *global);
static IrFunction *ir_program_find_function_mut(IrProgram *program, const char *name);
static IrGlobal *ir_program_find_global_mut(IrProgram *program, const char *name);
static const IrGlobal *ir_program_find_global(const IrProgram *program, const char *name);
static const IrGlobal *ir_program_get_global(const IrProgram *program, size_t global_id);
static int ir_program_append_global(IrProgram *program,
    const char *name,
    IrGlobal **out_global,
    IrError *error);
static int ir_program_append_function(IrProgram *program,
    const char *name,
    IrFunction **out_function,
    IrError *error);
static int ir_function_append_local(IrFunction *function,
    const char *source_name,
    int is_parameter,
    size_t *out_local_id,
    IrError *error);
static int ir_function_append_block(IrFunction *function,
    size_t *out_block_id,
    IrBasicBlock **out_block,
    IrError *error);
static int ir_block_append_instruction(IrBasicBlock *block,
    const IrInstruction *instruction,
    IrError *error);
static int ir_block_prepend_instruction(IrBasicBlock *block,
    const IrInstruction *instruction,
    IrError *error);
static int ir_block_set_return(IrBasicBlock *block, IrValueRef value, IrError *error);
static int ir_block_set_jump(IrBasicBlock *block, size_t target_block_id, IrError *error);
static int ir_block_set_branch(IrBasicBlock *block,
    IrValueRef condition,
    size_t then_target,
    size_t else_target,
    IrError *error);
static IrBasicBlock *ir_function_get_block_mut(IrFunction *function, size_t block_id);
static const IrLocal *ir_function_get_local(const IrFunction *function, size_t local_id);
static IrBasicBlock *ir_lower_current_block(IrLowerContext *ctx);

static int ir_append_string(IrStringBuilder *builder, const char *text);
static int ir_append_format(IrStringBuilder *builder, const char *fmt, ...);
static int ir_append_value_name(IrStringBuilder *builder,
    const IrProgram *program,
    const IrFunction *function,
    IrValueRef value);

static int ir_emit_mov(IrLowerContext *ctx,
    IrValueRef destination,
    IrValueRef value);
static int ir_emit_binary(IrLowerContext *ctx,
    IrValueRef destination,
    IrBinaryOp op,
    IrValueRef lhs,
    IrValueRef rhs);
static int ir_emit_call(IrLowerContext *ctx,
    IrValueRef destination,
    const char *callee_name,
    const IrValueRef *args,
    size_t arg_count);
static size_t ir_allocate_temp(IrFunction *function);

static int ir_lower_return_statement(IrLowerContext *ctx, const AstStatement *stmt);
static IrBinaryOp ir_binary_op_from_token(TokenType token_type, int *out_supported);
static const AstExpression *ir_unwrap_paren_expression(const AstExpression *expr);
static const char *ir_extract_direct_call_callee_name(const AstExpression *callee);
static int ir_lower_expression(IrLowerContext *ctx,
    const AstExpression *expr,
    IrValueRef *out_value);
static int ir_lower_while_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_for_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_if_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_statement(IrLowerContext *ctx, const AstStatement *stmt);
static int ir_lower_function(IrProgram *program,
    const AstExternal *external,
    IrError *error);
static int ir_dependency_try_eval_condition_truthiness(const AstExpression *expr,
    int *out_known,
    int *out_truthy);
static int ir_collect_global_initializer_dependencies(const IrProgram *program,
    const AstProgram *ast_program,
    const AstExpression *expr,
    size_t **deps,
    size_t *dep_count,
    size_t *dep_capacity,
    IrError *error);
static int ir_try_format_runtime_global_dependency_path(const IrProgram *program,
    const AstProgram *ast_program,
    size_t source_global_id,
    const AstExpression *source_expr,
    size_t blocked_global_id,
    char *out_path,
    size_t out_path_size,
    int *out_via_callee);
static int ir_try_format_runtime_global_cycle(const IrProgram *program,
    const AstProgram *ast_program,
    size_t source_global_id,
    size_t blocked_global_id,
    char *out_chain,
    size_t out_chain_size);
static int ir_verify_value_ref(const IrProgram *program,
    const IrFunction *function,
    IrValueRef value,
    const char *role,
    IrError *error);
static int ir_verify_function(const IrProgram *program,
    const IrFunction *function,
    IrError *error);

static void ir_set_error(IrError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }

    error->line = line;
    error->column = column;

    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static int ir_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity) {
    size_t next_capacity;

    if (!out_next_capacity || initial == 0 || elem_size == 0) {
        return 0;
    }

    if (current == 0) {
        next_capacity = initial;
    } else {
        if (current > ((size_t)-1) / 2) {
            return 0;
        }
        next_capacity = current * 2;
    }

    if (next_capacity > ((size_t)-1) / elem_size) {
        return 0;
    }

    *out_next_capacity = next_capacity;
    return 1;
}

static IrValueRef ir_value_immediate(long long value) {
    IrValueRef ref;

    ref.kind = IR_VALUE_IMMEDIATE;
    ref.immediate = value;
    ref.id = 0;
    return ref;
}

static IrValueRef ir_value_local(size_t id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_LOCAL;
    ref.immediate = 0;
    ref.id = id;
    return ref;
}

static IrValueRef ir_value_global(size_t id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_GLOBAL;
    ref.immediate = 0;
    ref.id = id;
    return ref;
}

static IrValueRef ir_value_temp(size_t id) {
    IrValueRef ref;

    ref.kind = IR_VALUE_TEMP;
    ref.immediate = 0;
    ref.id = id;
    return ref;
}

static const char *ir_binary_op_name(IrBinaryOp op) {
    switch (op) {
    case IR_BINARY_ADD:
        return "add";
    case IR_BINARY_SUB:
        return "sub";
    case IR_BINARY_MUL:
        return "mul";
    case IR_BINARY_DIV:
        return "div";
    case IR_BINARY_MOD:
        return "mod";
    case IR_BINARY_BIT_AND:
        return "and";
    case IR_BINARY_BIT_XOR:
        return "xor";
    case IR_BINARY_BIT_OR:
        return "or";
    case IR_BINARY_SHIFT_LEFT:
        return "shl";
    case IR_BINARY_SHIFT_RIGHT:
        return "shr";
    case IR_BINARY_EQ:
        return "eq";
    case IR_BINARY_NE:
        return "ne";
    case IR_BINARY_LT:
        return "lt";
    case IR_BINARY_LE:
        return "le";
    case IR_BINARY_GT:
        return "gt";
    case IR_BINARY_GE:
        return "ge";
    default:
        return "unknown";
    }
}

#define IR_SPLIT_AGGREGATOR 1
#include "ir_core.inc"
#include "ir_lower_scope.inc"
#include "ir_lower_expr.inc"
#include "ir_lower_stmt.inc"
#include "ir_global_dep.inc"
#include "ir_global_init.inc"
#include "ir_verify.inc"
#include "ir_dump.inc"
#undef IR_SPLIT_AGGREGATOR