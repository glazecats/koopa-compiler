#include "value_ssa.h"
#include "value_ssa_pass.h"
#include "memory_ssa_pass.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static void value_ssa_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...);
static int value_ssa_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity);
static void value_ssa_instruction_free(ValueSsaInstruction *instruction);
static void value_ssa_phi_free(ValueSsaPhi *phi);
static void value_ssa_basic_block_free(ValueSsaBasicBlock *block);
static int value_ssa_binary_op_is_removable_dead_def(ValueSsaBinaryOp op);
static int value_ssa_instruction_has_side_effects(ValueSsaInstructionKind kind);
static int value_ssa_instruction_is_removable_dead_def(const ValueSsaInstruction *instruction);
static int value_ssa_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs);
static size_t value_ssa_pass_get_single_idom_predecessor(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id);
static int value_ssa_pass_has_single_idom_entry(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id);
static int value_ssa_mark_reachable_blocks(const ValueSsaFunction *function,
    unsigned char *reachable,
    ValueSsaError *error);

static int value_ssa_trace_enabled(void) {
    const char *flag = getenv("VALUE_SSA_TRACE_TIMING");

    return flag && flag[0] != '\0' && strcmp(flag, "0") != 0;
}

static double value_ssa_now_s(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

static void value_ssa_trace_timing(const char *stage, double elapsed_s) {
    if (!stage || !value_ssa_trace_enabled()) {
        return;
    }

    fprintf(stderr, "[value-ssa-timing] %s %.3f\n", stage, elapsed_s);
}

static long long value_ssa_normalize_sysy_int_value(long long value) {
    uint32_t bits = (uint32_t)value;
    return (long long)(int32_t)bits;
}

static void value_ssa_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...) {
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

static int value_ssa_next_growth_capacity(size_t current,
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

static void value_ssa_instruction_free(ValueSsaInstruction *instruction) {
    if (!instruction) {
        return;
    }

    if (instruction->kind == VALUE_SSA_INSTR_CALL) {
        free(instruction->as.call.callee_name);
        free(instruction->as.call.args);
        instruction->as.call.callee_name = NULL;
        instruction->as.call.args = NULL;
        instruction->as.call.arg_count = 0;
    }
}

static void value_ssa_phi_free(ValueSsaPhi *phi) {
    if (!phi) {
        return;
    }

    free(phi->inputs);
    phi->inputs = NULL;
    phi->input_count = 0;
    phi->input_capacity = 0;
}

static void value_ssa_basic_block_free(ValueSsaBasicBlock *block) {
    size_t i;

    if (!block) {
        return;
    }

    for (i = 0; i < block->phi_count; ++i) {
        value_ssa_phi_free(&block->phis[i]);
    }
    free(block->phis);
    block->phis = NULL;
    block->phi_count = 0;
    block->phi_capacity = 0;

    for (i = 0; i < block->instruction_count; ++i) {
        value_ssa_instruction_free(&block->instructions[i]);
    }
    free(block->instructions);
    block->instructions = NULL;
    block->instruction_count = 0;
    block->instruction_capacity = 0;
    block->has_terminator = 0;
}

static int value_ssa_binary_op_is_removable_dead_def(ValueSsaBinaryOp op) {
    switch (op) {
    case VALUE_SSA_BINARY_ADD:
    case VALUE_SSA_BINARY_SUB:
    case VALUE_SSA_BINARY_MUL:
    case VALUE_SSA_BINARY_BIT_AND:
    case VALUE_SSA_BINARY_BIT_XOR:
    case VALUE_SSA_BINARY_BIT_OR:
    case VALUE_SSA_BINARY_EQ:
    case VALUE_SSA_BINARY_NE:
    case VALUE_SSA_BINARY_LT:
    case VALUE_SSA_BINARY_LE:
    case VALUE_SSA_BINARY_GT:
    case VALUE_SSA_BINARY_GE:
        return 1;
    case VALUE_SSA_BINARY_DIV:
    case VALUE_SSA_BINARY_MOD:
    case VALUE_SSA_BINARY_SHIFT_LEFT:
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
    default:
        return 0;
    }
}

static int value_ssa_instruction_has_side_effects(ValueSsaInstructionKind kind) {
    switch (kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
    case VALUE_SSA_INSTR_STORE_GLOBAL:
    case VALUE_SSA_INSTR_CALL:
        return 1;
    case VALUE_SSA_INSTR_MOV:
    case VALUE_SSA_INSTR_BINARY:
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
    default:
        return 0;
    }
}

static int value_ssa_instruction_is_removable_dead_def(const ValueSsaInstruction *instruction) {
    if (!instruction || !instruction->has_result || instruction->result.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }
    if (instruction->kind == VALUE_SSA_INSTR_BINARY) {
        return value_ssa_binary_op_is_removable_dead_def(instruction->as.binary.op);
    }

    return !value_ssa_instruction_has_side_effects(instruction->kind);
}

static int value_ssa_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs) {
    if (lhs.kind != rhs.kind) {
        return 0;
    }

    switch (lhs.kind) {
    case VALUE_SSA_VALUE_IMMEDIATE:
        return lhs.immediate == rhs.immediate;
    case VALUE_SSA_VALUE_ID:
        return lhs.value_id == rhs.value_id;
    default:
        return 0;
    }
}

static size_t value_ssa_pass_get_single_idom_predecessor(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id) {
    size_t predecessor_id = (size_t)-1;
    size_t candidate_id;

    if (!function || !analysis || block_id >= function->block_count) {
        return (size_t)-1;
    }
    if (block_id == 0 || analysis->predecessor_counts[block_id] != 1) {
        return (size_t)-1;
    }

    for (candidate_id = 0; candidate_id < function->block_count; ++candidate_id) {
        if (!analysis->predecessor_matrix[candidate_id * function->block_count + block_id]) {
            continue;
        }
        predecessor_id = candidate_id;
        break;
    }

    if (predecessor_id == (size_t)-1 || analysis->immediate_dominator[block_id] != predecessor_id) {
        return (size_t)-1;
    }

    return predecessor_id;
}

static int value_ssa_pass_has_single_idom_entry(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id) {
    return value_ssa_pass_get_single_idom_predecessor(function, analysis, block_id) != (size_t)-1;
}

static int value_ssa_mark_reachable_blocks(const ValueSsaFunction *function,
    unsigned char *reachable,
    ValueSsaError *error) {
    size_t *stack = NULL;
    size_t stack_size = 0;
    size_t block_count;

    if (!function || !reachable) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-042: invalid verifier contract");
        return 0;
    }
    if (function->block_count == 0) {
        return 1;
    }

    block_count = function->block_count;
    stack = (size_t *)malloc(block_count * sizeof(size_t));
    if (!stack) {
        value_ssa_set_error(error, 0, 0, "VALUE-SSA-058: out of memory while walking CFG reachability");
        return 0;
    }

    stack[stack_size++] = 0;
    reachable[0] = 1;

    while (stack_size > 0) {
        const ValueSsaBasicBlock *block = &function->blocks[stack[--stack_size]];

        switch (block->terminator.kind) {
        case VALUE_SSA_TERM_RETURN:
            break;
        case VALUE_SSA_TERM_JUMP:
            if (!reachable[block->terminator.as.jump_target]) {
                reachable[block->terminator.as.jump_target] = 1;
                stack[stack_size++] = block->terminator.as.jump_target;
            }
            break;
        case VALUE_SSA_TERM_BRANCH:
            if (!reachable[block->terminator.as.branch.then_target]) {
                reachable[block->terminator.as.branch.then_target] = 1;
                stack[stack_size++] = block->terminator.as.branch.then_target;
            }
            if (!reachable[block->terminator.as.branch.else_target]) {
                reachable[block->terminator.as.branch.else_target] = 1;
                stack[stack_size++] = block->terminator.as.branch.else_target;
            }
            break;
        default:
            free(stack);
            value_ssa_set_error(error,
                0,
                0,
                "VALUE-SSA-057: bb.%zu uses unknown terminator kind %d",
                block->id,
                (int)block->terminator.kind);
            return 0;
        }
    }

    free(stack);
    return 1;
}

#define VALUE_SSA_SPLIT_AGGREGATOR
#include "value_ssa_simplify.inc"
#include "value_ssa_load_forward.inc"
#include "value_ssa_inline_helper.inc"
#include "value_ssa_store_dce.inc"
#include "value_ssa_normalize.inc"
#include "value_ssa_identity.inc"
#include "value_ssa_fold.inc"
#include "value_ssa_cse.inc"
#include "value_ssa_perf_hotspot.inc"
#include "value_ssa_simplify_cfg.inc"
#include "value_ssa_dce.inc"
#include "value_ssa_pass_bridge.inc"
#include "value_ssa_pass_pipeline.inc"
