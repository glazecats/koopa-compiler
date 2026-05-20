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
static int value_ssa_binary_op_is_safe_reusable_pure_binary(ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs);
static int value_ssa_instruction_has_side_effects(ValueSsaInstructionKind kind);
static int value_ssa_instruction_is_removable_dead_def(const ValueSsaInstruction *instruction);
static int value_ssa_value_refs_equal(ValueSsaValueRef lhs, ValueSsaValueRef rhs);
static ValueSsaInstruction *value_ssa_pass_lookup_instruction_definition(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t value_id);
static size_t value_ssa_pass_get_single_idom_predecessor(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id);
static int value_ssa_pass_has_single_idom_entry(const ValueSsaFunction *function,
    const ValueSsaCfgAnalysis *analysis,
    size_t block_id);
static int value_ssa_mark_reachable_blocks(const ValueSsaFunction *function,
    unsigned char *reachable,
    ValueSsaError *error);
typedef enum {
    VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN = 0,
    VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT,
    VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT,
    VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER,
} ValueSsaPassAddressRootKind;

typedef struct {
    ValueSsaPassAddressRootKind kind;
    size_t id;
} ValueSsaPassAddressRoot;
static int value_ssa_pass_extract_address_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaPassAddressRoot *out_root,
    unsigned depth);
static int value_ssa_pass_store_indirect_may_alias_global(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef addr,
    size_t global_id);
static int value_ssa_pass_same_pure_address_value(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    unsigned depth);
static int value_ssa_pass_value_may_depend_on_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaSlotRef slot,
    unsigned depth);
static int value_ssa_pass_instruction_may_invalidate_address_value(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef address_value,
    const ValueSsaInstruction *instruction);
static int value_ssa_pass_address_roots_proven_non_alias(const ValueSsaPassAddressRoot *lhs,
    const ValueSsaPassAddressRoot *rhs);
static int value_ssa_pass_store_cannot_alias_loaded_address(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPassAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction);

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
        return 0;
    default:
        return 0;
    }
}

static int value_ssa_binary_op_is_safe_reusable_pure_binary(ValueSsaBinaryOp op,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs) {
    int32_t rhs32;

    (void)lhs;

    if (value_ssa_binary_op_is_removable_dead_def(op)) {
        return 1;
    }

    switch (op) {
    case VALUE_SSA_BINARY_DIV:
    case VALUE_SSA_BINARY_MOD:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
            rhs32 = (int32_t)value_ssa_normalize_sysy_int_value(rhs.immediate);
            if (rhs32 == 0) {
                return 0;
            }
        }
        return 1;
    case VALUE_SSA_BINARY_SHIFT_LEFT:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
            rhs32 = (int32_t)value_ssa_normalize_sysy_int_value(rhs.immediate);
            return rhs32 >= 0 && rhs32 < 32;
        }
        return 1;
    case VALUE_SSA_BINARY_SHIFT_RIGHT:
        if (rhs.kind == VALUE_SSA_VALUE_IMMEDIATE) {
            rhs32 = (int32_t)value_ssa_normalize_sysy_int_value(rhs.immediate);
            return rhs32 >= 0 && rhs32 < 32;
        }
        return 1;
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

static ValueSsaInstruction *value_ssa_pass_lookup_instruction_definition(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    size_t value_id) {
    size_t block_id;
    size_t instruction_index;

    if (!function || !analysis || value_id >= analysis->value_count || !analysis->has_def[value_id]) {
        return NULL;
    }

    instruction_index = analysis->def_instruction_indices[value_id];
    if (instruction_index == (size_t)-1) {
        return NULL;
    }

    block_id = analysis->def_block_ids[value_id];
    if (block_id >= function->block_count || instruction_index >= function->blocks[block_id].instruction_count) {
        return NULL;
    }

    return &function->blocks[block_id].instructions[instruction_index];
}

static int value_ssa_pass_extract_address_root(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaPassAddressRoot *out_root,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (!out_root) {
        return 0;
    }

    out_root->kind = VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN;
    out_root->id = 0u;
    if (!function || !analysis || depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 0;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
        out_root->kind = VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        out_root->kind = VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT;
        out_root->id = definition->as.addr_slot.id;
        return 1;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        if (definition->as.load_slot.kind == VALUE_SSA_SLOT_LOCAL &&
            definition->as.load_slot.id < function->local_count &&
            function->locals[definition->as.load_slot.id].is_parameter &&
            function->locals[definition->as.load_slot.id].array_rank > 0u) {
            out_root->kind = VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER;
            out_root->id = definition->as.load_slot.id;
            return 1;
        }
        return 0;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_pass_extract_address_root(
            function, analysis, definition->as.mov_value, out_root, depth + 1u);
    case VALUE_SSA_INSTR_BINARY: {
        ValueSsaPassAddressRoot lhs_root;
        ValueSsaPassAddressRoot rhs_root;
        int has_lhs_root;
        int has_rhs_root;

        if (definition->as.binary.op != VALUE_SSA_BINARY_ADD) {
            return 0;
        }

        has_lhs_root = value_ssa_pass_extract_address_root(
            function, analysis, definition->as.binary.lhs, &lhs_root, depth + 1u);
        has_rhs_root = value_ssa_pass_extract_address_root(
            function, analysis, definition->as.binary.rhs, &rhs_root, depth + 1u);
        if (has_lhs_root == has_rhs_root) {
            return 0;
        }

        *out_root = has_lhs_root ? lhs_root : rhs_root;
        return 1;
    }
    default:
        return 0;
    }
}

static int value_ssa_pass_store_indirect_may_alias_global(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef addr,
    size_t global_id) {
    ValueSsaPassAddressRoot root;

    if (!function || !analysis) {
        return 1;
    }
    if (!value_ssa_pass_extract_address_root(function, analysis, addr, &root, 0u)) {
        return 1;
    }

    switch (root.kind) {
    case VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT:
        return 0;
    case VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT:
        return root.id == global_id;
    case VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER:
    case VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN:
    default:
        return 1;
    }
}

static int value_ssa_pass_same_pure_address_value(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef lhs,
    ValueSsaValueRef rhs,
    unsigned depth) {
    ValueSsaInstruction *lhs_def;
    ValueSsaInstruction *rhs_def;

    if (depth > 8u) {
        return 0;
    }
    if (value_ssa_value_refs_equal(lhs, rhs)) {
        return 1;
    }
    if (!function || !analysis || lhs.kind != VALUE_SSA_VALUE_ID || rhs.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }

    lhs_def = value_ssa_pass_lookup_instruction_definition(function, analysis, lhs.value_id);
    rhs_def = value_ssa_pass_lookup_instruction_definition(function, analysis, rhs.value_id);
    if (!lhs_def || !rhs_def || lhs_def->kind != rhs_def->kind) {
        return 0;
    }

    switch (lhs_def->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return lhs_def->as.addr_slot.kind == rhs_def->as.addr_slot.kind &&
            lhs_def->as.addr_slot.id == rhs_def->as.addr_slot.id;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
        return lhs_def->as.load_slot.kind == rhs_def->as.load_slot.kind &&
            lhs_def->as.load_slot.id == rhs_def->as.load_slot.id;
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return lhs_def->as.load_slot.kind == rhs_def->as.load_slot.kind &&
            lhs_def->as.load_slot.id == rhs_def->as.load_slot.id;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_pass_same_pure_address_value(
            function, analysis, lhs_def->as.mov_value, rhs_def->as.mov_value, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return lhs_def->as.binary.op == rhs_def->as.binary.op &&
            value_ssa_pass_same_pure_address_value(
                function, analysis, lhs_def->as.binary.lhs, rhs_def->as.binary.lhs, depth + 1u) &&
            value_ssa_pass_same_pure_address_value(
                function, analysis, lhs_def->as.binary.rhs, rhs_def->as.binary.rhs, depth + 1u);
    default:
        return 0;
    }
}

static int value_ssa_pass_value_may_depend_on_slot(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef value,
    ValueSsaSlotRef slot,
    unsigned depth) {
    ValueSsaInstruction *definition;

    if (depth > 8u || value.kind != VALUE_SSA_VALUE_ID) {
        return 0;
    }
    if (!function || !analysis) {
        return 1;
    }

    definition = value_ssa_pass_lookup_instruction_definition(function, analysis, value.value_id);
    if (!definition) {
        return 1;
    }

    switch (definition->kind) {
    case VALUE_SSA_INSTR_ADDR_LOCAL:
    case VALUE_SSA_INSTR_ADDR_GLOBAL:
        return 0;
    case VALUE_SSA_INSTR_LOAD_LOCAL:
    case VALUE_SSA_INSTR_LOAD_GLOBAL:
        return definition->as.load_slot.kind == slot.kind &&
            definition->as.load_slot.id == slot.id;
    case VALUE_SSA_INSTR_MOV:
        return value_ssa_pass_value_may_depend_on_slot(
            function, analysis, definition->as.mov_value, slot, depth + 1u);
    case VALUE_SSA_INSTR_BINARY:
        return value_ssa_pass_value_may_depend_on_slot(
                   function, analysis, definition->as.binary.lhs, slot, depth + 1u) ||
            value_ssa_pass_value_may_depend_on_slot(
                   function, analysis, definition->as.binary.rhs, slot, depth + 1u);
    default:
        return 1;
    }
}

static int value_ssa_pass_instruction_may_invalidate_address_value(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    ValueSsaValueRef address_value,
    const ValueSsaInstruction *instruction) {
    ValueSsaSlotRef slot;

    if (!function || !analysis || !instruction) {
        return 1;
    }

    switch (instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        slot = instruction->as.store.slot;
        return value_ssa_pass_value_may_depend_on_slot(function, analysis, address_value, slot, 0u);
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        slot = instruction->as.store.slot;
        return value_ssa_pass_value_may_depend_on_slot(function, analysis, address_value, slot, 0u);
    default:
        return 0;
    }
}

static int value_ssa_pass_address_roots_proven_non_alias(const ValueSsaPassAddressRoot *lhs,
    const ValueSsaPassAddressRoot *rhs) {
    if (!lhs || !rhs ||
        lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN ||
        rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_UNKNOWN) {
        return 0;
    }

    if (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT &&
        rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT &&
        rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT) {
        return lhs->id != rhs->id;
    }
    if ((lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT) ||
        (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }
    if ((lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER) ||
        (lhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER &&
            rhs->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT)) {
        return 1;
    }

    return 0;
}

static int value_ssa_pass_store_cannot_alias_loaded_address(const ValueSsaFunction *function,
    const ValueSsaDefUseAnalysis *analysis,
    const ValueSsaPassAddressRoot *load_root,
    const ValueSsaInstruction *store_instruction) {
    ValueSsaPassAddressRoot store_root;

    if (!function || !analysis || !load_root || !store_instruction) {
        return 0;
    }

    switch (store_instruction->kind) {
    case VALUE_SSA_INSTR_STORE_LOCAL:
        if (load_root->kind != VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT ||
            store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_LOCAL ||
            store_instruction->as.store.slot.id >= function->local_count) {
            return load_root->kind != VALUE_SSA_PASS_ADDRESS_ROOT_PARAMETER_POINTER;
        }
        if (store_instruction->as.store.slot.id == load_root->id) {
            return 0;
        }
        return function->locals[store_instruction->as.store.slot.id].array_rank == 0u;
    case VALUE_SSA_INSTR_STORE_GLOBAL:
        if (store_instruction->as.store.slot.kind != VALUE_SSA_SLOT_GLOBAL) {
            return 0;
        }
        if (load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_GLOBAL_SLOT) {
            return store_instruction->as.store.slot.id != load_root->id;
        }
        return load_root->kind == VALUE_SSA_PASS_ADDRESS_ROOT_LOCAL_SLOT;
    case VALUE_SSA_INSTR_STORE_INDIRECT:
        if (!value_ssa_pass_extract_address_root(
                function, analysis, store_instruction->as.store_indirect.addr, &store_root, 0u)) {
            return 0;
        }
        return value_ssa_pass_address_roots_proven_non_alias(load_root, &store_root);
    default:
        return 1;
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
#include "value_ssa_sccp.inc"
#include "value_ssa_licm.inc"
#include "value_ssa_cse.inc"
#include "../value_ssa_perf/value_ssa_perf_entry_hoist.inc"
#include "../value_ssa_perf/value_ssa_perf_dispatch.inc"
#include "../value_ssa_perf/value_ssa_perf_induction.inc"
#include "../value_ssa_perf/value_ssa_perf_loop_memory.inc"
#include "../value_ssa_perf/value_ssa_perf_mm.inc"
#include "../value_ssa_perf/value_ssa_perf_spmv.inc"
#include "../value_ssa_perf/value_ssa_recursive_helper.inc"
#include "value_ssa_perf_hotspot.inc"
#include "value_ssa_simplify_cfg.inc"
#include "value_ssa_dce.inc"
#include "value_ssa_pass_bridge.inc"
#include "value_ssa_pass_pipeline.inc"
