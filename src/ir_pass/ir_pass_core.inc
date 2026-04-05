#if defined(__INTELLISENSE__) && !defined(IR_PASS_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(IR_PASS_SPLIT_AGGREGATOR)
#define IR_PASS_SPLIT_INTELLISENSE_CONTEXT
#include "ir_pass.c"
#else

#ifndef IR_PASS_SPLIT_AGGREGATOR
#include "ir_pass.h"

#include <stdarg.h>
#include <stdio.h>
#endif

static void ir_pass_set_error(IrError *error, int line, int column, const char *fmt, ...) {
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

static IrValueRef ir_pass_value_immediate(long long value) {
    IrValueRef ref;

    ref.kind = IR_VALUE_IMMEDIATE;
    ref.immediate = value;
    ref.id = 0;
    return ref;
}

static int ir_pass_value_ref_equals(IrValueRef lhs, IrValueRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }

    if (lhs.kind == IR_VALUE_IMMEDIATE) {
        return lhs.immediate == rhs.immediate;
    }

    return lhs.id == rhs.id;
}

static int ir_pass_binary_op_requires_runtime_validation(IrBinaryOp op) {
    switch (op) {
    case IR_BINARY_DIV:
    case IR_BINARY_MOD:
    case IR_BINARY_SHIFT_LEFT:
    case IR_BINARY_SHIFT_RIGHT:
        return 1;
    default:
        return 0;
    }
}

static int ir_pass_instruction_has_side_effects(const IrInstruction *instruction) {
    if (!instruction) {
        return 1;
    }

    switch (instruction->kind) {
    case IR_INSTR_MOV:
        return 0;
    case IR_INSTR_BINARY:
        return ir_pass_binary_op_requires_runtime_validation(instruction->as.binary.op);
    case IR_INSTR_CALL:
    default:
        return 1;
    }
}

#endif
